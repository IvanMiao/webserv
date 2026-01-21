#!/bin/bash
# Webserv Stress Test Script
# Usage: ./stress_test.sh [host] [port] [requests] [concurrency]

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"
REQUESTS="${3:-1000}"
CONCURRENCY="${4:-100}"
URL="http://${HOST}:${PORT}/"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "========================================"
echo "  Webserv Stress Test"
echo "========================================"
echo "Target: $URL"
echo "Requests: $REQUESTS | Concurrency: $CONCURRENCY"
echo ""

# Check if server is running
if ! curl -s --connect-timeout 2 "$URL" > /dev/null 2>&1; then
    echo -e "${RED}Error: Server not responding at $URL${NC}"
    exit 1
fi

# Test 1: Basic GET flood
echo -e "${YELLOW}[Test 1] GET Flood${NC}"
ab -n "$REQUESTS" -c "$CONCURRENCY" -q "$URL" 2>&1 | grep -E "(Requests per second|Failed requests|Time taken)"
siege -b -c "$CONCURRENCY" -r $((REQUESTS / CONCURRENCY)) -q "$URL" 2>&1 | head -10
echo ""

# Test 2: Keepalive connections
echo -e "${YELLOW}[Test 2] Keep-Alive (100 requests, 10 concurrent)${NC}"
for i in $(seq 1 10); do
    curl -s -o /dev/null -w "conn $i: %{http_code} time=%{time_total}s\n" \
        -H "Connection: keep-alive" "$URL" &
done
wait
echo ""

# Test 3: Slow client (drip-feed request)
echo -e "${YELLOW}[Test 3] Slow Client Simulation${NC}"
{
    printf "GET / HTTP/1.1\r\n"; sleep 0.3
    printf "Host: %s\r\n" "$HOST"; sleep 0.3
    printf "Connection: close\r\n\r\n"; sleep 0.5
} | nc -w 3 "$HOST" "$PORT" 2>/dev/null | head -1 || echo "(no response)"
echo ""

# Test 4: Large body POST (if upload enabled)
echo -e "${YELLOW}[Test 4] Large POST (1MB)${NC}"
dd if=/dev/zero bs=1M count=1 2>/dev/null | \
    curl -s -o /dev/null -w "Status: %{http_code}, Time: %{time_total}s\n" \
    -X POST -H "Content-Type: application/octet-stream" --data-binary @- "${URL}upload" 2>/dev/null || echo "Skipped (no upload route)"
echo ""

# Test 5: Rapid connect/disconnect
echo -e "${YELLOW}[Test 5] Rapid Connect/Disconnect (100 times)${NC}"
START=$(date +%s.%N)
for i in $(seq 1 100); do
    echo "" | nc -w 1 "$HOST" "$PORT" &>/dev/null &
done
wait
END=$(date +%s.%N)
echo "Completed in $(echo "$END - $START" | bc)s"
echo ""

# Test 6: Invalid requests
echo -e "${YELLOW}[Test 6] Malformed Requests${NC}"
echo -n "Invalid request: "
R1=$(echo "INVALID REQUEST" | timeout 2 nc -w 2 "$HOST" "$PORT" 2>/dev/null | head -1)
echo "${R1:-(no response)}"
echo -n "Path traversal:  "
R2=$(printf "GET /../../etc/passwd HTTP/1.1\r\nHost: $HOST\r\n\r\n" | timeout 2 nc -w 2 "$HOST" "$PORT" 2>/dev/null | head -1)
echo "${R2:-(no response)}"
echo ""

echo -e "${GREEN}========================================\n" \
        "  Stress Test Complete\n" \
        "========================================${NC}"
