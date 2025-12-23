#!/bin/bash
# test_workflow_adjusted.sh

echo "========================================="
echo "Complete Workflow Test"
echo "========================================="

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SERVER="http://localhost:8080"
UPLOAD_PATH="/tmp/uploads"
TEST_COUNT=0
PASS_COUNT=0

test_passed() {
    ((TEST_COUNT++))
    ((PASS_COUNT++))
    echo -e "${GREEN}✓ Step $TEST_COUNT passed${NC}"
}

test_failed() {
    ((TEST_COUNT++))
    echo -e "${RED}✗ Step $TEST_COUNT failed: $1${NC}"
}

echo -e "\n${BLUE}=== Workflow: Upload -> Verify -> Delete -> Verify ===${NC}"

# Step 1: 创建测试文件
echo -e "\n${YELLOW}Step 1: Create test file${NC}"
TEST_CONTENT="Workflow test - $(date)"
echo "$TEST_CONTENT" > test_files/workflow_test.txt
test_passed

# Step 2: 上传文件
echo -e "\n${YELLOW}Step 2: Upload file to /upload${NC}"
UPLOAD_RESPONSE=$(curl -X POST \
     -F "file=@test_files/workflow_test.txt" \
     -s -w "\n%{http_code}" \
     $SERVER/upload)

HTTP_CODE=$(echo "$UPLOAD_RESPONSE" | tail -n1)
RESPONSE_BODY=$(echo "$UPLOAD_RESPONSE" | head -n-1)

if [ "$HTTP_CODE" -eq 201 ]; then
    test_passed
    echo "Upload response: $RESPONSE_BODY"
else
    test_failed "Upload failed with code $HTTP_CODE"
    exit 1
fi

sleep 1

# Step 3: 验证文件存在于文件系统
echo -e "\n${YELLOW}Step 3: Verify file exists in $UPLOAD_PATH${NC}"
if [ -f "$UPLOAD_PATH/workflow_test.txt" ]; then
    test_passed
    echo "File found: $UPLOAD_PATH/workflow_test.txt"
    echo "File size: $(stat -f%z "$UPLOAD_PATH/workflow_test.txt" 2>/dev/null || stat -c%s "$UPLOAD_PATH/workflow_test.txt")"
else
    test_failed "File not found in filesystem"
    echo "Files in $UPLOAD_PATH:"
    ls -la $UPLOAD_PATH
fi

# Step 4: 验证文件内容
echo -e "\n${YELLOW}Step 4: Verify file content${NC}"
if [ -f "$UPLOAD_PATH/workflow_test.txt" ]; then
    ACTUAL_CONTENT=$(cat "$UPLOAD_PATH/workflow_test.txt")
    if [ "$ACTUAL_CONTENT" = "$TEST_CONTENT" ]; then
        test_passed
        echo "Content matches"
    else
        test_failed "Content mismatch"
        echo "Expected: $TEST_CONTENT"
        echo "Actual: $ACTUAL_CONTENT"
    fi
else
    test_failed "Cannot verify content - file not found"
fi

# Step 5: 删除文件
echo -e "\n${YELLOW}Step 5: DELETE the file${NC}"
DELETE_CODE=$(curl -X DELETE \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/upload/workflow_test.txt)

if [ "$DELETE_CODE" -eq 204 ]; then
    test_passed
    echo "DELETE returned 204 No Content"
else
    test_failed "DELETE failed with code $DELETE_CODE"
fi

sleep 1

# Step 6: 验证文件已被删除
echo -e "\n${YELLOW}Step 6: Verify file removed from filesystem${NC}"
if [ ! -f "$UPLOAD_PATH/workflow_test.txt" ]; then
    test_passed
    echo "File successfully removed"
else
    test_failed "File still exists after DELETE"
fi

# Step 7: 尝试再次删除（应该返回 404）
echo -e "\n${YELLOW}Step 7: Try DELETE again (should return 404)${NC}"
DELETE_AGAIN=$(curl -X DELETE \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/upload/workflow_test.txt)

if [ "$DELETE_AGAIN" -eq 404 ]; then
    test_passed
    echo "Correctly returned 404 for non-existent file"
else
    test_failed "Expected 404, got $DELETE_AGAIN"
fi

# 总结
echo -e "\n========================================="
echo -e "${BLUE}Workflow Test Summary${NC}"
echo -e "========================================="
echo -e "Total Steps: $TEST_COUNT"
echo -e "Passed: ${GREEN}$PASS_COUNT${NC}"
echo -e "Failed: ${RED}$((TEST_COUNT - PASS_COUNT))${NC}"

if [ $PASS_COUNT -eq $TEST_COUNT ]; then
    echo -e "\n${GREEN}✓✓✓ All workflow steps passed! ✓✓✓${NC}"
    exit 0
else
    echo -e "\n${RED}✗✗✗ Some steps failed! ✗✗✗${NC}"
    exit 1
fi