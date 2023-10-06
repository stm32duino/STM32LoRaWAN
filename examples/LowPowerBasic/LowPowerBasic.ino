/*
 *  LowPowerBasic
 *
 * This is a basic example that demonstrates how to configure the
 * library, join the network, then go to Low Power deep Sleep (STOP mode)
 * and on WakeUp, send regular packets with current RTC calendar value.
 * Then It goes to LowPower again for a period (TX_INTERVAL milliseconds)
 *
 * This example is using the RTC in MIX (binary and BCD) mode
 * and the LowPower library
 *
 * Revised BSD License - https://spdx.org/licenses/BSD-3-Clause.html
 */
#include <STM32LoRaWAN.h>
#include <STM32LowPower.h>
#include <STM32RTC.h>

STM32LoRaWAN modem;
/* Get the rtc object */
STM32RTC& rtc = STM32RTC::getInstance();

/* Change this value to set alarm match offset in millisecond */
static const unsigned long TX_INTERVAL = 60000; /* ms */

// Declare it volatile since it's incremented inside an interrupt
volatile int alarmMatch_counter = 0;

/* callback function once the Alarm A matched */
void alarmMatch(void* data)
{
  UNUSED(data);
  alarmMatch_counter++;
}

void background_work()
{
  /* Toggle LED */
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  Serial.print("  Alarm Match: ");
  Serial.print(alarmMatch_counter);
  Serial.println(" times. deepSleeping now ! ");
  Serial.flush(); /* Flush before going to sleep */
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

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
    while (true); /* infinite loop */
  }

  /* set the calendar */
  rtc.setTime(15, 30, 58);
  rtc.setDate(4, 9, 23);

  // Configure low power
  LowPower.begin();
  LowPower.enableWakeupFrom(&rtc, alarmMatch);

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
Serial.println(payload);
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

void loop()
{

  background_work();

  LowPower.deepSleep(TX_INTERVAL);

  send_packet();

}
