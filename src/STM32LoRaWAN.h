#pragma once

/**
  ******************************************************************************
  * @file    STM32LoRaWAN.h
  * @author  Matthijs Kooijman
  * @brief   Main header for the STM32LoRaWAN library.
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

#include "Arduino.h"
#include "STM32CubeWL/LoRaWAN/Mac/LoRaMac.h"
#include "BSP/mw_log_conf.h"
#include "BSP/timer_if.h"
#include "STM32RTC.h"


/**
 * Supported regions, to be passed to STM32LoRaWAN::begin() to select
 * region-specific settings and frequency plan.
 */
typedef enum {
  AS923 = LORAMAC_REGION_AS923,
  AU915 = LORAMAC_REGION_AU915,
  CN470 = LORAMAC_REGION_CN470,
  CN779 = LORAMAC_REGION_CN779,
  EU433 = LORAMAC_REGION_EU433,
  EU868 = LORAMAC_REGION_EU868,
  KR920 = LORAMAC_REGION_KR920,
  IN865 = LORAMAC_REGION_IN865,
  US915 = LORAMAC_REGION_US915,
  RU864 = LORAMAC_REGION_RU864,
  // TODO: AU915_TTN = 0x80 | AU915,
} _lora_band;

typedef enum {
  RFO = 0,
  PABOOST,
} _rf_mode;

// Reuse enum from STM32CubeWL that uses the same names
using _lora_class = DeviceClass_t;

class STM32LoRaWAN : public Stream {
  public:
    /**
     * @anchor initialization
     * @name Initialization
     * @{ */

    /**
     * Initialize the library. Must be called before any other methods.
     *
     * \param band The region/band to use. See \ref _lora_band for
     * allowed values.
     *
     * \note The selected band must be within the frequency range of the
     * board design, this cannot be automatically checked by the
     * software.
     */
    bool begin(_lora_band band);
    /// @}


    /**
     * @anchor joinOTAA
     * @name OTAA Join
     *
     * This method can be used to join the network using the OTAA
     * (over-the-air-activation) method, where the device uses the
     * credentials specified here to negotiate a session (including
     * session address and encryption keys) with the network by
     * exchanging messages.
     *
     * The appEUI and appKey are mandatory, the devEUI can be omitted,
     * in which case the devEUI baked into the chip is used.
     *
     * For both EUIs and the key, you can pass a hex-encoded (MSB-first)
     * string (`String` object or `const char*`), for the EUIs also
     * a raw integer (64-bits).
     *
     * These methods will perform multiple join attempts and block until
     * the join completes successfully, or a timeout (60 seconds) occurs.
     *
     * There are some subtle differences with MKRWAN in how the join
     * process works:
     *  - MKRWAN returns directly after the timeout, while this library
     *    finishes the running join attempt before joinOTAA returns.
     *  - With MKRWAN the module continues doing join attempts in the
     *    background, while this library stops attempting them after
     *    joinOTAA returns.
     *  - With MKRWAN the module automatically decreases/alternates the
     *    datarate in a region-specific way for all regions, this
     *    library uses the configured datarate (with `datarate()`) for
     *    most regions, but uses fixed datarates (according to LoRaWAN
     *    regional parameters) for US915/AU915/AS923.
     *
     * \MKRWANApiDifference{Timeout parameter omitted (also omitted in MKRWAN_v2)}
     * \MKRWANApiDifference{Added version that accepts uint64_t appEUI}
     * \MKRWANApiDifference{Differences in timeout and retry datarate handling}
     *
     * @{ */
    bool joinOTAA(const char *appEui, const char *appKey, const char *devEui) { return setDevEui(devEui) && joinOTAA(appEui, appKey); }
    bool joinOTAA(const char *appEui, const char *appKey) { return setAppEui(appEui) && setAppKey(appKey) && joinOTAA(); }
    bool joinOTAA(String appEui, String appKey) { return joinOTAA(appEui.c_str(), appKey.c_str()); }
    bool joinOTAA(String appEui, String appKey, String devEui) { return joinOTAA(appEui.c_str(), appKey.c_str(), devEui.c_str()); }
    /** \NotInMKRWAN */
    bool joinOTAA(uint64_t appEui, const char *appKey, uint64_t devEui) { return setDevEui(devEui) && joinOTAA(appEui, appKey); }
    /** \NotInMKRWAN */
    bool joinOTAA(uint64_t appEui, const char *appKey) { return setAppEui(appEui) && setAppKey(appKey) && joinOTAA(); }
    /** \NotInMKRWAN */
    bool joinOTAA(uint64_t appEui, String appKey, uint64_t devEui) { return joinOTAA(appEui, appKey.c_str(), devEui); }
    /** \NotInMKRWAN */
    bool joinOTAA(uint64_t appEui, String appKey) { return joinOTAA(appEui, appKey.c_str()); }

    /**
     * Perform a join with parameters previously set using after using
     * setAppEui(), setAppKey() and optionally setDevEui().
     *
     * \NotInMKRWAN
     */
    bool joinOTAA();
    /// @}

    /**
     * @anchor joinABP
     * @name ABP Join
     *
     * This method can be used to join the network using the ABP method,
     * where the session address and keys are preconfigured and no
     * exchange with the network is needed before data can be sent.
     *
     * For the address and both keys, you can pass a hex-encoded
     * (MSB-first) string (`String` object or `const char*`), for the
     * address also a raw integer (32-bits).
     *
     * \warning This library does *not* preserve frame counters in
     * non-volatile storage, so it starts at zero after every reset.
     * This only works when the server disables framecounter-based replay
     * attacks, otherwise only the first session will work and data will
     * be dropped after the first reset.
     *
     * \note An ABP join returns immediately, it does not need to wait
     * for the network, so there is no separate non-blocking/async
     * version of this method.
     *
     * @{ */
    bool joinABP(const char *devAddr, const char *nwkSKey, const char *appSKey) { return setDevAddr(devAddr) && setNwkSKey(nwkSKey) && setAppSKey(appSKey) && joinABP(); }
    bool joinABP(String devAddr, String nwkSKey, String appSKey) { return joinABP(devAddr.c_str(), nwkSKey.c_str(), appSKey.c_str()); }
    bool joinABP(uint32_t devAddr, String nwkSKey, String appSKey) { return setDevAddr(devAddr) && setNwkSKey(nwkSKey) && setAppSKey(appSKey) && joinABP(); }

    /**
     * Perform a join with parameters previously set using after using
     * setDevAddr(), setNwkSKey() and setAppSKey().
     *
     * \NotInMKRWAN
     */
    bool joinABP();
    /// @}


    /**
     * @anchor sending
     * @name Packet sending
     *
     * These methods allow sending a data packet.
     *
     * After a join was completed, sending a packet consists of calling
     * beginPacket(), writing data to using the Stream write methods,
     * and calling endPacket() to finish up and send the packet.
     *
     * The port number to use for the packet can be set using setPort()
     * (to be called before endPacket()). A confirmed packet can be sent
     * using the parameter to endPacket(), which will request the
     * network to confirm the packet was received (but if not, no
     * automatic retries are done).
     *
     * The send() method offers an alternative (and always non-blocking)
     * API, where you pass a payload already built into your own buffer.
     * @{ */
    void beginPacket();
    /**
     * Finalize and send a packet previously prepared using
     * beginPacket() and write().
     *
     * This blocks until the packet is completely sent an both RX
     * windows have been completed.
     *
     * \return The number of bytes sent when successful, or -1 when the
     * packet could not be sent. For confirmed packets, also returns -1
     * when no confirmation was received (in that case the packet might
     * still have been sent correctly).
     *
     * \MKRWANApiDifference{This library blocks for all packets, not just confirmed packets.}
     */
    int endPacket(bool confirmed = false);
    /// @}


    /**
     * @anchor print
     * @name Stream/Print writing
     *
     * These are standard methods defined by the Arduino Stream (or
     * actually its Print superclass) class to allow writing data into
     * a packet being built.
     *
     * In addition to the methods listed here, all methods offered by
     * the Arduino Print class are also available (but at the time of
     * writing, unfortunately not documented by Arduino).
     *
     * There is one addition, there is a templated write method that
     * accepts any type of object (e.g. a struct, integer, etc.) and
     * writes its memory representation into the packet. When using
     * this, note that data will be written exactly as the processor
     * stores it in memory, so typically little-endian.
     * @{ */
    virtual size_t write(uint8_t c);
    virtual size_t write(const uint8_t *buffer, size_t size);

    template <typename T> inline size_t write(T val) { return write((uint8_t *)&val, sizeof(T)); };
    using Print::write;

    /**
     * Returns how many bytes can still be written into the current packet.
     *
     * This takes the maximum payload size at the current datarate into
     * account. When this is called directly after beginPacket(), it returns
     * the maximum payload size.
     *
     * You can usually write more than this amount, but then sending the
     * packet with endPacket() will likely fail.
     */
    virtual int availableForWrite();

    /**
     * This is a no-op, to send written data, use endPacket(). \DummyImplementation
     */
    virtual void flush() { }

    /// @}


    /**
     * @anchor reception
     * @name Packet reception
     *
     * These methods are about reception of downlink packets.
     *
     * After an uplink packet was fully sent, a received downlink packet
     * can be checked by calling parsePacket() (or simply available() to
     * see if any bytes are ready to read). If so, the contents of the
     * packet can be read using the Stream read methods.
     *
     * @{ */

    /**
     * Alias of available(), returns the number of bytes available to
     * read.
     *
     * \note The MKRWAN documentation suggests this method must be
     * called before calling read, but the code gives no indication that
     * this is at all required.
     */
    int parsePacket();

    /**
     * Returns the port number of the most recently received packet.
     */
    uint8_t getDownlinkPort() { return rx_port; }
    /// @}


    /**
     * @anchor stream
     * @name Stream reading
     *
     * These are standard methods defined by the Arduino Stream class to
     * allow reading received data.
     *
     * In addition to the methods listed here, all methods offered by
     * the Arduino Stream class are also available, see
     * https://www.arduino.cc/reference/en/language/functions/communication/stream/
     *
     * After a packet is received, the available() method can be used to
     * query how much bytes are in the packet (or after some bytes were
     * read, how many are left to be read), and various read() versions
     * can be used to read the data.
     *
     * \note If data is left unread when a new packet is received,
     * the new data will be appended to the unread data and it becomes
     * impossible to query where the first packet ends and the second
     * begins. It is recommended to always fully read any received data
     * before transmitting a new packet (which is, in class A LoRaWAN,
     * the only time a new packet can be received).
     *
     * @{ */
    int read(uint8_t *buf, size_t size);
    virtual int available();
    virtual int read();
    virtual int peek();


    /**
     * @name Setters for identifiers and keys
     *
     * These methods allow setting various identifiers and keys.
     *
     * You can pass a hex-encoded (MSB-first) string (`String` object or
     * `const char*`), or a raw integer (32-bits for DevAddr and 64-bits
     * for EUIs, keys are too long to be passed as an integer).
     *
     * \NotInMKRWAN
     * @{
     */
    bool setDevEui(const char *value) { return mibSetHex("DevEui", MIB_DEV_EUI, value); }
    bool setDevEui(String value) { return setDevEui(value.c_str()); }
    bool setDevEui(uint64_t value) { return mibSetUint64("DevEui", MIB_DEV_EUI, value); }
    bool setAppEui(const char *value) { return mibSetHex("AppEui", MIB_JOIN_EUI, value); }
    bool setAppEui(String value) { return setDevEui(value.c_str()); }
    bool setAppEui(uint64_t value) { return mibSetUint64("AppEui", MIB_JOIN_EUI, value); }
    bool setDevAddr(const char *value) { return mibSetHex("DevAddr", MIB_DEV_ADDR, value); }
    bool setDevAddr(String value) { return setDevAddr(value.c_str()); }
    bool setDevAddr(uint32_t value) { return mibSetUint32("DevAddr", MIB_DEV_ADDR, value); }
    bool setAppKey(const char *value)
    {
      // In LoRaWAN 1.0, only the appKey was configured and all keys
      // were derived from that. In 1.1, this was split into an appKey
      // and nwkKey. However, when running the LoRaMac-Node in 1.0 mode,
      // it actually seems to use nwkKey, not appKey. So to support
      // sketches that only configure appKey for 1.0, this saves the
      // appKey to nwkKey as well. But to also prepare for future
      // support of 1.1 and sketches that configure both, only do this
      // if no nwkKey was explicitly configured.
      return mibSetHex("AppKey", MIB_APP_KEY, value)
             && (this->nwk_key_set || mibSetHex("NwkKey", MIB_NWK_KEY, value));

    }
    bool setAppKey(String value) { return setAppKey(value.c_str()); }
    bool setNwkKey(const char *value)
    {
      this->nwk_key_set = true;
      return mibSetHex("NwkKey", MIB_NWK_KEY, value);
    }
    bool setNwkKey(String value) { return setNwkKey(value.c_str()); }
    bool setAppSKey(const char *value) { return mibSetHex("AppSKey", MIB_APP_S_KEY, value); }
    bool setAppSKey(String value) { return setAppSKey(value.c_str()); }
    bool setNwkSKey(const char *value)
    {
#if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01010100 ))
      // When compiled for 1.1 crypto, three different keys are used.
      // When the sketch only supplies a single key, just set all
      // three keys to the same value.
      return mibSetHex("NwkSEncKey", MIB_NWK_S_ENC_KEY, value)
             && mibSetHex("FNwkSIntKey", MIB_F_NWK_S_INT_KEY, value)
             && mibSetHex("SNwkSIntKey", MIB_S_NWK_S_INT_KEY, value);
#else /* ( LORAMAC_VERSION == 0x01010100 ) */
      return mibSetHex("NwkSKey", MIB_NWK_S_KEY, value);
#endif /* ( LORAMAC_VERSION == 0x01010100 ) */
    }
    bool setNwkSKey(String value) { return setNwkSKey(value.c_str()); }

#if (defined( LORAMAC_VERSION ) && ( LORAMAC_VERSION == 0x01010100 ))
    bool setNwkSEncKey(const char *value) { return mibSetHex("NwkSEncKey", MIB_NWK_S_ENC_KEY, value); }
    bool setNwkSEncKey(String value) { return setNwkSEncKey(value.c_str()); }
    bool setFNwkSIntKey(const char *value) { return mibSetHex("FNwkSIntKey", MIB_F_NWK_S_INT_KEY, value); }
    bool setFNwkSIntKey(String value) { return setFNwkSIntKey(value.c_str()); }
    bool setSNwkSIntKey(const char *value) { return mibSetHex("SNwkSIntKey", MIB_S_NWK_S_INT_KEY, value); }
    bool setSNwkSIntKey(String value) { return setSNwkSIntKey(value.c_str()); }
#endif /* ( LORAMAC_VERSION == 0x01010100 ) */
    /// @}

    /**
     * @name Getters for identifiers
     *
     * These methods allow getting various identifiers.
     *
     * The value is written into the pointer passed, which can be
     * a String object to get a hex-encoded (MSB-first) string, or a raw
     * integer (32-bits for DevAddr and 64-bits for EUIs).
     *
     * Note that encryption keys cannot be retrieved.
     * @{
     */
    bool getDevEui(String *value) { return mibGetHex("DevEui", MIB_DEV_EUI, value); }
    bool getDevEui(uint64_t *value) { return mibGetUint64("DevEui", MIB_DEV_EUI, value); }
    bool getAppEui(String *value) { return mibGetHex("AppEui", MIB_JOIN_EUI, value); }
    bool getAppEui(uint64_t *value) { return mibGetUint64("AppEui", MIB_JOIN_EUI, value); }
    bool getDevAddr(String *value) { return mibGetHex("DevAddr", MIB_DEV_ADDR, value); }
    bool getDevAddr(uint32_t *value) { return mibGetUint32("DevAddr", MIB_DEV_ADDR, value); }
    /// @}


    /**
     * @name Querying current state
     *
     * @{
     */

    /**
     * Return the currently configured deviceEUI. On startup, this is
     * the EUI for the chip (stored in ROM). If changed (i.e. with
     * joinOTAA() or setDevEUI()), the changed value is returned
     * instead.
     */
    String deviceEUI();
    String getDevAddr();

    /**
     * Returns whether connected to the network (i.e. for OTAA
     * a successful join exchange has happened, or for ABP session
     * information has been supplied by calling joinABP).
     */
    bool connected();

    /**
     * Returns whether the modem is currently busy processing a request.
     * After a non-blocking request (e.g. unconfirmed uplink), this
     * returns true until the request is completed and then returns
     * false until some new request is made.
     *
     * \NotInMKRWAN
     */
    bool busy();

    /** Converts this object into a bool (e.g. for using in an if
     * directly), returning the same value as connected().
     */
    operator bool() { return connected(); }
    /// @}


    /** @name Radio / transmission configuration
     *
     * These methods allow configuring various radio and transmission
     * parameters.
     *
     * @{ */

    /**
     * Set the datarate to use for future packets. If ADR is enabled,
     * that might change the datarate again (but the rate passed should
     * be used for at least one uplink packet).
     */
    bool dataRate(uint8_t dr);
    /** Retrieve the current datarate (possibly modified by ADR) */
    int getDataRate();
    /**
     * Set the transmit power (MKRWAN_v2 version)
     *
     * \param index This sets the power index which is passed to the
     * underlying stack directly. Unexpectedly, a value of 0 indicates
     * the maximum power, and higher values *decrease* the value in
     * steps of 2dB (so 0 = max, 1 = -2dB, 2 = -4dB, etc.).
     *
     * The available options depend on the region (see the TX_POWER
     * constants in LoRaMacInterfaces.h), but indices 0-5 should be
     * supported by all regions (but most regions support additional
     * values).
     *
     * If an unavailable option is passed, this method will fail and
     * return false.
     */
    bool power(uint8_t index);

    /**
     * Set the transmit power (MKRWAN version)
     *
     * \see power(uint8_t) for details.
     *
     * \param mode This parameter is ignored, the mode is automatically
     * determined based on the selected power.
     * \param index See power(uint8_t) for details.
     */
    bool power(_rf_mode mode, uint8_t index) { (void)mode; return power(index); }

    /**
     * Set transmit power in dB.
     *
     * \param db The transmit power in dB. It must be 0 or negative,
     * where 0 is the maximum power and negative values indicate how
     * much dB below the maximum to operate.
     *
     * Note that only even values are supported, but an uneven value will
     * be rounded down automatically.
     *
     * The available options depend on the region (see the TX_POWER
     * constants in LoRaMacInterfaces.h), but down to -10 dB should be
     * supported by all regions (but most regions support additional
     * values, down to -28 dB for some regions).
     *
     * \NotInMKRWAN
     */
    bool powerdB(int8_t db);

    /** Return the port used for data transmission. \NotInMKRWAN */
    uint8_t getPort();
    /** Set the port to be used for data transmission. */
    bool setPort(uint8_t port);

    /**
     * Enable or disable ADR (automatic datarate).
     *
     * This is enabled by default and allows the network to increase the
     * datarate used (when sending downlink commands) when transmissions
     * from this device are received with sufficient quality that
     * a higher datarate is expected to still be received. In addition,
     * it causes the device to request periodic confirmations and lets
     * it reduce the datarate when transmissions are left unconfirmed.
     *
     * Together, these two mechanisms should ensure the highest possible
     * datarate is used, maximizing bandwidth and/or reducing spectrum
     * usage (time on air).
     */
    bool setADR(bool adr);
    /** Return whether ADR (automatic datarate) is enabled. */
    int getADR();
    /// @}


    /** @name Advanced configuration
     *
     * These methods allow configuring advanced settings, which are
     * usually not required for normal operation.
     *
     * @{ */

    /**
     * When false is passed, duty cycle limits are no longer checked.
     *
     * \warning Use with care, disabling duty cycle limits can cause
     * violations of laws and regulations around use of the RF spectrum!
     */
    bool dutyCycle(bool on);

    /**
     * Configure the syncword to use. The default is the "public"
     * (LoRaWAN standard) syncword, when set to false this uses another
     * syncword that is often recommended for "private" networks.
     */
    bool publicNetwork(bool publicNetwork);

    /** Return the datarate used for the RX2 window. */
    int getRX2DR();
    /**
     * Set the datarate used for the RX2 window. The network might
     * override this value using downlink commands (typically only in
     * OTAA mode).
     */
    bool setRX2DR(uint8_t dr);

    /** Return the frequency used for the RX2 window. */
    uint32_t getRX2Freq();
    /**
     * Set the frequency used for the RX2 window. The network might
     * override this value using downlink commands (typically only in
     * OTAA mode).
     */
    bool setRX2Freq(uint32_t freq);

    /** Alias of getRX2Freq() for MKRWAN_v2 compatibility */
    int getrxfreq();
    /// @}


    /** @name Channel manipulation
     *
     * These methods allow manipulating the list of enabled channels.
     *
     * The number of channels that are actually available and defined
     * and their frequency and other settings are region-dependent.
     * The fixed frequency regions (US915 and AU915) have 96 fixed
     * channels, while the other regions have just a couple (up to 16)
     * of channels defined.
     *
     * \note The list of channels will be reset when starting an OTAA
     * join an when the join completes. Also, the network can send
     * commands to modify the channel plan (define new channels or
     * replace them, and enable/disable them), also as part of ADR
     * messages.
     *
     * @{ */

    bool enableChannel(unsigned pos);
    bool disableChannel(unsigned pos);
    /** \NotInMKRWAN */
    bool modifyChannelEnabled(unsigned pos, bool value);
    bool isChannelEnabled(unsigned pos);
    /// @}


    /** @name Frame counters
     *
     * These methods allow access to the up and down frame counters.
     *
     * \note Currently setting the framecounters is not supported, since
     * the underlying LoRaWAN library has no obvious way to set these.
     * This might be implemented in the future with some extra work.
     *
     * @{ */

    /**
     * Return the frame counter of the most recent uplink message that
     * was completely transmitted (i.e. after the RX windows have
     * passed).
     */
    int32_t getFCU();

    /**
     * Return the frame counter of the most recent downlink message that
     * was received.
     */
    int32_t getFCD();

    /** \NotImplemented */
    [[gnu::error("Not implemented in STM32LoRaWAN")]]
    bool setFCD(uint16_t fcd);

    /** \NotImplemented */
    [[gnu::error("Not implemented in STM32LoRaWAN")]]
    bool setFCU(uint16_t fcu);

    /// @}


    /**
     * @name Advanced MIB access
     *
     * These methods allow direct access to the MIB (Mac Information
     * Base) layer of the underlying stack to set and query values.
     * These are only intended for advanced usage, when the regular API
     * does not provide sufficient access.
     *
     * @param name Parameter name, only used in error messages
     * @{
     */
    bool mibGet(const char *name, Mib_t type, MibRequestConfirm_t &mibReq);
    bool mibGetBool(const char *name, Mib_t type, bool *value);
    bool mibGetUint8(const char *name, Mib_t type, uint8_t *value);
    bool mibGetInt8(const char *name, Mib_t type, int8_t *value);
    bool mibGetUint32(const char *name, Mib_t type, uint32_t *value);
    bool mibGetUint64(const char *name, Mib_t type, uint64_t *value);
    bool mibGetHex(const char *name, Mib_t type, String *value);
    bool mibGetRxChannelParams(const char *name, Mib_t type, RxChannelParams_t *value);
    bool mibGetPtr(const char *name, Mib_t type, void **value);
    bool mibSet(const char *name, Mib_t type, MibRequestConfirm_t &mibReq);
    bool mibSetBool(const char *name, Mib_t type, bool value);
    bool mibSetUint8(const char *name, Mib_t type, uint8_t value);
    bool mibSetInt8(const char *name, Mib_t type, int8_t value);
    bool mibSetUint32(const char *name, Mib_t type, uint32_t value);
    bool mibSetUint64(const char *name, Mib_t type, uint64_t value);
    bool mibSetHex(const char *name, Mib_t type, const char *value);
    bool mibSetRxChannelParams(const char *name, Mib_t type, RxChannelParams_t value);
    bool mibSetPtr(const char *name, Mib_t type, void *value);
    size_t mibHexSize(const char *name, Mib_t type);
    /// @}


    /** @name Non-blocking (async) methods
     *
     * These are variants of other methods that are asynchronous, i.e.
     * these start an operation and then return immediately without
     * waiting (blocking) for the operation to complete.
     *
     * After calling these methods, the sketch must periodically call
     * the `maintain()` method to allow any background work to be
     * performed. This must be done at least until `busy()` returns
     * false. You can use `maintainUntilIdle()` for this if you no
     * longer have other things to do while waiting.
     */

    /**
     * Do an asynchronous OTAA join using previously configured AppEui,
     * AppKey and optionally DevEui. This can be used in place of
     * joinOTAA() when you do not want blocking behavior.
     *
     * This initiates a single join attempt (one JoinReq message). After
     * both receive windows are complete, `busy()` will become false and
     * the sketch can call `connected()` to see if the join attempt
     * was successful. If not, it is up to the sketch to decide on
     * retries.
     *
     * \NotInMKRWAN
     */
    bool joinOTAAAsync();

    /**
     * Finalize and asynchronously send a packet. This can be used in
     * place of `endPacket()` when you do not want blocking behavior.
     *
     * The return value only reflects whether the packet could
     * successfully queued, to see if a confirmed packet was actually
     * confirmed by the network, call lastAck().
     *
     * \NotInMKRWAN
     */
    int endPacketAsync(bool confirmed = false);

    /**
     * Send a packet asynchronously by passing a buffer. This can be
     * used instead of beginPacket() / write() / endPacket(), when you
     * prefer building the data to send in an external buffer and do not
     * want to skip one buffer copy.
     *
     * The return value only reflects whether the packet could
     * successfully queued, to see if a confirmed packet was actually
     * confirmed by the network, call lastAck().
     *
     * \NotInMKRWAN
     */
    bool send(const uint8_t *payload, size_t size, bool confirmed);

    /**
     * Returns true when the most recently transmitted packet has
     * received a confirmation from the network (if requested). Directly
     * after transmitting any packet, this will return false and it will
     * become true when the ack is received (which is, at the latest,
     * when busy() becomes false again).
     *
     * \NotInMKRWAN
     */
    uint8_t lastAck() { return last_tx_acked; }

    /**
     * Perform any pending work, or quickly return if there is none.
     *
     * While the stack is busy, this method should be called very
     * frequently to ensure accurate timing. It is automatically called
     * by all blocking methods in this library.
     */
    void maintain();

    /**
     * Call maintain() to process any background work for as long as the
     * stack is busy (i.e. until busy() returns false).
     *
     * \NotInMKRWAN
     */
    void maintainUntilIdle();

    /**
     * Registers a callback that is called whenever there is some work
     * to do and the maintain() function must be called soon.
     *
     * \warning This callback is called from an interrupt handler, so it
     * should not do any work and definitely *not* call `maintain()`,
     * but just set a flag, enable a task, or something similarly short,
     * and make sure to do that in a interrupt-safe manner.
     *
     * When using this callback, it is not needed to call the
     * `maintain()` method unless this callback is called, allowing
     * applications to be more efficient, or use sleeping (but care
     * should be taken to prevent race conditions).
     *
     * Only one callback can be active at the same time, so any
     * previously configured callback is replaced by the new one passed.
     *
     * \NotInMKRWAN
     */
    void setMaintainNeededCallback(std::function<void(void)> callback) { this->maintain_needed_callback = callback; }

    /// @}

    /**
     * Set the radio into continuous wave (CW) mode. In this mode radio outputs
     * a signal at the specified frequency and power for the specified duration.
     *
     * Used only for compliance testing and antenna testing.
     *
     * \param frequency Frequency in Hz.
     * \param powerdBm transmit power in dBm.
     * \param timeout Duration in seconds.
     * \returns true if CW mode was activated successfully.
     */

    bool continuousWave(uint32_t frequency, int8_t powerdBm, uint16_t timeout);

    /**
     * @name Dummy implementations
     *
     * These methods have only dummy implementations, because no
     * meaningful implementations exist and having a dummy
     * implementation is harmless (and also helps to make some examples
     * work without modification).
     *
     * @{
     */

    /**
     * Dummy for MKRWAN compatibility. On MKRWAN this returned the
     * module firmware version, but this does not apply here.
     * \DummyImplementation
     */
    String version() { return "N/A"; }

    /**
     * Dummy for MKRWAN compatibility. Exact purpose on MKRWAN is
     * unclear, see https://github.com/arduino-libraries/MKRWAN/issues/25
     * \DummyImplementation
     */
    [[gnu::deprecated("minPollInterval is a no-op on STM32LoRaWAN")]]
    void minPollInterval(unsigned long)
    { }
    /// @}

    /** @name Missing methods
     *
     * These methods are present in MKRWAN, but are not implemented in
     * this library (because they are specific to the module-based
     * serial approach used by MKRWAN, are for testing purposes only or
     * have better API not tied to the MKRWAN AT commands available.
     *
     * These methods *are* declared in this library, but marked so the
     * compiler can generate a friendly error message when they are
     * used.
     *
     * @{ */

    /** \NotImplemented{confirmed uplinked better handled through endPacket()} */
    [[gnu::error("Not implemented in STM32LoRaWAN: confirmed uplinked better handled through endPacket()")]]
    bool setCFM(bool cfm);

    /** \NotImplemented{confirmed uplinked better handled through endPacket()} */
    [[gnu::error("Not implemented in STM32LoRaWAN: confirmed uplinked better handled through endPacket()")]]
    int getCFM();

    /** \NotImplemented{confirmed uplinked better handled through endPacket()} */
    [[gnu::error("Not implemented in STM32LoRaWAN: confirmed uplinked better handled through endPacket()")]]
    int getCFS();

    /** \NotImplemented{no serial link to configure} */
    [[gnu::error("Not implemented in STM32LoRaWAN: No serial to configure")]]
    bool begin(_lora_band band, uint32_t baud, uint16_t config = SERIAL_8N2);

    /** \NotImplemented{no serial link to configure} */
    [[gnu::error("Not implemented in STM32LoRaWAN: No serial to configure")]]
    bool autoBaud(unsigned long timeout = 10000L);

    /** \NotImplemented{no serial link to configure} */
    [[gnu::error("Not implemented in STM32LoRaWAN: No serial to configure")]]
    void setBaud(unsigned long baud);

    /** \NotImplemented{test method} */
    [[gnu::error("Not implemented in STM32LoRaWAN: Test method")]]
    String getTConf();

    /** \NotImplemented{test method only} */
    [[gnu::error("Not implemented in STM32LoRaWAN: Test method")]]
    String setTConf(String params);

    /** \NotImplemented{test method only} */
    [[gnu::error("Not implemented in STM32LoRaWAN: Test method")]]
    bool enTtone();

    /** \NotImplemented{no remote module to restart} */
    [[gnu::error("Not implemented in STM32LoRaWAN: No remote module to restart")]]
    bool restart();

    /** \NotImplemented{no remote module to poll} */
    [[gnu::error("Not implemented in STM32LoRaWAN: No remote module to poll")]]
    void poll();

    /** \NotImplemented{no remote module to sleep} */
    [[gnu::error("Not implemented in STM32LoRaWAN: No remote module to sleep")]]
    bool sleep(bool on = true);

    /** \NotImplemented{no remote module to reset} */
    [[gnu::error("Not implemented in STM32LoRaWAN: No remote module to reset")]]
    bool factoryDefault();

    /** \NotImplemented{no protocol stream to configure} */
    [[gnu::error("Not implemented in STM32LoRaWAN: No protocol stream to configure")]]
    bool format(bool hexMode);

    /** \NotImplemented{no protocol stream to configure} */
    [[gnu::error("Not implemented in STM32LoRaWAN: No protocol stream to configure")]]
    void dumb();

    /** \NotImplemented{seems to be an internal method in MKRWAN} */
    [[gnu::error("Not implemented in STM32LoRaWAN: Internal method in MKRWAN")]]
    bool init();

    /** \NotImplemented{only class A supported} */
    [[gnu::error("Not implemented in STM32LoRaWAN: Only class A supported")]]
    bool configureClass(_lora_class _class);

    /** \NotImplemented{Keys cannot be retrieved} */
    [[gnu::error("Not implemented in STM32LoRaWAN: Keys cannot be retrieved")]]
    String getNwkSKey();

    /** \NotImplemented{Keys cannot be retrieved} */
    [[gnu::error("Not implemented in STM32LoRaWAN: Keys cannot be retrieved")]]
    String getAppSKey();

    /** \NotImplemented{Keys cannot be retrieved} */
    [[gnu::error("Not implemented in STM32LoRaWAN: Keys cannot be retrieved")]]
    String applicationKey();

    /** \NotImplemented{seems to be an internal method in MKRWAN (use enableChannel/disableChannel instead)} */
    [[gnu::error("Not implemented in STM32LoRaWAN: Internal method in MKRWAN (use enableChannel/disableChannel instead)")]]
    bool sendMask(String newMask);

    /** \NotImplemented{seems to be an internal method in MKRWAN (use enableChannel/disableChannel instead)} */
    [[gnu::error("Not implemented in STM32LoRaWAN: Internal method in MKRWAN (use enableChannel/disableChannel instead)")]]
    bool sendMask();

    /** \NotImplemented{seems to be an internal method in MKRWAN (use enableChannel/disableChannel instead)} */
    [[gnu::error("Not implemented in STM32LoRaWAN: Internal method in MKRWAN (use enableChannel/disableChannel instead)")]]
    String getChannelMask();

    /** \NotImplemented{seems to be an internal method in MKRWAN (use enableChannel/disableChannel instead)} */
    [[gnu::error("Not implemented in STM32LoRaWAN: Internal method in MKRWAN (use enableChannel/disableChannel instead)")]]
    int getChannelMaskSize(_lora_band band);

    /**
     * This could be supported, but needs careful consideration and
     * complete re-initialization.
     * \NotImplemented{Changing band after initialization not supported} */
    [[gnu::error("Not implemented in STM32LoRaWAN: Changing band after initialization not supported")]]
    bool configureBand(_lora_band band);
    /// @}

  protected:
    static void MacMcpsConfirm(McpsConfirm_t *McpsConfirm);
    static void MacMcpsIndication(McpsIndication_t *McpsIndication, LoRaMacRxStatus_t *RxStatus);
    static void MacMlmeConfirm(MlmeConfirm_t *MlmeConfirm);
    static void MacMlmeIndication(MlmeIndication_t *MlmeIndication, LoRaMacRxStatus_t *RxStatus);
    static void MacProcessNotify();

    static STM32LoRaWAN *instance;

    LoRaMacPrimitives_t LoRaMacPrimitives = {
      .MacMcpsConfirm = MacMcpsConfirm,
      .MacMcpsIndication = MacMcpsIndication,
      .MacMlmeConfirm = MacMlmeConfirm,
      .MacMlmeIndication = MacMlmeIndication,
    };

    LoRaMacCallback_t LoRaMacCallbacks = {
      .GetBatteryLevel = nullptr,
      .GetTemperatureLevel = nullptr,
      .GetUniqueId = nullptr, // Not needed, we just explicitly set the deveui in begin()
      .GetDevAddress = nullptr, // Not needed, user explicitly configures devaddr
      .NvmDataChange = nullptr,
      .MacProcessNotify = MacProcessNotify,
    };

    /** Generate the builtin DevEUI for this board */
    uint64_t builtinDevEUI();

    /**
     * Convert an enum value into string containing its name. The
     * string returned is a static constant, so no memory management
     * needed.
     */
    static const char *toString(LoRaMacStatus_t);
    /** \copydoc toString() */
    static const char *toString(LoRaMacEventInfoStatus_t);
    /** \copydoc toString() */
    static const char *toString(Mlme_t);
    /** \copydoc toString() */
    static const char *toString(Mcps_t);

    /** Convert a single hex char into its numerical value */
    static uint8_t parseHex(char c);
    /** Convert a hex string into a binary buffer */
    static bool parseHex(uint8_t *dest, const char *hex, size_t dest_len);
    /** Convert a nibble (0-15) to a hex char */
    static char toHex(uint8_t b);
    /** Convert a binary buffer to a hex string */
    static bool toHex(String *dest, const uint8_t *src, size_t src_len);

    /** Build a uint32_t from four bytes (big-endian, a is MSB) */
    static uint32_t makeUint32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { return (uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)c << 8 | (uint32_t)d << 0; }

    /**
     * Helper that prints an error and then always returns false, to
     * allow for combining reporting and returning in a single line.
     */
    static bool failure(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

    /** Empty the rx buffer */
    void clear_rx() { rx_ptr = rx_buf + sizeof(rx_buf); }

    /** Add data to the rx buffer */
    void add_rx(const uint8_t *buf, size_t len);

    /**
     * Datarate for joining and data transmission (until ADR changes it,
     * if enabled). This defaults to DR 4 since that is the
     * fastest/highest (least spectrum usage) DR that is supported by
     * all regions. If this DR has insufficient range, the join process
     * (and ADR) will fall back to lower datarates automatically.
     */
    uint8_t tx_dr = DR_4;

    /** Port for data transmissions. Default taken from MKRWAN_v2 / mkrwan1300-fw */
    uint8_t tx_port = 2;

    /** Port for most recently received packet */
    uint8_t rx_port = 0;

    bool nwk_key_set = false;

    // Buffer sizes match LORAMAC_PHY_MAXPAYLOAD (but that is not
    // public).
    uint8_t tx_buf[255];
    uint8_t *tx_ptr;
    uint8_t rx_buf[255];
    uint8_t *rx_ptr;

    bool last_tx_acked = false;
    uint32_t fcnt_up = 0;
    uint32_t fcnt_down = 0;

    std::function<void(void)> maintain_needed_callback;

    bool mac_process_pending = false;

    static constexpr uint32_t DEFAULT_JOIN_TIMEOUT = 60000;
};
// For MKRWAN compatibility
using LoRaModem = STM32LoRaWAN;
