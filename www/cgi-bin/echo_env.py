#!/usr/bin/env python3
import os

print("Content-Type: text/plain\r\n\r\n", end="")
for key, value in os.environ.items():
    print(f"{key}={value}")
