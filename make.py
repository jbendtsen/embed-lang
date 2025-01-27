#!/bin/python3

import os
import sys

DEBUG = False

def generate_ids_h(in_lines, out_path):
    out = ""
    nl = 0
    for l in in_lines:
        parts = l.split("\t")
        if len(parts) < 2:
            continue

        nl += 1
        out += "#define ID_" + parts[1] + " " + str(nl) + "\n"
        size = len(parts[0])
        if size < 2 or size > 8:
            continue

        num = 0
        i = 0
        while i < size:
            num |= ord(parts[0][i]) << (i*8)
            i += 1

        out += "#define VALUE_" + parts[1] + " " + str(num) + "ULL\n"

    with open(out_path, "w") as f:
        f.write(out)

def main(args):
    all_files = os.listdir()
    source_list = [e for e in filter(lambda x: x.endswith(".c"), all_files)]

    with open("ids.gc", "r") as f:
        content = f.read().splitlines()

    generate_ids_h(content, "ids.h")

    cmd = "gcc -g "
    if DEBUG:
        cmd += "-fsanitize=address "
    cmd += " ".join(source_list)
    cmd += " -o embedc"
    os.system(cmd)

main(sys.argv)

