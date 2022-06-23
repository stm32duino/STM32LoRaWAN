#include "STM32LoRaWAN.h"
#include <core_debug.h>

bool STM32LoRaWAN::begin(_lora_band band)
{
  UTIL_TIMER_Init();

  LoRaMacStatus_t res = LoRaMacInitialization(&LoRaMacPrimitives, &LoRaMacCallbacks, (LoRaMacRegion_t)band);
  if (res != LORAMAC_STATUS_OK) {
    return failure("LoRaMacInitialization failed: %s\r\n", toString(res));
  }

  res = LoRaMacStart();
  if (res != LORAMAC_STATUS_OK)
    return failure("LoRaMacStart failed: %s\r\n", toString(res));

  return true;
}

void STM32LoRaWAN::poll()
{
  LoRaMacProcess( );
}

bool STM32LoRaWAN::join(bool otaa)
{
  // TODO rx.clear();
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
    // TODO: Retry joins
    return true;
  } else {
    MibRequestConfirm_t mibReq;
    mibReq.Param.NetworkActivation = ACTIVATION_TYPE_ABP;
    if (!mibSet("MIB_NETWORK_ACTIVATION", MIB_NETWORK_ACTIVATION, mibReq))
      return false;

    return true;
  }
}

bool STM32LoRaWAN::dataRate(uint8_t dr) {
  this->tx_dr = dr;
  return true;
}

int STM32LoRaWAN::getDataRate() {
  // TODO: Check if MKRWAN returns actual datarate when ADR is in use and mimic that
  return this->tx_dr;
}

bool STM32LoRaWAN::send(const uint8_t *payload, size_t size, uint8_t port, bool confirmed) {
  McpsReq_t mcpsReq;
  mcpsReq.Type = confirmed ? MCPS_CONFIRMED : MCPS_UNCONFIRMED;

  if (confirmed) {
    // TODO: ADR
    mcpsReq.Req.Confirmed.Datarate = this->tx_dr;
    mcpsReq.Req.Confirmed.fPort = port;
    mcpsReq.Req.Confirmed.fBufferSize = size;
    mcpsReq.Req.Confirmed.fBuffer = const_cast<uint8_t*>(payload);
    #if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000300 ))
    // Mimic the 1.0.4 behavior where the application manages retransmissions.
    // See https://github.com/Lora-net/LoRaMac-node/discussions/1096
    mcpsReq.Req.Confirmed.NbTrials = 1;
    #endif /* LORAMAC_VERSION */
  } else {
    mcpsReq.Req.Unconfirmed.Datarate = this->tx_dr;
    mcpsReq.Req.Unconfirmed.fPort = port;
    mcpsReq.Req.Unconfirmed.fBufferSize = size;
    mcpsReq.Req.Unconfirmed.fBuffer = const_cast<uint8_t*>(payload);
  }

  LoRaMacTxInfo_t txInfo;
  if( LoRaMacQueryTxPossible( size, &txInfo ) == LORAMAC_STATUS_LENGTH_ERROR ) {
    if (size > txInfo.CurrentPossiblePayloadSize)
      return failure("Packet too long, only %s bytes of payload supported at current datarate\r\n", txInfo.CurrentPossiblePayloadSize);

    // If the packet would fit, but is still rejected, this means there
    // are pending MAC commands that do not fit in the header along with
    // the payload, or do not fit in the header at all (i.e. require an
    // empty uplink). Schedule an uplink to flush these commands.
    mcpsReq.Type = MCPS_UNCONFIRMED;
    mcpsReq.Req.Unconfirmed.fBuffer = NULL;
    mcpsReq.Req.Unconfirmed.fBufferSize = 0;
    LoRaMacMcpsRequest(&mcpsReq, /* allowDelayedTx */ true);

    return failure("Cannot send packet, wait for pending MAC commands to be sent\r\n");
  }

  LoRaMacStatus_t res = LoRaMacMcpsRequest(&mcpsReq, /* allowDelayedTx */ true);
  if (res != LORAMAC_STATUS_OK)
    return failure("Failed to send packet: %s\r\n", toString(res));

  // TODO: Report mcpsReq.ReqReturn.DutyCycleWaitTime somewhere?
  return true;
}

// All these MIB get and set functions have quite a bit of boilerplate,
// but at least they make their callers a lot less verbose.
bool STM32LoRaWAN::mibGet(const char* name, Mib_t type, MibRequestConfirm_t& mibReq) {
  mibReq.Type = type;
  LoRaMacStatus_t res = LoRaMacMibGetRequestConfirm(&mibReq);
  if (res != LORAMAC_STATUS_OK)
    return failure("Failed to get %s: %s\r\n", name, toString(res));

  return true;
}

bool STM32LoRaWAN::mibSet(const char* name, Mib_t type, MibRequestConfirm_t& mibReq) {
  mibReq.Type = type;
  LoRaMacStatus_t res = LoRaMacMibSetRequestConfirm(&mibReq);
  if (res != LORAMAC_STATUS_OK)
    return failure("Failed to set %s: %s\r\n", name, toString(res));

  return true;
}

bool STM32LoRaWAN::mibGetBool(const char* name, Mib_t type, bool *value) {
  MibRequestConfirm_t mibReq;
  if (!mibGet(name, type, mibReq))
    return false;

  switch(type) {
    case MIB_ADR: *value = mibReq.Param.AdrEnable; break;
    case MIB_PUBLIC_NETWORK: *value = mibReq.Param.EnablePublicNetwork; break;
    case MIB_REPEATER_SUPPORT: *value = mibReq.Param.EnableRepeaterSupport; break;
    default: return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }
  return true;
}

bool STM32LoRaWAN::mibSetBool(const char* name, Mib_t type, bool value) {
  MibRequestConfirm_t mibReq;
  switch(type) {
    case MIB_ADR: mibReq.Param.AdrEnable = value; break;
    case MIB_PUBLIC_NETWORK: mibReq.Param.EnablePublicNetwork = value; break;
    case MIB_REPEATER_SUPPORT: mibReq.Param.EnableRepeaterSupport = value; break;
    default: return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }

  return mibSet(name, type, mibReq);
}

bool STM32LoRaWAN::mibGetUint8(const char* name, Mib_t type, uint8_t *value) {
  MibRequestConfirm_t mibReq;
  if (!mibGet(name, type, mibReq))
    return false;

  switch(type) {
    case MIB_CHANNELS_NB_TRANS: *value = mibReq.Param.ChannelsNbTrans; break;
    case MIB_MIN_RX_SYMBOLS: *value = mibReq.Param.MinRxSymbols; break;
    default: return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }
  return true;
}

bool STM32LoRaWAN::mibSetUint8(const char* name, Mib_t type, uint8_t value) {
  MibRequestConfirm_t mibReq;
  switch(type) {
    case MIB_CHANNELS_NB_TRANS: mibReq.Param.ChannelsNbTrans = value; break;
    case MIB_MIN_RX_SYMBOLS: mibReq.Param.MinRxSymbols = value; break;
    default: return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }

  return mibSet(name, type, mibReq);
}

bool STM32LoRaWAN::mibGetInt8(const char* name, Mib_t type, int8_t *value) {
  MibRequestConfirm_t mibReq;
  if (!mibGet(name, type, mibReq))
    return false;

  switch(type) {
    #if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
    case MIB_CHANNELS_MIN_TX_DATARATE: *value = mibReq.Param.ChannelsMinTxDatarate; break;
    #endif /* LORAMAC_VERSION */
    case MIB_CHANNELS_DEFAULT_DATARATE: *value = mibReq.Param.ChannelsDefaultDatarate; break;
    case MIB_CHANNELS_DATARATE: *value = mibReq.Param.ChannelsDatarate; break;
    case MIB_CHANNELS_DEFAULT_TX_POWER: *value = mibReq.Param.ChannelsDefaultTxPower; break;
    case MIB_CHANNELS_TX_POWER: *value = mibReq.Param.ChannelsTxPower; break;
    case MIB_PING_SLOT_DATARATE: *value = mibReq.Param.PingSlotDatarate; break;
    default: return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }
  return true;
}

bool STM32LoRaWAN::mibSetInt8(const char* name, Mib_t type, int8_t value) {
  MibRequestConfirm_t mibReq;
  switch(type) {
    #if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
    case MIB_CHANNELS_MIN_TX_DATARATE: mibReq.Param.ChannelsMinTxDatarate = value; break;
    #endif /* LORAMAC_VERSION */
    case MIB_CHANNELS_DEFAULT_DATARATE: mibReq.Param.ChannelsDefaultDatarate = value; break;
    case MIB_CHANNELS_DATARATE: mibReq.Param.ChannelsDatarate = value; break;
    case MIB_CHANNELS_DEFAULT_TX_POWER: mibReq.Param.ChannelsDefaultTxPower = value; break;
    case MIB_CHANNELS_TX_POWER: mibReq.Param.ChannelsTxPower = value; break;
    case MIB_PING_SLOT_DATARATE: mibReq.Param.PingSlotDatarate = value; break;
    default: return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }

  return mibSet(name, type, mibReq);
}

bool STM32LoRaWAN::mibGetUint32(const char* name, Mib_t type, uint32_t *value) {
  MibRequestConfirm_t mibReq;
  if (!mibGet(name, type, mibReq))
    return false;

  switch(type) {
    case MIB_NET_ID: *value = mibReq.Param.NetID; break;
    case MIB_DEV_ADDR: *value = mibReq.Param.DevAddr; break;
    case MIB_MAX_RX_WINDOW_DURATION: *value = mibReq.Param.MaxRxWindow; break;
    case MIB_RECEIVE_DELAY_1: *value = mibReq.Param.ReceiveDelay1; break;
    case MIB_RECEIVE_DELAY_2: *value = mibReq.Param.ReceiveDelay2; break;
    case MIB_JOIN_ACCEPT_DELAY_1: *value = mibReq.Param.JoinAcceptDelay1; break;
    case MIB_JOIN_ACCEPT_DELAY_2: *value = mibReq.Param.JoinAcceptDelay2; break;
    case MIB_SYSTEM_MAX_RX_ERROR: *value = mibReq.Param.SystemMaxRxError; break;
    case MIB_BEACON_INTERVAL: *value = mibReq.Param.BeaconInterval; break;
    case MIB_BEACON_RESERVED: *value = mibReq.Param.BeaconReserved; break;
    case MIB_BEACON_GUARD: *value = mibReq.Param.BeaconGuard; break;
    case MIB_BEACON_WINDOW: *value = mibReq.Param.BeaconWindow; break;
    case MIB_BEACON_WINDOW_SLOTS: *value = mibReq.Param.BeaconWindowSlots; break;
    case MIB_PING_SLOT_WINDOW: *value = mibReq.Param.PingSlotWindow; break;
    case MIB_BEACON_SYMBOL_TO_DEFAULT: *value = mibReq.Param.BeaconSymbolToDefault; break;
    case MIB_BEACON_SYMBOL_TO_EXPANSION_MAX: *value = mibReq.Param.BeaconSymbolToExpansionMax; break;
    case MIB_PING_SLOT_SYMBOL_TO_EXPANSION_MAX: *value = mibReq.Param.PingSlotSymbolToExpansionMax; break;
    case MIB_BEACON_SYMBOL_TO_EXPANSION_FACTOR: *value = mibReq.Param.BeaconSymbolToExpansionFactor; break;
    case MIB_PING_SLOT_SYMBOL_TO_EXPANSION_FACTOR: *value = mibReq.Param.PingSlotSymbolToExpansionFactor; break;
    case MIB_MAX_BEACON_LESS_PERIOD: *value = mibReq.Param.MaxBeaconLessPeriod; break;
    case MIB_RXB_C_TIMEOUT: *value = mibReq.Param.RxBCTimeout; break;
    default: return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }
  return true;
}

bool STM32LoRaWAN::mibSetUint32(const char* name, Mib_t type, uint32_t value) {
  MibRequestConfirm_t mibReq;
  switch(type) {
    case MIB_NET_ID: mibReq.Param.NetID = value; break;
    case MIB_DEV_ADDR: mibReq.Param.DevAddr = value; break;
    case MIB_MAX_RX_WINDOW_DURATION: mibReq.Param.MaxRxWindow = value; break;
    case MIB_RECEIVE_DELAY_1: mibReq.Param.ReceiveDelay1 = value; break;
    case MIB_RECEIVE_DELAY_2: mibReq.Param.ReceiveDelay2 = value; break;
    case MIB_JOIN_ACCEPT_DELAY_1: mibReq.Param.JoinAcceptDelay1 = value; break;
    case MIB_JOIN_ACCEPT_DELAY_2: mibReq.Param.JoinAcceptDelay2 = value; break;
    case MIB_SYSTEM_MAX_RX_ERROR: mibReq.Param.SystemMaxRxError = value; break;
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
    case MIB_RXB_C_TIMEOUT: mibReq.Param.RxBCTimeout = value; break;
    default: return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }

  return mibSet(name, type, mibReq);
}

bool STM32LoRaWAN::mibGetUint64(const char* name, Mib_t type, uint64_t *value) {
  MibRequestConfirm_t mibReq;
  if (!mibGet(name, type, mibReq))
    return false;

  const uint8_t *buf;

  switch(type) {
    case MIB_DEV_EUI: buf = mibReq.Param.DevEui; break;
    case MIB_JOIN_EUI: buf = mibReq.Param.JoinEui; break;
    default: return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }

  *value = (uint64_t)buf[0] << 7*8 |
           (uint64_t)buf[1] << 6*8 |
           (uint64_t)buf[2] << 5*8 |
           (uint64_t)buf[3] << 4*8 |
           (uint64_t)buf[4] << 3*8 |
           (uint64_t)buf[5] << 2*8 |
           (uint64_t)buf[6] << 1*8 |
           (uint64_t)buf[7] << 0*8;

  return true;
}

bool STM32LoRaWAN::mibSetUint64(const char* name, Mib_t type, uint64_t value) {
  MibRequestConfirm_t mibReq;
  uint8_t buf[8] = {
    (uint8_t)(value >> 7*8),
    (uint8_t)(value >> 6*8),
    (uint8_t)(value >> 5*8),
    (uint8_t)(value >> 4*8),
    (uint8_t)(value >> 3*8),
    (uint8_t)(value >> 2*8),
    (uint8_t)(value >> 1*8),
    (uint8_t)(value >> 0*8),
  };

  switch(type) {
    case MIB_DEV_EUI: mibReq.Param.DevEui = buf; break;
    case MIB_JOIN_EUI: mibReq.Param.JoinEui = buf; break;
    default: return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }

  return mibSet(name, type, mibReq);
}

bool STM32LoRaWAN::mibSetHex(const char* name, Mib_t type, const char* value) {
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

/* These MIB types do not have easy getter/setters defined for them yet
 * (but since these were regexed together from LoRaMacInterfaces already, these
 * are left here to maybe put into use in the future.

      case MIB_CHANNELS: mibReq.Param.ChannelList = value; break;
      case MIB_RX2_CHANNEL: mibReq.Param.Rx2Channel = value; break;
      case MIB_RX2_DEFAULT_CHANNEL: mibReq.Param.Rx2DefaultChannel = value; break;
      case MIB_RXC_CHANNEL: mibReq.Param.RxCChannel = value; break;
      case MIB_RXC_DEFAULT_CHANNEL: mibReq.Param.RxCDefaultChannel = value; break;
      case MIB_CHANNELS_MASK: mibReq.Param.ChannelsMask = value; break;
      case MIB_CHANNELS_DEFAULT_MASK: mibReq.Param.ChannelsDefaultMask = value; break;
      case MIB_MULTICAST_CHANNEL: mibReq.Param.MulticastChannel = value; break;
      case MIB_ANTENNA_GAIN: mibReq.Param.AntennaGain = value; break;
      case MIB_DEFAULT_ANTENNA_GAIN: mibReq.Param.DefaultAntennaGain = value; break;
      case MIB_NVM_CTXS: mibReq.Param.Contexts = value; break;
      case MIB_NVM_CTXS: mibReq.Param.BackupContexts = value; break;
      case MIB_ABP_LORAWAN_VERSION: mibReq.Param.AbpLrWanVersion = value; break;
      case MIB_IS_CERT_FPORT_ON: mibReq.Param.IsCertPortOn = value; break;
      case MIB_BEACON_STATE: mibReq.Param.BeaconState = value; break;

      case MIB_CHANNELS: *value = mibReq.Param.ChannelList; break;
      case MIB_RX2_CHANNEL: *value = mibReq.Param.Rx2Channel; break;
      case MIB_RX2_DEFAULT_CHANNEL: *value = mibReq.Param.Rx2DefaultChannel; break;
      case MIB_RXC_CHANNEL: *value = mibReq.Param.RxCChannel; break;
      case MIB_RXC_DEFAULT_CHANNEL: *value = mibReq.Param.RxCDefaultChannel; break;
      case MIB_CHANNELS_MASK: *value = mibReq.Param.ChannelsMask; break;
      case MIB_CHANNELS_DEFAULT_MASK: *value = mibReq.Param.ChannelsDefaultMask; break;
      case MIB_MULTICAST_CHANNEL: *value = mibReq.Param.MulticastChannel; break;
      case MIB_ANTENNA_GAIN: *value = mibReq.Param.AntennaGain; break;
      case MIB_DEFAULT_ANTENNA_GAIN: *value = mibReq.Param.DefaultAntennaGain; break;
      case MIB_NVM_CTXS: *value = mibReq.Param.Contexts; break;
      case MIB_NVM_CTXS: *value = mibReq.Param.BackupContexts; break;
      case MIB_ABP_LORAWAN_VERSION: *value = mibReq.Param.AbpLrWanVersion; break;
      case MIB_IS_CERT_FPORT_ON: *value = mibReq.Param.IsCertPortOn; break;
      case MIB_BEACON_STATE: *value = mibReq.Param.BeaconState; break;
*/

bool STM32LoRaWAN::connected() {
  MibRequestConfirm_t mibReq;
  return mibGet("MIB_NETWORK_ACTIVATION", MIB_NETWORK_ACTIVATION, mibReq)
         && (mibReq.Param.NetworkActivation != ACTIVATION_TYPE_NONE);
}

const char *STM32LoRaWAN::toString(LoRaMacStatus_t status) {
  switch (status) {
    case LORAMAC_STATUS_OK:
      return "LORAMAC_STATUS_OK";
    case LORAMAC_STATUS_BUSY:
      return "LORAMAC_STATUS_BUSY";
    case LORAMAC_STATUS_SERVICE_UNKNOWN:
      return "LORAMAC_STATUS_SERVICE_UNKNOWN";
    case LORAMAC_STATUS_PARAMETER_INVALID:
      return "LORAMAC_STATUS_PARAMETER_INVALID";
    case LORAMAC_STATUS_FREQUENCY_INVALID:
      return "LORAMAC_STATUS_FREQUENCY_INVALID";
    case LORAMAC_STATUS_DATARATE_INVALID:
      return "LORAMAC_STATUS_DATARATE_INVALID";
    case LORAMAC_STATUS_FREQ_AND_DR_INVALID:
      return "LORAMAC_STATUS_FREQ_AND_DR_INVALID";
    case LORAMAC_STATUS_NO_NETWORK_JOINED:
      return "LORAMAC_STATUS_NO_NETWORK_JOINED";
    case LORAMAC_STATUS_LENGTH_ERROR:
      return "LORAMAC_STATUS_LENGTH_ERROR";
    case LORAMAC_STATUS_REGION_NOT_SUPPORTED:
      return "LORAMAC_STATUS_REGION_NOT_SUPPORTED";
    case LORAMAC_STATUS_SKIPPED_APP_DATA:
      return "LORAMAC_STATUS_SKIPPED_APP_DATA";
    case LORAMAC_STATUS_DUTYCYCLE_RESTRICTED:
      return "LORAMAC_STATUS_DUTYCYCLE_RESTRICTED";
    case LORAMAC_STATUS_NO_CHANNEL_FOUND:
      return "LORAMAC_STATUS_NO_CHANNEL_FOUND";
    case LORAMAC_STATUS_NO_FREE_CHANNEL_FOUND:
      return "LORAMAC_STATUS_NO_FREE_CHANNEL_FOUND";
    case LORAMAC_STATUS_BUSY_BEACON_RESERVED_TIME:
      return "LORAMAC_STATUS_BUSY_BEACON_RESERVED_TIME";
    case LORAMAC_STATUS_BUSY_PING_SLOT_WINDOW_TIME:
      return "LORAMAC_STATUS_BUSY_PING_SLOT_WINDOW_TIME";
    case LORAMAC_STATUS_BUSY_UPLINK_COLLISION:
      return "LORAMAC_STATUS_BUSY_UPLINK_COLLISION";
    case LORAMAC_STATUS_CRYPTO_ERROR:
      return "LORAMAC_STATUS_CRYPTO_ERROR";
    case LORAMAC_STATUS_FCNT_HANDLER_ERROR:
      return "LORAMAC_STATUS_FCNT_HANDLER_ERROR";
    case LORAMAC_STATUS_MAC_COMMAD_ERROR:
      return "LORAMAC_STATUS_MAC_COMMAD_ERROR";
    case LORAMAC_STATUS_CLASS_B_ERROR:
      return "LORAMAC_STATUS_CLASS_B_ERROR";
    case LORAMAC_STATUS_CONFIRM_QUEUE_ERROR:
      return "LORAMAC_STATUS_CONFIRM_QUEUE_ERROR";
    case LORAMAC_STATUS_MC_GROUP_UNDEFINED:
      return "LORAMAC_STATUS_MC_GROUP_UNDEFINED";
    case LORAMAC_STATUS_NVM_DATA_INCONSISTENT:
      return "LORAMAC_STATUS_NVM_DATA_INCONSISTENT";
    case LORAMAC_STATUS_ERROR:
      return "LORAMAC_STATUS_ERROR";
    default:
      return "LORAMAC_STATUS_UKNOWN";
  }
}

uint8_t STM32LoRaWAN::parseHex(char c) {
  return (c >= 'A') ? (c >= 'a') ? (c - 'a' + 10) : (c - 'A' + 10) : (c - '0');
}


bool STM32LoRaWAN::parseHex(uint8_t *dest, const char *hex, size_t dest_len) {
  uint8_t *end = dest + dest_len;;
  const char *next = hex;
  while (dest != end) {
    if (next[0] == '\0' || next[1] == '\0') {
      MW_LOG(TS_ON, VLEVEL_M, "Hex string too short, needed %u bytes (%u digits), got: %s\r\n", dest_len, dest_len * 2, hex);
      return false;
    }
    uint8_t high = parseHex(*next++);
    uint8_t low = parseHex(*next++);

    if (high > 16 || low > 16) {
      MW_LOG(TS_ON, VLEVEL_M, "Non-hex digit found in hex string %s\r\n", hex);
      return false;
    }
    *dest++ = high << 4 | low;
  }

  if (next[0] != '\0') {
    MW_LOG(TS_ON, VLEVEL_M, "Hex string too long, needed %u bytes (%u digits), got: %s\r\n", dest_len, dest_len * 2, hex);
    return false;
  }
  return true;
}


bool STM32LoRaWAN::failure(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vcore_debug(fmt, ap);
  va_end(ap);
  return false;
}
