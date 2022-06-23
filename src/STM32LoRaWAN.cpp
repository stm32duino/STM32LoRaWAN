#include "STM32LoRaWAN.h"
#include <core_debug.h>

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
  core_debug(fmt, ap);
  va_end(ap);
  return false;
}
