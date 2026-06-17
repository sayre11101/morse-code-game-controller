import subprocess
import pytest
import re

# --- Build the binary first ---
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

# Run the binary once and share the output across all register tests
@pytest.fixture(scope="module")
def zephyr_output(build_zephyr): # <-- Link added here so it waits for the build!
    try:
        result = subprocess.run(
            ['build/zephyr/zephyr.exe'], 
            capture_output=True, 
            text=True, 
            timeout=120
        )
        return result.stdout
    except subprocess.TimeoutExpired:
        pytest.fail("FAIL: The program timed out. Ensure there are no infinite loops.")
    except FileNotFoundError:
        pytest.fail("FAIL: Could not find the compiled binary.")

# Expected register values based on ADXL345 datasheet
EXPECTED_REGISTERS = {
    0x1E: 0x05,  # X offset
    0x1F: 0xFC,  # Y offset (-4)
    0x20: 0x02,  # Z offset
    0x24: 0x18,  # THRESH_ACT (1.5g)
    0x2C: 0x0C,  # BW_RATE (400Hz)
    0x31: 0x00,  # DATA_FORMAT (+/-2g 10-bit)
    0x38: 0xCC,  # FIFO_CTL (Trigger, INT1, 12 samples)
    0x2D: 0x08   # POWER_CTL (Measurement mode)
}

@pytest.mark.parametrize("reg_int, expected_val_int", EXPECTED_REGISTERS.items())
def test_register_configuration(zephyr_output, reg_int, expected_val_int):
    # Strip the '0x' and prepare the hex string for the regex (e.g., '1e')
    reg_hex = hex(reg_int)[2:]
    
    # REGEX: Matches "0x1E -> 0x05", "1e: 5", etc.
    pattern = rf"\b(?:0x)?0*{reg_hex}\b[\s\W]+(0x[0-9a-f]+|\d+)\b"
    match = re.search(pattern, zephyr_output, re.IGNORECASE)
    
    # Assert the write operation was actually found
    assert match is not None, f"FAIL: No valid write operation found for register 0x{reg_hex.upper()}"
    
    # Parse the captured value
    actual_val_str = match.group(1)
    actual_val_int = int(actual_val_str, 0)
    
    # Assert the value matches our expectation
    assert actual_val_int == expected_val_int, (
        f"FAIL: Register 0x{reg_hex.upper()} set to 0x{actual_val_int:02X}, expected 0x{expected_val_int:02X}"
    )