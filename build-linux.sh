#!/usr/bin/env bash
set -euo pipefail

IMAGE="comp442-linux-x86_64"
CONTAINER="comp442-extract"
OUT_DIR="bin-linux-x86_64"

echo "Building Docker image for linux/amd64..."
docker build --platform linux/amd64 -t "$IMAGE" .

echo "Extracting binaries..."
docker rm -f "$CONTAINER" 2>/dev/null || true
docker create --name "$CONTAINER" "$IMAGE"
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
docker cp "$CONTAINER":/project/bin/. "$OUT_DIR"/
docker rm "$CONTAINER"

echo "Done. Binaries in $OUT_DIR/:"
ls -lh "$OUT_DIR"
