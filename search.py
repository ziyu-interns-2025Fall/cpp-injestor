import urllib.request
import json
import sys

def get_embedding(text):
    url = "http://localhost:11434/api/embeddings"
    data = json.dumps({"model": "nomic-embed-text", "prompt": text}).encode('utf-8')
    req = urllib.request.Request(url, data=data, headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(req) as response:
        return json.loads(response.read().decode('utf-8'))['embedding']

def search_milvus(vector, top_k=3):
    url = "http://127.0.0.1:19530/v2/vectordb/entities/search"
    payload = {
        "collectionName": "ingestion_staging",
        "data": [vector],
        "limit": top_k,
        "outputFields": ["content"]
    }
    data = json.dumps(payload).encode('utf-8')
    req = urllib.request.Request(url, data=data, headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(req) as response:
        return json.loads(response.read().decode('utf-8'))

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python search.py <query>")
        sys.exit(1)
    
    query = sys.argv[1]
    print(f"Embedding query: '{query}'...")
    emb = get_embedding(query)
    
    print("Searching Milvus...")
    results = search_milvus(emb)
    
    if results.get("code") == 0:
        data = results.get("data", [])
        print(f"Found {len(data)} results:\n")
        for i, res in enumerate(data):
            print(f"--- Result {i+1} (Distance: {res.get('distance', 'N/A')}) ---")
            print(res.get("content", "")[:500] + "...\n")
    else:
        print("Error:", results)
