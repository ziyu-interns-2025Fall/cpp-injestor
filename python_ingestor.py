import os
import json
import hashlib
import time
import requests
import redis
import concurrent.futures
from pypdf import PdfReader

# Configuration
REDIS_HOST = os.environ.get("REDIS_HOST", "127.0.0.1")
REDIS_PORT = int(os.environ.get("REDIS_PORT", "6380")) # Using mapped port for local testing
QUEUE_NAME = "ingestion:jobs"

MILVUS_HOST = os.environ.get("MILVUS_HOST", "127.0.0.1")
MILVUS_PORT = int(os.environ.get("MILVUS_PORT", "19530"))

OLLAMA_HOST = os.environ.get("OLLAMA_HOST", "127.0.0.1")
OLLAMA_PORT = int(os.environ.get("OLLAMA_PORT", "11434"))

def compute_sha256(filepath):
    sha256 = hashlib.sha256()
    try:
        with open(filepath, 'rb') as f:
            for block in iter(lambda: f.read(8192), b''):
                sha256.update(block)
        return sha256.hexdigest()
    except Exception:
        return ""

def extract_text(pdf_path):
    try:
        reader = PdfReader(pdf_path)
        text = ""
        for page in reader.pages:
            t = page.extract_text()
            if t:
                text += t + "\n"
        return text
    except Exception as e:
        print(f"Failed to extract text: {e}")
        return ""

def chunk_text(text, chunk_size=1024, overlap=256):
    chunks = []
    start = 0
    while start < len(text):
        end = start + chunk_size
        chunks.append({"content": text[start:end], "metadata": {}})
        start += chunk_size - overlap
    return chunks

def get_embedding(text):
    url = f"http://{OLLAMA_HOST}:{OLLAMA_PORT}/api/embeddings"
    payload = {"model": "nomic-embed-text", "prompt": text}
    try:
        resp = requests.post(url, json=payload)
        resp.raise_for_status()
        return resp.json().get("embedding", [])
    except Exception as e:
        print(f"Embedding error: {e}")
        return [0.0] * 768

def insert_milvus(job_id, target_collection, chunks):
    url = f"http://{MILVUS_HOST}:{MILVUS_PORT}/v2/vectordb/entities/insert"
    data = []
    for c in chunks:
        data.append({
            "job_id": job_id,
            "content": c["content"],
            "target_collection": target_collection,
            "status": 1,
            "vector": c["embedding"],
            "metadata_json": json.dumps(c["metadata"])
        })
    payload = {
        "collectionName": "ingestion_staging",
        "data": data
    }
    try:
        resp = requests.post(url, json=payload, headers={"Content-Type": "application/json"})
        if resp.status_code == 200 and resp.json().get("code") == 0:
            return True
        else:
            print(f"Milvus error: {resp.text}")
            return False
    except Exception as e:
        print(f"Milvus connection error: {e}")
        return False

def worker_loop(worker_id):
    r = redis.Redis(host=REDIS_HOST, port=REDIS_PORT, db=0)
    print(f"[Python Worker {worker_id}] Waiting for jobs...")
    while True:
        try:
            # Block for 2 seconds so we can gracefully exit if needed
            result = r.blpop(QUEUE_NAME, timeout=2)
            if not result:
                # For benchmarking, we assume if queue is empty, we exit
                # In production, we'd continue loop
                break
            
            _, json_str = result
            job = json.loads(json_str)
            job_id = job.get("job_id", "")
            pdf_path = job.get("pdf_path", "")
            
            print(f"[Python Worker {worker_id}] Processing job: {job_id} for file: {pdf_path}")
            
            file_hash = compute_sha256(pdf_path)
            if not file_hash:
                continue
            
            if not r.setnx(f"processed_files:{file_hash}", 1):
                print(f"[Python Worker {worker_id}] File already processed, skipping: {pdf_path}")
                continue
            
            text = extract_text(pdf_path)
            chunks = chunk_text(text)
            print(f"[Python Worker {worker_id}] Generated {len(chunks)} chunks")
            
            for c in chunks:
                c["embedding"] = get_embedding(c["content"])
                
            if insert_milvus(job_id, job.get("target_collection", ""), chunks):
                print(f"[Python Worker {worker_id}] Successfully ingested job: {job_id}")
                
        except Exception as e:
            print(f"[Python Worker {worker_id}] Exception: {e}")
            break

if __name__ == "__main__":
    import sys
    if len(sys.argv) > 2 and sys.argv[1] == "--direct":
        pdf_path = sys.argv[2]
        text = extract_text(pdf_path)
        chunks = chunk_text(text)
        for c in chunks:
            c["embedding"] = get_embedding(c["content"])
        insert_milvus("direct_job", "arbitrary_collection", chunks)
        sys.exit(0)

    num_workers = 4
    if len(sys.argv) > 1:
        num_workers = int(sys.argv[1])
        
    print(f"Starting {num_workers} Python workers...")
    with concurrent.futures.ProcessPoolExecutor(max_workers=num_workers) as executor:
        for i in range(num_workers):
            executor.submit(worker_loop, i + 1)
