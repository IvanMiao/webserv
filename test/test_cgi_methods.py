import http.client
import sys

def colored(text, color):
    colors = {'green': '\033[92m', 'red': '\033[91m', 'reset': '\033[0m'}
    return f"{colors.get(color, '')}{text}{colors['reset']}"

def run_test(name, method, path, body=None, expected_status=200, checks=None):
    print(f"Running {name}...", end=" ")
    try:
        conn = http.client.HTTPConnection("localhost", 8080)
        headers = {"Content-Type": "text/plain"} if body else {}
        conn.request(method, path, body=body, headers=headers)
        resp = conn.getresponse()
        body_data = resp.read().decode('utf-8', errors='ignore')
        
        # Combine headers and body for simple checking
        headers_str = ""
        for k, v in resp.getheaders():
            headers_str += f"{k}: {v}\n"
            
        full_data = headers_str + "\n" + body_data
        
        conn.close()

        if resp.status != expected_status:
            print(colored(f"FAIL (Status: {resp.status} != {expected_status})", 'red'))
            return False

        if checks:
            for check in checks:
                if check not in full_data:
                    print(colored(f"FAIL (Missing content: '{check}')", 'red'))
                    print(f"DEBUG: Headers: {headers_str}")
                    print(f"DEBUG: Body: {body_data}")
                    return False
        
        print(colored("PASS", 'green'))
        return True
    except Exception as e:
        print(colored(f"ERROR: {e}", 'red'))
        return False

def main():
    results = []
    
    # Test 1: GET Environment Variables
    results.append(run_test(
        "GET Environment", 
        "GET", "/cgi-bin/echo_env.py", 
        expected_status=200, 
        checks=["REQUEST_METHOD=GET"]
    ))

    # Test 2: POST Echo Body
    results.append(run_test(
        "POST Echo", 
        "POST", "/cgi-bin/echo_body.py", 
        body="Hello World CGI", 
        expected_status=200, 
        checks=["Hello World CGI", "X-Custom-Header: EchoTest"]
    ))

    # Test 3: Query String
    results.append(run_test(
        "Query String", 
        "GET", "/cgi-bin/echo_env.py?foo=bar&baz=123", 
        expected_status=200, 
        checks=["QUERY_STRING=foo=bar&baz=123"]
    ))

    if all(results):
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
