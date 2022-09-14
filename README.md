# STM32LoRaWAN library

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

## Documentation
Overview and reference documentation is embedded in the source code.
Fully generated documentation will be published later, for now you can
generate it yourself using the [Doxygen](https://doxygen.nl/) tool.

To generate the documentation yourself, simply run the `doxygen` command
(no options needed) in the root of this repository. This will produce
HTML documentation in the `api-docs` subdirectory.

## Running checks
This repository is set up to run some checks in github workflows
automatically. You can also run them locally as follows.

For codespell spell checking, just run the command without options, it
will tell you about any spelling errors in the code.

    codespell

For astyle, you need to pass a few options. This will automatically fix
style errors, so this is probably best done after staging all changes,
so you can easily see what was changed. Add `--dry-run` to only see
which files need to be changed (does not display the changes made,
though).

    astyle --project=.astylerc --recursive '*.c' '*.h' '*.ino'

(you can also set `ARTISTIC_STYLE_PROJECT_OPTIONS=.astylerc` in your
environment and omit `--project` option)

## License
This library is based on LoRaMac-node developed by semtech, with
extensive modifications and additions made by STMicroelectronics.

All code included in this library is licensed under the [Revised BSD
license](https://spdx.org/licenses/BSD-3-Clause.html), the full license
text can be found in the [LICENSE]() file and in individual source files
as well.
