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
            ".",  # Assumes you are running this from the directory containing CMakeLists.txt
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
            ["build/zephyr/zephyr.exe"], capture_output=True, text=True, timeout=10
        )
        return result.stdout
    except subprocess.TimeoutExpired:
        pytest.fail(
            "FAIL: The program timed out. Check for infinite loops or unhandled interrupts."
        )
    except FileNotFoundError:
        pytest.fail("FAIL: Could not find the compiled binary.")


def test_standby_sequence_trap(zephyr_output):
    """Verifies the agent cleared the measure bit before writing to FIFO/Threshold registers."""
    # Extract all I2C writes chronologically
    write_pattern = r"\b(?:0x)?0*([0-9a-fA-F]{1,2})\b[\s\W]+(0x[0-9a-fA-F]+|\d+)\b"
    writes = re.findall(write_pattern, zephyr_output, re.IGNORECASE)

    parsed_writes = []
    for reg_str, val_str in writes:
        try:
            reg = int(reg_str, 16)
            val = int(val_str, 0)
            parsed_writes.append((reg, val))
        except ValueError:
            continue

    standby_achieved = False
    reconfig_found = False

    for reg, val in parsed_writes:
        if reg == 0x2D:
            # Check if measurement bit (D3 / 0x08) is cleared
            standby_achieved = (val & 0x08) == 0

        elif reg == 0x24:
            # 1.2g threshold = 1.2 / 0.0625 = 19.2 -> 19 (0x13)
            if val == 0x13:
                reconfig_found = True
                assert standby_achieved, (
                    "FAIL: Agent wrote to THRESH_ACT without placing the device in standby first."
                )

        elif reg == 0x38:
            # Trigger mode, INT1, 8 samples = 0xC8
            if val == 0xC8:
                assert standby_achieved, (
                    "FAIL: Agent wrote to FIFO_CTL without placing the device in standby first."
                )

    assert reconfig_found, (
        "FAIL: Did not find the new 1.2g threshold configuration (0x13 written to 0x24)."
    )


def test_output_formatting(zephyr_output):
    """Verifies the correct number of formatted output lines were printed."""
    # Looking for 'X', 'Y', or 'Z' followed by floats in a single line
    line_pattern = r"(?i)x[\s\W]+[+-]?\d*\.?\d+[\s\W]+y[\s\W]+[+-]?\d*\.?\d+[\s\W]+z[\s\W]+[+-]?\d*\.?\d+"
    sample_lines = re.findall(line_pattern, zephyr_output)

    # We expect 2 lines from the first phase (12th sample twice) + 8 lines from the new phase
    assert len(sample_lines) >= 10, (
        f"FAIL: Expected at least 10 lines of formatted output total, found {len(sample_lines)}."
    )
