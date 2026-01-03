import threading
import time
import http.client

def get_request(path):
    conn = http.client.HTTPConnection("localhost", 8080)
    start = time.time()
    conn.request("GET", path)
    resp = conn.getresponse()
    data = resp.read()
    end = time.time()
    conn.close()
    return end - start, resp.status

def test_slow_cgi():
    print("Requesting slow CGI...")
    duration, status = get_request("/cgi-bin/sleep.py")
    print(f"Slow CGI finished in {duration:.2f}s with status {status}")

def test_fast_static():
    # Wait a bit to ensure slow CGI started
    time.sleep(0.1) 
    print("Requesting static file...")
    duration, status = get_request("/index.html")
    print(f"Static file finished in {duration:.2f}s with status {status}")
    if duration > 1.0:
        print("FAIL: Static file took too long (blocked by CGI?)")
    else:
        print("PASS: Static file response was fast")

if __name__ == "__main__":
    t1 = threading.Thread(target=test_slow_cgi)
    t2 = threading.Thread(target=test_fast_static)

    t1.start()
    t2.start()

    t1.join()
    t2.join()
