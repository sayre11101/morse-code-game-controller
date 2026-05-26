#!/bin/bash
# tests/test.sh

# Ensure we are in the Harbor root
cd /app || exit 1

mkdir -p /logs/verifier

# Compile flat application in the root (Files are already here via Dockerfile!)
echo "Building Zephyr application..."
west build -b native_sim .
if [ $? -ne 0 ]; then
    echo "FAIL: Application failed to compile."
    echo 0 > /logs/verifier/reward.txt
    exit 1
fi

# Execute with Timeout (Outputs will land in /app/)
echo "Executing Zephyr binary..."
timeout 5 ./build/zephyr/zephyr.exe > agent_output.txt

printf "\nAgent output\n"
cat agent_output.txt
printf "\n------------------\n"
printf "\nAgent json\n"
cat state.json
printf "\n---------------\n"
cat agent_output.txt
printf "\n---------------\n"


EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ] && [ $EXIT_CODE -ne 124 ]; then
    echo "FAIL: Runtime Error (Code $EXIT_CODE)."
    echo 0 > /logs/verifier/reward.txt
    exit 1
fi

ulimit -n 1024

# Run tests strictly offline using the pre-warmed uv cache
uvx \
  --offline \
  -p 3.12 \
  -w pytest==8.4.1 \
  -w pytest-json-ctrf==0.3.5 \
  pytest --ctrf /logs/verifier/ctrf.json /tests/test_outputs.py -rA

# Produce reward file (REQUIRED)
if [ $? -eq 0 ]; then
  echo 1 > /logs/verifier/reward.txt
else
  echo 0 > /logs/verifier/reward.txt
fi
