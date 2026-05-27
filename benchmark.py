import os
import sys
import time
import subprocess
import glob
import concurrent.futures

def enqueue_pdfs(num_files=2):
    return glob.glob("files/**/*.pdf", recursive=True)[:num_files]

def run_cpp_direct(pdf):
    subprocess.run(["./build/ingestor", "--ingest-direct", pdf, "arbitrary_collection"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def run_python_direct(pdf):
    subprocess.run(["venv/bin/python3", "python_ingestor.py", "--direct", pdf], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def run_benchmark():
    NUM_FILES = 2
    WORKERS = 4
    pdfs = enqueue_pdfs(NUM_FILES)
    if not pdfs:
        print("No files found.")
        return

    print(f"\n--- Running C++ Benchmark ({WORKERS} workers) ---")
    # We flush redis before each test to clear locks
    subprocess.run(["venv/bin/python3", "-c", "import redis; redis.Redis(host='127.0.0.1', port=6380).flushall()"])
    
    start_time = time.time()
    with concurrent.futures.ThreadPoolExecutor(max_workers=WORKERS) as executor:
        executor.map(run_cpp_direct, pdfs)
    cpp_time = time.time() - start_time
    print(f"C++ Engine finished in {cpp_time:.2f} seconds.")

    print(f"\n--- Running Python Benchmark ({WORKERS} workers) ---")
    subprocess.run(["venv/bin/python3", "-c", "import redis; redis.Redis(host='127.0.0.1', port=6380).flushall()"])
    
    start_time = time.time()
    with concurrent.futures.ThreadPoolExecutor(max_workers=WORKERS) as executor:
        executor.map(run_python_direct, pdfs)
    py_time = time.time() - start_time
    print(f"Python Engine finished in {py_time:.2f} seconds.")

    print("\n=== Benchmark Results ===")
    print(f"C++ Engine: {cpp_time:.2f}s")
    print(f"Python Engine: {py_time:.2f}s")
    if cpp_time < py_time:
        print(f"C++ was {py_time/cpp_time:.2f}x faster.")
    else:
        print(f"Python was {cpp_time/py_time:.2f}x faster.")

if __name__ == "__main__":
    run_benchmark()
