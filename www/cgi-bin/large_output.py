#!/usr/bin/env python3
import sys

print("Content-Type: text/plain")
print("")

# Generate 1MB of data
chunk = "A" * 1024
for _ in range(1024):
    print(chunk, end="")
