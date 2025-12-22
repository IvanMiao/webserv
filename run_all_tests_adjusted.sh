#!/bin/bash
# run_all_tests_adjusted.sh

echo "========================================="
echo "Running All Tests for default.conf"
echo "========================================="

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

# 检查服务器是否运行
echo -e "\n${BLUE}Checking if server is running...${NC}"
if curl -s http://localhost:8080 > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Server is running${NC}"
else
    echo -e "${RED}✗ Server is not running!${NC}"
    echo "Please start the server first:"
    echo "  ./webserv default.conf"
    exit 1
fi

# 设置环境
echo -e "\n${BLUE}Setting up test environment...${NC}"
bash setup_test_environment.sh

# 给所有脚本添加执行权限
chmod +x test_*.sh

# 运行测试套件
FAILED_TESTS=0

echo -e "\n${BLUE}=== 1. POST Upload Tests ===${NC}"
if bash test_post_upload_adjusted.sh; then
    echo -e "${GREEN}✓ POST Upload Tests passed${NC}"
else
    echo -e "${RED}✗ POST Upload Tests failed${NC}"
    ((FAILED_TESTS++))
fi

echo -e "\n${BLUE}=== 2. DELETE Tests ===${NC}"
if bash test_delete_adjusted.sh; then
    echo -e "${GREEN}✓ DELETE Tests passed${NC}"
else
    echo -e "${RED}✗ DELETE Tests failed${NC}"
    ((FAILED_TESTS++))
fi

echo -e "\n${BLUE}=== 3. CGI POST Tests ===${NC}"
if bash test_cgi_post_adjusted.sh; then
    echo -e "${GREEN}✓ CGI POST Tests passed${NC}"
else
    echo -e "${RED}✗ CGI POST Tests failed${NC}"
    ((FAILED_TESTS++))
fi

echo -e "\n${BLUE}=== 4. Workflow Test ===${NC}"
if bash test_workflow_adjusted.sh; then
    echo -e "${GREEN}✓ Workflow Test passed${NC}"
else
    echo -e "${RED}✗ Workflow Test failed${NC}"
    ((FAILED_TESTS++))
fi

#最终总结
echo -e "\n========================================="
echo -e "${BLUE}Final Test Summary${NC}"
echo -e "========================================="

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}✓✓✓ All test suites passed! ✓✓✓${NC}"
    echo -e "\n${BLUE}Upload directory contents:${NC}"
    ls -lah /tmp/uploads
    exit 0
else
    echo -e "${RED}✗✗✗ $FAILED_TESTS test suite(s) failed ✗✗✗${NC}"
    exit 1
fi
