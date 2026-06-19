import subprocess
import pytest
import re
import os


BIN_PATH = "build/zephyr/zephyr.exe"


def _binary_diagnostics():
    file_out = "<unavailable>"
    uname_out = "<unavailable>"
    try:
        file_res = subprocess.run(
            ["file", "-Lb", BIN_PATH],
            capture_output=True,
            text=True,
            timeout=10,
        )
        if file_res.returncode == 0:
            file_out = file_res.stdout.strip()
        else:
            file_out = (file_res.stderr or "").strip() or "file command failed"
    except (FileNotFoundError, subprocess.SubprocessError):
        pass

    try:
        uname_res = subprocess.run(
            ["uname", "-m"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        if uname_res.returncode == 0:
            uname_out = uname_res.stdout.strip()
        else:
            uname_out = (uname_res.stderr or "").strip() or "uname command failed"
    except (FileNotFoundError, subprocess.SubprocessError):
        pass

    return {
        "exists": os.path.exists(BIN_PATH),
        "executable": os.access(BIN_PATH, os.X_OK),
        "file_out": file_out,
        "uname_out": uname_out,
    }


def _validate_binary_or_fail():
    diag = _binary_diagnostics()
    if not diag["exists"]:
        pytest.fail(f"FAIL: Expected compiled binary at {BIN_PATH} but it was not found.")
    if not diag["executable"]:
        pytest.fail(
            f"FAIL: Compiled binary exists but is not executable ({BIN_PATH}). file: {diag['file_out']}"
        )
    if "ELF" not in diag["file_out"]:
        pytest.fail(
            f"FAIL: Compiled binary is not ELF ({BIN_PATH}). file: {diag['file_out']}"
        )

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
            "native_sim/native/64",
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
    _validate_binary_or_fail()
    try:
        result = subprocess.run(
            [BIN_PATH], 
            capture_output=True, 
            text=True, 
            timeout=120
        )
        return result.stdout
    except subprocess.TimeoutExpired:
        pytest.fail("FAIL: The program timed out. Ensure there are no infinite loops.")
    except FileNotFoundError:
        pytest.fail("FAIL: Could not find the compiled binary.")
    except OSError as exc:
        diag = _binary_diagnostics()
        pytest.fail(
            "FAIL: Could not execute compiled binary (OS-level execution error).\n"
            f"Details: {exc}\n"
            f"Binary: {BIN_PATH}\n"
            f"file: {diag['file_out']}\n"
            f"uname -m: {diag['uname_out']}"
        )

# Expected register values based on ADXL345 datasheet
EXPECTED_REGISTERS = {
    0x1E: 0x05,  # X offset
    0x1F: 0xFC,  # Y offset (-4)
    0x20: 0x02,  # Z offset
    0x24: 0x18,  # THRESH_ACT (1.5g)
    0x27: 0x40,  # ACT_INACT_CTL (activity enabled on X only)
    0x2C: 0x0C,  # BW_RATE (400Hz)
    0x31: 0x00,  # DATA_FORMAT (+/-2g 10-bit)
    0x38: 0xCC,  # FIFO_CTL (Trigger, INT1, 12 samples)
    0x2D: 0x08   # POWER_CTL (Measurement mode)
}

@pytest.mark.parametrize("reg_int, expected_val_int", EXPECTED_REGISTERS.items())
def test_register_configuration(zephyr_output, reg_int, expected_val_int):
    # Match only the mock's canonical transfer log format.
    reg_hex = f"{reg_int:02X}"
    pattern = rf"^Write 0x{reg_hex} -> (0x[0-9A-F]{{2}})$"
    match = re.search(pattern, zephyr_output, re.MULTILINE)
    
    # Assert the write operation was actually found
    assert match is not None, f"FAIL: No valid write operation found for register 0x{reg_hex.upper()}"
    
    # Parse the captured value
    actual_val_str = match.group(1)
    actual_val_int = int(actual_val_str, 0)
    
    # Assert the value matches our expectation
    assert actual_val_int == expected_val_int, (
        f"FAIL: Register 0x{reg_hex.upper()} set to 0x{actual_val_int:02X}, expected 0x{expected_val_int:02X}"
    )


def test_minimum_write_activity(zephyr_output):
    writes = re.findall(r"^Write 0x[0-9A-F]{2} -> 0x[0-9A-F]{2}$", zephyr_output, re.MULTILINE)
    assert len(writes) >= 9, "FAIL: Expected at least 9 I2C register writes from the setup sequence."