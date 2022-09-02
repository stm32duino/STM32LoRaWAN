# STM32LoRaWAN library {#mainpage}

This library provides support for LoRaWAN communication using the
STM32WL55 microcontroller (with embedded LoRa radio) inside the Arduino
develop environment (together with the [stm32duino
core](https://github.com/stm32duino/Arduino_Core_STM32).

This library offers an API that is highly similar to the API offered by
the [Arduino MKRWAN
library](https://www.arduino.cc/reference/en/libraries/mkrwan/)
(including undocumented methods) so any sketches written for either
library should usually work on the other too (but note below for some
caveats).

## Supported features

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

## Errors and logging
bool return values
debug logging option in IDE

## Examples

## Differences with MKRWAN


