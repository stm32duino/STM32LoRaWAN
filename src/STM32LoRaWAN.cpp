/**
  ******************************************************************************
  * @file    STM32LoRaWAN.cpp
  * @author  Matthijs Kooijman
  * @brief   Main implementation for the STM32LoRaWAN library.
  *
  * API based on the MKRWAN / MKRWAN_v2 library.
  *
  ******************************************************************************
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * Revised BSD License - https://spdx.org/licenses/BSD-3-Clause.html
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted provided that the following conditions
  * are met:
  *
  *     1. Redistributions of source code must retain the above copyright notice,
  *        this list of conditions and the following disclaimer.
  *     2. Redistributions in binary form must reproduce the above copyright
  *        notice, this list of conditions and the following disclaimer in the
  *        documentation and/or other materials provided with the distribution.
  *     3. Neither the name of the copyright holder nor the names of its
  *        contributors may be used to endorse or promote products derived from this
  *        software without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  */

#include "STM32LoRaWAN.h"
#include "STM32CubeWL/LoRaWAN/Mac/LoRaMacTest.h"
#include <core_debug.h>

// The MKRWAN API has no constants for datarates, so just accepts 0 for
// DR0. The STM32CubeWL API uses DR_x constants, but they contain just
// the plain value, so no translation is needed. However, do doublecheck
// that this is really the case.
#if DR_0 != 0 || DR_1 != 1 || DR_2 != 2 || DR_3 != 3 || DR_4 != 4 || DR_5 != 5 || DR_6 != 6 || DR_8 != 8 || DR_9 != 9 || DR_10 != 10 || DR_11 != 11 || DR_12 != 12 || DR_13 != 13 || DR_14 != 14 || DR_15 != 15
#error "Unexpected datarate constants"
#endif

// Same for tx power
#if TX_POWER_0 != 0 || TX_POWER_1 != 1 || TX_POWER_2 != 2 || TX_POWER_3 != 3 || TX_POWER_4 != 4 || TX_POWER_5 != 5 || TX_POWER_6 != 6 || TX_POWER_8 != 8 || TX_POWER_9 != 9 || TX_POWER_10 != 10 || TX_POWER_11 != 11 || TX_POWER_12 != 12 || TX_POWER_13 != 13 || TX_POWER_14 != 14 || TX_POWER_15 != 15
#error "Unexpected txpower constants"
#endif

/* Get the RTC object for init */
STM32RTC& _rtc = STM32RTC::getInstance();

STM32LoRaWAN* STM32LoRaWAN::instance;

bool STM32LoRaWAN::begin(_lora_band band)
{
  if (instance != nullptr)
    return failure("Only one STM32LoRaWAN instance can be used");
  instance = this;

  /*
   * Init RTC as an object :
   * use the MIX mode = free running BCD calendar + binary mode for
   * the sub-second counter RTC_SSR on 32 bit
    */
  _rtc.setClockSource(STM32RTC::LSE_CLOCK);
  _rtc.setBinaryMode(STM32RTC::MODE_MIX);
  _rtc.begin(true, STM32RTC::HOUR_24);
  /* Attach the callback function before enabling Interrupt */
  _rtc.attachInterrupt(UTIL_TIMER_IRQ_MAP_PROCESS, STM32RTC::ALARM_B);
  _rtc.attachSecondsInterrupt(TIMER_IF_SSRUCallback);
  /* The subsecond alarm B is set during the StartTimerEvent */

  UTIL_TIMER_Init(_rtc.getHandle());

  LoRaMacStatus_t res = LoRaMacInitialization(&LoRaMacPrimitives, &LoRaMacCallbacks, (LoRaMacRegion_t)band);
  if (res != LORAMAC_STATUS_OK) {
    return failure("LoRaMacInitialization failed: %s\r\n", toString(res));
  }

  res = LoRaMacStart();
  if (res != LORAMAC_STATUS_OK)
    return failure("LoRaMacStart failed: %s\r\n", toString(res));

  /*
   * Default datarate for joining and data transmission (until ADR
   * changes it, if enabled). This defaults to DR 4 since that is the
   * fastest/highest (least spectrum usage) DR that is supported by all
   * regions. If this DR has insufficient range, the join process (and
   * ADR) will fall back to lower datarates automatically.
   */
  if (!dataRate(DR_4))
    return false;

  /*
   * Enable ADR by default, to be more friendly with the spectrum and to
   * match the default value of MKRWAN / mkrwan1300-fw */
  if (!setADR(true))
    return false;

  /*
   * Default to the builtin devEUI for this chip.
   * There is also a mechanism of passing a GetUniqueId callback to
   * LoRaMacIniitalization so it initialize the secure element with that
   * default, but just setting the devEUI explicitly here is easier.
   */
  if (!setDevEui(builtinDevEUI()))
    return false;

  return true;
}

void STM32LoRaWAN::MacProcessNotify()
{
  // Called by the stack from an ISR when there is work to do
  instance->mac_process_pending = true;
  if (instance->maintain_needed_callback)
    instance->maintain_needed_callback();
}

void STM32LoRaWAN::maintain()
{
  if (mac_process_pending) {
    mac_process_pending = false;
    LoRaMacProcess( );
  }
}

void STM32LoRaWAN::maintainUntilIdle()
{
  do {
    maintain();
  } while(busy());
}

bool STM32LoRaWAN::continuousWave(uint32_t frequency, int8_t powerdBm,
                                  uint16_t timeout) {
  MlmeReq_t mlmeReq;
  mlmeReq.Type = MLME_TXCW_1;
  mlmeReq.Req.TxCw.Frequency = frequency;
  mlmeReq.Req.TxCw.Power = powerdBm;
  mlmeReq.Req.TxCw.Timeout = timeout;
  LoRaMacStatus_t res = LoRaMacMlmeRequest(&mlmeReq);
  if (res != LORAMAC_STATUS_OK)
    return failure("Failed to enable CW mode: %s\r\n", toString(res));

  return true;
}

bool STM32LoRaWAN::joinOTAAAsync()
{
  clear_rx();
  this->fcnt_up = 0;
  this->fcnt_down = 0;

  MlmeReq_t mlmeReq;
  mlmeReq.Type = MLME_JOIN;
  // Just use the most recently configured datarate for join
  mlmeReq.Req.Join.Datarate = getDataRate();
  mlmeReq.Req.Join.NetworkActivation = ACTIVATION_TYPE_OTAA;

  // Starts the OTAA join procedure
  LoRaMacStatus_t res = LoRaMacMlmeRequest(&mlmeReq);
  if (res != LORAMAC_STATUS_OK)
    return failure("Join request failed: %s\r\n", toString(res));

  return true;
}

bool STM32LoRaWAN::joinOTAA() {
  unsigned long start = millis();

  do {
    // TODO: Should this decrease datarate after a few failed attempts?
    // MKRWAN has an older version of LoRaMAC-node that handles this
    // in a region-dependent way (usually decreasing datarate every
    // 8 attempts). Our newer version of LoRaMAC-node just uses the
    // configured datarate, except for some regions (US915/AU915/AS923)
    // where the regional parameters prescribe what datarate to use for
    // joins.
    // See https://github.com/Lora-net/LoRaMac-node/commit/bec887dd2414b091fe62277ed57d0166f77a5055
    // for the change in LoRaMAC-node.
    //
    // With a timeout of only 60s, increasing the DR only every
    // 8 attempts is probably not helpful, so maybe more often? Or maybe
    // leave it to the sketch to increase DR every batch of 60s? Note
    // that we (or the sketch) can probably freely just increase the DR
    // without worrying about region-specific limits, since if we go
    // outside regional DR limits, the stack will just refuse to set the
    // DR.
    // Or maybe use PHY_NEXT_LOWER_TX_DR
    joinOTAAAsync();

    // TODO: Should this cancel a pending join attempt if the timeout
    // runs out?
    maintainUntilIdle();
  } while (!connected() && (millis() - start < DEFAULT_JOIN_TIMEOUT));

  return connected();
}


bool STM32LoRaWAN::joinABP() {
  clear_rx();
  MibRequestConfirm_t mibReq;
  mibReq.Param.NetworkActivation = ACTIVATION_TYPE_ABP;
  if (!mibSet("MIB_NETWORK_ACTIVATION", MIB_NETWORK_ACTIVATION, mibReq))
    return false;

  return true;
}

bool STM32LoRaWAN::dataRate(uint8_t dr) {
  return mibSetInt8("dataRate", MIB_CHANNELS_DATARATE, dr);
}

int STM32LoRaWAN::getDataRate() {
  int8_t dr;
  if (!mibGetInt8("dataRate", MIB_CHANNELS_DATARATE, &dr))
    return -1;
  return dr;
}

bool STM32LoRaWAN::power(uint8_t index){
  return mibSetInt8("power", MIB_CHANNELS_TX_POWER, index);
}

bool STM32LoRaWAN::powerdB(int8_t db){
  // This uses knowledge about the radio implementation to calculate the
  // index to use. See RegionCommonComputeTxPower()
  int8_t index = -(db - 1) / 2;
  return mibSetInt8("power", MIB_CHANNELS_TX_POWER, index);
}

bool STM32LoRaWAN::dutyCycle(bool on){
  LoRaMacTestSetDutyCycleOn(on);
  return true;
}

bool STM32LoRaWAN::setPort(uint8_t port){
  this->tx_port = port;
  return true;
}

uint8_t STM32LoRaWAN::getPort(){
  return this->tx_port;
}


/* TODO: Changing band requires complete reinitialization. How to do
 * that reliably?
bool STM32LoRaWAN::configureBand(_lora_band band){
}
*/

bool STM32LoRaWAN::publicNetwork(bool publicNetwork){
  return mibSetBool("publicNetwork", MIB_PUBLIC_NETWORK, publicNetwork);
}


bool STM32LoRaWAN::setADR(bool adr){
  return mibSetBool("ADR", MIB_ADR, adr);
}

int STM32LoRaWAN::getADR(){
  bool res;
  if (!mibGetBool("ADR", MIB_ADR, &res))
    return -1;
  return res;
}

// MKRWAN_v2 version
int STM32LoRaWAN::getrxfreq(){
  return getRX2Freq();
}

int STM32LoRaWAN::getRX2DR(){
  RxChannelParams_t rx;
  if (!mibGetRxChannelParams("RX2DR", MIB_RX2_CHANNEL, &rx))
      return -1;
  return rx.Datarate;
}

bool STM32LoRaWAN::setRX2DR(uint8_t dr){
  // MIB_RX2_CHANNEL contains multiple values, so do get-modify-set
  RxChannelParams_t rx;
  if (!mibGetRxChannelParams("RX2DR", MIB_RX2_CHANNEL, &rx))
      return false;
  rx.Datarate = dr;
  return mibSetRxChannelParams("RX2DR", MIB_RX2_CHANNEL, rx);
}

uint32_t STM32LoRaWAN::getRX2Freq(){
  RxChannelParams_t rx;
  if (!mibGetRxChannelParams("RX2Freq", MIB_RX2_CHANNEL, &rx))
      return -1;
  return rx.Frequency;
}

bool STM32LoRaWAN::setRX2Freq(uint32_t freq){
  // MIB_RX2_CHANNEL contains multiple values, so do get-modify-set
  RxChannelParams_t rx;
  if (!mibGetRxChannelParams("RX2Freq", MIB_RX2_CHANNEL, &rx))
      return false;
  rx.Frequency = freq;
  return mibSetRxChannelParams("RX2Freq", MIB_RX2_CHANNEL, rx);
}

/* TODO: Implement frame counters setting
 *
 * STM32CubeWL does not seem to have an obvious interface to access
 * these. They are stored by LoRaMacCrypto.c, but the functions to set
 * and query the last values are static (the only non-static one is to
 * complete a 16-bit framecounter with the upper bits).
 *
 * Maybe we can set them through the NVM interface somehow?
bool STM32LoRaWAN::setFCU(uint16_t fcu){
}

bool STM32LoRaWAN::setFCD(uint16_t fcd){
}
*/

int32_t STM32LoRaWAN::getFCU(){
  return this->fcnt_up;
}

int32_t STM32LoRaWAN::getFCD(){
  return this->fcnt_down;
}

bool STM32LoRaWAN::enableChannel(unsigned idx) {
  return modifyChannelEnabled(idx, true);
}

bool STM32LoRaWAN::disableChannel(unsigned idx) {
  return modifyChannelEnabled(idx, false);
}

bool STM32LoRaWAN::modifyChannelEnabled(unsigned idx, bool value) {
  const char *enabledisable = value ? "enable" : "disable";
  if (idx >= REGION_NVM_MAX_NB_CHANNELS)
    return failure("Cannot %s channel %u, only %u channels are available\r\n", enabledisable, idx, REGION_NVM_MAX_NB_CHANNELS);

  ChannelParams_t *channels;
  if (!mibGetPtr("Channels", MIB_CHANNELS, (void**)&channels))
    return false;

  if (channels[idx].Frequency == 0)
    return failure("Cannot %s channel %u, channel not defined\r\n", enabledisable, idx);

  uint16_t new_mask[REGION_NVM_CHANNELS_MASK_SIZE];
  uint16_t* cur_mask;
  if (!mibGetPtr("ChannelsMask", MIB_CHANNELS_MASK, (void**)&cur_mask))
    return false;

  // Ensure the channel mask can hold all channels, so the check against
  // REGION_NVM_MAX_NB_CHANNELS above is sufficient to prevent overflows here.
  static_assert(REGION_NVM_CHANNELS_MASK_SIZE * 16 <= REGION_NVM_MAX_NB_CHANNELS, "Mask too short for all channels?");

  memcpy(new_mask, cur_mask, sizeof(new_mask));
  if (value)
    new_mask[idx / 16] |= 1 << (idx % 16);
  else
    new_mask[idx / 16] &= ~(1 << (idx % 16));

  return mibSetPtr("ChannelsMask", MIB_CHANNELS_MASK, (void*)new_mask);
}

bool STM32LoRaWAN::isChannelEnabled(unsigned idx){
  if (idx >= REGION_NVM_MAX_NB_CHANNELS)
    return false;

  ChannelParams_t *channels;
  if (!mibGetPtr("Channels", MIB_CHANNELS, (void**)&channels))
    return false;

  // Treat unconfigured channels as disabled (even though the enabled
  // mask defaults to being set).
  if (channels[idx].Frequency == 0)
    return 0;

  uint16_t* cur_mask;
  if (!mibGetPtr("ChannelsMask", MIB_CHANNELS_MASK, (void**)&cur_mask))
    return false;

  // Ensure the channel mask can hold all channels, so the check against
  // REGION_NVM_MAX_NB_CHANNELS above is sufficient to prevent overflows here.
  static_assert(REGION_NVM_CHANNELS_MASK_SIZE * 16 <= REGION_NVM_MAX_NB_CHANNELS, "Mask too short for all channels?");

  return cur_mask[idx / 16] & (1 << (idx % 16));
}

bool STM32LoRaWAN::send(const uint8_t *payload, size_t size, bool confirmed) {
  McpsReq_t mcpsReq;

  if (confirmed) {
    mcpsReq.Type = MCPS_CONFIRMED;
    // When ADR is used, the datarate passed here is ignored. If not,
    // just pass the current rate, which should be the most recently
    // configured one..
    mcpsReq.Req.Confirmed.Datarate = getDataRate();
    mcpsReq.Req.Confirmed.fPort = this->tx_port;
    mcpsReq.Req.Confirmed.fBufferSize = size;
    mcpsReq.Req.Confirmed.fBuffer = const_cast<uint8_t*>(payload);
    #if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000300 ))
    // Mimic the 1.0.4 behavior where the application manages retransmissions.
    // See https://github.com/Lora-net/LoRaMac-node/discussions/1096
    mcpsReq.Req.Confirmed.NbTrials = 1;
    #endif /* LORAMAC_VERSION */
  } else {
    mcpsReq.Type = MCPS_UNCONFIRMED;
    // See comment about datarate above
    mcpsReq.Req.Unconfirmed.Datarate = getDataRate();
    mcpsReq.Req.Unconfirmed.fPort = this->tx_port;
    mcpsReq.Req.Unconfirmed.fBufferSize = size;
    mcpsReq.Req.Unconfirmed.fBuffer = const_cast<uint8_t*>(payload);
  }

  LoRaMacTxInfo_t txInfo;
  if( LoRaMacQueryTxPossible( size, &txInfo ) == LORAMAC_STATUS_LENGTH_ERROR ) {
    if (size > txInfo.CurrentPossiblePayloadSize)
      return failure("Packet too long, only %u bytes of payload supported at current datarate\r\n", txInfo.CurrentPossiblePayloadSize);

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

size_t STM32LoRaWAN::mibHexSize(const char *name, Mib_t type) {
  switch(type) {
    case MIB_DEV_EUI:
    case MIB_JOIN_EUI:
      return SE_EUI_SIZE;

    case MIB_DEV_ADDR:
      return sizeof(MibRequestConfirm_t::Param.DevAddr);

    case MIB_APP_KEY:
    case MIB_NWK_KEY:
    #if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01010100 ))
    case MIB_J_S_INT_KEY:
    case MIB_J_S_ENC_KEY:
    case MIB_F_NWK_S_INT_KEY:
    case MIB_S_NWK_S_INT_KEY:
    case MIB_NWK_S_ENC_KEY:
    #else /* ( LORAMAC_VERSION == 0x01010100 ) */
    case MIB_NWK_S_KEY:
    #endif /* ( LORAMAC_VERSION == 0x01010100 ) */
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
      return SE_KEY_SIZE;

    default:
      return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }
}

bool STM32LoRaWAN::mibGetHex(const char* name, Mib_t type, String* value) {
  MibRequestConfirm_t mibReq;
  if (!mibGet(name, type, mibReq))
    return false;

  size_t size = mibHexSize(name, type);

  if (!size)
    return false;

  uint8_t dev_addr_buf[4];;
  uint8_t *buf;

  switch(type) {
    case MIB_DEV_EUI: buf = mibReq.Param.DevEui; break;
    case MIB_JOIN_EUI: buf = mibReq.Param.JoinEui; break;
    // This assumes big endian, since that's the natural way to
    // write down a a number in hex
    case MIB_DEV_ADDR:
      dev_addr_buf[0] = mibReq.Param.DevAddr >> (3*8);
      dev_addr_buf[1] = mibReq.Param.DevAddr >> (2*8);
      dev_addr_buf[2] = mibReq.Param.DevAddr >> (1*8);
      dev_addr_buf[3] = mibReq.Param.DevAddr >> (0*8);
      buf = dev_addr_buf;
      break;
    case MIB_APP_KEY: buf = mibReq.Param.AppKey; break;
    case MIB_NWK_KEY: buf = mibReq.Param.NwkKey; break;
    #if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01010100 ))
    case MIB_J_S_INT_KEY: buf = mibReq.Param.JSIntKey; break;
    case MIB_J_S_ENC_KEY: buf = mibReq.Param.JSEncKey; break;
    case MIB_F_NWK_S_INT_KEY: buf = mibReq.Param.FNwkSIntKey; break;
    case MIB_S_NWK_S_INT_KEY: buf = mibReq.Param.SNwkSIntKey; break;
    case MIB_NWK_S_ENC_KEY: buf = mibReq.Param.NwkSEncKey; break;
    #else /* ( LORAMAC_VERSION == 0x01010100 ) */
    case MIB_NWK_S_KEY: buf = mibReq.Param.NwkSKey; break;
    #endif /* ( LORAMAC_VERSION == 0x01010100 ) */
    case MIB_APP_S_KEY: buf = mibReq.Param.AppSKey; break;
    case MIB_MC_KE_KEY: buf = mibReq.Param.McKEKey; break;
    #if ( LORAMAC_MAX_MC_CTX > 0 )
    case MIB_MC_KEY_0: buf = mibReq.Param.McKey0; break;
    case MIB_MC_APP_S_KEY_0: buf = mibReq.Param.McAppSKey0; break;
    case MIB_MC_NWK_S_KEY_0: buf = mibReq.Param.McNwkSKey0; break;
    #endif /* LORAMAC_MAX_MC_CTX > 0 */
    #if ( LORAMAC_MAX_MC_CTX > 1 )
    case MIB_MC_KEY_1: buf = mibReq.Param.McKey1; break;
    case MIB_MC_APP_S_KEY_1: buf = mibReq.Param.McAppSKey1; break;
    case MIB_MC_NWK_S_KEY_1: buf = mibReq.Param.McNwkSKey1; break;
    #endif /* LORAMAC_MAX_MC_CTX > 1 */
    #if ( LORAMAC_MAX_MC_CTX > 2 )
    case MIB_MC_KEY_2: buf = mibReq.Param.McKey2; break;
    case MIB_MC_APP_S_KEY_2: buf = mibReq.Param.McAppSKey2; break;
    case MIB_MC_NWK_S_KEY_2: buf = mibReq.Param.McNwkSKey2; break;
    #endif /* LORAMAC_MAX_MC_CTX > 2 */
    #if ( LORAMAC_MAX_MC_CTX > 3 )
    case MIB_MC_KEY_3: buf = mibReq.Param.McKey3; break;
    case MIB_MC_APP_S_KEY_3: buf = mibReq.Param.McAppSKey3; break;
    case MIB_MC_NWK_S_KEY_3: buf = mibReq.Param.McNwkSKey3; break;
    #endif /* LORAMAC_MAX_MC_CTX > 3 */
    default:
      return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }

  return toHex(value, buf, size);
}

bool STM32LoRaWAN::mibSetHex(const char* name, Mib_t type, const char* value) {
  // The buffer-passing API is a bit fragile, since the size of the
  // buffer to be passed is implicit, and also not very well
  // documented. So we need to derive the size here.
  size_t size = mibHexSize(name, type);

  if (!size)
    return false;

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
    #if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01010100 ))
    case MIB_J_S_INT_KEY: mibReq.Param.JSIntKey = buf; break;
    case MIB_J_S_ENC_KEY: mibReq.Param.JSEncKey = buf; break;
    case MIB_F_NWK_S_INT_KEY: mibReq.Param.FNwkSIntKey = buf; break;
    case MIB_S_NWK_S_INT_KEY: mibReq.Param.SNwkSIntKey = buf; break;
    case MIB_NWK_S_ENC_KEY: mibReq.Param.NwkSEncKey = buf; break;
    #else /* ( LORAMAC_VERSION == 0x01010100 ) */
    case MIB_NWK_S_KEY: mibReq.Param.NwkSKey = buf; break;
    #endif /* ( LORAMAC_VERSION == 0x01010100 ) */
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

bool STM32LoRaWAN::mibGetRxChannelParams(const char* name, Mib_t type, RxChannelParams_t *value) {
  MibRequestConfirm_t mibReq;
  if (!mibGet(name, type, mibReq))
    return false;

  switch(type) {
    case MIB_RX2_CHANNEL: *value = mibReq.Param.Rx2Channel; break;
    case MIB_RX2_DEFAULT_CHANNEL: *value = mibReq.Param.Rx2DefaultChannel; break;
    case MIB_RXC_CHANNEL: *value = mibReq.Param.RxCChannel; break;
    case MIB_RXC_DEFAULT_CHANNEL: *value = mibReq.Param.RxCDefaultChannel; break;
    default: return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }
  return true;
}

bool STM32LoRaWAN::mibSetRxChannelParams(const char* name, Mib_t type, RxChannelParams_t value) {
  MibRequestConfirm_t mibReq;
  switch(type) {
    case MIB_RX2_CHANNEL: mibReq.Param.Rx2Channel = value; break;
    case MIB_RX2_DEFAULT_CHANNEL: mibReq.Param.Rx2DefaultChannel = value; break;
    case MIB_RXC_CHANNEL: mibReq.Param.RxCChannel = value; break;
    case MIB_RXC_DEFAULT_CHANNEL: mibReq.Param.RxCDefaultChannel = value; break;
    default: return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }

  return mibSet(name, type, mibReq);
}

bool STM32LoRaWAN::mibGetPtr(const char* name, Mib_t type, void **value) {
  MibRequestConfirm_t mibReq;
  if (!mibGet(name, type, mibReq))
    return false;

  switch(type) {
    case MIB_CHANNELS: *value = mibReq.Param.ChannelList; break;
    case MIB_CHANNELS_MASK: *value = mibReq.Param.ChannelsMask; break;
    case MIB_CHANNELS_DEFAULT_MASK: *value = mibReq.Param.ChannelsDefaultMask; break;
    default: return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }
  return true;
}

bool STM32LoRaWAN::mibSetPtr(const char* name, Mib_t type, void *value) {
  MibRequestConfirm_t mibReq;
  switch(type) {
    case MIB_CHANNELS: mibReq.Param.ChannelList = (ChannelParams_t*)value; break;
    case MIB_CHANNELS_MASK: mibReq.Param.ChannelsMask = (uint16_t*)value; break;
    case MIB_CHANNELS_DEFAULT_MASK: mibReq.Param.ChannelsDefaultMask = (uint16_t*)value; break;
    default: return failure("Internal error: Unknown MIB type: %s / %u\r\n", name, type);
  }

  return mibSet(name, type, mibReq);
}

/* These MIB types do not have easy getter/setters defined for them yet
 * (but since these were regexed together from LoRaMacInterfaces already, these
 * are left here to maybe put into use in the future.

      case MIB_MULTICAST_CHANNEL: mibReq.Param.MulticastChannel = value; break;
      case MIB_ANTENNA_GAIN: mibReq.Param.AntennaGain = value; break;
      case MIB_DEFAULT_ANTENNA_GAIN: mibReq.Param.DefaultAntennaGain = value; break;
      case MIB_NVM_CTXS: mibReq.Param.Contexts = value; break;
      case MIB_NVM_CTXS: mibReq.Param.BackupContexts = value; break;
      case MIB_ABP_LORAWAN_VERSION: mibReq.Param.AbpLrWanVersion = value; break;
      case MIB_IS_CERT_FPORT_ON: mibReq.Param.IsCertPortOn = value; break;
      case MIB_BEACON_STATE: mibReq.Param.BeaconState = value; break;

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

bool STM32LoRaWAN::busy() {
  return LoRaMacIsBusy();
}

String STM32LoRaWAN::deviceEUI() {
  String res;
  // Do not check for error, a message will have been generated and the
  // return String will remain invalid to signal the error
  getDevEui(&res);
  return res;
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
      return "<unknown>";
  }
}

const char *STM32LoRaWAN::toString(LoRaMacEventInfoStatus_t status) {
  switch(status) {
    case LORAMAC_EVENT_INFO_STATUS_OK:
      return "LORAMAC_EVENT_INFO_STATUS_OK";
    case LORAMAC_EVENT_INFO_STATUS_ERROR:
      return "LORAMAC_EVENT_INFO_STATUS_ERROR";
    case LORAMAC_EVENT_INFO_STATUS_TX_TIMEOUT:
      return "LORAMAC_EVENT_INFO_STATUS_TX_TIMEOUT";
    case LORAMAC_EVENT_INFO_STATUS_RX1_TIMEOUT:
      return "LORAMAC_EVENT_INFO_STATUS_RX1_TIMEOUT";
    case LORAMAC_EVENT_INFO_STATUS_RX2_TIMEOUT:
      return "LORAMAC_EVENT_INFO_STATUS_RX2_TIMEOUT";
    case LORAMAC_EVENT_INFO_STATUS_RX1_ERROR:
      return "LORAMAC_EVENT_INFO_STATUS_RX1_ERROR";
    case LORAMAC_EVENT_INFO_STATUS_RX2_ERROR:
      return "LORAMAC_EVENT_INFO_STATUS_RX2_ERROR";
    case LORAMAC_EVENT_INFO_STATUS_JOIN_FAIL:
      return "LORAMAC_EVENT_INFO_STATUS_JOIN_FAIL";
    case LORAMAC_EVENT_INFO_STATUS_DOWNLINK_REPEATED:
      return "LORAMAC_EVENT_INFO_STATUS_DOWNLINK_REPEATED";
    case LORAMAC_EVENT_INFO_STATUS_TX_DR_PAYLOAD_SIZE_ERROR:
      return "LORAMAC_EVENT_INFO_STATUS_TX_DR_PAYLOAD_SIZE_ERROR";
    #if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000300 ))
    case LORAMAC_EVENT_INFO_STATUS_DOWNLINK_TOO_MANY_FRAMES_LOSS:
      return "LORAMAC_EVENT_INFO_STATUS_DOWNLINK_TOO_MANY_FRAMES_LOSS";
    #endif /* LORAMAC_VERSION */
    case LORAMAC_EVENT_INFO_STATUS_ADDRESS_FAIL:
      return "LORAMAC_EVENT_INFO_STATUS_ADDRESS_FAIL";
    case LORAMAC_EVENT_INFO_STATUS_MIC_FAIL:
      return "LORAMAC_EVENT_INFO_STATUS_MIC_FAIL";
    case LORAMAC_EVENT_INFO_STATUS_MULTICAST_FAIL:
      return "LORAMAC_EVENT_INFO_STATUS_MULTICAST_FAIL";
    case LORAMAC_EVENT_INFO_STATUS_BEACON_LOCKED:
      return "LORAMAC_EVENT_INFO_STATUS_BEACON_LOCKED";
    case LORAMAC_EVENT_INFO_STATUS_BEACON_LOST:
      return "LORAMAC_EVENT_INFO_STATUS_BEACON_LOST";
    case LORAMAC_EVENT_INFO_STATUS_BEACON_NOT_FOUND:
      return "LORAMAC_EVENT_INFO_STATUS_BEACON_NOT_FOUND";
    default:
      return "<unknown>";
  }
}

const char *STM32LoRaWAN::toString(Mlme_t mlme) {
  switch(mlme) {
    case MLME_UNKNOWN:
      return "MLME_UNKNOWN";
    case MLME_JOIN:
      return "MLME_JOIN";
    case MLME_REJOIN_0:
      return "MLME_REJOIN_0";
    case MLME_REJOIN_1:
      return "MLME_REJOIN_1";
    case MLME_REJOIN_2:
      return "MLME_REJOIN_2";
    case MLME_LINK_CHECK:
      return "MLME_LINK_CHECK";
    case MLME_TXCW:
      return "MLME_TXCW";
    #if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000300 ))
    case MLME_TXCW_1:
      return "MLME_TXCW_1";
    #endif /* LORAMAC_VERSION */
    case MLME_DERIVE_MC_KE_KEY:
      return "MLME_DERIVE_MC_KE_KEY";
    case MLME_DERIVE_MC_KEY_PAIR:
      return "MLME_DERIVE_MC_KEY_PAIR";
    case MLME_DEVICE_TIME:
      return "MLME_DEVICE_TIME";
    case MLME_BEACON:
      return "MLME_BEACON";
    case MLME_BEACON_ACQUISITION:
      return "MLME_BEACON_ACQUISITION";
    case MLME_PING_SLOT_INFO:
      return "MLME_PING_SLOT_INFO";
    case MLME_BEACON_TIMING:
      return "MLME_BEACON_TIMING";
    case MLME_BEACON_LOST:
      return "MLME_BEACON_LOST";
    case MLME_REVERT_JOIN:
      return "MLME_REVERT_JOIN";
    default:
      return "<unknown>";
  }
}

const char *STM32LoRaWAN::toString(Mcps_t mcps) {
  switch(mcps) {
    case MCPS_UNCONFIRMED:
      return "MCPS_UNCONFIRMED";
    case MCPS_CONFIRMED:
      return "MCPS_CONFIRMED";
    case MCPS_MULTICAST:
      return "MCPS_MULTICAST";
    case MCPS_PROPRIETARY:
      return "MCPS_PROPRIETARY";
    default:
      return "<unknown>";
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

char STM32LoRaWAN::toHex(uint8_t b) {
  return (b < 0xa) ? '0' + b : 'A' + (b - 0xa);
}

bool STM32LoRaWAN::toHex(String *dest, const uint8_t *src, size_t src_len) {
  if (!dest->reserve(src_len * 2))
    return failure("Failed to allocate string for hex output");

  *dest = "";
  while (src_len--) {
    dest->concat(toHex(*src >> 4));
    dest->concat(toHex(*src & 0x0f));
    ++src;
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

void STM32LoRaWAN::beginPacket() {
  tx_ptr = &tx_buf[0];
}

int STM32LoRaWAN::endPacketAsync(bool confirmed) {
  size_t len = tx_ptr - tx_buf;
  // MKRWAN has more error codes, but those are fairly
  // arbitrary and undocumented, so just return -1 for any error.
  if (!send(tx_buf, len, confirmed))
    return -1;
  return len;
}

int STM32LoRaWAN::endPacket(bool confirmed) {
  size_t res = endPacketAsync(confirmed);

  maintainUntilIdle();

  if (confirmed && !lastAck())
    return -1;

  return res;
}

size_t STM32LoRaWAN::write(uint8_t c) {
  if (tx_ptr == &tx_buf[sizeof(tx_buf)])
    return 0;
  *tx_ptr++ = c;
  return 1;
}

size_t STM32LoRaWAN::write(const uint8_t *buffer, size_t size) {
  size_t room = &tx_buf[sizeof(tx_buf)] - tx_ptr;
  if (size > room)
    size = room;
  memcpy(tx_ptr, buffer, size);
  tx_ptr += size;
  return size;
}

int STM32LoRaWAN::availableForWrite() {
  LoRaMacTxInfo_t txInfo;
  size_t avail = 0;

  if( LoRaMacQueryTxPossible( 0, &txInfo ) == LORAMAC_STATUS_OK ) {
    size_t written = tx_ptr - tx_buf;
    size_t max_payload = txInfo.MaxPossibleApplicationDataSize;
    if (max_payload > sizeof(tx_buf))
      max_payload = sizeof(tx_buf);

    if (max_payload > written)
      avail = max_payload - written;
  }
  return avail;
}

int STM32LoRaWAN::read(uint8_t *buf, size_t size) {
  size_t avail = available();
  if (size > avail)
    size = avail;

  memcpy(buf, rx_ptr, size);
  rx_ptr += size;

  return size;
}

int STM32LoRaWAN::available() {
  return rx_buf + sizeof(rx_buf) - rx_ptr;
}

int STM32LoRaWAN::read() {
  if (rx_ptr >= rx_buf + sizeof(rx_buf))
    return -1;
  return *rx_ptr++;
}

int STM32LoRaWAN::peek() {
  if (rx_ptr >= rx_buf + sizeof(rx_buf))
    return -1;
  return *rx_ptr;
}

int STM32LoRaWAN::parsePacket() {
  // This is what MKRWAN and MKRWAN_v2 also do
  return available();
}

void STM32LoRaWAN::add_rx(const uint8_t *buf, size_t len) {
  size_t room = rx_ptr - rx_buf;
  if (len > room) {
    failure("RX buffer overflow (%u > %u)", len, room);
    len = room;
  }

  size_t avail = available();
  if (available()) {
    // There is still data in the buffer, so we have to move that out of
    // the way (but keep it available at what will be the start of the
    // buffer).
    // Note that this could have been more efficient with a circular
    // buffer, but since this really a corner case that should not occur
    // normally, this extra copy is preferred over the extra complexity
    // of a circular buffer.
    memmove(rx_ptr - len, rx_ptr, avail);
  }
  memcpy(rx_buf + sizeof(rx_buf) - len, buf, len);
  // By modifying rather than setting rx_ptr, this is also correct when
  // we moved old data
  rx_ptr -= len;;
}

uint64_t STM32LoRaWAN::builtinDevEUI() {
  return (uint64_t)LL_FLASH_GetSTCompanyID() << (5*8) | (uint64_t)LL_FLASH_GetDeviceID() << (4*8) | (uint64_t)LL_FLASH_GetUDN();
}

void STM32LoRaWAN::MacMcpsConfirm(McpsConfirm_t* c) {
  // Called after an Mcps request (data TX) when the stack becomes idle again (so after RX windows)
  core_debug(
    "McpsConfirm: req=%s, status=%s, datarate=%u, power=%d, ack=%u, %s=%u, airtime=%u, upcnt=%u, channel=%u\r\n",
     toString(c->McpsRequest), toString(c->Status), c->Datarate, c->TxPower,
     c->AckReceived,
    #if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000300 ))
      "retries", c->NbRetries,
    #elif (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01000400 ))
      "trans", c->NbTrans,
    #endif /* LORAMAC_VERSION */
    (unsigned)c->TxTimeOnAir, (unsigned)c->UpLinkCounter, (unsigned)c->Channel);
  instance->last_tx_acked = c->AckReceived;
  instance->fcnt_up = c->UpLinkCounter;
}

void STM32LoRaWAN::MacMcpsIndication(McpsIndication_t* i, LoRaMacRxStatus_t* status) {
  // Called on Mcps event (data received or rx aborted), after McpsConfirm
  core_debug(
    "McpsIndication: ind=%s, status=%s, multicast=%u, port=%u, datarate=%u, pending=%u, size=%u, rxdata=%u, ack=%u, dncnt=%u, devaddr=%08x, rssi=%d, snr=%d, slot=%u\r\n",
    toString(i->McpsIndication), toString(i->Status), i->Multicast, i->Port,
    i->RxDatarate, i->IsUplinkTxPending, i->BufferSize, i->RxData,
    i->AckReceived, i->DownLinkCounter, i->DevAddress,
    status->Rssi, status->Snr, status->RxSlot);

  instance->fcnt_down = i->DownLinkCounter;

  if ((i->McpsIndication == MCPS_CONFIRMED || i->McpsIndication == MCPS_UNCONFIRMED) && i->Status == LORAMAC_EVENT_INFO_STATUS_OK) {
    instance->add_rx(i->Buffer, i->BufferSize);
    instance->rx_port = i->Port;
  }
}

void STM32LoRaWAN::MacMlmeConfirm(MlmeConfirm_t* c) {
  // Called when a Mlme request is completed (e.g. join complete or
  // failed, link check answer received, etc.)
  core_debug(
    "MlmeConfirm: req=%s, status=%s, airtime=%u, margin=%u, gateways=%u\r\n",
     toString(c->MlmeRequest), toString(c->Status), c->TxTimeOnAir, c->DemodMargin, c->NbGateways);
}

void STM32LoRaWAN::MacMlmeIndication(MlmeIndication_t* i, LoRaMacRxStatus_t* status) {
  // Called on join accept (and some class B events), after MlmeConfirm
  core_debug(
    "MlmeIndication: ind=%s, status=%s, datarate=%u, dncnt=%u, rssi=%d, snr=%d, slot=%u\r\n",
    toString(i->MlmeIndication), toString(i->Status),
    i->RxDatarate, i->DownLinkCounter,
    status->Rssi, status->Snr, status->RxSlot);
}
