#!/usr/bin/env python3
import os
import sys
query_string = os.environ.get("QUERY_STRING", "No Query")
sys.stdout.write("Content-Type: text/plain\r\n\r\n")
sys.stdout.write(f"Query: {query_string}")