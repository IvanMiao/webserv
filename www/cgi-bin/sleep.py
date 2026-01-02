#!/usr/bin/python3
import time
import sys

# Send headers
print("Content-Type: text/plain\r\n\r\n", end='')
# Flush headers to ensure they are sent (though CgiHandler buffers output, so it doesn't matter much)
sys.stdout.flush()

# Sleep to simulate long processing
time.sleep(2)

print("Slept for 2 seconds")
