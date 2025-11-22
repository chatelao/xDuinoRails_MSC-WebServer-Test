import requests
import sys

def test_webserver():
    url = "http://192.168.7.1"
    print(f"Attempting to connect to {url}...")
    try:
        response = requests.get(url, timeout=5)
        if response.status_code == 200:
            print("Success: Connected to webserver.")
            if "Hello from RP2040 RNDIS!" in response.text:
                print("Success: Content verified.")
            else:
                print("Failure: Content mismatch.")
                print("Received:", response.text)
                return False
        else:
            print(f"Failure: Status code {response.status_code}")
            return False

        # Test API
        api_url = f"{url}/api/status"
        print(f"Testing API: {api_url}")
        response = requests.get(api_url, timeout=5)
        if response.status_code == 200:
            try:
                data = response.json()
                print("Success: API returned JSON:", data)
                if "uptime" in data and "heap_free" in data:
                    print("Success: API Content verified.")
                    return True
                else:
                    print("Failure: API JSON missing keys.")
                    return False
            except ValueError:
                print("Failure: API did not return valid JSON.")
                return False
        else:
            print(f"Failure: API Status code {response.status_code}")
            return False

    except requests.exceptions.ConnectionError:
        print("Failure: Could not connect. Check your network settings and ensure the device is plugged in.")
        print("Ensure your computer's RNDIS interface is configured with IP 192.168.7.x (e.g., 192.168.7.2).")
        return False
    except Exception as e:
        print(f"An error occurred: {e}")
        return False

if __name__ == "__main__":
    if test_webserver():
        sys.exit(0)
    else:
        sys.exit(1)
