#pragma once

#include "Arduino.h"
#include "STM32CubeWL/LoRaWAN/Mac/LoRaMac.h"
#include "STM32CubeWL/Utilities/timer/stm32_timer.h"
#include "BSP/mw_log_conf.h"


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

class STM32LoRaWAN {
  public:
    bool begin(_lora_band band);

    // TODO: See if this should be poll or maybe named otherwise
    void poll();

    bool join(bool otaa);

    bool dataRate(uint8_t dr);
    int getDataRate();

    bool joinOTAA(const char *appEui, const char *appKey, const char *devEui) { return setDevEui(devEui) && joinOTAA(appEui, appKey); }
    bool joinOTAA(const char *appEui, const char *appKey) { return setAppEui(appEui) && setAppKey(appKey) && join(true); }
    bool joinOTAA(String appEui, String appKey) { return joinOTAA(appEui.c_str(), appKey.c_str()); }
    bool joinOTAA(String appEui, String appKey, String devEui) { return joinOTAA(appEui.c_str(), appKey.c_str(), devEui.c_str()); }
    bool joinOTAA(uint64_t appEui, const char* appKey, uint64_t devEui) { return setDevEui(devEui) && joinOTAA(appEui, appKey); }
    bool joinOTAA(uint64_t appEui, const char* appKey) { return setAppEui(appEui) && setAppKey(appKey) && join(true); }
    bool joinOTAA(uint64_t appEui, String appKey, uint64_t devEui) { return joinOTAA(appEui, appKey.c_str(), devEui); }
    bool joinOTAA(uint64_t appEui, String appKey) { return joinOTAA(appEui, appKey.c_str()); }

    bool joinABP(const char * devAddr, const char * nwkSKey, const char * appSKey) { return setDevAddr(devAddr) && setNwkSKey(nwkSKey) && setAppSKey(appSKey) && join(false); }
    bool joinABP(String devAddr, String nwkSKey, String appSKey) { return joinABP(devAddr.c_str(), nwkSKey.c_str(), appSKey.c_str()); }
    bool joinABP(uint32_t devAddr, String nwkSKey, String appSKey) { return setDevAddr(devAddr) && setNwkSKey(nwkSKey) && setAppSKey(appSKey) && join(false); }

    bool send(const uint8_t *payload, size_t size, uint8_t port, bool confirmed);

    /**
     * @name Advanced MIB access
     *
     * These methods allow direct access to the MIB (Mac Information
     * Base) layer of the underlying stack to set and query values.
     * These are only intended for advanced usage, when the regular API
     * does not provide sufficient access.
     *
     * @param name Parameter name, only used in error messages
     */
    /// @{
    bool mibGet(const char* name, Mib_t type, MibRequestConfirm_t& mibReq);
    bool mibGetBool(const char* name, Mib_t type, bool *value);
    bool mibGetUint8(const char* name, Mib_t type, uint8_t *value);
    bool mibGetInt8(const char* name, Mib_t type, int8_t *value);
    bool mibGetUint32(const char* name, Mib_t type, uint32_t *value);
    bool mibGetUint64(const char* name, Mib_t type, uint64_t *value);
    bool mibSet(const char* name, Mib_t type, MibRequestConfirm_t& mibReq);
    bool mibSetBool(const char* name, Mib_t type, bool value);
    bool mibSetUint8(const char* name, Mib_t type, uint8_t value);
    bool mibSetInt8(const char* name, Mib_t type, int8_t value);
    bool mibSetUint32(const char* name, Mib_t type, uint32_t value);
    bool mibSetUint64(const char* name, Mib_t type, uint64_t value);
    bool mibSetHex(const char* name, Mib_t type, const char* value);
    /// @}

    /**
     * @name Setters for identifiers and keys
     *
     * These methods allow setting various identifiers and keys.
     *
     * You can pass a hex-encoded (MSB-first) string (`String` object or
     * `const char*`), or a raw integer (32-bits for DevAddr and 64-bits
     * for EUIs, keys are too long to be passed as an integer).
     */
    bool setDevEui(const char* value) { return mibSetHex("DevEui", MIB_DEV_EUI, value); }
    bool setDevEui(String value) { return setDevEui(value.c_str()); }
    bool setDevEui(uint64_t value) { return mibSetUint64("DevEui", MIB_DEV_EUI, value); }
    bool setAppEui(const char* value) { return mibSetHex("AppEui", MIB_JOIN_EUI, value); }
    bool setAppEui(String value) { return setDevEui(value.c_str()); }
    bool setAppEui(uint64_t value) { return mibSetUint64("AppEui", MIB_JOIN_EUI, value); }
    bool setDevAddr(const char* value) { return mibSetHex("DevAddr", MIB_DEV_ADDR, value); }
    bool setDevAddr(String value) { return setDevAddr(value.c_str()); }
    bool setDevAddr(uint32_t value) { return mibSetUint32("DevAddr", MIB_DEV_ADDR, value); }
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

    bool connected();

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
    static uint32_t makeUint32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { return (uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)c << 8 | (uint32_t)d << 0; }

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
// For MKRWAN compatibility
using LoRaModem = STM32LoRaWAN;
