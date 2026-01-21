

## TEST 200 OK
curl -i http://localhost:8080/

## TEST 201 CREATED
echo "test content" > test.txt
curl -i -X POST -F "file=@test.txt" http://localhost:8080/uploads/

## TEST 204 NO CONTENT
curl -i -X DELETE http://localhost:8080/uploads/test.txt

## TEST 301 MOVED PERMANENTLY
curl -i http://localhost:8080/uploads

## TEST 404 BAD REQUEST
echo -e "INVALID REQUEST\r\n\r\n" | nc localhost 8080

## TEST 403 FORBIDDEN 
curl -i http://localhost:8080/../../../etc/passwd

## TEST 404 NOT FOUND
curl -i http://localhost:8080/nonexistent.html

## TEST 405 METHOD NOT ALLOWED
curl -i -X POST http://localhost:8083/docs

## TEST 413 PAYLOAD TOO LARGE

#REMEMBER TO CHANGE THE client_max_body_size
curl -i -X POST -H "Content-Type: text/plain" \
  --data "$(printf 'A%.0s' {1..200})" \
  http://localhost:8080/uploads/

## TEST 504 GATEWAY TIMEOUT
cat > www/cgi-bin/slow.py << 'EOF'
#!/usr/bin/env python3
import time
time.sleep(500)
print("Content-Type: text/plain\n")
print("Too slow!")
EOF
chmod +x www/cgi-bin/slow.py

curl -i http://localhost:8080/cgi-bin/slow.py


curl -i http://localhost:8080/does_not_exist.html

## test DELETE
echo "test" > test_delete.txt
curl -X POST -F "file=@test_delete.txt" http://localhost:8080/uploads/

curl -i -X DELETE http://localhost:8080/uploads/test_delete.txt

curl -i http://localhost:8080/uploads/test_delete.txt

## test DOWNLOAD
echo "test" > my_file.txt
curl -X POST -F "file=@my_file.txt" http://localhost:8080/uploads/

curl -o downloaded_file.txt http://localhost:8080/uploads/my_file.txt

diff my_file.txt downloaded_file.txt

