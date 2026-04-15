#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"
clang -O2 -march=native -Wall -Wextra -Werror -std=c11 -o creation_kernel \
  creation_kernel.c hdc.c orchestrator.c facts_store.c || exit 1
echo 'Kompiloi: OK'
echo 'mikä on suomen pääkaupunki' | ./creation_kernel facts.json || true
echo '---'
cat test_queries.txt | ./creation_kernel facts.json
EC=$?
echo '=== BENCHMARK ==='
exit "$EC"
