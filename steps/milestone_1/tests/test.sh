#!/bin/bash
set -uo pipefail

mkdir -p /logs/verifier

# Word bank with 20+ character phrases containing spaces
WORDS=(
  "SOS SEND HELP NOW PLEASE"
  "RADIO WAVES ARE COOL"
  "MORSE CODE IS VERY OLD"
  "PHYSICS AND SOFTWARE"
  "ACCELEROMETER READS G"
  "SOLVE THE PUZZLE FAST"
  "THE CAR IS DRIVING NOW"
  "WAVES TRAVEL FAST FAR"
)

echo "42" > /tests/seed.txt

# Get length of words array
NUM_WORDS=${#WORDS[@]}
echo "$NUM_WORDS" > /tests/num_words.txt

# Compile the files
cd /app
# Compile binary to /app/app.out compiling directly against the tests mock
gcc -Wall -Wextra -O0 -g -I/app -o /app/app.out /app/main.c /tests/mock_sensor.c -lm
COMPILE_RC=$?

if [ "$COMPILE_RC" -ne 0 ]; then
  echo "ERROR: Compilation failed"
  echo 0 > /logs/verifier/reward.txt
fi

rm -f /app/index.txt

set +e
python3 -m pytest \
    -o cache_dir=/tmp/pytest_cache \
    --ctrf /logs/verifier/ctrf.json \
    /tests/test_m1.py -rA
PYTEST_RC=$?

test "$PYTEST_RC" -eq 0
RC=$?
if [ "$RC" -eq 0 ]; then
  echo 1 > /logs/verifier/reward.txt
else
  echo 0 > /logs/verifier/reward.txt
fi
