/**
 * This is a more complex asynchronous sketch that shows how the
 * asynchronous code could be used in a sketch that uses a scheduler of
 * some sort to decide what code to run when.
 *
 * This sketch defines its own very, very simple scheduler that is not
 * suggested to be used directly, but just serves to show how a sketch
 * that uses some scheduling mechanism can be used with the
 * asynchronous STM32LoRaWAN API and the MaintainNeeded callback.
 *
 * The behavior of the sketch is simple, it just blinks a LED
 * (asynchronously) and at the same time joins the network and transmit
 * a packet periodically.
 *
 * Revised BSD License - https://spdx.org/licenses/BSD-3-Clause.html
 */
#include <STM32LoRaWAN.h>

STM32LoRaWAN modem;

/*********************************************************************
 * This part of the sketch defines a very simple scheduler and defines
 * two tasks that handle the actual work.
 *********************************************************************/
struct Task {
  unsigned long time; /* When to run, 0 == never */
  void (*callback)();
};

enum Tasks {
  BLINK_TASK,
  LORA_WORK_TASK,
  LORA_MAINTAIN_TASK,
  NUM_TASKS,
};

void do_blink();
void do_lora_maintain();
void do_lora_work();
void maintain_needed_callback();

Task tasks[NUM_TASKS] = {
  [BLINK_TASK] = {0, do_blink},
  [LORA_WORK_TASK] = {0, do_lora_work},
  [LORA_MAINTAIN_TASK] = {0, do_lora_maintain},
};


void run_scheduler()
{
  // Super-simple scheduler that just checks all tasks and runs
  // any that are due.
  for (size_t i = 0; i < NUM_TASKS; ++i) {
    if (tasks[i].time != 0 && (int)(millis() - tasks[i].time) >= 0) {
      tasks[i].time = 0;
      tasks[i].callback();
    }
  }
}

/*********************************************************************
 * This part of the sketch defines the blink task, which just toggles
 * a LED whenever it is called.
 * And this is the entry points of the sketch.
 *********************************************************************/
static const unsigned long BLINK_TASK_TIME = 1000; /* ms */

void do_blink()
{
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  tasks[BLINK_TASK].time = millis() + BLINK_TASK_TIME;
}

/*********************************************************************
 * This part of the sketch defines the lora work task, which iniates new
 * work and the lora_done() function that processes the results.
 *********************************************************************/

static const unsigned long TX_INTERVAL = 60000; /* ms */
static const unsigned long RETRY_JOIN_INTERVAL = 5000; /* ms */

enum LoraState {
  IDLE,
  JOINING,
  TRANSMITTING,
};
LoraState lora_state;

void start_join()
{
  // Configure join method by (un)commenting the right method
  // call, and fill in credentials in that method call.
  modem.setAppEui("0000000000000000");
  modem.setAppKey("00000000000000000000000000000000");
  modem.setDevEui("0000000000000000");
  modem.joinOTAAAsync();
  //modem.setDevAddr("00000000");
  //modem.setNwkSKey("00000000000000000000000000000000");
  //modem.setAppSKey("00000000000000000000000000000000");
  //modem.joinABP();

  lora_state = JOINING;
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
    Serial.println("Failed to queue packet");
  }
  lora_state = TRANSMITTING;
}

void process_rx()
{
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

void do_lora_work()
{
  // Time to start new work
  if (!modem.connected()) {
    start_join();
  } else {
    send_packet();
  }
}

void lora_done()
{
  // If, after calling maintain() the library is no longer
  // busy, then the asynchronous operation has completed,
  // so check its results.
  if (lora_state == TRANSMITTING) {
    Serial.println("Sent packet");
    // Done transmitting
    process_rx();
    lora_state = IDLE;
    // Schedule transmission of next packet
    tasks[LORA_WORK_TASK].time = millis() + TX_INTERVAL;
  } else if (lora_state == JOINING) {
    if (modem.connected()) {
      Serial.println("Joined");
      send_packet();
    } else {
      Serial.println("Join failed");
      lora_state = IDLE;
      tasks[LORA_WORK_TASK].time = millis() + RETRY_JOIN_INTERVAL;
    }
  }
}

/*********************************************************************
 * This part of the sketch defines the lora maintain task, which calls
 * maintain() to let the lora library do any background work that it
 * needs to do. It is called whenever request by the callback.
 *********************************************************************/

void do_lora_maintain()
{
  modem.maintain();

  // If, after calling maintain() the library is no longer
  // busy, then the asynchronous operation has completed,
  // so check its results.
  if (!modem.busy()) {
    lora_done();
  }
}

void maintain_needed_callback()
{
  // This is called from interrupt context, so this must *not*
  // call maintain() directly and return as fast as possible.
  // So just schedule the maintain task to run ASAP.
  tasks[LORA_MAINTAIN_TASK].time = millis();
}

/*********************************************************************
 * And this is the entry points of the sketch.
 *********************************************************************/

void setup()
{
  Serial.begin(115200);
  Serial.println("Start");
  modem.begin(EU868);
  modem.setMaintainNeededCallback(maintain_needed_callback);

  pinMode(LED_BUILTIN, OUTPUT);

  do_blink();
  do_lora_work();
}

void loop()
{
  run_scheduler();

}
