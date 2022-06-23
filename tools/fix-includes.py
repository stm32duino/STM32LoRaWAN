#!/usr/bin/env python3

# This script can be used to transform include directives to use
# relative paths. This is helpful for moving code that assumes other
# files are on the compiler include path into an environment that has no
# such include path setup (such as within a single Arduio library).
#
# This script simply lists the filenames of all header files in the
# (given) source directory, then finds all include directives containing
# any of these filenames, replacing them with relative includes.
#
# Includes of specific files will be removed, this list of files is
# currently hardcoded below.

import argparse
import glob
import os
import re

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src_path = os.path.relpath(os.path.join(root, 'src'))

parser = argparse.ArgumentParser(
    formatter_class=argparse.ArgumentDefaultsHelpFormatter,
)
parser.add_argument('-v', '--verbose', action='store_true')
parser.add_argument('-s', '--src', help="Path to directory to process", default=src_path)
args = parser.parse_args()

header_glob = os.path.join(args.src, "**/*.h")
header_source_glob = os.path.join(args.src, "**/*.[ch]")
headers = {
    os.path.basename(f): f for f in glob.glob(header_glob, recursive=True)
}

# These headers can be omitted
headers['stm32_lpm.h'] = False
headers['utilities_def.h'] = False

for filename in glob.glob(header_source_glob, recursive=True):
    with open(filename, 'r+') as f:
        curdir = os.path.dirname(filename)
        contents = f.read()
        prints = []

        def fix_include(match):
            original = match.group(0)
            include = match.group(3)
            new_path = headers.get(os.path.basename(include))
            if os.path.exists(os.path.join(curdir, include)):
                if args.verbose:
                    prints.append(f"Same directory: {original}")
                return original
            elif new_path is None:
                if args.verbose:
                    prints.append(f"Not found: {original}")
                return original
            elif new_path is False:
                prints.append(f"Removed: {original}")
                return '// ' + original
            elif new_path[0] == '<':
                repl = match.group(1) + new_path
                prints.append(f"{original} -> {repl}")
                return repl
            else:
                rel_path = os.path.relpath(new_path, curdir)
                repl = match.group(1) + match.group(2) + rel_path + match.group(4)
                prints.append(f"{original} -> {repl}")
                return repl

        # This matches "" in addition to <>, since the former *also*
        # searches the include path and is often used for non-local
        # includes as well.
        new_contents = re.sub(r'^(#include )(["<])([^>"]*)([">])', fix_include, contents, flags=re.MULTILINE)

        if prints or args.verbose:
            print(filename)
            for p in prints:
                print("  {}".format(p))

        if new_contents != contents:
            f.seek(0)
            f.truncate()
            f.write(new_contents)
