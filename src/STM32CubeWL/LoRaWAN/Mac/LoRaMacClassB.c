/*!
 * \file      LoRaMacClassB.c
 *
 * \brief     LoRa MAC Class B layer implementation
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013 Semtech
 *
 *               ___ _____ _   ___ _  _____ ___  ___  ___ ___
 *              / __|_   _/_\ / __| |/ / __/ _ \| _ \/ __| __|
 *              \__ \ | |/ _ \ (__| ' <| _| (_) |   / (__| _|
 *              |___/ |_/_/ \_\___|_|\_\_| \___/|_|_\\___|___|
 *              embedded.connectivity.solutions===============
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 *
 * \author    Daniel Jaeckle ( STACKFORCE )
 */
/**
  ******************************************************************************
  *
  *          Portions COPYRIGHT 2020 STMicroelectronics
  *
  * @file    LoRaMacClassB.c
  * @author  MCD Application Team
  * @brief   LoRa MAC Class B layer implementation
  ******************************************************************************
  */
#include <math.h>
#include "../Utilities/utilities.h"
#include "secure-element.h"
#include "LoRaMac.h"
#include "LoRaMacVersion.h"
#include "LoRaMacClassB.h"
#include "LoRaMacClassBNvm.h"
#include "LoRaMacClassBConfig.h"
#include "LoRaMacConfirmQueue.h"
#include "../../SubGHz_Phy/radio.h"
#include "Region/Region.h"
#include "../../../Glue/mw_log_conf.h"

#if ( LORAMAC_CLASSB_ENABLED == 1 )

/*
 * LoRaMac Class B Context structure
 */
typedef struct sLoRaMacClassBCtx
{
    /*!
    * Class B ping slot context
    */
    PingSlotContext_t PingSlotCtx;
    /*!
    * Class B beacon context
    */
    BeaconContext_t BeaconCtx;
    /*!
    * State of the beaconing mechanism
    */
    BeaconState_t BeaconState;
    /*!
    * State of the ping slot mechanism
    */
    PingSlotState_t PingSlotState;
    /*!
    * State of the multicast slot mechanism
    */
    PingSlotState_t MulticastSlotState;
    /*!
    * Timer for CLASS B beacon acquisition and tracking.
    */
    TimerEvent_t BeaconTimer;
    /*!
    * Timer for CLASS B ping slot timer.
    */
    TimerEvent_t PingSlotTimer;
    /*!
    * Timer for CLASS B multicast ping slot timer.
    */
    TimerEvent_t MulticastSlotTimer;
    /*!
    * Container for the callbacks related to class b.
    */
    LoRaMacClassBCallback_t LoRaMacClassBCallbacks;
    /*!
    * Data structure which holds the parameters which needs to be set
    * in class b operation.
    */
    LoRaMacClassBParams_t LoRaMacClassBParams;
} LoRaMacClassBCtx_t;

/*!
 * Defines the LoRaMac radio events status
 */
typedef union uLoRaMacClassBEvents
{
    uint32_t Value;
    struct sEvents
    {
        uint32_t Beacon        : 1;
        uint32_t PingSlot      : 1;
        uint32_t MulticastSlot : 1;
    }Events;
}LoRaMacClassBEvents_t;

LoRaMacClassBEvents_t LoRaMacClassBEvents = { .Value = 0 };

/*
 * Module context.
 */
static LoRaMacClassBCtx_t Ctx;

/*
 * Beacon transmit time precision in milliseconds.
 * The usage of these values shall be determined by the
 * prec value in param field received in a beacon frame.
 * As the time base is milli seconds, the precision will be either 0 ms or 1 ms.
 */
static const uint8_t BeaconPrecTimeValue[4] = { 0, 1, 1, 1 };

/*!
 * Data structure which holds the parameters which needs to be stored
 * in the NVM.
 */
static LoRaMacClassBNvmData_t* ClassBNvm;

// The CRC calculation follows CRC16-CCITT
static const uint16_t polynom = 0x1021;

/* ST_WORKAROUND_BEGIN: Move timer function (ClassB specific) */
/*!
 * \brief Computes the temperature compensation for a period of time on a
 *        specific temperature.
 *
 * \param [in] period Time period to compensate
 * \param [in] temperature Current temperature
 *
 * \retval Compensated time period
 */
static TimerTime_t TimerTempCompensation( TimerTime_t period, int16_t temperature );

static TimerTime_t TimerTempCompensation( TimerTime_t period, int16_t temperature )
{
  float k = RTC_TEMP_COEFFICIENT;
  float kDev = RTC_TEMP_DEV_COEFFICIENT;
  float t = RTC_TEMP_TURNOVER;
  float tDev = RTC_TEMP_DEV_TURNOVER;
  float interim = 0.0f;
  float ppm = 0.0f;

  if (k < 0.0f)
  {
    ppm = (k - kDev);
  }
  else
  {
    ppm = (k + kDev);
  }
  interim = (float)temperature - (t - tDev);
  ppm *=  interim * interim;

  // Calculate the drift in time
  interim = ((float) period * ppm) / 1000000.0f;
  // Calculate the resulting time period
  interim += period;
  interim = floor(interim);

  if (interim < 0.0f)
  {
    interim = (float)period;
  }

  // Calculate the resulting period
  return ( TimerTime_t ) interim;
}
/* ST_WORKAROUND_END */

/*!
 * Computes the Ping Offset
 *
 * \param [in]  beaconTime      - Time of the recent received beacon
 * \param [in]  address         - Frame address
 * \param [in]  pingPeriod      - Ping period of the node
 * \param [out] pingOffset      - Pseudo random ping offset
 */
static void ComputePingOffset( uint64_t beaconTime, uint32_t address, uint16_t pingPeriod, uint16_t *pingOffset )
{
    uint8_t buffer[16];
    uint8_t cipher[16];
    uint32_t result = 0;
    /* Refer to chapter 15.2 of the LoRaWAN specification v1.1. The beacon time
     * GPS time in seconds modulo 2^32
     */
    uint32_t time = ( beaconTime % ( ( ( uint64_t ) 1 ) << 32 ) );

    memset1( buffer, 0, 16 );
    memset1( cipher, 0, 16 );

    buffer[0] = ( time ) & 0xFF;
    buffer[1] = ( time >> 8 ) & 0xFF;
    buffer[2] = ( time >> 16 ) & 0xFF;
    buffer[3] = ( time >> 24 ) & 0xFF;

    buffer[4] = ( address ) & 0xFF;
    buffer[5] = ( address >> 8 ) & 0xFF;
    buffer[6] = ( address >> 16 ) & 0xFF;
    buffer[7] = ( address >> 24 ) & 0xFF;

    SecureElementAesEncrypt( buffer, 16, SLOT_RAND_ZERO_KEY, cipher );

    result = ( ( ( uint32_t ) cipher[0] ) + ( ( ( uint32_t ) cipher[1] ) * 256 ) );

    *pingOffset = ( uint16_t )( result % pingPeriod );
}

/*!
 * \brief Calculates the downlink frequency for a given channel.
 *
 * \param [in] channel The channel according to the channel plan.
 *
 * \param [in] isBeacon Set to true, if the function shall
 *                      calculate the frequency for a beacon.
 *
 * \retval The downlink frequency
 */
static uint32_t CalcDownlinkFrequency( uint8_t channel, bool isBeacon )
{
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;

    getPhy.Attribute = PHY_PING_SLOT_CHANNEL_FREQ;

    if( isBeacon == true )
    {
        getPhy.Attribute = PHY_BEACON_CHANNEL_FREQ;
    }
    getPhy.Channel = channel;
    phyParam = RegionGetPhyParam( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &getPhy );

    return phyParam.Value;
}

/*!
 * \brief Calculates the downlink channel for the beacon and for
 *        ping slot downlinks.
 *
 * \param [in] devAddr The address of the device. Assign 0 if its a beacon.
 *
 * \param [in] beaconTime The beacon time of the beacon.
 *
 * \param [in] beaconInterval The beacon interval.
 *
 * \param [in] isBeacon Set to true, if the function shall
 *                      calculate the frequency for a beacon.
 *
 * \retval The downlink channel
 */
static uint32_t CalcDownlinkChannelAndFrequency( uint32_t devAddr, TimerTime_t beaconTime,
                                                 TimerTime_t beaconInterval, bool isBeacon )
{
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;
    uint32_t channel = 0;
    uint8_t nbChannels = 0;
    uint8_t offset = 0;

    // Default initialization - ping slot channels
    getPhy.Attribute = PHY_PING_SLOT_NB_CHANNELS;

    if( isBeacon == true )
    {
        // Beacon channels
        getPhy.Attribute = PHY_BEACON_NB_CHANNELS;
    }
    phyParam = RegionGetPhyParam( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &getPhy );
    nbChannels = ( uint8_t ) phyParam.Value;

    // nbChannels is > 1, when the channel plan requires more than one possible channel
    // defined by the calculation below.
    if( nbChannels > 1 )
    {
        getPhy.Attribute = PHY_BEACON_CHANNEL_OFFSET;
        phyParam = RegionGetPhyParam( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &getPhy );
        offset = ( uint8_t ) phyParam.Value;

        // Calculate the channel for the next downlink
        channel = devAddr + ( beaconTime / ( beaconInterval / 1000 ) );
        channel = channel % nbChannels;
        channel += offset;
    }

    // Calculate the frequency for the next downlink. This holds
    // for beacons and ping slots.
    return CalcDownlinkFrequency( channel, isBeacon );
}

/*!
 * \brief Calculates the correct frequency and opens up the beacon reception window. Please
 *        note that the variable WindowTimeout and WindowOffset will be updated according
 *        to the current settings. Also, the function perform a calculation only, when
 *        Ctx.BeaconCtx.Ctrl.BeaconAcquired OR Ctx.BeaconCtx.Ctrl.AcquisitionPending is
 *        set to 1.
 *
 * \param [in] rxConfig Reception parameters for the beacon window.
 *
 * \param [in] currentSymbolTimeout Current symbol timeout.
 */
static void CalculateBeaconRxWindowConfig( RxConfigParams_t* rxConfig, uint16_t currentSymbolTimeout )
{
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;
#if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
    uint32_t maxRxError = 0;
#endif /* LORAMAC_VERSION */

    rxConfig->WindowTimeout = currentSymbolTimeout;
    rxConfig->WindowOffset = 0;

    if( ( Ctx.BeaconCtx.Ctrl.BeaconAcquired == 1 ) || ( Ctx.BeaconCtx.Ctrl.AcquisitionPending == 1 ) )
    {
        // Apply the symbol timeout only if we have acquired the beacon
        // Otherwise, take the window enlargement into account
        // Read beacon datarate
        getPhy.Attribute = PHY_BEACON_CHANNEL_DR;
        phyParam = RegionGetPhyParam( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &getPhy );

#if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000300 ))
        // Calculate downlink symbols
        RegionComputeRxWindowParameters( *Ctx.LoRaMacClassBParams.LoRaMacRegion,
                                        ( int8_t )phyParam.Value, // datarate
                                        Ctx.LoRaMacClassBParams.LoRaMacParams->MinRxSymbols,
                                        Ctx.LoRaMacClassBParams.LoRaMacParams->SystemMaxRxError,
                                        rxConfig );
    }
#elif (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
        // Compare and assign the maximum between the region specific rx error window time
        // and time precision received from beacon frame format.
        maxRxError = MAX( Ctx.LoRaMacClassBParams.LoRaMacParams->SystemMaxRxError,
                          ( uint32_t ) Ctx.BeaconCtx.BeaconTimePrecision.SubSeconds );

        // Calculate downlink symbols
        RegionComputeRxWindowParameters( *Ctx.LoRaMacClassBParams.LoRaMacRegion,
                                        ( int8_t )phyParam.Value, // datarate
                                        Ctx.LoRaMacClassBParams.LoRaMacParams->MinRxSymbols,
                                        maxRxError,
                                        rxConfig );
    }
#endif /* LORAMAC_VERSION */
}

/*!
 * \brief Calculates the correct frequency and opens up the beacon reception window.
 *
 * \param [in] rxTime The reception time which should be setup
 *
 * \param [in] activateDefaultChannel Set to true, if the function shall setup the default channel
 *
 * \param [in] symbolTimeout Symbol timeout
 */
static void RxBeaconSetup( TimerTime_t rxTime, bool activateDefaultChannel, uint16_t symbolTimeout )
{
    RxBeaconSetup_t rxBeaconSetup;
    uint32_t frequency = 0;

    if( activateDefaultChannel == true )
    {
        // This is the default frequency in case we don't know when the next
        // beacon will be transmitted. We select channel 0 as default.
        frequency = CalcDownlinkFrequency( 0, true );
    }
    else
    {
        // This is the frequency according to the channel plan
        frequency = CalcDownlinkChannelAndFrequency( 0, Ctx.BeaconCtx.BeaconTime.Seconds + ( CLASSB_BEACON_INTERVAL / 1000 ),
                                                     CLASSB_BEACON_INTERVAL, true );
    }

    if( ClassBNvm->BeaconCtx.Ctrl.CustomFreq == 1 )
    {
        // Set the frequency from the BeaconFreqReq
        frequency = ClassBNvm->BeaconCtx.Frequency;
    }

    if( Ctx.BeaconCtx.Ctrl.BeaconChannelSet == 1 )
    {
        // Set the frequency which was provided by BeaconTimingAns MAC command
        Ctx.BeaconCtx.Ctrl.BeaconChannelSet = 0;
        frequency = CalcDownlinkFrequency( Ctx.BeaconCtx.BeaconTimingChannel, true );
    }

    rxBeaconSetup.SymbolTimeout = symbolTimeout;
    rxBeaconSetup.RxTime = rxTime;
    rxBeaconSetup.Frequency = frequency;

    RegionRxBeaconSetup( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &rxBeaconSetup, &Ctx.LoRaMacClassBParams.McpsIndication->RxDatarate );

    Ctx.LoRaMacClassBParams.MlmeIndication->BeaconInfo.Frequency = frequency;
    Ctx.LoRaMacClassBParams.MlmeIndication->BeaconInfo.Datarate = Ctx.LoRaMacClassBParams.McpsIndication->RxDatarate;
}

/*!
 * \brief Calculates the next ping slot time.
 *
 * \param [in] slotOffset The ping slot offset
 * \param [in] pingPeriod The ping period
 * \param [out] timeOffset Time offset of the next slot, based on current time
 *
 * \retval [true: ping slot found, false: no ping slot found]
 */
static bool CalcNextSlotTime( uint16_t slotOffset, uint16_t pingPeriod, uint16_t pingNb, TimerTime_t* timeOffset )
{
    uint8_t currentPingSlot = 0;
    TimerTime_t slotTime = 0;
    TimerTime_t currentTime = TimerGetCurrentTime( );

    // Calculate the point in time of the last beacon even if we missed it
    slotTime = ( ( currentTime - SysTimeToMs( Ctx.BeaconCtx.LastBeaconRx ) ) % CLASSB_BEACON_INTERVAL );
    slotTime = currentTime - slotTime;

    // Add the reserved time and the ping offset
    slotTime += CLASSB_BEACON_RESERVED;
    slotTime += slotOffset * CLASSB_PING_SLOT_WINDOW;

    if( slotTime < currentTime )
    {
        currentPingSlot = ( ( currentTime - slotTime ) /
                          ( pingPeriod * CLASSB_PING_SLOT_WINDOW ) ) + 1;
        slotTime += ( ( TimerTime_t )( currentPingSlot * pingPeriod ) *
                    CLASSB_PING_SLOT_WINDOW );
    }

    if( currentPingSlot < pingNb )
    {
        if( slotTime <= ( SysTimeToMs( Ctx.BeaconCtx.NextBeaconRx ) - CLASSB_BEACON_GUARD - CLASSB_PING_SLOT_WINDOW ) )
        {
            // Calculate the relative ping slot time
            slotTime -= currentTime;
            slotTime -= Radio.GetWakeupTime( );
            slotTime = TimerTempCompensation( slotTime, Ctx.BeaconCtx.Temperature );
            *timeOffset = slotTime;
            return true;
        }
    }
    return false;
}

/*!
 * \brief Calculates CRC's of the beacon frame
 *
 * \param [in] buffer Pointer to the data
 * \param [in] length Length of the data
 *
 * \retval CRC
 */
static uint16_t BeaconCrc( uint8_t *buffer, uint16_t length )
{
    // CRC initial value
    uint16_t crc = 0x0000;

    if( buffer == NULL )
    {
        return 0;
    }

    for( uint16_t i = 0; i < length; ++i )
    {
        crc ^= ( uint16_t ) buffer[i] << 8;
        for( uint16_t j = 0; j < 8; ++j )
        {
            crc = ( crc & 0x8000 ) ? ( crc << 1 ) ^ polynom : ( crc << 1 );
        }
    }

    return crc;
}

static void GetTemperature( LoRaMacClassBCallback_t *callbacks, BeaconContext_t *beaconCtx )
{
    // Measure temperature, if available
    if( ( callbacks != NULL ) && ( callbacks->GetTemperatureLevel != NULL ) )
    {
        beaconCtx->Temperature = callbacks->GetTemperatureLevel( );
    }
}

static void InitClassB( void )
{
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;

    // Init events
    LoRaMacClassBEvents.Value = 0;

    // Init variables to default
    memset1( ( uint8_t* ) ClassBNvm, 0, sizeof( LoRaMacClassBNvmData_t ) );
    memset1( ( uint8_t* ) &Ctx.PingSlotCtx, 0, sizeof( PingSlotContext_t ) );
    memset1( ( uint8_t* ) &Ctx.BeaconCtx, 0, sizeof( BeaconContext_t ) );

    // Setup default temperature
    Ctx.BeaconCtx.Temperature = 25.0;
    GetTemperature( &Ctx.LoRaMacClassBCallbacks, &Ctx.BeaconCtx );

    // Setup default ping slot datarate
    getPhy.Attribute = PHY_PING_SLOT_CHANNEL_DR;
    phyParam = RegionGetPhyParam( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &getPhy );
    ClassBNvm->PingSlotCtx.Datarate = ( int8_t )( phyParam.Value );

#if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
    // Setup default FPending bit
    ClassBNvm->PingSlotCtx.FPendingSet = 0;
#endif /* LORAMAC_VERSION */

    // Setup default states
    Ctx.BeaconState = BEACON_STATE_ACQUISITION;
    Ctx.PingSlotState = PINGSLOT_STATE_CALC_PING_OFFSET;
    Ctx.MulticastSlotState = PINGSLOT_STATE_CALC_PING_OFFSET;
}

static void InitClassBDefaults( void )
{
    // This function shall reset the Class B settings to default,
    // but should keep important configurations
    LoRaMacClassBBeaconNvmData_t beaconCtx = ClassBNvm->BeaconCtx;
    LoRaMacClassBPingSlotNvmData_t pingSlotCtx = ClassBNvm->PingSlotCtx;

    InitClassB( );

    // Parameters from BeaconFreqReq
    ClassBNvm->BeaconCtx.Frequency = beaconCtx.Frequency;
    ClassBNvm->BeaconCtx.Ctrl.CustomFreq = beaconCtx.Ctrl.CustomFreq;

    // Parameters from PingSlotChannelReq
    ClassBNvm->PingSlotCtx.Ctrl.CustomFreq = pingSlotCtx.Ctrl.CustomFreq;
    ClassBNvm->PingSlotCtx.Frequency = pingSlotCtx.Frequency;
    ClassBNvm->PingSlotCtx.Datarate = pingSlotCtx.Datarate;
}

static void EnlargeWindowTimeout( void )
{
    // Update beacon movement
    Ctx.BeaconCtx.BeaconWindowMovement *= CLASSB_WINDOW_MOVE_EXPANSION_FACTOR;
    if( Ctx.BeaconCtx.BeaconWindowMovement > CLASSB_WINDOW_MOVE_EXPANSION_MAX )
    {
        Ctx.BeaconCtx.BeaconWindowMovement = CLASSB_WINDOW_MOVE_EXPANSION_MAX;
    }
    // Update symbol timeout
    Ctx.BeaconCtx.SymbolTimeout *= CLASSB_BEACON_SYMBOL_TO_EXPANSION_FACTOR;
    if( Ctx.BeaconCtx.SymbolTimeout > CLASSB_BEACON_SYMBOL_TO_EXPANSION_MAX )
    {
        Ctx.BeaconCtx.SymbolTimeout = CLASSB_BEACON_SYMBOL_TO_EXPANSION_MAX;
    }
    Ctx.PingSlotCtx.SymbolTimeout *= CLASSB_BEACON_SYMBOL_TO_EXPANSION_FACTOR;
    if( Ctx.PingSlotCtx.SymbolTimeout > CLASSB_PING_SLOT_SYMBOL_TO_EXPANSION_MAX )
    {
        Ctx.PingSlotCtx.SymbolTimeout = CLASSB_PING_SLOT_SYMBOL_TO_EXPANSION_MAX;
    }
}

static void ResetWindowTimeout( void )
{
    Ctx.BeaconCtx.SymbolTimeout = CLASSB_BEACON_SYMBOL_TO_DEFAULT;
    Ctx.PingSlotCtx.SymbolTimeout = CLASSB_BEACON_SYMBOL_TO_DEFAULT;
    Ctx.BeaconCtx.BeaconWindowMovement  = CLASSB_WINDOW_MOVE_DEFAULT;
}

static TimerTime_t CalcDelayForNextBeacon( TimerTime_t currentTime, TimerTime_t lastBeaconRx )
{
    TimerTime_t nextBeaconRxTime = 0;

    // Calculate the point in time of the next beacon
    nextBeaconRxTime = ( ( currentTime - lastBeaconRx ) % CLASSB_BEACON_INTERVAL );
    return ( CLASSB_BEACON_INTERVAL - nextBeaconRxTime );
}

static void IndicateBeaconStatus( LoRaMacEventInfoStatus_t status )
{
    if( Ctx.BeaconCtx.Ctrl.ResumeBeaconing == 0 )
    {
        Ctx.LoRaMacClassBParams.MlmeIndication->MlmeIndication = MLME_BEACON;
        Ctx.LoRaMacClassBParams.MlmeIndication->Status = status;
        Ctx.LoRaMacClassBParams.LoRaMacFlags->Bits.MlmeInd = 1;

        Ctx.LoRaMacClassBParams.LoRaMacFlags->Bits.MacDone = 1;
    }
    Ctx.BeaconCtx.Ctrl.ResumeBeaconing = 0;
}

static TimerTime_t ApplyGuardTime( TimerTime_t beaconEventTime )
{
    TimerTime_t timeGuard = beaconEventTime;

    if( timeGuard > CLASSB_BEACON_GUARD )
    {
        timeGuard -= CLASSB_BEACON_GUARD;
    }
    return timeGuard;
}

static TimerTime_t UpdateBeaconState( LoRaMacEventInfoStatus_t status,
                                      TimerTime_t windowMovement, TimerTime_t currentTime )

{
    TimerTime_t beaconEventTime = 0;

    // Calculate the next beacon RX time
    beaconEventTime = CalcDelayForNextBeacon( currentTime, SysTimeToMs( Ctx.BeaconCtx.LastBeaconRx ) );
    Ctx.BeaconCtx.NextBeaconRx = SysTimeFromMs( currentTime + beaconEventTime );

    // Take temperature compensation into account
    beaconEventTime = TimerTempCompensation( beaconEventTime, Ctx.BeaconCtx.Temperature );

    // Move the window
    if( beaconEventTime > windowMovement )
    {
        beaconEventTime -= windowMovement;
    }
    Ctx.BeaconCtx.NextBeaconRxAdjusted = currentTime + beaconEventTime;

    // Start the RX slot state machine for ping and multicast slots
    LoRaMacClassBStartRxSlots( );

    // Setup an MLME_BEACON indication to inform the upper layer
    IndicateBeaconStatus( status );

    // Apply guard time
    return ApplyGuardTime( beaconEventTime );
}

static uint8_t CalcPingNb( uint16_t periodicity )
{
    return 128 / ( 1 << periodicity );
}

static uint16_t CalcPingPeriod( uint8_t pingNb )
{
    return CLASSB_BEACON_WINDOW_SLOTS / pingNb;
}

#if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
static bool CheckSlotPriority( uint32_t currentAddress, uint8_t currentFPendingSet, uint8_t currentIsMulticast,
                               uint32_t address, uint8_t fPendingSet, uint8_t isMulticast )
{
    if( currentFPendingSet != fPendingSet )
    {
        if( currentFPendingSet < fPendingSet )
        {
            // New slot sequence has priority. It does not matter
            // which type it is
            return true;
        }
        return false;
    }
    else
    {
        // FPendingSet has the same priority level, decide
        // based on multicast or unicast setting
        if( currentIsMulticast != isMulticast )
        {
            if( currentIsMulticast < isMulticast )
            {
                // New slot sequence has priority. Multicasts have
                // more priority than unicasts
                return true;
            }
            return false;
        }
        else
        {
            // IsMulticast has the same priority level, decide
            // based on the highest address
            if( currentAddress < address )
            {
                // New slot sequence has priority. The sequence with
                // the highest address has priority
                return true;
            }
        }
    }
    return false;
}
#endif /* LORAMAC_VERSION */

#endif /* LORAMAC_CLASSB_ENABLED */

void LoRaMacClassBInit( LoRaMacClassBParams_t *classBParams, LoRaMacClassBCallback_t *callbacks, LoRaMacClassBNvmData_t* nvm )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    // Assign non-volatile context
    if( nvm == NULL )
    {
        return;
    }
    ClassBNvm = nvm;

    // Store callbacks
    Ctx.LoRaMacClassBCallbacks = *callbacks;

    // Store parameter pointers
    Ctx.LoRaMacClassBParams = *classBParams;

    // Initialize timers
    TimerInit( &Ctx.BeaconTimer, LoRaMacClassBBeaconTimerEvent );
    TimerInit( &Ctx.PingSlotTimer, LoRaMacClassBPingSlotTimerEvent );
    TimerInit( &Ctx.MulticastSlotTimer, LoRaMacClassBMulticastSlotTimerEvent );

    InitClassB( );
#endif /* LORAMAC_CLASSB_ENABLED */
}

void LoRaMacClassBSetBeaconState( BeaconState_t beaconState )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    if( beaconState == BEACON_STATE_ACQUISITION )
    {
        // If the MAC has received a time reference for the beacon,
        // apply the state BEACON_STATE_ACQUISITION_BY_TIME.
        if( ( Ctx.BeaconCtx.Ctrl.BeaconDelaySet == 1 ) &&
            ( LoRaMacClassBIsAcquisitionPending( ) == false ) )
        {
            Ctx.BeaconState = BEACON_STATE_ACQUISITION_BY_TIME;
        }
        else
        {
           Ctx.BeaconState = beaconState;
        }
    }
    else
    {
        if( ( Ctx.BeaconState != BEACON_STATE_ACQUISITION ) &&
            ( Ctx.BeaconState != BEACON_STATE_ACQUISITION_BY_TIME ) )
        {
            Ctx.BeaconState = beaconState;
        }
    }
#endif /* LORAMAC_CLASSB_ENABLED */
}

void LoRaMacClassBSetPingSlotState( PingSlotState_t pingSlotState )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    Ctx.PingSlotState = pingSlotState;
#endif /* LORAMAC_CLASSB_ENABLED */
}

void LoRaMacClassBSetMulticastSlotState( PingSlotState_t multicastSlotState )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    Ctx.MulticastSlotState = multicastSlotState;
#endif /* LORAMAC_CLASSB_ENABLED */
}

bool LoRaMacClassBIsAcquisitionInProgress( void )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    if( Ctx.BeaconState == BEACON_STATE_ACQUISITION_BY_TIME )
    {
        // In this case the acquisition is in progress, as the MAC has
        // a time reference for the next beacon RX.
        return true;
    }
    if( LoRaMacClassBIsAcquisitionPending( ) == true )
    {
        // In this case the acquisition is in progress, as the MAC
        // searches for a beacon.
        return true;
    }
    return false;
#else
    return false;
#endif /* LORAMAC_CLASSB_ENABLED */
}

void LoRaMacClassBBeaconTimerEvent( void* context )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    Ctx.BeaconCtx.TimeStamp = TimerGetCurrentTime( );
    TimerStop( &Ctx.BeaconTimer );
    LoRaMacClassBEvents.Events.Beacon = 1;

    if( Ctx.LoRaMacClassBCallbacks.MacProcessNotify != NULL )
    {
        Ctx.LoRaMacClassBCallbacks.MacProcessNotify( );
    }
#endif /* LORAMAC_CLASSB_ENABLED */
}

#if ( LORAMAC_CLASSB_ENABLED == 1 )
static void LoRaMacClassBProcessBeacon( void )
{
    bool activateTimer = false;
    TimerTime_t beaconEventTime = 1;
    RxConfigParams_t beaconRxConfig;
    TimerTime_t currentTime = Ctx.BeaconCtx.TimeStamp;

    // Beacon state machine
    switch( Ctx.BeaconState )
    {
        case BEACON_STATE_ACQUISITION_BY_TIME:
        {
            activateTimer = true;

            if( Ctx.BeaconCtx.Ctrl.AcquisitionPending == 1 )
            {
                Radio.Sleep();
                Ctx.BeaconState = BEACON_STATE_LOST;
            }
            else
            {
                // Default symbol timeouts
                ResetWindowTimeout( );

                if( Ctx.BeaconCtx.Ctrl.BeaconDelaySet == 1 )
                {
                    // The goal is to calculate beaconRxConfig.WindowTimeout
                    CalculateBeaconRxWindowConfig( &beaconRxConfig, Ctx.BeaconCtx.SymbolTimeout );

                    if( Ctx.BeaconCtx.BeaconTimingDelay > 0 )
                    {
                        if( SysTimeToMs( Ctx.BeaconCtx.NextBeaconRx ) > currentTime )
                        {
                            // Calculate the time when we expect the next beacon
                            beaconEventTime = TimerTempCompensation( SysTimeToMs( Ctx.BeaconCtx.NextBeaconRx ) - currentTime, Ctx.BeaconCtx.Temperature );

                            if( ( int32_t ) beaconEventTime > beaconRxConfig.WindowOffset )
                            {
                                // Apply the offset of the system error respectively beaconing precision setting
                                beaconEventTime += beaconRxConfig.WindowOffset;
                            }
                        }
                        else
                        {
                            // Reset status provides by BeaconTimingAns
                            Ctx.BeaconCtx.Ctrl.BeaconDelaySet = 0;
                            Ctx.BeaconCtx.Ctrl.BeaconChannelSet = 0;
                            Ctx.BeaconState = BEACON_STATE_ACQUISITION;
                        }
                        Ctx.BeaconCtx.BeaconTimingDelay = 0;
                    }
                    else
                    {
                        activateTimer = false;

                        // Reset status provides by BeaconTimingAns
                        Ctx.BeaconCtx.Ctrl.BeaconDelaySet = 0;
                        // Set the node into acquisition mode
                        Ctx.BeaconCtx.Ctrl.AcquisitionPending = 1;

                        // Don't use the default channel. We know on which
                        // channel the next beacon will be transmitted
                        RxBeaconSetup( CLASSB_BEACON_RESERVED, false, beaconRxConfig.WindowTimeout );
                    }
                }
                else
                {
                    Ctx.BeaconCtx.NextBeaconRx.Seconds = 0;
                    Ctx.BeaconCtx.NextBeaconRx.SubSeconds = 0;
                    Ctx.BeaconCtx.BeaconTimingDelay = 0;

                    Ctx.BeaconState = BEACON_STATE_ACQUISITION;
                }
            }
            break;
        }
        case BEACON_STATE_ACQUISITION:
        {
            activateTimer = true;

            if( Ctx.BeaconCtx.Ctrl.AcquisitionPending == 1 )
            {
                Radio.Sleep();
                Ctx.BeaconState = BEACON_STATE_LOST;
            }
            else
            {
                // Default symbol timeouts
                ResetWindowTimeout( );

                Ctx.BeaconCtx.Ctrl.AcquisitionPending = 1;
                beaconEventTime = CLASSB_BEACON_INTERVAL;

                // The goal is to calculate beaconRxConfig.WindowTimeout
                CalculateBeaconRxWindowConfig( &beaconRxConfig, Ctx.BeaconCtx.SymbolTimeout );

                // Start the beacon acquisition. When the MAC has received a beacon in function
                // RxBeacon successfully, the next state is BEACON_STATE_LOCKED. If the MAC does not
                // find a beacon, the state machine will stay in state BEACON_STATE_ACQUISITION.
                // This state detects that a acquisition was pending previously and will change the next
                // state to BEACON_STATE_LOST.
                RxBeaconSetup( 0, true, beaconRxConfig.WindowTimeout );
            }
            break;
        }
        case BEACON_STATE_TIMEOUT:
        {
            // We have to update the beacon time, since we missed a beacon
            Ctx.BeaconCtx.BeaconTime.Seconds += ( CLASSB_BEACON_INTERVAL / 1000 );
            Ctx.BeaconCtx.BeaconTime.SubSeconds = 0;

            // Enlarge window timeouts to increase the chance to receive the next beacon
            EnlargeWindowTimeout( );

            // Setup next state
            Ctx.BeaconState = BEACON_STATE_REACQUISITION;
        }
            // Intentional fall through
        case BEACON_STATE_REACQUISITION:
        {
            activateTimer = true;

            // The beacon is no longer acquired
            Ctx.BeaconCtx.Ctrl.BeaconAcquired = 0;

            // Verify if the maximum beacon less period has been elapsed
            if( ( currentTime - SysTimeToMs( Ctx.BeaconCtx.LastBeaconRx ) ) > CLASSB_MAX_BEACON_LESS_PERIOD )
            {
                Ctx.BeaconState = BEACON_STATE_LOST;
            }
            else
            {
                // Handle beacon miss
                beaconEventTime = UpdateBeaconState( LORAMAC_EVENT_INFO_STATUS_BEACON_NOT_FOUND,
                                                     Ctx.BeaconCtx.BeaconWindowMovement, currentTime );

                // Setup next state
                Ctx.BeaconState = BEACON_STATE_IDLE;
            }
            break;
        }
        case BEACON_STATE_LOCKED:
        {
            activateTimer = true;

            // We have received a beacon. Acquisition is no longer pending.
            Ctx.BeaconCtx.Ctrl.AcquisitionPending = 0;

            // Handle beacon reception
            beaconEventTime = UpdateBeaconState( LORAMAC_EVENT_INFO_STATUS_BEACON_LOCKED,
                                                 0, currentTime );

            // Setup the MLME confirm for the MLME_BEACON_ACQUISITION
            if( Ctx.LoRaMacClassBParams.LoRaMacFlags->Bits.MlmeReq == 1 )
            {
                if( LoRaMacConfirmQueueIsCmdActive( MLME_BEACON_ACQUISITION ) == true )
                {
                    LoRaMacConfirmQueueSetStatus( LORAMAC_EVENT_INFO_STATUS_OK, MLME_BEACON_ACQUISITION );
                    Ctx.LoRaMacClassBParams.MlmeConfirm->TxTimeOnAir = 0;
                }
            }

            // Setup next state
            Ctx.BeaconState = BEACON_STATE_IDLE;
            break;
        }
        case BEACON_STATE_IDLE:
        {
            activateTimer = true;
            GetTemperature( &Ctx.LoRaMacClassBCallbacks, &Ctx.BeaconCtx );
            beaconEventTime = Ctx.BeaconCtx.NextBeaconRxAdjusted - Radio.GetWakeupTime( );
            currentTime = TimerGetCurrentTime( );

            // The goal is to calculate beaconRxConfig.WindowTimeout and beaconRxConfig.WindowOffset
            CalculateBeaconRxWindowConfig( &beaconRxConfig, Ctx.BeaconCtx.SymbolTimeout );

            if( beaconEventTime > currentTime )
            {
                Ctx.BeaconState = BEACON_STATE_GUARD;
                beaconEventTime -= currentTime;
                beaconEventTime = TimerTempCompensation( beaconEventTime, Ctx.BeaconCtx.Temperature );

                if( ( int32_t ) beaconEventTime > beaconRxConfig.WindowOffset )
                {
                    // Apply the offset of the system error respectively beaconing precision setting
                    beaconEventTime += beaconRxConfig.WindowOffset;
                }
            }
            else
            {
                Ctx.BeaconState = BEACON_STATE_REACQUISITION;
                beaconEventTime = 1;
            }
            break;
        }
        case BEACON_STATE_GUARD:
        {
            Ctx.BeaconState = BEACON_STATE_RX;

            // Stop slot timers
            LoRaMacClassBStopRxSlots( );

            // Don't use the default channel. We know on which
            // channel the next beacon will be transmitted
            RxBeaconSetup( CLASSB_BEACON_RESERVED, false, beaconRxConfig.WindowTimeout );
            break;
        }
        case BEACON_STATE_LOST:
        {
            // Handle events
            if( Ctx.LoRaMacClassBParams.LoRaMacFlags->Bits.MlmeReq == 1 )
            {
                if( LoRaMacConfirmQueueIsCmdActive( MLME_BEACON_ACQUISITION ) == true )
                {
                    LoRaMacConfirmQueueSetStatus( LORAMAC_EVENT_INFO_STATUS_BEACON_NOT_FOUND, MLME_BEACON_ACQUISITION );
                }
            }
            else
            {
                Ctx.LoRaMacClassBParams.MlmeIndication->MlmeIndication = MLME_BEACON_LOST;
                Ctx.LoRaMacClassBParams.MlmeIndication->Status = LORAMAC_EVENT_INFO_STATUS_OK;
                Ctx.LoRaMacClassBParams.LoRaMacFlags->Bits.MlmeInd = 1;
            }

            // Stop slot timers
            LoRaMacClassBStopRxSlots( );

            // Initialize default state for class b
            InitClassBDefaults( );

            Ctx.LoRaMacClassBParams.LoRaMacFlags->Bits.MacDone = 1;

            break;
        }
        default:
        {
            Ctx.BeaconState = BEACON_STATE_ACQUISITION;
            break;
        }
    }
    MW_LOG(TS_ON, VLEVEL_H, "beacon state %d\r\n", Ctx.BeaconState);

    if( activateTimer == true )
    {
        TimerSetValue( &Ctx.BeaconTimer, beaconEventTime );
        TimerStart( &Ctx.BeaconTimer );
    }
}
#endif /* LORAMAC_CLASSB_ENABLED */

void LoRaMacClassBPingSlotTimerEvent( void* context )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    LoRaMacClassBEvents.Events.PingSlot = 1;

    if( Ctx.LoRaMacClassBCallbacks.MacProcessNotify != NULL )
    {
        Ctx.LoRaMacClassBCallbacks.MacProcessNotify( );
    }
#endif /* LORAMAC_CLASSB_ENABLED */
}

#if ( LORAMAC_CLASSB_ENABLED == 1 )
static void LoRaMacClassBProcessPingSlot( void )
{
    static RxConfigParams_t pingSlotRxConfig;
    TimerTime_t pingSlotTime = 0;
#if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
    uint32_t maxRxError = 0;
    bool slotHasPriority = false;
#endif /* LORAMAC_VERSION */

    switch( Ctx.PingSlotState )
    {
        case PINGSLOT_STATE_CALC_PING_OFFSET:
        {
            ComputePingOffset( Ctx.BeaconCtx.BeaconTime.Seconds,
                               *Ctx.LoRaMacClassBParams.LoRaMacDevAddr,
                               ClassBNvm->PingSlotCtx.PingPeriod,
                               &( Ctx.PingSlotCtx.PingOffset ) );
            Ctx.PingSlotState = PINGSLOT_STATE_SET_TIMER;
        }
            // Intentional fall through
        case PINGSLOT_STATE_SET_TIMER:
        {
            if( CalcNextSlotTime( Ctx.PingSlotCtx.PingOffset, ClassBNvm->PingSlotCtx.PingPeriod, ClassBNvm->PingSlotCtx.PingNb, &pingSlotTime ) == true )
            {
                if( Ctx.BeaconCtx.Ctrl.BeaconAcquired == 1 )
                {
#if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000300 ))
                    // Compute the symbol timeout. Apply it only, if the beacon is acquired
                    // Otherwise, take the enlargement of the symbols into account.
                    RegionComputeRxWindowParameters( *Ctx.LoRaMacClassBParams.LoRaMacRegion,
                                                     ClassBNvm->PingSlotCtx.Datarate,
                                                     Ctx.LoRaMacClassBParams.LoRaMacParams->MinRxSymbols,
                                                     Ctx.LoRaMacClassBParams.LoRaMacParams->SystemMaxRxError,
                                                     &pingSlotRxConfig );
#elif (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
                    // Compare and assign the maximum between the region specific rx error window time
                    // and time precision received from beacon frame format.
                    maxRxError = MAX( Ctx.LoRaMacClassBParams.LoRaMacParams->SystemMaxRxError ,
                                      ( uint32_t ) Ctx.BeaconCtx.BeaconTimePrecision.SubSeconds );

                    // Compute the symbol timeout. Apply it only, if the beacon is acquired
                    // Otherwise, take the enlargement of the symbols into account.
                    RegionComputeRxWindowParameters( *Ctx.LoRaMacClassBParams.LoRaMacRegion,
                                                     ClassBNvm->PingSlotCtx.Datarate,
                                                     Ctx.LoRaMacClassBParams.LoRaMacParams->MinRxSymbols,
                                                     maxRxError,
                                                     &pingSlotRxConfig );
#endif /* LORAMAC_VERSION */
                    Ctx.PingSlotCtx.SymbolTimeout = pingSlotRxConfig.WindowTimeout;

                    if( ( int32_t )pingSlotTime > pingSlotRxConfig.WindowOffset )
                    {// Apply the window offset
                        pingSlotTime += pingSlotRxConfig.WindowOffset;
                    }
                }

                // Start the timer if the ping slot time is in range
                Ctx.PingSlotState = PINGSLOT_STATE_IDLE;
                TimerSetValue( &Ctx.PingSlotTimer, pingSlotTime );
                TimerStart( &Ctx.PingSlotTimer );
            }
            break;
        }
        case PINGSLOT_STATE_IDLE:
        {
            uint32_t frequency = ClassBNvm->PingSlotCtx.Frequency;

            // Apply a custom frequency if the following bit is set
            if( ClassBNvm->PingSlotCtx.Ctrl.CustomFreq == 0 )
            {
                // Restore floor plan
                frequency = CalcDownlinkChannelAndFrequency( *Ctx.LoRaMacClassBParams.LoRaMacDevAddr, Ctx.BeaconCtx.BeaconTime.Seconds,
                                                             CLASSB_BEACON_INTERVAL, false );
            }

#if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000300 ))
            // Open the ping slot window only, if there is no multicast ping slot
            // open. Multicast ping slots have always priority
            if( Ctx.MulticastSlotState != PINGSLOT_STATE_RX )
            {
                Ctx.PingSlotState = PINGSLOT_STATE_RX;

                pingSlotRxConfig.Datarate = ClassBNvm->PingSlotCtx.Datarate;
                pingSlotRxConfig.DownlinkDwellTime = Ctx.LoRaMacClassBParams.LoRaMacParams->DownlinkDwellTime;
                pingSlotRxConfig.RepeaterSupport = Ctx.LoRaMacClassBParams.LoRaMacParams->RepeaterSupport; /* ST_WORKAROUND: keep repeater feature */
                pingSlotRxConfig.Frequency = frequency;
                pingSlotRxConfig.RxContinuous = false;
                pingSlotRxConfig.RxSlot = RX_SLOT_WIN_CLASS_B_PING_SLOT;

                RegionRxConfig( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &pingSlotRxConfig, ( int8_t* )&Ctx.LoRaMacClassBParams.McpsIndication->RxDatarate );

                if( pingSlotRxConfig.RxContinuous == false )
                {
                    Radio.Rx( Ctx.LoRaMacClassBParams.LoRaMacParams->MaxRxWindow );
                }
                else
                {
                    Radio.Rx( 0 ); // Continuous mode
                }
            }
            else
            {
                // Multicast slots have priority. Skip Rx
                Ctx.PingSlotState = PINGSLOT_STATE_CALC_PING_OFFSET;
                TimerSetValue( &Ctx.PingSlotTimer, CLASSB_PING_SLOT_WINDOW );
                TimerStart( &Ctx.PingSlotTimer );
            }
#elif (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
            if( Ctx.PingSlotCtx.NextMulticastChannel != NULL )
            {
                // Verify, if the unicast has priority.
                slotHasPriority = CheckSlotPriority( *Ctx.LoRaMacClassBParams.LoRaMacDevAddr, ClassBNvm->PingSlotCtx.FPendingSet, 0,
                                                     Ctx.PingSlotCtx.NextMulticastChannel->ChannelParams.Address, Ctx.PingSlotCtx.NextMulticastChannel->FPendingSet, 1 );
            }

            // Open the ping slot window only, if there is no multicast ping slot
            // open or if the unicast has priority.
            if( ( Ctx.MulticastSlotState != PINGSLOT_STATE_RX ) || ( slotHasPriority == true ) )
            {
                if( Ctx.MulticastSlotState == PINGSLOT_STATE_RX )
                {
                    // Close multicast slot window, if necessary. Multicast slots have priority
                    Radio.Standby( );
                    Ctx.MulticastSlotState = PINGSLOT_STATE_CALC_PING_OFFSET;
                    TimerSetValue( &Ctx.MulticastSlotTimer, CLASSB_PING_SLOT_WINDOW );
                    TimerStart( &Ctx.MulticastSlotTimer );
                }

                Ctx.PingSlotState = PINGSLOT_STATE_RX;

                pingSlotRxConfig.Datarate = ClassBNvm->PingSlotCtx.Datarate;
                pingSlotRxConfig.DownlinkDwellTime = Ctx.LoRaMacClassBParams.LoRaMacParams->DownlinkDwellTime;
                pingSlotRxConfig.RepeaterSupport = Ctx.LoRaMacClassBParams.LoRaMacParams->RepeaterSupport; /* ST_WORKAROUND: keep repeater feature */
                pingSlotRxConfig.Frequency = frequency;
                pingSlotRxConfig.RxContinuous = false;
                pingSlotRxConfig.RxSlot = RX_SLOT_WIN_CLASS_B_PING_SLOT;
                pingSlotRxConfig.NetworkActivation = *Ctx.LoRaMacClassBParams.NetworkActivation;

                RegionRxConfig( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &pingSlotRxConfig, ( int8_t* )&Ctx.LoRaMacClassBParams.McpsIndication->RxDatarate );

                if( pingSlotRxConfig.RxContinuous == false )
                {
                    Radio.Rx( Ctx.LoRaMacClassBParams.LoRaMacParams->MaxRxWindow );
                }
                else
                {
                    Radio.Rx( 0 ); // Continuous mode
                }
            }
            else
            {
                // Multicast slots have priority. Skip Rx
                Ctx.PingSlotState = PINGSLOT_STATE_CALC_PING_OFFSET;
                TimerSetValue( &Ctx.PingSlotTimer, CLASSB_PING_SLOT_WINDOW );
                TimerStart( &Ctx.PingSlotTimer );
            }
#endif /* LORAMAC_VERSION */
            break;
        }
        default:
        {
            Ctx.PingSlotState = PINGSLOT_STATE_CALC_PING_OFFSET;
            break;
        }
    }
}
#endif /* LORAMAC_CLASSB_ENABLED */

void LoRaMacClassBMulticastSlotTimerEvent( void* context )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    LoRaMacClassBEvents.Events.MulticastSlot = 1;

    if( Ctx.LoRaMacClassBCallbacks.MacProcessNotify != NULL )
    {
        Ctx.LoRaMacClassBCallbacks.MacProcessNotify( );
    }
#endif /* LORAMAC_CLASSB_ENABLED */
}

#if ( LORAMAC_CLASSB_ENABLED == 1 )
static void LoRaMacClassBProcessMulticastSlot( void )
{
    static RxConfigParams_t multicastSlotRxConfig;
    TimerTime_t multicastSlotTime = 0;
    TimerTime_t slotTime = 0;
    MulticastCtx_t *cur = Ctx.LoRaMacClassBParams.MulticastChannels;
#if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
    uint32_t maxRxError = 0;
    bool slotHasPriority = false;
#endif /* LORAMAC_VERSION */

    if( cur == NULL )
    {
        return;
    }

    if( Ctx.MulticastSlotState == PINGSLOT_STATE_RX )
    {
        // A multicast slot is already open
        return;
    }

    switch( Ctx.MulticastSlotState )
    {
        case PINGSLOT_STATE_CALC_PING_OFFSET:
        {
            // Compute all offsets for every multicast slots
            for( uint8_t i = 0; i < LORAMAC_MAX_MC_CTX; i++ ) /* ST_WORKAROUND: reduced LORAMAC_MAX_MC_CTX */
            {
                ComputePingOffset( Ctx.BeaconCtx.BeaconTime.Seconds,
                                   cur->ChannelParams.Address,
                                   cur->PingPeriod,
                                   &( cur->PingOffset ) );
                cur++;
            }
            Ctx.MulticastSlotState = PINGSLOT_STATE_SET_TIMER;
        }
            // Intentional fall through
        case PINGSLOT_STATE_SET_TIMER:
        {
            cur = Ctx.LoRaMacClassBParams.MulticastChannels;
            Ctx.PingSlotCtx.NextMulticastChannel = NULL;

            for( uint8_t i = 0; i < LORAMAC_MAX_MC_CTX; i++ )
            {
                // Calculate the next slot time for every multicast slot
                if( CalcNextSlotTime( cur->PingOffset, cur->PingPeriod, cur->PingNb, &slotTime ) == true )
                {
                    if( ( multicastSlotTime == 0 ) || ( multicastSlotTime > slotTime ) )
                    {
                        // Update the slot time and the next multicast channel
                        multicastSlotTime = slotTime;
                        Ctx.PingSlotCtx.NextMulticastChannel = cur;
                    }
                }
                cur++;
            }

            // Schedule the next multicast slot
            if( Ctx.PingSlotCtx.NextMulticastChannel != NULL )
            {
                if( Ctx.BeaconCtx.Ctrl.BeaconAcquired == 1 )
                {
#if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000300 ))
                    RegionComputeRxWindowParameters( *Ctx.LoRaMacClassBParams.LoRaMacRegion,
                                                    ClassBNvm->PingSlotCtx.Datarate,
                                                    Ctx.LoRaMacClassBParams.LoRaMacParams->MinRxSymbols,
                                                    Ctx.LoRaMacClassBParams.LoRaMacParams->SystemMaxRxError,
                                                    &multicastSlotRxConfig );
#elif (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
                    // Compare and assign the maximum between the region specific rx error window time
                    // and time precision received from beacon frame format.
                    maxRxError = MAX( Ctx.LoRaMacClassBParams.LoRaMacParams->SystemMaxRxError ,
                                      ( uint32_t ) Ctx.BeaconCtx.BeaconTimePrecision.SubSeconds );

                    RegionComputeRxWindowParameters( *Ctx.LoRaMacClassBParams.LoRaMacRegion,
                                                    ClassBNvm->PingSlotCtx.Datarate,
                                                    Ctx.LoRaMacClassBParams.LoRaMacParams->MinRxSymbols,
                                                    maxRxError,
                                                    &multicastSlotRxConfig );
#endif /* LORAMAC_VERSION */
                    Ctx.PingSlotCtx.SymbolTimeout = multicastSlotRxConfig.WindowTimeout;
                }

                if( ( int32_t )multicastSlotTime > multicastSlotRxConfig.WindowOffset )
                {// Apply the window offset
                    multicastSlotTime += multicastSlotRxConfig.WindowOffset;
                }

                // Start the timer if the ping slot time is in range
                Ctx.MulticastSlotState = PINGSLOT_STATE_IDLE;
                TimerSetValue( &Ctx.MulticastSlotTimer, multicastSlotTime );
                TimerStart( &Ctx.MulticastSlotTimer );
            }
            break;
        }
        case PINGSLOT_STATE_IDLE:
        {
            uint32_t frequency = 0;

            // Verify if the multicast channel is valid
            if( Ctx.PingSlotCtx.NextMulticastChannel == NULL )
            {
                Ctx.MulticastSlotState = PINGSLOT_STATE_CALC_PING_OFFSET;
                TimerSetValue( &Ctx.MulticastSlotTimer, 1 );
                TimerStart( &Ctx.MulticastSlotTimer );
                break;
            }

            // Apply frequency
            frequency = Ctx.PingSlotCtx.NextMulticastChannel->ChannelParams.RxParams.ClassB.Frequency;

            // Restore the floor plan frequency if there is no individual frequency assigned
            if( frequency == 0 )
            {
                // Restore floor plan
                frequency = CalcDownlinkChannelAndFrequency( Ctx.PingSlotCtx.NextMulticastChannel->ChannelParams.Address,
                                                             Ctx.BeaconCtx.BeaconTime.Seconds, CLASSB_BEACON_INTERVAL, false );
            }

#if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000300 ))
            Ctx.MulticastSlotState = PINGSLOT_STATE_RX;

            multicastSlotRxConfig.Datarate = Ctx.PingSlotCtx.NextMulticastChannel->ChannelParams.RxParams.ClassB.Datarate;
            multicastSlotRxConfig.DownlinkDwellTime = Ctx.LoRaMacClassBParams.LoRaMacParams->DownlinkDwellTime;
            multicastSlotRxConfig.RepeaterSupport = Ctx.LoRaMacClassBParams.LoRaMacParams->RepeaterSupport; /* ST_WORKAROUND: keep repeater feature */
            multicastSlotRxConfig.Frequency = frequency;
            multicastSlotRxConfig.RxContinuous = false;
            multicastSlotRxConfig.RxSlot = RX_SLOT_WIN_CLASS_B_MULTICAST_SLOT;

            RegionRxConfig( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &multicastSlotRxConfig, ( int8_t* )&Ctx.LoRaMacClassBParams.McpsIndication->RxDatarate );

            if( Ctx.PingSlotState == PINGSLOT_STATE_RX )
            {
                // Close ping slot window, if necessary. Multicast slots have priority
                Radio.Standby( );
                Ctx.PingSlotState = PINGSLOT_STATE_CALC_PING_OFFSET;
                TimerSetValue( &Ctx.PingSlotTimer, CLASSB_PING_SLOT_WINDOW );
                TimerStart( &Ctx.PingSlotTimer );
            }

            if( multicastSlotRxConfig.RxContinuous == false )
            {
                Radio.Rx( Ctx.LoRaMacClassBParams.LoRaMacParams->MaxRxWindow );
            }
            else
            {
                Radio.Rx( 0 ); // Continuous mode
            }
#elif (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
            // Verify, if the unicast has priority.
            slotHasPriority = CheckSlotPriority( Ctx.PingSlotCtx.NextMulticastChannel->ChannelParams.Address, Ctx.PingSlotCtx.NextMulticastChannel->FPendingSet, 1,
                                                 *Ctx.LoRaMacClassBParams.LoRaMacDevAddr, ClassBNvm->PingSlotCtx.FPendingSet, 0 );

            // Open the ping slot window only, if there is no multicast ping slot
            // open or if the unicast has priority.
            if( ( Ctx.PingSlotState != PINGSLOT_STATE_RX ) || ( slotHasPriority == true ) )
            {
                if( Ctx.PingSlotState == PINGSLOT_STATE_RX )
                {
                    // Close ping slot window, if necessary. Multicast slots have priority
                    Radio.Standby( );
                    Ctx.PingSlotState = PINGSLOT_STATE_CALC_PING_OFFSET;
                    TimerSetValue( &Ctx.PingSlotTimer, CLASSB_PING_SLOT_WINDOW );
                    TimerStart( &Ctx.PingSlotTimer );
                }

                Ctx.MulticastSlotState = PINGSLOT_STATE_RX;

                multicastSlotRxConfig.Datarate = Ctx.PingSlotCtx.NextMulticastChannel->ChannelParams.RxParams.ClassB.Datarate;
                multicastSlotRxConfig.DownlinkDwellTime = Ctx.LoRaMacClassBParams.LoRaMacParams->DownlinkDwellTime;
                multicastSlotRxConfig.Frequency = frequency;
                multicastSlotRxConfig.RxContinuous = false;
                multicastSlotRxConfig.RxSlot = RX_SLOT_WIN_CLASS_B_MULTICAST_SLOT;
                multicastSlotRxConfig.NetworkActivation = *Ctx.LoRaMacClassBParams.NetworkActivation;

                RegionRxConfig( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &multicastSlotRxConfig, ( int8_t* )&Ctx.LoRaMacClassBParams.McpsIndication->RxDatarate );

                if( multicastSlotRxConfig.RxContinuous == false )
                {
                    Radio.Rx( Ctx.LoRaMacClassBParams.LoRaMacParams->MaxRxWindow );
                }
                else
                {
                    Radio.Rx( 0 ); // Continuous mode
                }
            }
            else
            {
                // Unicast slots have priority. Skip Rx
                Ctx.MulticastSlotState = PINGSLOT_STATE_CALC_PING_OFFSET;
                TimerSetValue( &Ctx.MulticastSlotTimer, CLASSB_PING_SLOT_WINDOW );
                TimerStart( &Ctx.MulticastSlotTimer );
            }
#endif /* LORAMAC_VERSION */
            break;
        }
        default:
        {
            Ctx.MulticastSlotState = PINGSLOT_STATE_CALC_PING_OFFSET;
            break;
        }
    }
}
#endif /* LORAMAC_CLASSB_ENABLED */

bool LoRaMacClassBRxBeacon( uint8_t *payload, uint16_t size )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;
    bool beaconProcessed = false;
    uint16_t crc0 = 0;
    uint16_t crc1 = 0;
    uint16_t beaconCrc0 = 0;
    uint16_t beaconCrc1 = 0;

    getPhy.Attribute = PHY_BEACON_FORMAT;
    phyParam = RegionGetPhyParam( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &getPhy );

    // Verify if we are in the state where we expect a beacon
    if( ( Ctx.BeaconState == BEACON_STATE_RX ) || ( Ctx.BeaconCtx.Ctrl.AcquisitionPending == 1 ) )
    {
        if( size == phyParam.BeaconFormat.BeaconSize )
        {
#if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000300 ))
            // A beacon frame is defined as:
            // Bytes: |  x   |  4   |  2   |     7      |  y   |  2   |
            //        |------|------|------|------------|------|------|
            // Field: | RFU1 | Time | CRC1 | GwSpecific | RFU2 | CRC2 |
            //
            // Field RFU1 and RFU2 have variable sizes. It depends on the region specific implementation

            // Read CRC1 field from the frame
            beaconCrc0 = ( ( uint16_t )payload[phyParam.BeaconFormat.Rfu1Size + 4] ) & 0x00FF;
            beaconCrc0 |= ( ( uint16_t )payload[phyParam.BeaconFormat.Rfu1Size + 4 + 1] << 8 ) & 0xFF00;
            crc0 = BeaconCrc( payload, phyParam.BeaconFormat.Rfu1Size + 4 );

            // Validate the first crc of the beacon frame
            if( crc0 == beaconCrc0 )
            {
                // Read Time field from the frame
                Ctx.BeaconCtx.BeaconTime.Seconds  = ( ( uint32_t )payload[phyParam.BeaconFormat.Rfu1Size] ) & 0x000000FF;
                Ctx.BeaconCtx.BeaconTime.Seconds |= ( ( uint32_t )( payload[phyParam.BeaconFormat.Rfu1Size + 1] << 8 ) ) & 0x0000FF00;
                Ctx.BeaconCtx.BeaconTime.Seconds |= ( ( uint32_t )( payload[phyParam.BeaconFormat.Rfu1Size + 2] << 16 ) ) & 0x00FF0000;
                Ctx.BeaconCtx.BeaconTime.Seconds |= ( ( uint32_t )( payload[phyParam.BeaconFormat.Rfu1Size + 3] << 24 ) ) & 0xFF000000;
                Ctx.BeaconCtx.BeaconTime.SubSeconds = 0;
                Ctx.LoRaMacClassBParams.MlmeIndication->BeaconInfo.Time = Ctx.BeaconCtx.BeaconTime;
                beaconProcessed = true;
            }

            // Read CRC2 field from the frame
            beaconCrc1 = ( ( uint16_t )payload[phyParam.BeaconFormat.Rfu1Size + 4 + 2 + 7 + phyParam.BeaconFormat.Rfu2Size] ) & 0x00FF;
            beaconCrc1 |= ( ( uint16_t )payload[phyParam.BeaconFormat.Rfu1Size + 4 + 2 + 7 + phyParam.BeaconFormat.Rfu2Size + 1] << 8 ) & 0xFF00;
            crc1 = BeaconCrc( &payload[phyParam.BeaconFormat.Rfu1Size + 4 + 2], 7 + phyParam.BeaconFormat.Rfu2Size );

            // Validate the second crc of the beacon frame
            if( crc1 == beaconCrc1 )
            {
                // Read GwSpecific field from the frame
                // The GwSpecific field contains 1 byte InfoDesc and 6 bytes Info
                Ctx.LoRaMacClassBParams.MlmeIndication->BeaconInfo.GwSpecific.InfoDesc = payload[phyParam.BeaconFormat.Rfu1Size + 4 + 2];
                memcpy1( Ctx.LoRaMacClassBParams.MlmeIndication->BeaconInfo.GwSpecific.Info, &payload[phyParam.BeaconFormat.Rfu1Size + 4 + 2 + 1], 6 );
            }
#elif (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
            // A beacon frame is defined as:
            // Bytes: |  x   |   1   |  4   |  2   |     7      |  y   |  2   |
            //        |------|-------|------|------|------------|------|------|
            // Field: | RFU1 | Param | Time | CRC1 | GwSpecific | RFU2 | CRC2 |
            //
            // Field RFU1 and RFU2 have variable sizes. It depends on the region specific implementation

            // Read CRC1 field from the frame
            beaconCrc0 = ( ( uint16_t )payload[phyParam.BeaconFormat.Rfu1Size + 1 + 4] ) & 0x00FF;
            beaconCrc0 |= ( ( uint16_t )payload[phyParam.BeaconFormat.Rfu1Size + 1 + 4 + 1] << 8 ) & 0xFF00;
            crc0 = BeaconCrc( payload, phyParam.BeaconFormat.Rfu1Size + 1 + 4 );

            // Validate the first crc of the beacon frame
            if( crc0 == beaconCrc0 )
            {
                // Copy the param field for app layer
                Ctx.LoRaMacClassBParams.MlmeIndication->BeaconInfo.Param = ( payload[phyParam.BeaconFormat.Rfu1Size] );
                // Fetch the precise time value in milliseconds that will be used for Rx ping slot delay.
                Ctx.BeaconCtx.BeaconTimePrecision.SubSeconds = BeaconPrecTimeValue[Ctx.LoRaMacClassBParams.MlmeIndication->BeaconInfo.Param];

                // Read Time field from the frame
                Ctx.BeaconCtx.BeaconTime.Seconds  = ( ( uint32_t )payload[phyParam.BeaconFormat.Rfu1Size + 1] ) & 0x000000FF;
                Ctx.BeaconCtx.BeaconTime.Seconds |= ( ( uint32_t )( payload[phyParam.BeaconFormat.Rfu1Size + 2] << 8 ) ) & 0x0000FF00;
                Ctx.BeaconCtx.BeaconTime.Seconds |= ( ( uint32_t )( payload[phyParam.BeaconFormat.Rfu1Size + 3] << 16 ) ) & 0x00FF0000;
                Ctx.BeaconCtx.BeaconTime.Seconds |= ( ( uint32_t )( payload[phyParam.BeaconFormat.Rfu1Size + 4] << 24 ) ) & 0xFF000000;
                Ctx.BeaconCtx.BeaconTime.SubSeconds = 0;
                Ctx.LoRaMacClassBParams.MlmeIndication->BeaconInfo.Time = Ctx.BeaconCtx.BeaconTime;
                beaconProcessed = true;
            }

            // Read CRC2 field from the frame
            beaconCrc1 = ( ( uint16_t )payload[phyParam.BeaconFormat.Rfu1Size + 1 + 4 + 2 + 7 + phyParam.BeaconFormat.Rfu2Size] ) & 0x00FF;
            beaconCrc1 |= ( ( uint16_t )payload[phyParam.BeaconFormat.Rfu1Size + 1 + 4 + 2 + 7 + phyParam.BeaconFormat.Rfu2Size + 1] << 8 ) & 0xFF00;
            crc1 = BeaconCrc( &payload[phyParam.BeaconFormat.Rfu1Size + 1 + 4 + 2], 7 + phyParam.BeaconFormat.Rfu2Size );

            // Validate the second crc of the beacon frame
            if( crc1 == beaconCrc1 )
            {
                // Read GwSpecific field from the frame
                // The GwSpecific field contains 1 byte InfoDesc and 6 bytes Info
                Ctx.LoRaMacClassBParams.MlmeIndication->BeaconInfo.GwSpecific.InfoDesc = payload[phyParam.BeaconFormat.Rfu1Size + 1 + 4 + 2];
                memcpy1( Ctx.LoRaMacClassBParams.MlmeIndication->BeaconInfo.GwSpecific.Info, &payload[phyParam.BeaconFormat.Rfu1Size + 1 + 4 + 2 + 1], 6 );
            }
#endif /* LORAMAC_VERSION */

            // Reset beacon variables, if one of the crc is valid
            if( beaconProcessed == true )
            {
                uint32_t spreadingFactor = 0;
                uint32_t bandwidth = 0;

                getPhy.Attribute = PHY_BEACON_CHANNEL_DR;
                phyParam = RegionGetPhyParam( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &getPhy );

                getPhy.Attribute = PHY_SF_FROM_DR;
                getPhy.Datarate = phyParam.Value;
                phyParam = RegionGetPhyParam( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &getPhy );
                spreadingFactor = phyParam.Value;

                getPhy.Attribute = PHY_BW_FROM_DR;
                phyParam = RegionGetPhyParam( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &getPhy );
                bandwidth = phyParam.Value;

                TimerTime_t time = Radio.TimeOnAir( MODEM_LORA, bandwidth, spreadingFactor, 1, 10, true, size, false );
                SysTime_t timeOnAir;
                timeOnAir.Seconds = time / 1000;
                timeOnAir.SubSeconds = time - timeOnAir.Seconds * 1000;

                Ctx.BeaconCtx.LastBeaconRx = Ctx.BeaconCtx.BeaconTime;
                Ctx.BeaconCtx.LastBeaconRx.Seconds += UNIX_GPS_EPOCH_OFFSET;

                // Update system time.
                SysTimeSet( SysTimeAdd( Ctx.BeaconCtx.LastBeaconRx, timeOnAir ) );

                Ctx.BeaconCtx.Ctrl.BeaconAcquired = 1;
                Ctx.BeaconCtx.Ctrl.BeaconMode = 1;
                ResetWindowTimeout( );
                Ctx.BeaconState = BEACON_STATE_LOCKED;

                LoRaMacClassBBeaconTimerEvent( NULL );
            }
        }

        if( Ctx.BeaconState == BEACON_STATE_RX )
        {
            Ctx.BeaconState = BEACON_STATE_TIMEOUT;
            LoRaMacClassBBeaconTimerEvent( NULL );
        }
        // When the MAC listens for a beacon, it is not allowed to process any other
        // downlink except the beacon frame itself. The reason for this is that no valid downlink window is open.
        // If it receives a frame which is
        // 1. not a beacon or
        // 2. a beacon with a crc fail
        // the MAC shall ignore the frame completely. Thus, the function must always return true, even if no
        // valid beacon has been received.
        beaconProcessed = true;
    }
    return beaconProcessed;
#else
    return false;
#endif /* LORAMAC_CLASSB_ENABLED */
}

bool LoRaMacClassBIsBeaconExpected( void )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    if( ( Ctx.BeaconCtx.Ctrl.AcquisitionPending == 1 ) ||
        ( Ctx.BeaconState == BEACON_STATE_RX ) )
    {
        return true;
    }
    return false;
#else
    return false;
#endif /* LORAMAC_CLASSB_ENABLED */
}

bool LoRaMacClassBIsPingExpected( void )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    if( Ctx.PingSlotState == PINGSLOT_STATE_RX )
    {
        return true;
    }
    return false;
#else
    return false;
#endif /* LORAMAC_CLASSB_ENABLED */
}

bool LoRaMacClassBIsMulticastExpected( void )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    if( Ctx.MulticastSlotState == PINGSLOT_STATE_RX )
    {
        return true;
    }
    return false;
#else
    return false;
#endif /* LORAMAC_CLASSB_ENABLED */
}

bool LoRaMacClassBIsAcquisitionPending( void )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    if( Ctx.BeaconCtx.Ctrl.AcquisitionPending == 1 )
    {
        return true;
    }
    return false;
#else
    return false;
#endif /* LORAMAC_CLASSB_ENABLED */
}

bool LoRaMacClassBIsBeaconModeActive( void )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    if( ( Ctx.BeaconCtx.Ctrl.BeaconMode == 1 ) ||
        ( Ctx.BeaconState == BEACON_STATE_ACQUISITION_BY_TIME ) )
    {
        return true;
    }
    return false;
#else
    return false;
#endif /* LORAMAC_CLASSB_ENABLED */
}

void LoRaMacClassBSetPingSlotInfo( uint8_t periodicity )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    ClassBNvm->PingSlotCtx.PingNb = CalcPingNb( periodicity );
    ClassBNvm->PingSlotCtx.PingPeriod = CalcPingPeriod( ClassBNvm->PingSlotCtx.PingNb );
#endif /* LORAMAC_CLASSB_ENABLED */
}

void LoRaMacClassBHaltBeaconing( void )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    if( Ctx.BeaconCtx.Ctrl.BeaconMode == 1 )
    {
        if( ( Ctx.BeaconState == BEACON_STATE_TIMEOUT ) ||
            ( Ctx.BeaconState == BEACON_STATE_LOST ) )
        {
            // Update the state machine before halt
            LoRaMacClassBBeaconTimerEvent( NULL );
        }

        CRITICAL_SECTION_BEGIN( );
        LoRaMacClassBEvents.Events.Beacon = 0;
        CRITICAL_SECTION_END( );

        // Halt ping slot state machine
        TimerStop( &Ctx.BeaconTimer );

        // Halt beacon state machine
        Ctx.BeaconState = BEACON_STATE_HALT;

        // Halt ping and multicast slot state machines
        LoRaMacClassBStopRxSlots( );
    }
#endif /* LORAMAC_CLASSB_ENABLED */
}

void LoRaMacClassBResumeBeaconing( void )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    if( Ctx.BeaconState == BEACON_STATE_HALT )
    {
        Ctx.BeaconCtx.Ctrl.ResumeBeaconing = 1;

        // Set default state
        Ctx.BeaconState = BEACON_STATE_LOCKED;

        if( Ctx.BeaconCtx.Ctrl.BeaconAcquired == 0 )
        {
            // Set the default state for beacon less operation
            Ctx.BeaconState = BEACON_STATE_REACQUISITION;
        }

        LoRaMacClassBBeaconTimerEvent( NULL );
    }
#endif /* LORAMAC_CLASSB_ENABLED */
}

LoRaMacStatus_t LoRaMacClassBSwitchClass( DeviceClass_t nextClass )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    if( nextClass == CLASS_B )
    {// Switch to from class a to class b
        if( ( Ctx.BeaconCtx.Ctrl.BeaconMode == 1 ) && ( ClassBNvm->PingSlotCtx.Ctrl.Assigned == 1 ) )
        {
            return LORAMAC_STATUS_OK;
        }
    }
    if( nextClass == CLASS_A )
    {// Switch from class b to class a
        LoRaMacClassBHaltBeaconing( );

        // Initialize default state for class b
        InitClassBDefaults( );

        return LORAMAC_STATUS_OK;
    }
    return LORAMAC_STATUS_SERVICE_UNKNOWN;
#else
    return LORAMAC_STATUS_SERVICE_UNKNOWN;
#endif /* LORAMAC_CLASSB_ENABLED */
}

LoRaMacStatus_t LoRaMacClassBMibGetRequestConfirm( MibRequestConfirm_t *mibGet )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    LoRaMacStatus_t status = LORAMAC_STATUS_OK;

    switch( mibGet->Type )
    {
        case MIB_PING_SLOT_DATARATE:
        {
            mibGet->Param.PingSlotDatarate = ClassBNvm->PingSlotCtx.Datarate;
            break;
        }
        case MIB_BEACON_STATE:
        {
            mibGet->Param.BeaconState = Ctx.BeaconState;
            break;
        }
        default:
        {
            status = LORAMAC_STATUS_SERVICE_UNKNOWN;
            break;
        }
    }
    return status;
#else
    return LORAMAC_STATUS_SERVICE_UNKNOWN;
#endif /* LORAMAC_CLASSB_ENABLED */
}

LoRaMacStatus_t LoRaMacMibClassBSetRequestConfirm( MibRequestConfirm_t *mibSet )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    LoRaMacStatus_t status = LORAMAC_STATUS_OK;

    switch( mibSet->Type )
    {
        case MIB_PING_SLOT_DATARATE:
        {
            ClassBNvm->PingSlotCtx.Datarate = mibSet->Param.PingSlotDatarate;
            break;
        }
        default:
        {
            status = LORAMAC_STATUS_SERVICE_UNKNOWN;
            break;
        }
    }
    return status;
#else
    return LORAMAC_STATUS_SERVICE_UNKNOWN;
#endif /* LORAMAC_CLASSB_ENABLED */
}

void LoRaMacClassBPingSlotInfoAns( void )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    if( LoRaMacConfirmQueueIsCmdActive( MLME_PING_SLOT_INFO ) == true )
    {
        LoRaMacConfirmQueueSetStatus( LORAMAC_EVENT_INFO_STATUS_OK, MLME_PING_SLOT_INFO );
        ClassBNvm->PingSlotCtx.Ctrl.Assigned = 1;
    }
#endif /* LORAMAC_CLASSB_ENABLED */
}

uint8_t LoRaMacClassBPingSlotChannelReq( uint8_t datarate, uint32_t frequency )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    uint8_t status = 0x03;
    VerifyParams_t verify;
    bool isCustomFreq = false;

    if( frequency != 0 )
    {
        isCustomFreq = true;
        verify.Frequency = frequency;
        if( RegionVerify( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &verify, PHY_FREQUENCY ) == false )
        {
            status &= 0xFE; // Channel frequency KO
        }
    }

    verify.DatarateParams.Datarate = datarate;
    verify.DatarateParams.DownlinkDwellTime = Ctx.LoRaMacClassBParams.LoRaMacParams->DownlinkDwellTime;

    if( RegionVerify( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &verify, PHY_RX_DR ) == false )
    {
        status &= 0xFD; // Datarate range KO
    }

    if( status == 0x03 )
    {
        if( isCustomFreq == true )
        {
            ClassBNvm->PingSlotCtx.Ctrl.CustomFreq = 1;
            ClassBNvm->PingSlotCtx.Frequency = frequency;
        }
        else
        {
            ClassBNvm->PingSlotCtx.Ctrl.CustomFreq = 0;
            ClassBNvm->PingSlotCtx.Frequency = 0;
        }
        ClassBNvm->PingSlotCtx.Datarate = datarate;
    }

    return status;
#else
    return 0;
#endif /* LORAMAC_CLASSB_ENABLED */
}

void LoRaMacClassBBeaconTimingAns( uint16_t beaconTimingDelay, uint8_t beaconTimingChannel, TimerTime_t lastRxDone )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    Ctx.BeaconCtx.BeaconTimingDelay = ( CLASSB_BEACON_DELAY_BEACON_TIMING_ANS * beaconTimingDelay );
    Ctx.BeaconCtx.BeaconTimingChannel = beaconTimingChannel;

    if( LoRaMacConfirmQueueIsCmdActive( MLME_BEACON_TIMING ) == true )
    {
        if( Ctx.BeaconCtx.BeaconTimingDelay > CLASSB_BEACON_INTERVAL )
        {
            // We missed the beacon already
            Ctx.BeaconCtx.BeaconTimingDelay = 0;
            Ctx.BeaconCtx.BeaconTimingChannel = 0;
            LoRaMacConfirmQueueSetStatus( LORAMAC_EVENT_INFO_STATUS_BEACON_NOT_FOUND, MLME_BEACON_TIMING );
        }
        else
        {
            Ctx.BeaconCtx.Ctrl.BeaconDelaySet = 1;
            Ctx.BeaconCtx.Ctrl.BeaconChannelSet = 1;
            Ctx.BeaconCtx.NextBeaconRx = SysTimeFromMs( lastRxDone + Ctx.BeaconCtx.BeaconTimingDelay );
            LoRaMacConfirmQueueSetStatus( LORAMAC_EVENT_INFO_STATUS_OK, MLME_BEACON_TIMING );
        }

        Ctx.LoRaMacClassBParams.MlmeConfirm->BeaconTimingDelay = Ctx.BeaconCtx.BeaconTimingDelay;
        Ctx.LoRaMacClassBParams.MlmeConfirm->BeaconTimingChannel = Ctx.BeaconCtx.BeaconTimingChannel;
    }
#endif /* LORAMAC_CLASSB_ENABLED */
}

void LoRaMacClassBDeviceTimeAns( void )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )

    SysTime_t nextBeacon = SysTimeGet( );
    uint32_t currentTimeMs = SysTimeToMs( nextBeacon );

    nextBeacon.Seconds = nextBeacon.Seconds + ( 128 - ( nextBeacon.Seconds % 128 ) );
    nextBeacon.SubSeconds = 0;

    Ctx.BeaconCtx.NextBeaconRx = nextBeacon;
    Ctx.BeaconCtx.LastBeaconRx = SysTimeSub( Ctx.BeaconCtx.NextBeaconRx, ( SysTime_t ){ .Seconds = CLASSB_BEACON_INTERVAL / 1000, .SubSeconds = 0 } );

    if( LoRaMacConfirmQueueIsCmdActive( MLME_DEVICE_TIME ) == true )
    {
        if( currentTimeMs > SysTimeToMs( Ctx.BeaconCtx.NextBeaconRx ) )
        {
            // We missed the beacon already
            Ctx.BeaconCtx.LastBeaconRx.Seconds = 0;
            Ctx.BeaconCtx.LastBeaconRx.SubSeconds = 0;
            Ctx.BeaconCtx.NextBeaconRx.Seconds = 0;
            Ctx.BeaconCtx.NextBeaconRx.SubSeconds = 0;
            LoRaMacConfirmQueueSetStatus( LORAMAC_EVENT_INFO_STATUS_BEACON_NOT_FOUND, MLME_DEVICE_TIME );
        }
        else
        {
            Ctx.BeaconCtx.Ctrl.BeaconDelaySet = 1;
            Ctx.BeaconCtx.BeaconTimingDelay = SysTimeToMs( Ctx.BeaconCtx.NextBeaconRx ) - currentTimeMs;
            Ctx.BeaconCtx.BeaconTime.Seconds = nextBeacon.Seconds - UNIX_GPS_EPOCH_OFFSET - 128;
            Ctx.BeaconCtx.BeaconTime.SubSeconds = 0;
            LoRaMacConfirmQueueSetStatus( LORAMAC_EVENT_INFO_STATUS_OK, MLME_DEVICE_TIME );
        }
    }
#endif /* LORAMAC_CLASSB_ENABLED */
}

bool LoRaMacClassBBeaconFreqReq( uint32_t frequency )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    VerifyParams_t verify;

    if( frequency != 0 )
    {
        verify.Frequency = frequency;

        if( RegionVerify( *Ctx.LoRaMacClassBParams.LoRaMacRegion, &verify, PHY_FREQUENCY ) == true )
        {
            ClassBNvm->BeaconCtx.Ctrl.CustomFreq = 1;
            ClassBNvm->BeaconCtx.Frequency = frequency;
            return true;
        }
    }
    else
    {
        ClassBNvm->BeaconCtx.Ctrl.CustomFreq = 0;
        return true;
    }
    return false;
#else
    return false;
#endif /* LORAMAC_CLASSB_ENABLED */
}

TimerTime_t LoRaMacClassBIsUplinkCollision( TimerTime_t txTimeOnAir )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    TimerTime_t currentTime = TimerGetCurrentTime( );
    TimerTime_t beaconReserved = 0;
    TimerTime_t nextBeacon = SysTimeToMs( Ctx.BeaconCtx.NextBeaconRx );

    beaconReserved = nextBeacon -
                     CLASSB_BEACON_GUARD -
                     Ctx.LoRaMacClassBParams.LoRaMacParams->ReceiveDelay1 -
                     Ctx.LoRaMacClassBParams.LoRaMacParams->ReceiveDelay2 -
                     txTimeOnAir;

    // Check if the next beacon will be received during the next uplink.
    if( ( currentTime >= beaconReserved ) && ( currentTime < ( nextBeacon + CLASSB_BEACON_RESERVED ) ) )
    {// Next beacon will be sent during the next uplink.
        return CLASSB_BEACON_RESERVED;
    }
    return 0;
#else
    return 0;
#endif /* LORAMAC_CLASSB_ENABLED */
}

void LoRaMacClassBStopRxSlots( void )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    TimerStop( &Ctx.PingSlotTimer );
    TimerStop( &Ctx.MulticastSlotTimer );

    CRITICAL_SECTION_BEGIN( );
    LoRaMacClassBEvents.Events.PingSlot = 0;
    LoRaMacClassBEvents.Events.MulticastSlot = 0;
    CRITICAL_SECTION_END( );
#endif /* LORAMAC_CLASSB_ENABLED */
}

void LoRaMacClassBStartRxSlots( void )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    if( ClassBNvm->PingSlotCtx.Ctrl.Assigned == 1 )
    {
        Ctx.PingSlotState = PINGSLOT_STATE_CALC_PING_OFFSET;
        TimerSetValue( &Ctx.PingSlotTimer, 1 );
        TimerStart( &Ctx.PingSlotTimer );

        Ctx.MulticastSlotState = PINGSLOT_STATE_CALC_PING_OFFSET;
        TimerSetValue( &Ctx.MulticastSlotTimer, 1 );
        TimerStart( &Ctx.MulticastSlotTimer );
    }
#endif /* LORAMAC_CLASSB_ENABLED */
}

void LoRaMacClassBSetMulticastPeriodicity( MulticastCtx_t* multicastChannel )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    if( multicastChannel != NULL )
    {
        multicastChannel->PingNb = CalcPingNb( multicastChannel->ChannelParams.RxParams.ClassB.Periodicity );
        multicastChannel->PingPeriod = CalcPingPeriod( multicastChannel->PingNb );
    }
#endif /* LORAMAC_CLASSB_ENABLED */
}

#if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
void LoRaMacClassBSetFPendingBit( uint32_t address, uint8_t fPendingSet )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    MulticastCtx_t *cur = Ctx.LoRaMacClassBParams.MulticastChannels;

    if( address == *Ctx.LoRaMacClassBParams.LoRaMacDevAddr )
    {
        // Unicast
        ClassBNvm->PingSlotCtx.FPendingSet = fPendingSet;
    }
    else
    {
        for( uint8_t i = 0; i < LORAMAC_MAX_MC_CTX; i++ )
        {
            if( cur != NULL )
            {
                // Set the fPending bit, if its a multicast
                if( address == cur->ChannelParams.Address )
                {
                    cur->FPendingSet = fPendingSet;
                }
            }
            cur++;
        }
    }
#endif /* LORAMAC_CLASSB_ENABLED */
}
#endif /* LORAMAC_VERSION */

void LoRaMacClassBProcess( void )
{
#if ( LORAMAC_CLASSB_ENABLED == 1 )
    LoRaMacClassBEvents_t events;

    CRITICAL_SECTION_BEGIN( );
    events = LoRaMacClassBEvents;
    LoRaMacClassBEvents.Value = 0;
    CRITICAL_SECTION_END( );

    if( events.Value != 0 )
    {
        if( events.Events.Beacon == 1 )
        {
            LoRaMacClassBProcessBeacon( );
        }
        if( events.Events.PingSlot == 1 )
        {
            LoRaMacClassBProcessPingSlot( );
        }
        if( events.Events.MulticastSlot == 1 )
        {
            LoRaMacClassBProcessMulticastSlot( );
        }
    }
#endif /* LORAMAC_CLASSB_ENABLED */
}
