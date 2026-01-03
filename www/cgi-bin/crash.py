#!/usr/bin/env python3
import sys

print("Content-Type: text/plain\r\n\r\n", end="")
sys.stderr.write("Crashing now...\n")
sys.exit(1)
