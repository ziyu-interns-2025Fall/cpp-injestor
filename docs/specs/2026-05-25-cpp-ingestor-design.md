# C++ Ingestor Design Specification

## Overview
The `cpp-ingestor` is a high-performance C++ worker designed to replace the Python-based PDF processing layer in the `pyapi` system. It utilizes the Poppler library for rapid PDF text extraction, replicates the Markdown-based chunking logic, and pushes raw chunks to a Milvus staging collection. Coordination between `pyapi` and the `cpp-ingestor` is managed via Redis, ensuring a robust Producer-Consumer architecture.

## Architecture & Workflow

### 1. Job Coordination (Redis)
- **Role:** Message queue for distributing ingestion tasks.
- **Producer:** `pyapi` pushes tasks to a Redis List.
- **Consumer:** `cpp-ingestor` uses `BLPOP` to consume tasks efficiently.
- **Queue Name:** `ingestion:jobs`
- **CLI Submit Path:** `ingestor --enqueue <pdf_path> <target_collection> [job_id] [config_type]` pushes a user-submitted PDF job with `RPUSH`, allowing one or more worker processes to drain jobs with `BLPOP`.
- **Job Payload (JSON):**
  ```json
  {
    "job_id": "uuid-string",
    "pdf_path": "/absolute/path/to/document.pdf",
    "config_type": "ieee", 
    "target_collection": "destination_collection"
  }
  ```

### 2. PDF Processing Pipeline (C++)
- **Library:** `poppler-cpp`
- **Responsibilities:**
  - Open and read PDF files specified in the job payload.
  - Replicate the header/footer filtering rules defined in `pyapi` (`ieee` and `nfpa` configurations).
  - Extract text and format it into Markdown.
  - Implement chunking logic equivalent to `MarkdownHeaderTextSplitter` and `RecursiveCharacterTextSplitter` (chunk_size=1024, chunk_overlap=256).

### 3. Data Handoff (Milvus Staging)
- **Role:** Intermediary storage for raw chunks before embedding.
- **Library:** `milvus-sdk-cpp`
- **Collection Name:** `ingestion_staging`
- **Schema:**
  - `id`: Int64 (Primary Key, Auto-ID)
  - `job_id`: VarChar (Indexed for fast lookup)
  - `content`: VarChar (The chunked text)
  - `metadata_json`: VarChar (JSON string containing doc name, headers, char_count)
  - `status`: Int8 (0: Ready, 1: Processing, 2: Completed)
  - `target_collection`: VarChar (Final destination for vectors)
- **Behavior:** The `cpp-ingestor` writes processed chunks directly to this collection.

### 4. Embedding Consumer (`pyapi`)
- **Role:** Finalizing the RAG pipeline.
- **Behavior:** 
  - A background process in Python polls the `ingestion_staging` collection for records where `status = 0`.
  - Computes dense (`Ollama`) and sparse (`BGE-M3`) embeddings.
  - Inserts the final vectors into the `target_collection`.
  - Updates the staging record `status` to `2`.

## Dependencies
- **Compiler:** `clang` (Apple clang version 21.0.0)
- **Libraries:**
  - `poppler-cpp` (PDF parsing)
  - `hiredis` (Redis client)
  - `nlohmann/json` (JSON parsing)
  - `milvus-sdk-cpp` (Milvus integration)
- **Build System:** `CMake`

## Security & Constraints
- Ensure safe file path handling (validate paths before opening).
- Handle missing files or corrupted PDFs gracefully, updating a theoretical error queue or logging appropriately.
