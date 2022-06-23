#!/bin/sh -e

if [ ! -d "$1" ] || ! grep STM32CubeWL "$1/Package_license.md" 1>/dev/null 2>/dev/null; then
    echo "No argument passed, or is not an extracted STM32CubeWL package directory"
    exit 1;
fi
CUBE=$1
ROOT=$(cd $(dirname $0)/.. && pwd)
DEST="$ROOT/src/STM32CubeWL"

rsync -av "$CUBE/Middlewares/Third_Party/LoRaWAN" "$DEST/" --exclude '*_template.[ch]' --exclude "/LoRaWAN/LmHandler/*"
rsync -av "$CUBE/Middlewares/Third_Party/SubGHz_Phy" "$DEST/" --exclude '*_template.[ch]'
rsync -av "$CUBE/Utilities/timer" --exclude '*_template.[ch]' "$DEST/Utilities"
rsync -av "$CUBE/Utilities/misc" "$DEST/Utilities" --include '/misc/stm32_systime.[ch]' --include '/misc/*.html' --include '/misc/*.txt' --exclude '/misc/*'
