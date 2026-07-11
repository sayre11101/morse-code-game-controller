#!/bin/bash
set -uo pipefail

mkdir -p /logs/verifier

# Generate a random word index and write to verifier-only file
echo $((RANDOM % 8)) > /tests/secret_word.txt

# Compile the files
cp /tests/mock_sensor.c /app/mock_sensor.c
cd /app
# Compile binary to /app/app.out
gcc -Wall -Wextra -O0 -g -I. -o app.out main.c mock_sensor.c -lm
COMPILE_RC=$?

if [ "$COMPILE_RC" -ne 0 ]; then
  echo "ERROR: Compilation failed"
  echo 0 > /logs/verifier/reward.txt
  exit 1
fi

set +e
python3 -m pytest \
    -o cache_dir=/tmp/pytest_cache \
    --ctrf /logs/verifier/ctrf.json \
    /tests/test_m1.py -rA
PYTEST_RC=$?

# Restore baseline starter main.c to avoid carrying milestone solution forward.
if [ -f /tests/main.c.baseline ]; then
  if ! cp /tests/main.c.baseline /app/main.c; then
    echo "ERROR: Failed to restore baseline main.c for next milestone"
    PYTEST_RC=1
  fi
else
  if ! cp /app/main.c.orig /app/main.c; then
    echo "ERROR: Failed to restore baseline main.c for next milestone"
    PYTEST_RC=1
  fi
fi

test "$PYTEST_RC" -eq 0
RC=$?
if [ "$RC" -eq 0 ]; then
  echo 1 > /logs/verifier/reward.txt
else
  echo 0 > /logs/verifier/reward.txt
fi
