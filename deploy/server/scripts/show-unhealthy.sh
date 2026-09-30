#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
env_file="$repo_root/build/server/local/compose.env"
[[ -f "$env_file" ]] || exit 0
project=$(awk -F= '$1 == "COMPOSE_PROJECT_NAME" { print $2 }' "$env_file")
[[ -n "$project" ]] || exit 0

echo "Local server containers:"
docker ps -a --filter "label=com.docker.compose.project=$project" \
    --format 'table {{.Names}}\t{{.Status}}'

while IFS= read -r container; do
    [[ -n "$container" ]] || continue
    echo "Recent logs from unhealthy container $container:"
    docker logs --tail 25 "$container" 2>&1 || true
done < <(docker ps -a --filter "label=com.docker.compose.project=$project" \
    --filter health=unhealthy --format '{{.Names}}')
