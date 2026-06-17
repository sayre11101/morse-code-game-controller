import subprocess
import pytest
import re
import math

@pytest.fixture(scope="module", autouse=True)
def build_zephyr():
    """Compiles the Zephyr application."""
    build_res = subprocess.run(
        [
            "west",
            "build",
            "-p",
            "always",
            "-b",
            "native_sim",
            "." # Assumes you are running this from the directory containing CMakeLists.txt
        ],
        capture_output=True,
        timeout=180,
        text=True,
    )
    assert build_res.returncode == 0, (
        f"FAIL: Zephyr Build Failed:\n{build_res.stderr}\n\nSTDOUT:\n{build_res.stdout}"
    )
    yield
# -----------------------------------

# Run the compiled binary once for all tests in this module
@pytest.fixture(scope="module")
def zephyr_output():
    try:
        result = subprocess.run(
            ['build/zephyr/zephyr.exe'], 
            capture_output=True, 
            text=True, 
            timeout=120
        )
        return result.stdout
    except subprocess.TimeoutExpired:
        pytest.fail("FAIL: The program timed out. The agent likely wrote an infinite polling loop that never exits.")
    except FileNotFoundError:
        pytest.fail("FAIL: Could not find the compiled binary.")

def test_fifo_polling_loop(zephyr_output):
    """Verifies that the agent is actively polling the FIFO_STATUS register."""
    output_lower = zephyr_output.lower()
    poll_detected = output_lower.count("read 0x39") >= 2 or output_lower.count("poll") > 0
    assert poll_detected, "FAIL: No evidence of the FIFO status register being polled."

# Expected gravity values based on the mock_sensor.c injection
EXPECTED_G = {
    'x': 0.99,
    'y': -0.50,
    'z': 1.99
}

@pytest.mark.parametrize("axis, expected_val", EXPECTED_G.items())
def test_math_extraction(zephyr_output, axis, expected_val):
    """Verifies the raw bytes are properly extracted, sign-extended, and converted to floats."""
    # Looks for 'x' (or 'y'/'z'), ignores spaces/punctuation, grabs the float
    pattern = rf"{axis}[\s=:,]+([+-]?\d*\.?\d+)"
    match = re.search(pattern, zephyr_output, re.IGNORECASE)
    
    assert match is not None, f"FAIL: Could not find a parsed floating point value for axis {axis.upper()} in the output."
    
    actual_val = float(match.group(1))
    
    # Use math.isclose to handle slight float rounding differences (e.g., 0.998 vs 0.99)
    assert math.isclose(actual_val, expected_val, abs_tol=0.05), (
        f"FAIL: {axis.upper()} axis value was {actual_val}g, expected ~{expected_val}g"
    )