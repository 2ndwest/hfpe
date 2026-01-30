#!/usr/bin/env python3
"""
Mock MIT PE registration server for testing retry logic.
Simulates the endpoints at https://eduapps.mit.edu/mitpe/student/registration
with configurable failure rates to test bot resilience.
"""

from http.server import HTTPServer, BaseHTTPRequestHandler
import random
import time
import argparse
import os

# Configuration
FAILURE_RATE = 0.7  # 70% of requests fail
SLOW_RATE = 0.3     # 30% of successful requests are slow
MAX_DELAY_SEC = 3   # Max artificial delay for slow requests

# Path to the real section list HTML
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SECTION_LIST_PATH = os.path.join(SCRIPT_DIR, "test", "sectionlist_resp.html")


class MockPEHandler(BaseHTTPRequestHandler):
    request_count = 0

    def log_message(self, format, *args):
        print(f"[{self.request_count}] {format % args}")

    def maybe_fail(self):
        """Randomly fail or delay requests to simulate server under load."""
        MockPEHandler.request_count += 1

        if random.random() < FAILURE_RATE:
            failure_type = random.choice([500, 502, 503, 429, "timeout", "reset"])

            if failure_type == "timeout":
                print(f"  -> Simulating timeout (sleeping 30s)...")
                time.sleep(30)
                return True
            elif failure_type == "reset":
                print(f"  -> Simulating connection reset")
                self.connection.close()
                return True
            else:
                print(f"  -> Simulating HTTP {failure_type}")
                self.send_error(failure_type)
                return True

        if random.random() < SLOW_RATE:
            delay = random.uniform(0.5, MAX_DELAY_SEC)
            print(f"  -> Slow response ({delay:.1f}s delay)...")
            time.sleep(delay)

        return False

    def do_GET(self):
        print(f"GET {self.path}")

        if self.maybe_fail():
            return

        if self.path == "/mitpe/student/registration/home":
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(b"<html><body><h1>MIT PE Registration</h1></body></html>")

        elif self.path == "/mitpe/student/registration/sectionList":
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            with open(SECTION_LIST_PATH, "rb") as f:
                self.wfile.write(f.read())

        elif self.path.startswith("/mitpe/student/registration/section"):
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(b"<html><body><h1>Section Details</h1></body></html>")

        else:
            self.send_error(404)

    def do_POST(self):
        print(f"POST {self.path}")

        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode() if content_length else ""
        print(f"  Body: {body}")

        if self.maybe_fail():
            return

        if self.path == "/mitpe/student/registration/create":
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(b"<html><body><h1>Registration Successful!</h1></body></html>")
        else:
            self.send_error(404)


def main():
    parser = argparse.ArgumentParser(description="Mock MIT PE registration server")
    parser.add_argument("--port", type=int, default=8080, help="Port to listen on")
    parser.add_argument("--failure-rate", type=float, default=0.7,
                        help="Probability of request failure (0.0-1.0)")
    parser.add_argument("--slow-rate", type=float, default=0.3,
                        help="Probability of slow response for successful requests")
    args = parser.parse_args()

    global FAILURE_RATE, SLOW_RATE
    FAILURE_RATE = args.failure_rate
    SLOW_RATE = args.slow_rate

    if not os.path.exists(SECTION_LIST_PATH):
        print(f"ERROR: Section list file not found: {SECTION_LIST_PATH}")
        return 1

    server = HTTPServer(("localhost", args.port), MockPEHandler)
    print(f"Mock PE server starting on http://localhost:{args.port}")
    print(f"Failure rate: {FAILURE_RATE*100:.0f}%, Slow rate: {SLOW_RATE*100:.0f}%")
    print(f"Section list: {SECTION_LIST_PATH}")
    print()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        server.shutdown()


if __name__ == "__main__":
    main()
