#!/bin/bash
# setup_test_environment.sh

echo "========================================="
echo "Setting Up Test Environment"
echo "========================================="

# 创建必要的目录结构
mkdir -p www
mkdir -p www/cgi-bin
mkdir -p www/errors
mkdir -p /tmp/uploads
mkdir -p test_files

# 设置权限
chmod 755 www
chmod 755 www/cgi-bin
chmod 777 /tmp/uploads  # 确保可写

# 创建简单的 index.html
cat > www/index.html << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>Test Server</title>
</head>
<body>
    <h1>Welcome to Test Server</h1>
    <p>This is a test page.</p>
</body>
</html>
EOF

# 创建错误页面
cat > www/errors/404.html << 'EOF'
<!DOCTYPE html>
<html>
<head><title>404 Not Found</title></head>
<body>
    <h1>404 - Page Not Found</h1>
    <p>The requested resource was not found on this server.</p>
</body>
</html>
EOF

cat > www/errors/403.html << 'EOF'
<!DOCTYPE html>
<html>
<head><title>403 Forbidden</title></head>
<body>
    <h1>403 - Forbidden</h1>
    <p>You don't have permission to access this resource.</p>
</body>
</html>
EOF

cat > www/errors/50x.html << 'EOF'
<!DOCTYPE html>
<html>
<head><title>500 Server Error</title></head>
<body>
    <h1>500 - Internal Server Error</h1>
    <p>The server encountered an error.</p>
</body>
</html>
EOF

# 创建 CGI 测试脚本
cat > www/cgi-bin/form_handler.py << 'EOF'
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
EOF

chmod +x www/cgi-bin/form_handler.py

echo "✓ Test environment setup complete!"
echo "Directory structure:"
tree -L 2 www /tmp/uploads 2>/dev/null || find www /tmp/uploads -type d