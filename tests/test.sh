#!/bin/bash
# tests/test.sh

mkdir -p /logs/verifier
mkdir -p /tmp/eval

echo "Staging build environment in /tmp/eval..."

# 1. Grab configs and emulator
cp /tests/CMakeLists.txt /tmp/eval/
cp /tests/app.overlay /tmp/eval/
cp /tests/prj.conf /tmp/eval/
cp /tests/mock_sensor.c /tmp/eval/

# 2. Grab the agent's code
if [ ! -f /app/main.c ]; then
    echo "FAIL: Agent did not generate /app/main.c"
    echo 0 > /logs/verifier/reward.txt
    exit 1
fi
cp /app/main.c /tmp/eval/

# Navigate to the sandbox
cd /tmp/eval

# --- PRE-FLIGHT CHEAT DETECTION ---
echo "Running pre-flight cheat detection..."
python3 /tests/helper.py preflight --file /tmp/eval/main.c
if [ $? -ne 0 ]; then
    echo "FAIL: Pre-flight check failed. Agent attempted to cheat or missed required headers."
    echo 0 > /logs/verifier/reward.txt
    exit 1
fi
echo "Pre-flight check passed. Proceeding to build..."
# ----------------------------------

# 3. Compile
echo "Building Zephyr application..."
west build -b native_sim .
if [ $? -ne 0 ]; then
    echo "FAIL: Application failed to compile."
    echo 0 > /logs/verifier/reward.txt
    exit 1
fi

# 4. Execute
echo "Executing Zephyr binary..."
timeout 5 ./build/zephyr/zephyr.exe > agent_output.txt

# CRITICAL FIX: Capture the exit code immediately after execution!
EXIT_CODE=$?

# 5. Diagnostic Logging (Matched exactly to your old format)
printf "\nAgent output\n"
cat agent_output.txt
printf "\n------------------\n"
printf "\nAgent json\n"
if [ -f state.json ]; then
    cat state.json
else
    echo "NO STATE.JSON GENERATED"
fi
printf "\n---------------\n"
cat agent_output.txt
printf "\n---------------\n"

# 6. Check execution success
if [ $EXIT_CODE -ne 0 ] && [ $EXIT_CODE -ne 124 ]; then
    echo "FAIL: Runtime Error (Code $EXIT_CODE)."
    echo 0 > /logs/verifier/reward.txt
    exit 1
fi

ulimit -n 1024

# 7. Run tests offline
uvx \
  --offline \
  -p 3.12 \
  -w pytest==8.4.1 \
  -w pytest-json-ctrf==0.3.5 \
  pytest --ctrf /logs/verifier/ctrf.json /tests/test_outputs.py -rA

# 8. Reward File
if [ $? -eq 0 ]; then
  echo 1 > /logs/verifier/reward.txt
else
  echo 0 > /logs/verifier/reward.txt
fi