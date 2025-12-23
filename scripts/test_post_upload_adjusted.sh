#!/bin/bash
# test_post_upload_adjusted.sh

echo "========================================="
echo "POST Upload Tests (Adjusted for default.conf)"
echo "========================================="

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

SERVER="http://localhost:8080"
UPLOAD_PATH="/tmp/uploads"  # 根据配置文件调整

# 创建测试文件
mkdir -p test_files
echo "This is a test file for upload" > test_files/test.txt
echo "Hello World from test" > test_files/hello.txt

# Test 1: 上传简单文本文件到 /upload (注意：是 /upload 不是 /uploads)
echo -e "\n${YELLOW}Test 1: Upload simple text file to /upload${NC}"
RESPONSE=$(curl -X POST \
     -F "file=@test_files/test.txt" \
     -w "\n%{http_code}" \
     -s \
     $SERVER/upload)

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
BODY=$(echo "$RESPONSE" | head -n-1)

echo "Response code: $HTTP_CODE"
echo "Response body: $BODY"

if [ "$HTTP_CODE" -eq 201 ]; then
    echo -e "${GREEN}✓ Test 1 passed${NC}"
    
    # 验证文件存在于 /tmp/uploads
    if [ -f "$UPLOAD_PATH/test.txt" ]; then
        echo -e "${GREEN}✓ File exists at $UPLOAD_PATH/test.txt${NC}"
    else
        echo -e "${RED}✗ File not found at $UPLOAD_PATH/test.txt${NC}"
        echo "Files in $UPLOAD_PATH:"
        ls -la $UPLOAD_PATH
    fi
else
    echo -e "${RED}✗ Test 1 failed (expected 201, got $HTTP_CODE)${NC}"
fi

# Test 2: 上传自定义文件名
echo -e "\n${YELLOW}Test 2: Upload with custom filename${NC}"
RESPONSE=$(curl -X POST \
     -F "file=@test_files/hello.txt;filename=custom_hello.txt" \
     -w "\n%{http_code}" \
     -s \
     $SERVER/upload)

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)

if [ "$HTTP_CODE" -eq 201 ]; then
    echo -e "${GREEN}✓ Test 2 passed${NC}"
    
    if [ -f "$UPLOAD_PATH/custom_hello.txt" ]; then
        echo -e "${GREEN}✓ Custom filename saved correctly${NC}"
    else
        echo -e "${RED}✗ Custom filename not found${NC}"
    fi
else
    echo -e "${RED}✗ Test 2 failed${NC}"
fi

# Test 3: 上传二进制文件
echo -e "\n${YELLOW}Test 3: Upload binary file (small image)${NC}"
# 创建一个 1x1 PNG 图片
printf '\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52\x00\x00\x00\x01\x00\x00\x00\x01\x08\x06\x00\x00\x00\x1f\x15\xc4\x89\x00\x00\x00\x0a\x49\x44\x41\x54\x78\x9c\x63\x00\x01\x00\x00\x05\x00\x01\x0d\x0a\x2d\xb4\x00\x00\x00\x00\x49\x45\x4e\x44\xae\x42\x60\x82' > test_files/test.png

HTTP_CODE=$(curl -X POST \
     -F "file=@test_files/test.png" \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/upload)

if [ "$HTTP_CODE" -eq 201 ]; then
    echo -e "${GREEN}✓ Test 3 passed${NC}"
else
    echo -e "${RED}✗ Test 3 failed${NC}"
fi

# Test 4: 上传接近限制的文件 (1MB 限制，上传 900KB)
echo -e "\n${YELLOW}Test 4: Upload file close to size limit (900KB)${NC}"
dd if=/dev/zero of=test_files/large900k.dat bs=1024 count=900 2>/dev/null

HTTP_CODE=$(curl -X POST \
     -F "file=@test_files/large900k.dat" \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/upload)

if [ "$HTTP_CODE" -eq 201 ]; then
    echo -e "${GREEN}✓ Test 4 passed (file within limit accepted)${NC}"
else
    echo -e "${RED}✗ Test 4 failed (expected 201, got $HTTP_CODE)${NC}"
fi

# Test 5: 上传超大文件 (应该失败 - 413)
echo -e "\n${YELLOW}Test 5: Upload oversized file (1.5MB - should fail with 413)${NC}"
dd if=/dev/zero of=test_files/oversized1.5m.dat bs=1024 count=1536 2>/dev/null

HTTP_CODE=$(curl -X POST \
     -F "file=@test_files/oversized1.5m.dat" \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/upload)

if [ "$HTTP_CODE" -eq 413 ]; then
    echo -e "${GREEN}✓ Test 5 passed (correctly rejected with 413 Payload Too Large)${NC}"
else
    echo -e "${RED}✗ Test 5 failed (expected 413, got $HTTP_CODE)${NC}"
fi

# Test 6: 尝试用 GET 访问 /upload (应该失败 - 405)
echo -e "\n${YELLOW}Test 6: Try GET on /upload (should fail with 405)${NC}"
HTTP_CODE=$(curl -X GET \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/upload)

if [ "$HTTP_CODE" -eq 405 ]; then
    echo -e "${GREEN}✓ Test 6 passed (405 Method Not Allowed)${NC}"
else
    echo -e "${RED}✗ Test 6 failed (expected 405, got $HTTP_CODE)${NC}"
fi

# Test 7: 路径遍历攻击测试
echo -e "\n${YELLOW}Test 7: Path traversal attack in filename${NC}"
HTTP_CODE=$(curl -X POST \
     -F "file=@test_files/test.txt;filename=../../../etc/passwd" \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/upload)

if [ "$HTTP_CODE" -eq 400 ] || [ "$HTTP_CODE" -eq 403 ]; then
    echo -e "${GREEN}✓ Test 7 passed (path traversal blocked with $HTTP_CODE)${NC}"
else
    echo -e "${RED}✗ Test 7 failed (expected 400/403, got $HTTP_CODE)${NC}"
fi

# Test 8: 验证已上传文件的内容
echo -e "\n${YELLOW}Test 8: Verify uploaded file content${NC}"
EXPECTED="This is a test file for upload"
if [ -f "$UPLOAD_PATH/test.txt" ]; then
    ACTUAL=$(cat "$UPLOAD_PATH/test.txt")
    if [ "$ACTUAL" = "$EXPECTED" ]; then
        echo -e "${GREEN}✓ Test 8 passed (file content matches)${NC}"
    else
        echo -e "${RED}✗ Test 8 failed (content mismatch)${NC}"
        echo "Expected: $EXPECTED"
        echo "Actual: $ACTUAL"
    fi
else
    echo -e "${RED}✗ Test 8 failed (file not found)${NC}"
fi

echo -e "\n========================================="
echo "POST Upload Tests Completed"
echo "Check uploaded files: ls -la $UPLOAD_PATH"
echo "========================================="