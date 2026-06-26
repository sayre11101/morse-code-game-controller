#!/bin/bash
set -uo pipefail

# Verifier dependencies are installed in environment/Dockerfile.
# Add task-specific verifier-only python3 packages there, not here.

mkdir -p /logs/verifier

# Fail fast if the datasheet PDF was not copied into the container.
if [ ! -f /app/adxl345.pdf ]; then
  echo "ERROR: Missing required datasheet /app/adxl345.pdf"
  echo 0 > /logs/verifier/reward.txt
  exit 1
fi

cp /tests/mock_sensor.c /app/mock_sensor.c

set +e
python -m pytest \
    -o cache_dir=/tmp/pytest_cache \
    --ctrf /logs/verifier/ctrf.json \
    /tests/test_m1.py -rA
PYTEST_RC=$?

# Restore the dummy non-revealing mock for the next milestone
if ! cp /app/mock_sensor.c.orig /app/mock_sensor.c; then
  echo "ERROR: Failed to restore dummy mock for next milestone"
  PYTEST_RC=1
fi

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
