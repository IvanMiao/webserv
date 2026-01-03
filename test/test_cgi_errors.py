import http.client
import sys

def colored(text, color):
    colors = {'green': '\033[92m', 'red': '\033[91m', 'reset': '\033[0m'}
    return f"{colors.get(color, '')}{text}{colors['reset']}"

def run_test(name, path, expected_status):
    print(f"Running {name}...", end=" ")
    try:
        conn = http.client.HTTPConnection("localhost", 8080, timeout=10) # 10s client timeout
        conn.request("GET", path)
        resp = conn.getresponse()
        data = resp.read() # Read to clear
        conn.close()

        if resp.status == expected_status:
            print(colored(f"PASS (Got {resp.status})", 'green'))
            return True
        else:
            print(colored(f"FAIL (Expected {expected_status}, got {resp.status})", 'red'))
            return False
    except Exception as e:
        print(colored(f"ERROR: {e}", 'red'))
        return False

def main():
    results = []

    # Test 1: Crash Script
    # Ideally should return 500 or 502
    results.append(run_test("Crash Script", "/cgi-bin/crash.py", 500))

    # Test 2: Missing Script
    results.append(run_test("Missing Script", "/cgi-bin/non_existent.py", 404))

    # Test 3: Large Output (check if server handles it without crashing)
    results.append(run_test("Large Output", "/cgi-bin/large_output.py", 200))

    if all(results):
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
