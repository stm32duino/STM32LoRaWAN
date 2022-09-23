#!/usr/bin/env python3

import argparse
from pathlib import Path

pragma_begin = """
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
"""

pragma_end = """
#pragma GCC diagnostic pop
"""

script_path = Path(__file__).parent.resolve()
process_path = script_path.parent / "src" / "STM32CubeWL"
process_glob = "**/*.c"


# Parser
parser = argparse.ArgumentParser(
    description="Add pragma directive to ignore unused parameter warning to STM32CubeWL files."
)
args = parser.parse_args()

for filename in process_path.glob(process_glob):
    # Check if -Wunused-parameter already available
    with open(filename, "r+") as fp:
        contents = fp.read()
        if "-Wunused-parameter" not in contents:
            # Add pragma
            fp.seek(0)
            fp.truncate()
            fp.write(f"{pragma_begin}{contents}{pragma_end}")
        else:
            print(f"Found in {filename}")
