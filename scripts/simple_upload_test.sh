#!/bin/bash

# 启动服务器
cd /home/jinhuang/webs2
rm -rf /tmp/uploads
./webserv &
SERVER_PID=$!
sleep 2

# 创建一个简单的测试文件
echo "Hello, this is test file content" > /tmp/test_file.txt

# 使用 curl 上传文件（multipart/form-data）
echo "上传文件..."
curl -v -X POST \
    -F "file=@/tmp/test_file.txt" \
    http://127.0.0.1:8080/upload 2>&1 | head -30

echo ""
echo "=== 检查上传目录内容 ==="
ls -la /tmp/uploads/

echo ""
echo "=== 上传文件内容 ==="
if [ -n "$(ls /tmp/uploads/ 2>/dev/null)" ]; then
    cat /tmp/uploads/*
fi

# 清理
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
echo ""
echo "测试完成"
