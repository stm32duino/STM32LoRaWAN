/**
 * This is a an example of the asynchronous API. This sketch is not
 * completely asynchronous (see ScheduledAsync.ino for that) and its
 * flow still follows the LoRa flow, but this sketch shows how some
 * background work can be done while waiting for the LoRa library to
 * complete its work.
 *
 * Revised BSD License - https://spdx.org/licenses/BSD-3-Clause.html
 */
#include <STM32LoRaWAN.h>

STM32LoRaWAN modem;

static const unsigned long TX_INTERVAL = 60000; /* ms */
unsigned long last_tx = 0;

static const unsigned long BLINK_INTERVAL = 1000; /* ms */
unsigned long last_blink = 0;

void background_work()
{
  if (millis() - last_blink > BLINK_INTERVAL) {
    last_blink = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}

void wait_for_idle()
{
  while (modem.busy()) {
    // Call maintain() so the lora library can do any
    // pending background work too.
    modem.maintain();
    background_work();
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Start");
  modem.begin(EU868);

  pinMode(LED_BUILTIN, OUTPUT);

  // Configure join method by (un)commenting the right method
  // calls, and fill in credentials in those method calls.
  modem.setAppEui("0000000000000000");
  modem.setAppKey("00000000000000000000000000000000");
  modem.setDevEui("0000000000000000");
  modem.joinOTAAAsync();
  //modem.setDevAddr("00000000");
  //modem.setNwkSKey("00000000000000000000000000000000");
  //modem.setAppSKey("00000000000000000000000000000000");
  //modem.joinABPAsync();

  wait_for_idle();

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
  if (modem.endPacketAsync() == sizeof(payload)) {
    Serial.println("Queued packet");
  } else {
    Serial.println("Failed to send packet");
    return;
  }

  wait_for_idle();
  Serial.println("Sent packet");

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

void loop()
{
  background_work();
  if (!last_tx || millis() - last_tx > TX_INTERVAL) {
    send_packet();
    last_tx = millis();
  }
}
