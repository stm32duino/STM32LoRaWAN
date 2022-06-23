#include <STM32LoRaWAN.h>
#include <Glue/mw_log_conf.h>

STM32LoRaWAN modem;

void setup() {
	Serial.begin(115200);
	Serial.println("Start");
	modem.begin();
	modem.dataRate(0); // 0 == slowest == most range

	modem.joinOTAA(/* AppEui */ "0000000000000000", /* AppKey */ "E8555970F49B7819B58936D6A5AEFA8C", /* DevEui */ "0080E115000AA078");
	//modem.joinABP(/* DevAddr */ "260B1932", /* NwkSKey */ "69E7D9D308FA9C79BBBAF40DF7A00440", /* AppSKey */ "8BFB6CF0797B1C455A60CFC7CD722C37");

	while(!modem.connected())
		modem.poll();

	Serial.println("Joined");

	uint8_t payload[] = {0xde, 0xad, 0xbe, 0xef};
	modem.send(payload, sizeof(payload), /* port */ 10, /* confirmed */ false);
}

void loop() {
	modem.poll();
}
