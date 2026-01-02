import threading
import http.client
import sys
import time

SUCCESS_COUNT = 0
FAILURE_COUNT = 0
LOCK = threading.Lock()

def make_request(idx):
    global SUCCESS_COUNT, FAILURE_COUNT
    try:
        conn = http.client.HTTPConnection("localhost", 8080)
        conn.request("GET", "/cgi-bin/echo_env.py")
        resp = conn.getresponse()
        data = resp.read()
        conn.close()
        
        with LOCK:
            if resp.status == 200:
                SUCCESS_COUNT += 1
            else:
                FAILURE_COUNT += 1
                print(f"Req {idx} failed: {resp.status}")
    except Exception as e:
        with LOCK:
            FAILURE_COUNT += 1
            print(f"Req {idx} error: {e}")

def main():
    threads = []
    num_requests = 20
    
    print(f"Starting {num_requests} concurrent requests...")
    start_time = time.time()
    
    for i in range(num_requests):
        t = threading.Thread(target=make_request, args=(i,))
        threads.append(t)
        t.start()
        
    for t in threads:
        t.join()
        
    duration = time.time() - start_time
    print(f"Finished in {duration:.2f}s")
    print(f"Success: {SUCCESS_COUNT}, Failures: {FAILURE_COUNT}")
    
    if FAILURE_COUNT == 0:
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
