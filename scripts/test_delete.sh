#!/bin/bash
# test_delete.sh - Test DELETE functionality

echo "========================================="
echo "DELETE File Tests"
echo "========================================="

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

SERVER="http://localhost:8080"
UPLOAD_PATH="/tmp/uploads"

# Create test files first
echo -e "\n${YELLOW}Preparing test files...${NC}"
echo "Content for delete test 1" > "$UPLOAD_PATH/delete_test1.txt"
echo "Content for delete test 2" > "$UPLOAD_PATH/delete_test2.txt"
echo "Binary content" > "$UPLOAD_PATH/delete_binary.bin"

# Test 1: Delete existing file
echo -e "\n${YELLOW}Test 1: Delete existing file${NC}"
if [ -f "$UPLOAD_PATH/delete_test1.txt" ]; then
    HTTP_CODE=$(curl -X DELETE \
         -s -o /dev/null -w "%{http_code}" \
         $SERVER/upload/delete_test1.txt)
    
    if [ "$HTTP_CODE" -eq 204 ]; then
        if [ ! -f "$UPLOAD_PATH/delete_test1.txt" ]; then
            echo -e "${GREEN}✓ Test 1 passed (file deleted successfully)${NC}"
        else
            echo -e "${RED}✗ Test 1 failed (file still exists after DELETE)${NC}"
        fi
    else
        echo -e "${RED}✗ Test 1 failed (expected 204, got $HTTP_CODE)${NC}"
    fi
else
    echo -e "${RED}✗ Test 1 skipped (test file not created)${NC}"
fi

# Test 2: Delete non-existent file
echo -e "\n${YELLOW}Test 2: Delete non-existent file${NC}"
HTTP_CODE=$(curl -X DELETE \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/upload/nonexistent_file.txt)

if [ "$HTTP_CODE" -eq 404 ]; then
    echo -e "${GREEN}✓ Test 2 passed (404 for non-existent file)${NC}"
else
    echo -e "${RED}✗ Test 2 failed (expected 404, got $HTTP_CODE)${NC}"
fi

# Test 3: Path traversal attack in DELETE
echo -e "\n${YELLOW}Test 3: Path traversal attack in DELETE${NC}"
HTTP_CODE=$(curl -X DELETE \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/upload/../../../etc/passwd)

if [ "$HTTP_CODE" -eq 403 ]; then
    echo -e "${GREEN}✓ Test 3 passed (403 for path traversal)${NC}"
else
    echo -e "${RED}✗ Test 3 failed (expected 403, got $HTTP_CODE)${NC}"
fi

# Test 4: DELETE is allowed on /upload
echo -e "\n${YELLOW}Test 4: DELETE is allowed on /upload location${NC}"
if [ -f "$UPLOAD_PATH/delete_test2.txt" ]; then
    HTTP_CODE=$(curl -X DELETE \
         -s -o /dev/null -w "%{http_code}" \
         $SERVER/upload/delete_test2.txt)
    
    if [ "$HTTP_CODE" -eq 204 ]; then
        echo -e "${GREEN}✓ Test 4 passed (DELETE allowed)${NC}"
    else
        echo -e "${RED}✗ Test 4 failed (expected 204, got $HTTP_CODE)${NC}"
    fi
fi

# Test 5: DELETE on root location (should be disallowed)
echo -e "\n${YELLOW}Test 5: DELETE on / location (should fail)${NC}"
HTTP_CODE=$(curl -X DELETE \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/www/test.html)

# / location doesn't allow DELETE, so should get 405 or 403
if [ "$HTTP_CODE" -eq 405 ] || [ "$HTTP_CODE" -eq 403 ]; then
    echo -e "${GREEN}✓ Test 5 passed ($HTTP_CODE for disallowed DELETE)${NC}"
else
    echo -e "${RED}✗ Test 5 failed (expected 405/403, got $HTTP_CODE)${NC}"
fi

echo -e "\n========================================="
echo "DELETE File Tests Completed"
echo "Remaining files in $UPLOAD_PATH:"
ls -la $UPLOAD_PATH | tail -10
echo "========================================="
