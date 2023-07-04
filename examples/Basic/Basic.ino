/**
 * This is a very basic example that demonstrates how to configure the
 * library, join the network, send regular packets and print any
 * downlink packets received.
 * This example is using the RTC in MIX (binary and BCD) mode
 *
 * Revised BSD License - https://spdx.org/licenses/BSD-3-Clause.html
 */
#include <STM32LoRaWAN.h>

STM32LoRaWAN modem;

static const unsigned long TX_INTERVAL = 60000; /* ms */
unsigned long last_tx = 0;

/* Get the rtc object */
STM32RTC& rtc = STM32RTC::getInstance();

void setup() {
  Serial.begin(115200);
  Serial.println("Start");
  modem.begin(EU868);

  // Configure join method by (un)commenting the right method
  // call, and fill in credentials in that method call.
  bool connected = modem.joinOTAA(/* AppEui */ "0000000000000000", /* AppKey */ "00000000000000000000000000000000", /* DevEui */ "0000000000000000");
  //bool connected = modem.joinABP(/* DevAddr */ "00000000", /* NwkSKey */ "00000000000000000000000000000000", /* AppSKey */ "00000000000000000000000000000000");

  if (connected) {
    Serial.println("Joined");
  } else {
    Serial.println("Join failed");
    while (true) /* infinite loop */
      ;
  }

  /* set the calendar */
  rtc.setTime(15, 30, 58);
  rtc.setDate(04, 07, 23);
}

void send_packet() {
  char payload[27] = { 0 }; /* packet to be sent */
  /* prepare the Tx packet : get date and format string */
  sprintf(payload, "%02d/%02d/%04d - %02d:%02d:%02d",
          rtc.getMonth(), rtc.getDay(), 2000 + rtc.getYear(),
          rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());
  modem.setPort(10);
  modem.beginPacket();
  modem.write(payload, strlen(payload));
  if (modem.endPacket() == (int)strlen(payload)) {
    Serial.println("Sent packet");
  } else {
    Serial.println("Failed to send packet");
  }

  if (modem.available()) {
    Serial.print("Received packet on port ");
    Serial.print(modem.getDownlinkPort());
    Serial.print(":");
    while (modem.available()) {
      uint8_t b = modem.read();
      Serial.print(" ");
      Serial.print(b >> 4, HEX);
      Serial.print(b & 0xF, HEX);
    }
    Serial.println();
  }
}

void loop() {
  if (!last_tx || millis() - last_tx > TX_INTERVAL) {
    send_packet();
    last_tx = millis();
  }
}
