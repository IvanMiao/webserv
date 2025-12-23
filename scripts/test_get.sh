#!/bin/bash

# WebServ 测试脚本 - GET 请求
# 使用 localhost:8080

HOST="http://localhost:8080"

# 测试路径列表
PATHS=(
    "/"                  # 根目录
    "/index.html"        # 静态文件
    "/uploads/"          # 目录
    "/cgi-bin/test.cgi"  # CGI 脚本
)

for path in "${PATHS[@]}"
do
    echo "=============================="
    echo "GET ${path}"
    
    # 使用 curl 发起请求，并显示状态码和内容
    RESPONSE=$(curl -s -w "\nHTTP_STATUS:%{http_code}\n" "$HOST$path")
    
    # 分离状态码和正文
    BODY=$(echo "$RESPONSE" | sed '/HTTP_STATUS:/d')
    STATUS=$(echo "$RESPONSE" | grep HTTP_STATUS | cut -d: -f2)
    
    echo "Status Code: $STATUS"
    echo "Response Body:"
    echo "$BODY"
    echo "=============================="
    echo
done

