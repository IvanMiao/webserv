

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


## TEST "setup default error page"
# create 404 page
cat > www/site_8080/custom_404.html << 'EOF'
<!DOCTYPE html>
<html>
<head><title>Custom 404</title></head>
<body>
  <h1>Oops! Page Not Found</h1>
  <p>This is a custom 404 error page.</p>
  <a href="/">Go Home</a>
</body>
</html>
EOF

# crate 500 page
cat > www/site_8080/custom_500.html << 'EOF'
<!DOCTYPE html>
<html>
<head><title>Custom 500</title></head>
<body>
  <h1>Server Error</h1>
  <p>This is a custom 500 error page.</p>
</body>
</html>
EOF

curl -i http://localhost:8080/does_not_exist.html

## TEST "the limitation of request body" 
## send a request < limitation
curl -i -X POST \
  -H "Content-Type: text/plain" \
  --data "This is a short body with less than 100 bytes." \
  http://localhost:8080/uploads/
  
## send a request > limitation
curl -i -X POST \
  -H "Content-Type: text/plain" \
  --data "$(printf 'A%.0s' {1..200})" \
  http://localhost:8080/uploads/
  
## send a request = limitation
curl -i -X POST \
  -H "Content-Type: text/plain" \
  --data "$(printf 'A%.0s' {1..100})" \
  http://localhost:8080/uploads/

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

