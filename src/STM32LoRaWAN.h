#pragma once

#include "Arduino.h"
#include "STM32CubeWL/LoRaWAN/Mac/LoRaMac.h"
#include "STM32CubeWL/Utilities/timer/stm32_timer.h"
#include "Glue/mw_log_conf.h"

class STM32LoRaWAN {
  public:
    bool begin(/* TODO: Region/band param */)
    {
      UTIL_TIMER_Init();

      // TODO: Check that selected region is actually enabled at
      // compiletime (or maybe LoraMacInitialization already reports
      // this properly?)

      LoRaMacStatus_t res = LoRaMacInitialization(&LoRaMacPrimitives, &LoRaMacCallbacks, LORAMAC_REGION_EU868);
      if (res != LORAMAC_STATUS_OK) {
        return failure("LoRaMacInitialization failed: %s\r\n", toString(res));
      }

      res = LoRaMacStart();
      if (res != LORAMAC_STATUS_OK)
        return failure("LoRaMacStart failed: %s\r\n", toString(res));
      return true;
    }

    // TODO: See if this should be poll or maybe named otherwise
    void poll()
    {
      LoRaMacProcess( );
    }

    bool join(bool otaa)
    {
      if (otaa) {
        MlmeReq_t mlmeReq;

        mlmeReq.Type = MLME_JOIN;
        mlmeReq.Req.Join.Datarate = tx_dr;
        mlmeReq.Req.Join.NetworkActivation = ACTIVATION_TYPE_OTAA;

        // Starts the OTAA join procedure
        LoRaMacStatus_t res = LoRaMacMlmeRequest(&mlmeReq);
        if (res != LORAMAC_STATUS_OK)
          return failure("Join request failed: %s\r\n", toString(res));

        // TODO: Block for join complete? Timeout?
        // TODO: Does LoRaMac retry joins, or must we handle that?
        return true;
      } else {
        MibRequestConfirm_t mibReq;
        mibReq.Param.NetworkActivation = ACTIVATION_TYPE_ABP;
        if (!mibSet("MIB_NETWORK_ACTIVATION", MIB_NETWORK_ACTIVATION, mibReq))
          return false;

        return true;
      }
    }

    bool dataRate(uint8_t dr) {
      this->tx_dr = dr;
      return true;
    }

    int getDataRate() {
      // TODO: Check if MKRWAN returns actual datarate when ADR is in use and mimic that
      return this->tx_dr;
    }

    bool joinOTAA(const char *appEui, const char *appKey, const char *devEui = NULL) {
      if (devEui != nullptr) {
        if (!setDevEui(devEui))
          return false;
      } else {
        // TODO: Set default EUI? Or assume it is already set?
      }
      if (!setAppEui(appEui))
        return false;
      if (!setAppKey(appKey))
        return false;

      // TODO rx.clear();
      return join(true);
    }
    bool joinOTAA(String appEui, String appKey) { return joinOTAA(appEui.c_str(), appKey.c_str(), nullptr); }
    bool joinOTAA(String appEui, String appKey, String devEui) { return joinOTAA(appEui.c_str(), appKey.c_str(), devEui.c_str()); }

    bool joinABP(const char * devAddr, const char * nwkSKey, const char * appSKey) {
      if (!setDevAddr(devAddr))
        return false;
      if (!setNwkSKey(nwkSKey))
        return false;
      if (!setAppSKey(appSKey))
        return false;

      // TODO rx.clear();
      return join(false);
    }
    bool joinABP(String devAddr, String nwkSKey, String appSKey) { return joinABP(devAddr.c_str(), nwkSKey.c_str(), appSKey.c_str()); }

    // TODO: Mimic MKRWAN API
    bool send(const uint8_t *payload, size_t size, uint8_t port, bool confirmed) {
      // TODO: Check busy?
      // TODO :Check that not joined produces proper error

      McpsReq_t mcpsReq;
      mcpsReq.Type = confirmed ? MCPS_CONFIRMED : MCPS_UNCONFIRMED;

      // TODO: ADR?
      if (confirmed) {
        mcpsReq.Req.Confirmed.Datarate = this->tx_dr;
        mcpsReq.Req.Confirmed.fPort = port;
        mcpsReq.Req.Confirmed.fBufferSize = size;
        mcpsReq.Req.Confirmed.fBuffer = const_cast<uint8_t*>(payload);
        // TODO: What value to use? And why is this only defined for 1.0.3?
        // See https://github.com/Lora-net/LoRaMac-node/discussions/1096
        #if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000300 ))
        mcpsReq.Req.Confirmed.NbTrials = 1;
        #endif /* LORAMAC_VERSION */
      } else {
        mcpsReq.Req.Unconfirmed.Datarate = this->tx_dr;
        mcpsReq.Req.Unconfirmed.fPort = port;
        mcpsReq.Req.Unconfirmed.fBufferSize = size;
        mcpsReq.Req.Unconfirmed.fBuffer = const_cast<uint8_t*>(payload);
      }

      /* TODO
       * It seems LmHandler silently discards data in this case?
      if( LoRaMacQueryTxPossible( appData->BufferSize, &txInfo ) != LORAMAC_STATUS_OK )
      {
          // Send empty frame in order to flush MAC commands
          mcpsReq.Type = MCPS_UNCONFIRMED;
          mcpsReq.Req.Unconfirmed.fBuffer = NULL;
          mcpsReq.Req.Unconfirmed.fBufferSize = 0;
          lmhStatus = LORAMAC_HANDLER_PAYLOAD_LENGTH_RESTRICTED;
      }
      else
      {
      }
      */

      // TODO: allowDelayedTx
      LoRaMacStatus_t res = LoRaMacMcpsRequest(&mcpsReq, /* allowDelayedTx */ true);
      if (res != LORAMAC_STATUS_OK)
        return failure("Failed to send packet: %s\r\n", toString(res));

      // TODO: Block on confirmed uplinks?

      // TODO? DutyCycleWaitTime = mcpsReq.ReqReturn.DutyCycleWaitTime;
      return true;
    }

    bool mibGet(const char* name, Mib_t type, MibRequestConfirm_t& mibReq) {
      mibReq.Type = type;
      LoRaMacStatus_t res = LoRaMacMibGetRequestConfirm(&mibReq);
      if (res != LORAMAC_STATUS_OK)
        return failure("Failed to get %s: %s\r\n", name, toString(res));

      return true;
    }

    bool mibGetBool(const char* name, Mib_t type, bool *value) {
      MibRequestConfirm_t mibReq;
      if (!mibGet(name, type, mibReq))
        return false;

      switch(type) {
        case MIB_ADR: *value = mibReq.Param.AdrEnable; break;
        case MIB_PUBLIC_NETWORK: *value = mibReq.Param.EnablePublicNetwork; break;
        case MIB_REPEATER_SUPPORT: *value = mibReq.Param.EnableRepeaterSupport; break;
        default:
          return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
      }
      return true;
    }

    bool mibSet(const char* name, Mib_t type, MibRequestConfirm_t& mibReq) {
      mibReq.Type = type;
      LoRaMacStatus_t res = LoRaMacMibSetRequestConfirm(&mibReq);
      if (res != LORAMAC_STATUS_OK)
        return failure("Failed to set %s: %s\r\n", name, toString(res));

      return true;
    }

    bool mibSetHex(const char* name, Mib_t type, const char* value) {
      // The buffer-passing API is a bit fragile, since the size of the
      // buffer to be passed is implicit, and also not very well
      // documented. So we need to derive the size here.
      size_t size;
      switch(type) {
        case MIB_DEV_EUI:
        case MIB_JOIN_EUI:
          size = SE_EUI_SIZE;
          break;

        case MIB_DEV_ADDR:
          size = sizeof(MibRequestConfirm_t::Param.DevAddr);
          break;

        case MIB_APP_KEY:
        case MIB_NWK_KEY:
        #if ( USE_LRWAN_1_1_X_CRYPTO == 1 )
        case MIB_J_S_INT_KEY:
        case MIB_J_S_ENC_KEY:
        case MIB_F_NWK_S_INT_KEY:
        case MIB_S_NWK_S_INT_KEY:
        case MIB_NWK_S_ENC_KEY:
        #else /* USE_LRWAN_1_1_X_CRYPTO == 0 */
        case MIB_NWK_S_KEY:
        #endif /* USE_LRWAN_1_1_X_CRYPTO */
        case MIB_APP_S_KEY:
        case MIB_MC_KE_KEY:
        #if ( LORAMAC_MAX_MC_CTX > 0 )
        case MIB_MC_KEY_0:
        case MIB_MC_APP_S_KEY_0:
        case MIB_MC_NWK_S_KEY_0:
        #endif /* LORAMAC_MAX_MC_CTX > 0 */
        #if ( LORAMAC_MAX_MC_CTX > 1 )
        case MIB_MC_KEY_1:
        case MIB_MC_APP_S_KEY_1:
        case MIB_MC_NWK_S_KEY_1:
        #endif /* LORAMAC_MAX_MC_CTX > 1 */
        #if ( LORAMAC_MAX_MC_CTX > 2 )
        case MIB_MC_KEY_2:
        case MIB_MC_APP_S_KEY_2:
        case MIB_MC_NWK_S_KEY_2:
        #endif /* LORAMAC_MAX_MC_CTX > 2 */
        #if ( LORAMAC_MAX_MC_CTX > 3 )
        case MIB_MC_KEY_3:
        case MIB_MC_APP_S_KEY_3:
        case MIB_MC_NWK_S_KEY_3:
        #endif /* LORAMAC_MAX_MC_CTX > 3 */
          size = SE_KEY_SIZE;
          break;

        default:
          return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
      }

      uint8_t buf[size];
      if (!parseHex(buf, value, size))
        return false;

      MibRequestConfirm_t mibReq;
      mibReq.Type = type;
      switch(type) {
        case MIB_DEV_EUI: mibReq.Param.DevEui = buf; break;
        case MIB_JOIN_EUI: mibReq.Param.JoinEui = buf; break;
        // This assumes big endian, since that's the natural way to
        // write down a a number in hex
        case MIB_DEV_ADDR: mibReq.Param.DevAddr = makeUint32(buf[0], buf[1], buf[2], buf[3]); break;
        case MIB_APP_KEY: mibReq.Param.AppKey = buf; break;
        case MIB_NWK_KEY: mibReq.Param.NwkKey = buf; break;
        #if ( USE_LRWAN_1_1_X_CRYPTO == 1 )
        case MIB_J_S_INT_KEY: mibReq.Param.JSIntKey = buf; break;
        case MIB_J_S_ENC_KEY: mibReq.Param.JSEncKey = buf; break;
        case MIB_F_NWK_S_INT_KEY: mibReq.Param.FNwkSIntKey = buf; break;
        case MIB_S_NWK_S_INT_KEY: mibReq.Param.SNwkSIntKey = buf; break;
        case MIB_NWK_S_ENC_KEY: mibReq.Param.NwkSEncKey = buf; break;
        #else /* USE_LRWAN_1_1_X_CRYPTO == 0 */
        case MIB_NWK_S_KEY: mibReq.Param.NwkSKey = buf; break;
        #endif /* USE_LRWAN_1_1_X_CRYPTO */
        case MIB_APP_S_KEY: mibReq.Param.AppSKey = buf; break;
        case MIB_MC_KE_KEY: mibReq.Param.McKEKey = buf; break;
        #if ( LORAMAC_MAX_MC_CTX > 0 )
        case MIB_MC_KEY_0: mibReq.Param.McKey0 = buf; break;
        case MIB_MC_APP_S_KEY_0: mibReq.Param.McAppSKey0 = buf; break;
        case MIB_MC_NWK_S_KEY_0: mibReq.Param.McNwkSKey0 = buf; break;
        #endif /* LORAMAC_MAX_MC_CTX > 0 */
        #if ( LORAMAC_MAX_MC_CTX > 1 )
        case MIB_MC_KEY_1: mibReq.Param.McKey1 = buf; break;
        case MIB_MC_APP_S_KEY_1: mibReq.Param.McAppSKey1 = buf; break;
        case MIB_MC_NWK_S_KEY_1: mibReq.Param.McNwkSKey1 = buf; break;
        #endif /* LORAMAC_MAX_MC_CTX > 1 */
        #if ( LORAMAC_MAX_MC_CTX > 2 )
        case MIB_MC_KEY_2: mibReq.Param.McKey2 = buf; break;
        case MIB_MC_APP_S_KEY_2: mibReq.Param.McAppSKey2 = buf; break;
        case MIB_MC_NWK_S_KEY_2: mibReq.Param.McNwkSKey2 = buf; break;
        #endif /* LORAMAC_MAX_MC_CTX > 2 */
        #if ( LORAMAC_MAX_MC_CTX > 3 )
        case MIB_MC_KEY_3: mibReq.Param.McKey3 = buf; break;
        case MIB_MC_APP_S_KEY_3: mibReq.Param.McAppSKey3 = buf; break;
        case MIB_MC_NWK_S_KEY_3: mibReq.Param.McNwkSKey3 = buf; break;
        #endif /* LORAMAC_MAX_MC_CTX > 3 */
        default:
          return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
      }

      return mibSet(name, type, mibReq);
    }

/*
          case MIB_ADR: mibReq.Param.AdrEnable = value; break;
          case MIB_NET_ID: mibReq.Param.NetID = value; break;
          case MIB_PUBLIC_NETWORK: mibReq.Param.EnablePublicNetwork = value; break;
          case MIB_REPEATER_SUPPORT: mibReq.Param.EnableRepeaterSupport = value; break;
          case MIB_CHANNELS: mibReq.Param.ChannelList = value; break;
          case MIB_RX2_CHANNEL: mibReq.Param.Rx2Channel = value; break;
          case MIB_RX2_DEFAULT_CHANNEL: mibReq.Param.Rx2DefaultChannel = value; break;
          case MIB_RXC_CHANNEL: mibReq.Param.RxCChannel = value; break;
          case MIB_RXC_DEFAULT_CHANNEL: mibReq.Param.RxCDefaultChannel = value; break;
          case MIB_CHANNELS_MASK: mibReq.Param.ChannelsMask = value; break;
          case MIB_CHANNELS_DEFAULT_MASK: mibReq.Param.ChannelsDefaultMask = value; break;
          case MIB_CHANNELS_NB_TRANS: mibReq.Param.ChannelsNbTrans = value; break;
          case MIB_MAX_RX_WINDOW_DURATION: mibReq.Param.MaxRxWindow = value; break;
          case MIB_RECEIVE_DELAY_1: mibReq.Param.ReceiveDelay1 = value; break;
          case MIB_RECEIVE_DELAY_2: mibReq.Param.ReceiveDelay2 = value; break;
          case MIB_JOIN_ACCEPT_DELAY_1: mibReq.Param.JoinAcceptDelay1 = value; break;
          case MIB_JOIN_ACCEPT_DELAY_2: mibReq.Param.JoinAcceptDelay2 = value; break;
          case MIB_CHANNELS_MIN_TX_DATARATE: mibReq.Param.ChannelsMinTxDatarate = value; break;
          case MIB_CHANNELS_DEFAULT_DATARATE: mibReq.Param.ChannelsDefaultDatarate = value; break;
          case MIB_CHANNELS_DATARATE: mibReq.Param.ChannelsDatarate = value; break;
          case MIB_CHANNELS_DEFAULT_TX_POWER: mibReq.Param.ChannelsDefaultTxPower = value; break;
          case MIB_CHANNELS_TX_POWER: mibReq.Param.ChannelsTxPower = value; break;
          case MIB_MULTICAST_CHANNEL: mibReq.Param.MulticastChannel = value; break;
          case MIB_SYSTEM_MAX_RX_ERROR: mibReq.Param.SystemMaxRxError = value; break;
          case MIB_MIN_RX_SYMBOLS: mibReq.Param.MinRxSymbols = value; break;
          case MIB_ANTENNA_GAIN: mibReq.Param.AntennaGain = value; break;
          case MIB_DEFAULT_ANTENNA_GAIN: mibReq.Param.DefaultAntennaGain = value; break;
          case MIB_NVM_CTXS: mibReq.Param.Contexts = value; break;
          case MIB_NVM_CTXS: mibReq.Param.BackupContexts = value; break;
          case MIB_ABP_LORAWAN_VERSION: mibReq.Param.AbpLrWanVersion = value; break;
          case MIB_BEACON_INTERVAL: mibReq.Param.BeaconInterval = value; break;
          case MIB_BEACON_RESERVED: mibReq.Param.BeaconReserved = value; break;
          case MIB_BEACON_GUARD: mibReq.Param.BeaconGuard = value; break;
          case MIB_BEACON_WINDOW: mibReq.Param.BeaconWindow = value; break;
          case MIB_BEACON_WINDOW_SLOTS: mibReq.Param.BeaconWindowSlots = value; break;
          case MIB_PING_SLOT_WINDOW: mibReq.Param.PingSlotWindow = value; break;
          case MIB_BEACON_SYMBOL_TO_DEFAULT: mibReq.Param.BeaconSymbolToDefault = value; break;
          case MIB_BEACON_SYMBOL_TO_EXPANSION_MAX: mibReq.Param.BeaconSymbolToExpansionMax = value; break;
          case MIB_PING_SLOT_SYMBOL_TO_EXPANSION_MAX: mibReq.Param.PingSlotSymbolToExpansionMax = value; break;
          case MIB_BEACON_SYMBOL_TO_EXPANSION_FACTOR: mibReq.Param.BeaconSymbolToExpansionFactor = value; break;
          case MIB_PING_SLOT_SYMBOL_TO_EXPANSION_FACTOR: mibReq.Param.PingSlotSymbolToExpansionFactor = value; break;
          case MIB_MAX_BEACON_LESS_PERIOD: mibReq.Param.MaxBeaconLessPeriod = value; break;
          case MIB_PING_SLOT_DATARATE: mibReq.Param.PingSlotDatarate = value; break;
          case MIB_RXB_C_TIMEOUT: mibReq.Param.RxBCTimeout = value; break;
          case MIB_IS_CERT_FPORT_ON: mibReq.Param.IsCertPortOn = value; break;
          case MIB_BEACON_STATE: mibReq.Param.BeaconState = value; break;


          case MIB_NET_ID: *value = mibReq.Param.NetID break;
          case MIB_CHANNELS: *value = mibReq.Param.ChannelList break;
          case MIB_RX2_CHANNEL: *value = mibReq.Param.Rx2Channel break;
          case MIB_RX2_DEFAULT_CHANNEL: *value = mibReq.Param.Rx2DefaultChannel break;
          case MIB_RXC_CHANNEL: *value = mibReq.Param.RxCChannel break;
          case MIB_RXC_DEFAULT_CHANNEL: *value = mibReq.Param.RxCDefaultChannel break;
          case MIB_CHANNELS_MASK: *value = mibReq.Param.ChannelsMask break;
          case MIB_CHANNELS_DEFAULT_MASK: *value = mibReq.Param.ChannelsDefaultMask break;
          case MIB_CHANNELS_NB_TRANS: *value = mibReq.Param.ChannelsNbTrans break;
          case MIB_MAX_RX_WINDOW_DURATION: *value = mibReq.Param.MaxRxWindow break;
          case MIB_RECEIVE_DELAY_1: *value = mibReq.Param.ReceiveDelay1 break;
          case MIB_RECEIVE_DELAY_2: *value = mibReq.Param.ReceiveDelay2 break;
          case MIB_JOIN_ACCEPT_DELAY_1: *value = mibReq.Param.JoinAcceptDelay1 break;
          case MIB_JOIN_ACCEPT_DELAY_2: *value = mibReq.Param.JoinAcceptDelay2 break;
          case MIB_CHANNELS_MIN_TX_DATARATE: *value = mibReq.Param.ChannelsMinTxDatarate break;
          case MIB_CHANNELS_DEFAULT_DATARATE: *value = mibReq.Param.ChannelsDefaultDatarate break;
          case MIB_CHANNELS_DATARATE: *value = mibReq.Param.ChannelsDatarate break;
          case MIB_CHANNELS_DEFAULT_TX_POWER: *value = mibReq.Param.ChannelsDefaultTxPower break;
          case MIB_CHANNELS_TX_POWER: *value = mibReq.Param.ChannelsTxPower break;
          case MIB_MULTICAST_CHANNEL: *value = mibReq.Param.MulticastChannel break;
          case MIB_SYSTEM_MAX_RX_ERROR: *value = mibReq.Param.SystemMaxRxError break;
          case MIB_MIN_RX_SYMBOLS: *value = mibReq.Param.MinRxSymbols break;
          case MIB_ANTENNA_GAIN: *value = mibReq.Param.AntennaGain break;
          case MIB_DEFAULT_ANTENNA_GAIN: *value = mibReq.Param.DefaultAntennaGain break;
          case MIB_NVM_CTXS: *value = mibReq.Param.Contexts break;
          case MIB_NVM_CTXS: *value = mibReq.Param.BackupContexts break;
          case MIB_ABP_LORAWAN_VERSION: *value = mibReq.Param.AbpLrWanVersion break;
          case MIB_BEACON_INTERVAL: *value = mibReq.Param.BeaconInterval break;
          case MIB_BEACON_RESERVED: *value = mibReq.Param.BeaconReserved break;
          case MIB_BEACON_GUARD: *value = mibReq.Param.BeaconGuard break;
          case MIB_BEACON_WINDOW: *value = mibReq.Param.BeaconWindow break;
          case MIB_BEACON_WINDOW_SLOTS: *value = mibReq.Param.BeaconWindowSlots break;
          case MIB_PING_SLOT_WINDOW: *value = mibReq.Param.PingSlotWindow break;
          case MIB_BEACON_SYMBOL_TO_DEFAULT: *value = mibReq.Param.BeaconSymbolToDefault break;
          case MIB_BEACON_SYMBOL_TO_EXPANSION_MAX: *value = mibReq.Param.BeaconSymbolToExpansionMax break;
          case MIB_PING_SLOT_SYMBOL_TO_EXPANSION_MAX: *value = mibReq.Param.PingSlotSymbolToExpansionMax break;
          case MIB_BEACON_SYMBOL_TO_EXPANSION_FACTOR: *value = mibReq.Param.BeaconSymbolToExpansionFactor break;
          case MIB_PING_SLOT_SYMBOL_TO_EXPANSION_FACTOR: *value = mibReq.Param.PingSlotSymbolToExpansionFactor break;
          case MIB_MAX_BEACON_LESS_PERIOD: *value = mibReq.Param.MaxBeaconLessPeriod break;
          case MIB_PING_SLOT_DATARATE: *value = mibReq.Param.PingSlotDatarate break;
          case MIB_RXB_C_TIMEOUT: *value = mibReq.Param.RxBCTimeout break;
          case MIB_IS_CERT_FPORT_ON: *value = mibReq.Param.IsCertPortOn break;
          case MIB_BEACON_STATE: *value = mibReq.Param.BeaconState break;
*/


    // TODO: uin64_t / uint32_t versions of EUI and Id setters?
    bool setDevEui(const char* value) { return mibSetHex("DevEui", MIB_DEV_EUI, value); }
    bool setDevEui(String value) { return setDevEui(value.c_str()); }
    bool setAppEui(const char* value) { return mibSetHex("AppEui", MIB_JOIN_EUI, value); }
    bool setAppEui(String value) { return setDevEui(value.c_str()); }
    bool setDevAddr(const char* value) { return mibSetHex("DevAddr", MIB_DEV_ADDR, value); }
    bool setDevAddr(String value) { return setDevAddr(value.c_str()); }
    bool setAppKey(const char* value) {
      // In LoRaWAN 1.0, only the appKey was configured and all keys
      // were derived from that. In 1.1, this was split into an appKey
      // and nwkKey. However, when running the LoRaMac-Node in 1.0 mode,
      // it actually seems to use nwkKey, not appKey. So to support
      // sketches that only configure appKey for 1.0, this saves the
      // appKey to nwkKey as well. But to also prepare for future
      // support of 1.1 and sketches that configure both, only do this
      // if no nwkKey was explicitly configured.
      return mibSetHex("AppKey", MIB_APP_KEY, value)
             && (this->nwkKeySet || mibSetHex("NwkKey", MIB_NWK_KEY, value));

    }
    bool setAppKey(String value) { return setAppKey(value.c_str()); }
    bool setNwkKey(const char* value) {
      this->nwkKeySet = true;
      return mibSetHex("NwkKey", MIB_NWK_KEY, value);
    }
    bool setNwkKey(String value) { return setNwkKey(value.c_str()); }
    bool setAppSKey(const char* value) { return mibSetHex("AppSKey", MIB_APP_S_KEY, value); }
    bool setAppSKey(String value) { return setAppSKey(value.c_str()); }
    bool setNwkSKey(const char* value) {
      #if ( USE_LRWAN_1_1_X_CRYPTO == 1 )
        // When compiled for 1.1 crypto, three different keys are used.
        // When the sketch only supplies a single key, just set all
        // three keys to the same value.
        return mibSetHex("NwkSEncKey", MIB_NWK_S_ENC_KEY, value)
               && mibSetHex("FNwkSIntKey", MIB_F_NWK_S_INT_KEY, value)
               && mibSetHex("SNwkSIntKey", MIB_S_NWK_S_INT_KEY, value);
      #else /* USE_LRWAN_1_1_X_CRYPTO == 0 */
        return mibSetHex("NwkSKey", MIB_NWK_S_KEY, value);
      #endif /* USE_LRWAN_1_1_X_CRYPTO */
    }
    bool setNwkSKey(String value) { return setNwkSKey(value.c_str()); }

    #if ( USE_LRWAN_1_1_X_CRYPTO == 1 )
    bool setNwkSEncKey(const char* value) { return mibSetHex("NwkSEncKey", MIB_NWK_S_ENC_KEY, value); }
    bool setNwkSEncKey(String value) { return setNwkSEncKey(value.c_str()); }
    bool setFNwkSIntKey(const char* value) { return mibSetHex("FNwkSIntKey", MIB_F_NWK_S_INT_KEY, value); }
    bool setFNwkSIntKey(String value) { return setFNwkSIntKey(value.c_str()); }
    bool setSNwkSIntKey(const char* value) { return mibSetHex("SNwkSIntKey", MIB_S_NWK_S_INT_KEY, value); }
    bool setSNwkSIntKey(String value) { return setSNwkSIntKey(value.c_str()); }
    #endif /* USE_LRWAN_1_1_X_CRYPTO */

    bool connected() {
      MibRequestConfirm_t mibReq;
      return mibGet("MIB_NETWORK_ACTIVATION", MIB_NETWORK_ACTIVATION, mibReq)
             && (mibReq.Param.NetworkActivation != ACTIVATION_TYPE_NONE);
    }

  protected:
    // TODO: Implement these
    static void MacMcpsConfirm(McpsConfirm_t* McpsConfirm) { }
    static void MacMcpsIndication(McpsIndication_t* McpsIndication, LoRaMacRxStatus_t* RxStatus) { }
    static void MacMlmeConfirm(MlmeConfirm_t* MlmeConfirm) { }
    static void MacMlmeIndication(MlmeIndication_t* MlmeIndication, LoRaMacRxStatus_t* RxStatus) { }

    LoRaMacPrimitives_t LoRaMacPrimitives = {
      .MacMcpsConfirm = MacMcpsConfirm,
      .MacMcpsIndication = MacMcpsIndication,
      .MacMlmeConfirm = MacMlmeConfirm,
      .MacMlmeIndication = MacMlmeIndication,
    };

    LoRaMacCallback_t LoRaMacCallbacks = {
      // TODO: Do we need these?
      .GetBatteryLevel = nullptr,
      .GetTemperatureLevel = nullptr,
      .GetUniqueId = nullptr,
      .NvmDataChange = nullptr,
      .MacProcessNotify = nullptr,
    };

    static const char *toString(LoRaMacStatus_t);
    static uint8_t parseHex(char c);
    static bool parseHex(uint8_t *dest, const char *hex, size_t dest_len);
    static bool makeUint32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { return (uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)c << 8 | (uint32_t)d << 0; }

    /**
     * Helper that prints an error and then always returns false, to
     * allow for combining reporting and returning in a single line.
     */
    static bool failure(const char* fmt, ...);

    // This is used for joining and transmission (until ADR changes
    // it, if enabled). This defaults to DR 4 since that is the
    // fastest/highest (least spectrum usage) DR that is supported
    // by all regions. If this DR has insufficient range, the join
    // process (and ADR) will fall back to lower datarates
    // automaticall.
    uint8_t tx_dr = DR_4;

    bool nwkKeySet = false;
};
