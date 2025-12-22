#!/bin/bash

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}=== 上传功能测试 ===${NC}"

# 创建测试文件
TEST_FILE="/tmp/test_upload.txt"
echo "这是一个测试文件内容" > "$TEST_FILE"
echo -e "${GREEN}✓ 创建测试文件: $TEST_FILE${NC}"

# 启动服务器（后台运行）
echo -e "${YELLOW}启动服务器...${NC}"
./webserv &
SERVER_PID=$!
sleep 2

# 检查服务器是否运行
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}✗ 服务器启动失败${NC}"
    exit 1
fi
echo -e "${GREEN}✓ 服务器已启动 (PID: $SERVER_PID)${NC}"

# 检查上传目录是否创建
if [ -d "/tmp/uploads" ]; then
    echo -e "${GREEN}✓ 上传目录已创建: /tmp/uploads${NC}"
    ls -la /tmp/uploads
else
    echo -e "${RED}✗ 上传目录未创建${NC}"
fi

# 测试上传
echo -e "${YELLOW}\n测试文件上传...${NC}"
UPLOAD_RESPONSE=$(curl -s -X POST \
    -H "Content-Type: multipart/form-data" \
    -F "file=@$TEST_FILE" \
    http://127.0.0.1:8080/upload)

echo "服务器响应:"
echo "$UPLOAD_RESPONSE"

# 检查是否上传成功（状态码 201）
STATUS=$(curl -s -o /dev/null -w "%{http_code}" -X POST \
    -H "Content-Type: multipart/form-data" \
    -F "file=@$TEST_FILE" \
    http://127.0.0.1:8080/upload)

if [ "$STATUS" = "201" ]; then
    echo -e "${GREEN}✓ 文件上传成功 (HTTP $STATUS)${NC}"
    
    # 列出上传目录中的文件
    echo -e "${YELLOW}上传目录中的文件:${NC}"
    ls -la /tmp/uploads/
else
    echo -e "${RED}✗ 文件上传失败 (HTTP $STATUS)${NC}"
fi

# 清理
echo -e "${YELLOW}\n清理...${NC}"
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null
echo -e "${GREEN}✓ 服务器已停止${NC}"

rm -f "$TEST_FILE"
echo -e "${GREEN}✓ 测试完成${NC}"
