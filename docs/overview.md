# STM32LoRaWAN library {#mainpage}

This library provides support for LoRaWAN communication using the
STM32WL55 microcontroller (with embedded LoRa radio) inside the Arduino
develop environment (together with the [stm32duino
core](https://github.com/stm32duino/Arduino_Core_STM32)).

This library offers an API that is highly similar to the API offered by
the [Arduino MKRWAN
library](https://www.arduino.cc/reference/en/libraries/mkrwan/)
(including undocumented methods) so any sketches written for either
library should usually work on the other too (but note below for some
caveats).

In the Arduino IDE, the supported NUCLEO-WL55JC1 board can be configured
in the Tools menu by selecting "Nucleo-64" under "Board", and then "Nucleo WL55JC" under
"Board part number". You likely also want to enable "Core logs" under
"Debug symbols and core logs" (see more below).

The main part of this documentation can be found in the STM32LoRaWAN
class documentation.

## Supported features
 - Works with the NUCLEO-WL55JC1 board (other WL55-based boards are
   probably easy to support, but might need some changes).
 - LoRaWAN 1.0.3 with random nonces
 - OTAA and ABP joining (but framecounters are not saved for ABP)
 - Class A, with confirmed and unconfirmed uplink and downlink. Port and
   datarate can be set for uplinks.
 - Various radio parameters configurable.
 - Automatic Data Rate (ADR) and other Mac commands as specified by
   LoRaWAN.

Not supported:
 - Storing data in non-volatile storage (e.g. framecounters for ABP, or
   nonce for LoRaWAN 1.0.4 incremental OTAA nonces).
 - Class B and C (the underlying stack has support for this, but this is
   not enabled or tested).
 - Hardware AES encryption.
 - Automatic sleeping (can be implemented in the sketch).

## Library structure
The STM32LoRaWAN class is the main entrypoint for this library. Your
sketch must create one instance of this class (in the examples named
`modem`).

\note Creating multiple instances is technically possible, but you can only
call begin() in one of them.

To initialize the library, use the `STM32LoRaWAN::begin()` method, then
use the \ref joinABP or \ref joinOTAA methods to set up a session with
the network.

After that, packets can be sent using \ref sending "beginPacket() and endPacket()",
using the \ref print "Print methods" to write data into the packet.

After each packet transmitted, there is an opportunity for the network
to send a reply packet. To see if any data was received, use the
\ref reception "reception methods" and the actual data can be read using
the \ref stream "Stream methods" (e.g. `available()` and `read()`).

## Core logging
This library uses the STM32Duino `core_debug()` mechanism for logging to
the default serial port. This mechanism is disabled by default, but can
be enabled in the board options in the Arduino IDE (Tools -> Debug
Symbols and core logs").

When enabled, the library prints informational messages and also error
messages.

## Error handling
Most methods that change or do something return a `bool` value that
indicates whether the operation was successful. Typically, if the `false`
is returned, the operation was not executed at all, though it might have
been done partially.

Methods do not return more details about what went wrong exactly (i.e.
no error codes), like the MKRWAN library. However, whenever a method
returns false, there will also be an error message printed to the core
log (e.g. serial when enabled, see above), so it is recommended to run
with core logs enabled.

## Examples
How to use this library is probably easiest learned from the examples.
The library currently supplies these two examples:

 - Basic.ino has a very basic sketch that shows setting up the library,
   joining the network (using OTAA or ABP) and transmitting packets
   periodically. It also shows how to read downlink packets. To use this
   sketch, configure device credentials in the `joinOTAA()` or
   `joinABP()` calls.

 - LoraSendAndReceive.ino is a similar sketch, but is a bit more
   interactive, so might be a good starting point. This sketch is taken
   (nearly) verbatim from the MKRWAN. Credentials are configured in the
   accompanying `arduino_secrets.h` file.

 - SimpleAsync is similar in structure as Basic.ino, but uses the
   asynchronous API instead of the regular blocking API. Because of
   this, it can do other work in parallel with the LoRaWAN operations,
   in this case the example blinks a LED at the same time.

 - ScheduledAsync is a more complete asynchronous example where the
   control flow of the code no longer follows the LoRaWAN flow, but the
   main loop runs a (very, very simple) cooperative scheduler that can
   alternate between running different tasks whenever they need
   attention, and the LoRaWAN flow is handled by a simple state machine
   run inside that scheduler.

All examples default to the EU868 frequency plan, so if are in
a different region, be sure to change the argument to
STM32LoRaWAN::begin() accordingly.

## Duty cycle limits

In most regions, local regulations limit the amount of airtime that
a device can use. This is typically expressed in a percentage, for
example 1% duty cycle means that a device can be transmitting (in
a given frequncye band) at most 1% of the time.

The default The Things Network frequency plan in EU868 for example has
channels enabled in two different 1% bands, making the total effective
duty cycle of the available airtime 2%.

This library accounts for these duty cycle limits in 30 minute
intervals, meaning that you can never transmit more than (in this
example) 2% × 30min = 36s in any given period of 30 minutes (sliding
window). On startup, you get the full 30-minute budget, so you can do
a big burst of packets, but if you do, the available airtime is reduced
as soon as you initial budget is used up.

Also note that The Things Network has an additional fair use policy that
is more strict than the official regulations, but this is not enforced
by this library.

Rather than relying on the library's duty cycle limits, it is usually
a good idea to keep these limits in mind when deciding on the
transmission frequency (and possibly adapt this frequency dynamically
when the datarate is changed by ADR or manually, which changes the
airtime of a single packet) to ensure you never cross these limits
(especially since the library only enforces the official limits, not the
TTN policy.

## Differences with MKRWAN
Where possible, the API offered by this library is identical to the
MKRWAN library. In some cases, this library offers additional methods
not present in MKRWAN.

One big difference is that all normal methods in this library are
blocking (in MKRWAN, joining and confirmed uplinks are blocking, but
unconfirmed uplinks return immediately). For more advanced cases,
non-blocking/asynchronous methods are available as well. See below for
details.

Additionally, some minor improvements were made to the API (more
appropriate types, for example) that can hopefully be applied to the
MKRWAN library as well.

All differences with the MKRWAN API have been clearly marked in the
reference documentation, and a list can be found on the \ref extensions
page.

### Blocking and asynchronous behavior
There is big hardware difference between the MKRWAN and the STM32WL:
with MKRWAN, the LoRaWAN stack runs in a completely different module
that can independently run background tasks such as handling reception,
only needing to communicate the results to the main microcontroller
using a UART.

For this library, the LoRaWAN stack runs in the main microcontroller, so
needs cooperation of the sketch to run these background tasks. The RTC
(and its alarm interrupt handler) is used to handle timing critical
things, but some tasks need to happen in the mainloop.

In practice, when using the normal (non-async) methods, this is all
handled automatically - the normal methods simply block until the join
or TX+RX is fully done, doing any work directly. When these methods
return, the library is idle again and there is no need for mainloop work
anymore.

However, when the sketch wants to do other work while waiting for
airtime or for the RX windows, it can use the async functions instead.
These functions start some operation and then immediately return while
the library is still busy (has future work to do). While the library is
busy (some pending task, mostly waiting for RX windows or waiting for TX
airtime), the sketch must regularly allow the library to perform some
work. This is done by calling the `maintain()` method, which checks to
see if there is some work to be done and does it.

\note All timing-critical work (mostly starting RX at the right moment)
is done inside the interrupt handler, it should not be problematic when
`maintain()` is called a couple (probably even up to a hundred)
milliseconds after new work has become available, but in general it is
good to just call it often, especially during long processing (if there
is no work, it will return quickly).

For even more advanced usecases, sketches can use
STM32LoRaWAN::setMaintainNeededCallback() to register a callback that is
called whenever there is some background work to be done. This can be
used to remove the need to call `maintain()` all the time (just make
sure that `maintain()` is called from the main loop after the callback
was called) and can even be used to implement sleeping between bits of
work (see below).

## Low power applications
This library does not handle low-power and sleeping automatically, but
has been designed to allow the sketch to implement this.

In particular:
 - Whenever the radio is not actively used, it is put into sleep mode,
   minimizing power usage from the radio module.

 - When the library is idle (no operation in progress, `busy()`
   returning false) it requires no attention and should not generate any
   interrupts (until the sketch starts a new operation). The MCU can be
   put into a sleep mode at the sketch's discretion.

   In this scenario, the RTC must ideally be kept running. It will not
   generate any interrupts, but will be used for timing duty cycle
   limits.

 - When the library *is* running an operation, the microcontroller can
   also be put in sleep mode, provided that the RTC and radio modules
   are kept enabled and can wake up the MCU with their interrupts.

   To prevent race conditions (where a work-generating interrupt is
   triggered between the last call to `maintain()` and actually
   sleeping), some care will be needed. For example using
   STM32LoRaWAN::setMaintainNeededCallback() to set a flag when there is
   pending work, and when it is time for sleeping disable interrupts,
   check the flag and only if it is unset, actually sleep. This way,
   when more work is triggered (e.g. by the RTC IRQ) just before
   sleeping, that IRQ will be postponed and cause the MCU to wake up
   immediately after sleeping and the work can be handled.

Note that sleeping has not actually been tested during development of
this library, so if you run into problems (or successfully implement any
of this), please open a Github issue.

## Used resources
This library makes use of:

 - The radio module, obviously. The library handles enabling the module
   and the associated SPI block, so nothing is needed in the sketch.

 - The RTC for timing. This library currently completely configures the
   RTC and defines the interrupt handler, making it impossible to use
   the RTC for anything else. In the future, this library could be
   modified to co-exist with e.g. the STM32RTC library or use
   a (low-power) timer instead of the RTC, but this is not possible
   right now.

 - A number of GPIO pins that are connected to external RF circuitry on
   the board. This just uses the Arduino `digitalWrite()` functions.
