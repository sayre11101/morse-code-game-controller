#!/bin/bash
set -uo pipefail

mkdir -p /logs/verifier
mkdir -p /logs/agentsrc
mkdir -p /tests/build

echo "92" > /tests/seed.txt

# Number of possible words
NUM_WORDS=8
echo "$NUM_WORDS" > /tests/num_words.txt

# Compile the files
cd /app
# Compile binary to /tests/build/app.out compiling directly against the tests mock
gcc -Wall -Wextra -O0 -g -I/app -o /tests/build/app.out /app/main.c /tests/mock_sensor.c -lm
COMPILE_RC=$?

if [ "$COMPILE_RC" -ne 0 ]; then
  echo "ERROR: Compilation failed"
  echo 0 > /logs/verifier/reward.txt
  exit 0
fi

timestamp=$(date +"%Y%m%d-%H%M%S")
mainname="main.c-${timestamp}.c"
cp /app/main.c "/logs/verifier/${mainname}"
mockname="mock_sensor.c-${timestamp}.c"
if [ -f /app/mock_sensor.c ]; then
  cp /app/mock_sensor.c "/logs/verifier/${mockname}"
fi
rm -f /app/index.txt

set +e
python3 -m pytest \
    -o cache_dir=/tmp/pytest_cache \
    --ctrf /logs/verifier/ctrf.json \
    /tests/test_m1.py -rA
PYTEST_RC=$?

# delete so that agent in M2 does not see this.
rm -f /tests/build/index.txt
rm -rf /tests/build

test "$PYTEST_RC" -eq 0
RC=$?
if [ "$RC" -eq 0 ]; then
  echo 1 > /logs/verifier/reward.txt
else
  echo 0 > /logs/verifier/reward.txt
fi
