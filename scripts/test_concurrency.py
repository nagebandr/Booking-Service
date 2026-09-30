#!/usr/bin/env python3
"""
Test script demonstrating concurrent booking race condition.
Launches User A and User B concurrently trying to book the exact same seat.
"""

import threading
import urllib.request
import json
import time

SERVER_URL = "http://localhost:8080"
EVENT_ID = 1
SEAT_ID = 10  # Seat to compete for

results = []

def book_seat(user_id, user_name, barrier):
    url = f"{SERVER_URL}/events/{EVENT_ID}/bookings"
    payload = json.dumps({"user_id": user_id, "seat_id": SEAT_ID}).encode('utf-8')
    req = urllib.request.Request(
        url,
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST"
    )

    barrier.wait()  # Synchronize both threads to fire at the exact same microsecond

    try:
        start_time = time.perf_counter()
        with urllib.request.urlopen(req) as response:
            duration = (time.perf_counter() - start_time) * 1000
            data = json.loads(response.read().decode())
            results.append((user_name, response.status, data, duration))
    except urllib.error.HTTPError as e:
        duration = (time.perf_counter() - start_time) * 1000
        error_data = json.loads(e.read().decode())
        results.append((user_name, e.code, error_data, duration))
    except Exception as e:
        results.append((user_name, -1, str(e), 0))

def main():
    print("=" * 60)
    print(f" Race Condition Test: 2 Concurrent Users for Seat #{SEAT_ID}")
    print("=" * 60)

    barrier = threading.Barrier(2)
    t1 = threading.Thread(target=book_seat, args=(1, "User A (Alice)", barrier))
    t2 = threading.Thread(target=book_seat, args=(2, "User B (Bob)", barrier))

    t1.start()
    t2.start()

    t1.join()
    t2.join()

    print("\nResults:")
    for user_name, status, body, dur in results:
        status_text = "SUCCESS (201)" if status == 201 else f"CONFLICT ({status})"
        print(f"[{user_name}] -> HTTP {status} ({status_text}) in {dur:.2f}ms")
        print(f"  Response: {json.dumps(body, indent=2)}\n")

    successes = [r for r in results if r[1] == 201]
    conflicts = [r for r in results if r[1] == 409]

    if len(successes) == 1 and len(conflicts) == 1:
        print("VERIFICATION PASSED: Exactly 1 user succeeded, 1 user received Conflict 409!")
    else:
        print(f"VERIFICATION FAILED: {len(successes)} succeeded, {len(conflicts)} conflicts.")

if __name__ == "__main__":
    main()
