/**
 * This is a very basic example that demonstrates how to configure the
 * library, join the network, send regular packets and print any
 * downlink packets received.
 *
 * Revised BSD License - https://spdx.org/licenses/BSD-3-Clause.html
 */
#include <STM32LoRaWAN.h>
#include <BSP/mw_log_conf.h>

STM32LoRaWAN modem;

static const unsigned long TX_INTERVAL = 60000; /* ms */
unsigned long last_tx = 0;

void setup()
{
  Serial.begin(115200);
  Serial.println("Start");
  modem.begin(EU868);
  modem.dataRate(0); // 0 == slowest == most range

  // Configure join method by (un)commenting the right method
  // call, and fill in credentials in that method call.
  modem.joinOTAA(/* AppEui */ "0000000000000000", /* AppKey */ "00000000000000000000000000000000", /* DevEui */ "0000000000000000");
  //modem.joinABP(/* DevAddr */ "00000000", /* NwkSKey */ "00000000000000000000000000000000", /* AppSKey */ "00000000000000000000000000000000");

  if (modem.connected()) {
    Serial.println("Joined");
  } else {
    Serial.println("Join failed");
    while (true) /* infinite loop */;
  }
}

void send_packet()
{
  uint8_t payload[] = {0xde, 0xad, 0xbe, 0xef};
  modem.setPort(10);
  modem.beginPacket();
  modem.write(payload, sizeof(payload));
  modem.endPacket();
  Serial.println("Sent packet");

  if (modem.available()) {
    Serial.print("Received packet on port ");
    Serial.print(modem.getDownlinkPort());
    Serial.print(": ");
    while (modem.available()) {
      uint8_t b = modem.read();
      Serial.print(b >> 4, HEX);
      Serial.print(b & 0xF, HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
}

void loop()
{
  if (!last_tx || millis() - last_tx > TX_INTERVAL) {
    send_packet();
    last_tx = millis();
  }
}
