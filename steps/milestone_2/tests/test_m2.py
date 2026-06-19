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


def _normalize_timeout_output(raw_output):
    if raw_output is None:
        return ""
    if isinstance(raw_output, bytes):
        return raw_output.decode("utf-8", errors="replace")
    return str(raw_output)


def _tail_lines(text, line_count=40):
    lines = text.splitlines()
    if len(lines) <= line_count:
        return "\n".join(lines)
    return "\n".join(lines[-line_count:])

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

# Run the compiled binary once for all tests in this module
@pytest.fixture(scope="module")
def zephyr_output():
    _validate_binary_or_fail()
    try:
        result = subprocess.run(
            [BIN_PATH], 
            capture_output=True, 
            text=True, 
            timeout=120
        )
        return result.stdout
    except subprocess.TimeoutExpired as exc:
        partial_output = _normalize_timeout_output(exc.stdout or exc.output)
        fifo_polls = partial_output.count("Read 0x39")
        int_clears = partial_output.count("Read 0x30 (INT_SOURCE cleared)")
        formatted_lines = re.findall(
            r"X:\s*([+-]?\d+\.\d+)\s*g,\s*Y:\s*([+-]?\d+\.\d+)\s*g,\s*Z:\s*([+-]?\d+\.\d+)\s*g",
            partial_output,
        )

        if fifo_polls > 0 and len(formatted_lines) == 0:
            likely_cause = (
                "Likely cause: the code is polling FIFO but never reaches the trigger/read path "
                "(for example, waiting on the wrong FIFO condition)."
            )
        else:
            likely_cause = "Likely cause: infinite polling loop or missing trigger reset path."

        pytest.fail(
            "FAIL: Program timed out after 120s.\n"
            f"Diagnostics: FIFO polls={fifo_polls}, INT_SOURCE clears={int_clears}, "
            f"formatted output lines={len(formatted_lines)}.\n"
            f"{likely_cause}\n\n"
            "Captured stdout tail:\n"
            f"{_tail_lines(partial_output)}"
        )
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

def test_fifo_polling_loop(zephyr_output):
    """Verifies that the agent is actively polling the FIFO_STATUS register."""
    output_lower = zephyr_output.lower()
    poll_detected = output_lower.count("read 0x39") >= 4
    assert poll_detected, "FAIL: No evidence of the FIFO status register being polled."

def test_interrupt_clear_between_cycles(zephyr_output):
    clears = len(re.findall(r"^Read 0x30", zephyr_output, re.MULTILINE))
    assert clears >= 2, "FAIL: Expected INT_SOURCE to be read in both trigger cycles."


def test_fifo_reset_between_cycles(zephyr_output):
    """Verifies trigger handling is re-armed between cycles via FIFO_CTL reset sequence."""
    reset_positions = [m.start() for m in re.finditer(r"^Write 0x38 -> 0x00$", zephyr_output, re.MULTILINE)]
    trigger_positions = [m.start() for m in re.finditer(r"^Write 0x38 -> 0xCC$", zephyr_output, re.MULTILINE)]

    assert len(reset_positions) >= 1, (
        "FAIL: Expected at least one FIFO_CTL reset write (0x38 -> 0x00) between trigger cycles."
    )

    assert len(trigger_positions) >= 2, (
        "FAIL: Expected FIFO trigger mode (0x38 -> 0xCC) to be written at setup and re-armed before the second cycle."
    )

    first_reset = reset_positions[0]
    later_triggers = [pos for pos in trigger_positions if pos > first_reset]
    assert later_triggers, (
        "FAIL: FIFO trigger mode was not re-enabled after FIFO_CTL reset; expected 0xCC write after 0x00."
    )


def test_two_cycle_output_format_and_values(zephyr_output):
    """Verify line format and plausible gravity-unit values for both trigger cycles."""
    line_pattern = r"X:\s*([+-]?\d+\.\d+)\s*g,\s*Y:\s*([+-]?\d+\.\d+)\s*g,\s*Z:\s*([+-]?\d+\.\d+)\s*g"
    matches = re.findall(line_pattern, zephyr_output)

    assert len(matches) >= 2, "FAIL: Expected two formatted output lines (two trigger cycles)."

    non_trivial_axes = 0
    for idx, (x_str, y_str, z_str) in enumerate(matches[:2], start=1):
        x_val = float(x_str)
        y_val = float(y_str)
        z_val = float(z_str)

        # Values should be in g-units and within a plausible accelerometer output range.
        assert abs(x_val) <= 4.0, f"FAIL: Cycle {idx} X axis value {x_val}g is outside plausible g-range."
        assert abs(y_val) <= 4.0, f"FAIL: Cycle {idx} Y axis value {y_val}g is outside plausible g-range."
        assert abs(z_val) <= 4.0, f"FAIL: Cycle {idx} Z axis value {z_val}g is outside plausible g-range."

        non_trivial_axes += int(abs(x_val) > 0.05)
        non_trivial_axes += int(abs(y_val) > 0.05)
        non_trivial_axes += int(abs(z_val) > 0.05)

    assert non_trivial_axes >= 2, (
        "FAIL: Output appears trivial/constant; expected meaningful converted sample values in both cycles."
    )