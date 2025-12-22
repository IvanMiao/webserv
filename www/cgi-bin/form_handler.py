#!/usr/bin/env python3
import sys
import os

# Read POST data
content_length = int(os.environ.get('CONTENT_LENGTH', 0))
post_data = sys.stdin.read(content_length) if content_length > 0 else ""

# Parse form data
params = {}
if post_data:
    for pair in post_data.split('&'):
        if '=' in pair:
            key, value = pair.split('=', 1)
            params[key] = value

# Output response
print("Content-Type: text/html")
print("Status: 200 OK")
print()
print("<html><body>")
print("<h1>CGI Form Handler</h1>")
print("<p>POST Data: {}</p>".format(post_data))
print("<p>Parsed Parameters:</p><ul>")
for key, value in params.items():
    print("<li>{}: {}</li>".format(key, value))
print("</ul>")
print("<p>Environment Variables:</p><ul>")
for key in ['REQUEST_METHOD', 'CONTENT_TYPE', 'CONTENT_LENGTH', 'QUERY_STRING']:
    print("<li>{}: {}</li>".format(key, os.environ.get(key, 'N/A')))
print("</ul></body></html>")
