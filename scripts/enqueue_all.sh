#!/bin/bash

# Ensure Redis is running
if ! pgrep redis-server > /dev/null; then
    echo "Starting redis-server..."
    /opt/homebrew/opt/redis/bin/redis-server --daemonize yes || brew services start redis
fi

# Iterate over all PDFs
find files -type f -name "*.pdf" | while read -r file; do
  if [[ "$file" == *"IEEE"* ]]; then
    CONFIG="ieee"
  else
    CONFIG="generic"
  fi

  JOB_ID=$(basename "$file" | tr -dc 'a-zA-Z0-9' | cut -c 1-32)

  echo "Enqueuing $file with config $CONFIG"
  ./build/ingestor --enqueue "$file" arbitrary_collection "job_$JOB_ID" "$CONFIG"
done
