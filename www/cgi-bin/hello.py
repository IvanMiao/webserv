import os
import sys

print("Status: 200 OK")
print("Content-Type: text/plain")
print("")
print("Hello from Python CGI!")
print("Environment Variables:")
for key, value in os.environ.items():
    print(f"{key}={value}")
