#!/bin/bash
# test_cgi_post_adjusted.sh

echo "========================================="
echo "CGI POST Tests (Adjusted for default.conf)"
echo "========================================="

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

SERVER="http://localhost:8080"

# 确保 CGI 脚本存在且可执行
if [ ! -x "www/cgi-bin/form_handler.py" ]; then
    echo -e "${RED}Error: CGI script not found or not executable${NC}"
    echo "Run setup_test_environment.sh first"
    exit 1
fi

# Test 1: 简单表单 POST
echo -e "\n${YELLOW}Test 1: Simple form POST to CGI${NC}"
RESPONSE=$(curl -X POST \
     -d "name=John&age=30&city=Paris" \
     -H "Content-Type: application/x-www-form-urlencoded" \
     -s \
     $SERVER/cgi-bin/form_handler.py)

if echo "$RESPONSE" | grep -q "POST Data"; then
    echo -e "${GREEN}✓ Test 1 passed${NC}"
    echo "Response snippet:"
    echo "$RESPONSE" | head -n 20
else
    echo -e "${RED}✗ Test 1 failed${NC}"
    echo "Response: $RESPONSE"
fi

# Test 2: JSON POST 到 CGI
echo -e "\n${YELLOW}Test 2: JSON POST to CGI${NC}"
RESPONSE=$(curl -X POST \
     -d '{"username":"alice","password":"secret"}' \
     -H "Content-Type: application/json" \
     -s \
     $SERVER/cgi-bin/form_handler.py)

if echo "$RESPONSE" | grep -q "CGI Form Handler"; then
    echo -e "${GREEN}✓ Test 2 passed${NC}"
else
    echo -e "${RED}✗ Test 2 failed${NC}"
fi

# Test 3: 空 POST
echo -e "\n${YELLOW}Test 3: Empty POST body${NC}"
HTTP_CODE=$(curl -X POST \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/cgi-bin/form_handler.py)

if [ "$HTTP_CODE" -eq 200 ]; then
    echo -e "${GREEN}✓ Test 3 passed (200 OK)${NC}"
else
    echo -e "${RED}✗ Test 3 failed (expected 200, got $HTTP_CODE)${NC}"
fi

# Test 4: GET 请求到 CGI（应该也能工作）
echo -e "\n${YELLOW}Test 4: GET request to CGI with query string${NC}"
RESPONSE=$(curl -X GET \
     -s \
     "$SERVER/cgi-bin/form_handler.py?param1=value1&param2=value2")

if echo "$RESPONSE" | grep -q "CGI Form Handler"; then
    echo -e "${GREEN}✓ Test 4 passed${NC}"
else
    echo -e "${RED}✗ Test 4 failed${NC}"
fi

# Test 5: 不存在的 CGI 脚本（应该返回 404）
echo -e "\n${YELLOW}Test 5: POST to non-existent CGI script${NC}"
HTTP_CODE=$(curl -X POST \
     -d "test=data" \
     -s -o /dev/null -w "%{http_code}" \
     $SERVER/cgi-bin/nonexistent.py)

if [ "$HTTP_CODE" -eq 404 ]; then
    echo -e "${GREEN}✓ Test 5 passed (404 Not Found)${NC}"
else
    echo -e "${RED}✗ Test 5 failed (expected 404, got $HTTP_CODE)${NC}"
fi

echo -e "\n========================================="
echo "CGI POST Tests Completed"
echo "========================================="