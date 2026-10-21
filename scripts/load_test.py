#!/usr/bin/env python3
"""
Simple load testing script for the C++ HTTP Load Balancer.
Sends N concurrent requests and measures response times.

Usage:
    python3 load_test.py [--host HOST] [--port PORT] [--requests N] [--concurrency C]
"""
import argparse
import time
import http.client
import threading
import statistics
from collections import defaultdict

results_lock = threading.Lock()
results = []
backend_distribution = defaultdict(int)

def send_request(host, port, path, req_id):
    start = time.time()
    try:
        conn = http.client.HTTPConnection(host, port, timeout=10)
        conn.request("GET", path)
        resp = conn.getresponse()
        body = resp.read().decode("utf-8", errors="replace")
        elapsed = time.time() - start

        # Extract which backend replied (from X-Backend-Port header or body)
        backend_port = resp.getheader("X-Backend-Port", "unknown")
        status = resp.status
        conn.close()

        with results_lock:
            results.append({"id": req_id, "status": status,
                             "time": elapsed, "backend": backend_port})
            backend_distribution[backend_port] += 1
    except Exception as e:
        elapsed = time.time() - start
        with results_lock:
            results.append({"id": req_id, "status": 0,
                             "time": elapsed, "backend": "error", "error": str(e)})

def run_test(host, port, path, total_requests, concurrency):
    print(f"\n{'='*55}")
    print(f"  Load Test: http://{host}:{port}{path}")
    print(f"  Total Requests : {total_requests}")
    print(f"  Concurrency    : {concurrency}")
    print(f"{'='*55}\n")

    all_threads = []
    start_all = time.time()

    for batch_start in range(0, total_requests, concurrency):
        batch = []
        for i in range(concurrency):
            req_id = batch_start + i
            if req_id >= total_requests:
                break
            t = threading.Thread(target=send_request,
                                  args=(host, port, path, req_id))
            batch.append(t)
            all_threads.append(t)

        for t in batch: t.start()
        for t in batch: t.join()

    total_time = time.time() - start_all

    # --- Stats ---
    times    = [r["time"] for r in results]
    statuses = [r["status"] for r in results]
    errors   = [r for r in results if r["status"] == 0]

    print(f"Results:")
    print(f"  Total time      : {total_time:.3f}s")
    print(f"  Req/sec         : {total_requests / total_time:.1f}")
    print(f"  Success (2xx)   : {sum(1 for s in statuses if 200 <= s < 300)}")
    print(f"  Errors          : {len(errors)}")
    if times:
        print(f"  Latency (avg)   : {statistics.mean(times)*1000:.1f}ms")
        print(f"  Latency (p50)   : {statistics.median(times)*1000:.1f}ms")
        print(f"  Latency (p95)   : {statistics.quantiles(times, n=20)[18]*1000:.1f}ms")
        print(f"  Latency (max)   : {max(times)*1000:.1f}ms")

    print(f"\n  Backend Distribution:")
    for backend, count in sorted(backend_distribution.items()):
        pct = 100.0 * count / total_requests
        bar = "#" * int(pct / 2)
        print(f"    Port {backend:>5}: {count:4d} requests ({pct:5.1f}%) {bar}")

    print()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Load test the C++ LB")
    parser.add_argument("--host",        default="127.0.0.1")
    parser.add_argument("--port",        type=int, default=8080)
    parser.add_argument("--path",        default="/")
    parser.add_argument("--requests",    type=int, default=300)
    parser.add_argument("--concurrency", type=int, default=30)
    args = parser.parse_args()

    run_test(args.host, args.port, args.path, args.requests, args.concurrency)
