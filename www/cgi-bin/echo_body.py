#!/usr/bin/env python3
import sys

# Read everything from stdin
body = sys.stdin.read()

print("Content-Type: text/plain")
print(f"Content-Length: {len(body)}")
print("X-Custom-Header: EchoTest")
print("")
print(body, end="")
