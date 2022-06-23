#include <STM32LoRaWAN.h>
#include <BSP/mw_log_conf.h>

STM32LoRaWAN modem;

static const unsigned long TX_INTERVAL = 60000; /* ms */
unsigned long last_tx = 0;

void setup() {
	Serial.begin(115200);
	Serial.println("Start");
	modem.begin();
	modem.dataRate(0); // 0 == slowest == most range

	modem.joinOTAA(/* AppEui */ "0000000000000000", /* AppKey */ "00000000000000000000000000000000", /* DevEui */ "0000000000000000");
	//modem.joinABP(/* DevAddr */ "00000000", /* NwkSKey */ "00000000000000000000000000000000", /* AppSKey */ "00000000000000000000000000000000");

	while(!modem.connected())
		modem.poll();

	Serial.println("Joined");
}

void send_packet() {
	uint8_t payload[] = {0xde, 0xad, 0xbe, 0xef};
	modem.send(payload, sizeof(payload), /* port */ 10, /* confirmed */ false);
}

void loop() {
	modem.poll();

	if (!last_tx || millis() - last_tx > TX_INTERVAL) {
		send_packet();
		last_tx = millis();
	}
}
