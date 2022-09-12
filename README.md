# STM32LoRaWAN library

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

## Documentation
Overview and reference documentation is embedded in the source code.
Fully generated documentation will be published later, for now you can
generate it yourself using the [Doxygen](https://doxygen.nl/) tool.

To generate the documentation yourself, simply run the `doxygen` command
(no options needed) in the root of this repository. This will produce
HTML documentation in the `api-docs` subdirectory.

## License
This library is based on LoRaMac-node developed by semtech, with
extensive modifications and additions made by STMicroelectronics.

All code included in this library is licensed under the [Revised BSD
license](https://spdx.org/licenses/BSD-3-Clause.html), the full license
text can be found in the [LICENSE]() file and in individual source files
as well.
