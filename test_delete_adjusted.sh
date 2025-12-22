#!/bin/bash
# test_delete_adjusted.sh

echo "========================================="
echo "DELETE Tests (Adjusted for default.conf)"
echo "========================================="

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

SERVER="http://localhost:8080"
UPLOAD_PATH="/tmp/uploads"

# 准备：先上传几个测试文件
echo -e "\n${YELLOW}Preparation: Uploading test files${NC}"
echo "Delete test 1" > test_files/delete1.txt
echo "Delete test 2" > test_files/delete2.txt
echo "Delete test 3" > test_files/delete3.txt

curl -X POST -F "file=@test_files/delete1.txt" $SERVER/upload -s > /dev/null
curl -X POST -F "file=@test_files/delete2.txt" $SERVER/upload -s > /dev/null
curl -X POST -F "file=@test_files/delete3.txt" $SERVER/upload -s > /dev/null

sleep 1

echo "Files in $UPLOAD_PATH before deletion:"
ls -la $UPLOAD_PATH

# Test 1: 删除第一个文件 (注意：DELETE 只在 /upload 允许)
echo -e "\n${YELLOW}Test 1: DELETE single file from /upload${NC}"
HTTP_CODE=$(curl -X DELETE \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/upload/delete1.txt)

echo "Response code: $HTTP_CODE"

if [ "$HTTP_CODE" -eq 204 ]; then
    echo -e "${GREEN}✓ Test 1 passed (204 No Content)${NC}"
    
    # 验证文件已被删除
    if [ ! -f "$UPLOAD_PATH/delete1.txt" ]; then
        echo -e "${GREEN}✓ File successfully deleted from $UPLOAD_PATH${NC}"
    else
        echo -e "${RED}✗ File still exists after DELETE${NC}"
    fi
else
    echo -e "${RED}✗ Test 1 failed (expected 204, got $HTTP_CODE)${NC}"
fi

# Test 2: 删除第二个文件
echo -e "\n${YELLOW}Test 2: DELETE another file${NC}"
HTTP_CODE=$(curl -X DELETE \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/upload/delete2.txt)

if [ "$HTTP_CODE" -eq 204 ] && [ ! -f "$UPLOAD_PATH/delete2.txt" ]; then
    echo -e "${GREEN}✓ Test 2 passed${NC}"
else
    echo -e "${RED}✗ Test 2 failed${NC}"
fi

# Test 3: 删除不存在的文件 (应该返回 404)
echo -e "\n${YELLOW}Test 3: DELETE non-existent file${NC}"
HTTP_CODE=$(curl -X DELETE \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/upload/nonexistent.txt)

if [ "$HTTP_CODE" -eq 404 ]; then
    echo -e "${GREEN}✓ Test 3 passed (404 Not Found)${NC}"
else
    echo -e "${RED}✗ Test 3 failed (expected 404, got $HTTP_CODE)${NC}"
fi

# Test 4: 尝试在根路径删除（应该失败 - 405，因为 / 只允许 GET POST）
echo -e "\n${YELLOW}Test 4: Try DELETE on root path (should fail with 405)${NC}"
# 先在 www 目录创建一个测试文件
echo "test" > www/test_delete_root.txt

HTTP_CODE=$(curl -X DELETE \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/test_delete_root.txt)

if [ "$HTTP_CODE" -eq 405 ]; then
    echo -e "${GREEN}✓ Test 4 passed (405 Method Not Allowed on root)${NC}"
else
    echo -e "${RED}✗ Test 4 failed (expected 405, got $HTTP_CODE)${NC}"
fi

rm -f www/test_delete_root.txt

# Test 5: 路径遍历攻击测试
echo -e "\n${YELLOW}Test 5: Path traversal attack in DELETE${NC}"
HTTP_CODE=$(curl -X DELETE \
     -s -o /dev/null -w "%{http_code}" \
     "$SERVER/upload/../../../etc/hosts")

if [ "$HTTP_CODE" -eq 403 ]; then
    echo -e "${GREEN}✓ Test 5 passed (403 Forbidden - attack blocked)${NC}"
else
    echo -e "${RED}✗ Test 5 failed (expected 403, got $HTTP_CODE)${NC}"
fi

# Test 6: URL 编码文件名
echo -e "\n${YELLOW}Test 6: DELETE file with spaces (URL encoded)${NC}"
echo "test with spaces" > test_files/test_file_spaces.txt
curl -X POST -F "file=@test_files/test_file_spaces.txt;filename=test file spaces.txt" $SERVER/upload -s > /dev/null
sleep 1

HTTP_CODE=$(curl -X DELETE \
     -s -o /dev/null -w "%{http_code}" \
     "$SERVER/upload/test%20file%20spaces.txt")

if [ "$HTTP_CODE" -eq 204 ]; then
    echo -e "${GREEN}✓ Test 6 passed${NC}"
else
    echo -e "${RED}✗ Test 6 failed (expected 204, got $HTTP_CODE)${NC}"
fi

# Test 7: 尝试删除目录（应该失败）
echo -e "\n${YELLOW}Test 7: Try to DELETE a directory (should fail with 403)${NC}"
mkdir -p $UPLOAD_PATH/test_directory

HTTP_CODE=$(curl -X DELETE \
     -s -o /dev/null -w "%{http_code}" \
     "$SERVER/upload/test_directory")

if [ "$HTTP_CODE" -eq 403 ]; then
    echo -e "${GREEN}✓ Test 7 passed (403 Forbidden for directory)${NC}"
else
    echo -e "${RED}✗ Test 7 failed (expected 403, got $HTTP_CODE)${NC}"
fi

rmdir $UPLOAD_PATH/test_directory 2>/dev/null

echo -e "\n========================================="
echo "Files remaining in $UPLOAD_PATH after tests:"
ls -la $UPLOAD_PATH
echo "========================================="