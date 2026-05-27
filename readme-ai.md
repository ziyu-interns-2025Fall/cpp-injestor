# cpp-ingestor

A high-performance, fault-tolerant C++ background daemon designed to ingest, parse, chunk, and embed massive PDF documents into a vector database (Milvus) for Retrieval-Augmented Generation (RAG).

## Architecture Overview

This project is built around an **Event-Driven Producer/Consumer Architecture** to handle high-concurrency ingestion streams without bottlenecks. 

### 1. The Queue (Redis)

### 2. The Worker Pool (Consumer Daemon)
The `cpp-ingestor` acts as a multi-threaded daemon. When started, it spins up multiple background workers.
- Workers use Redis `BLPOP` (Blocking List Pop), meaning they sit in a zero-CPU sleep state until a new job arrives.
- The exact millisecond a file is enqueued, Redis wakes up an available worker, handing it the payload for immediate processing. This makes the ingestion pipeline truly **real-time**.

### 3. Fault-Tolerant Deduplication (OpenSSL + SETNX)
Extracting text from a 1000-page PDF and generating vectors is computationally expensive. 
- Before parsing, the worker streams the file through OpenSSL to compute a unique **SHA-256 hash**.
- It uses the Redis `SETNX` (Set if Not eXists) atomic command to claim a lock (e.g., `processed_files:<hash>`).
- If ten users upload the exact same file simultaneously, exactly one worker will claim the lock and process the file. The others will instantly skip it.

### 4. PDF Parsing & Chunking (Poppler)
Once locked, the worker uses `poppler-cpp` to extract UTF-8 encoded text directly from the PDF blocks. 
- **Dynamic Configs**: Depending on the queue payload (`ieee` or `generic`), the parser uses different heuristics to identify headers, footers, and structural boundaries.
- **Chunking**: The extracted markdown-like text is split into overlapping chunks (e.g., 1024 chars, 256 overlap) to ensure critical context isn't severed between vector embeddings.

### 5. Vector Storage (Milvus)
Finally, the worker constructs an efficient JSON payload and ships the chunks via REST to a remote Milvus instance (`ingestion_staging`). Metadata like the source file name, hierarchical headers, and ingestion timestamps are embedded directly alongside the vectors to enable rich metadata filtering during RAG.

---

## How to Run

### Building
The project requires `CMake`, `OpenSSL`, `poppler-cpp`, `hiredis`, `nlohmann_json`, and `libcurl`.
```bash
mkdir build && cd build
cmake ..
make -j4
```

### Running the Workers
To launch the background processing daemon (defaults to 4 workers):
```bash
./build/ingestor --workers 4
```

### Enqueuing a Job (Producer)
To push a job onto the Redis queue for the workers to pick up:
```bash
./build/ingestor --enqueue "/path/to/document.pdf" target_collection "job_123" generic
```

### Direct Bypass (Testing)
To bypass the Redis queue entirely and process a file synchronously in the foreground:
```bash
./build/ingestor --ingest-direct "/path/to/document.pdf" target_collection "job_123" generic
```
