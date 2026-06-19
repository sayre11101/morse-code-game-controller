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
    _validate_binary_or_fail()
    try:
        result = subprocess.run(
            [BIN_PATH], capture_output=True, text=True, timeout=10
        )
        return result.stdout
    except subprocess.TimeoutExpired as exc:
        partial_output = _normalize_timeout_output(exc.stdout or exc.output)
        fifo_polls = partial_output.count("Read 0x39")
        standby_writes = partial_output.count("Write 0x2D -> 0x00")
        threshold_writes = partial_output.count("Write 0x24 -> 0x13")
        fifo_reconfig_writes = partial_output.count("Write 0x38 -> 0xC8")

        pytest.fail(
            "FAIL: Program timed out after 10s.\n"
            f"Diagnostics: FIFO polls={fifo_polls}, POWER_CTL standby writes={standby_writes}, "
            f"THRESH_ACT(0x13) writes={threshold_writes}, FIFO_CTL(0xC8) writes={fifo_reconfig_writes}.\n"
            "Likely cause: code stuck in polling or never completed milestone-3 reconfiguration flow.\n\n"
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
    """Verifies milestone-3 output formatting and that 8 post-reconfiguration samples are printed."""
    line_pattern = r"X:\s*([+-]?\d+\.\d+)\s*g,\s*Y:\s*([+-]?\d+\.\d+)\s*g,\s*Z:\s*([+-]?\d+\.\d+)\s*g"
    sample_lines = re.findall(line_pattern, zephyr_output)

    assert len(sample_lines) >= 8, (
        f"FAIL: Expected at least 8 lines of formatted output for the post-reconfiguration burst, found {len(sample_lines)}."
    )

    last_eight = sample_lines[-8:]
    peak_event_detected = False
    for idx, (x_str, y_str, z_str) in enumerate(last_eight, start=1):
        x_val = float(x_str)
        y_val = float(y_str)
        z_val = float(z_str)

        assert abs(x_val) <= 4.0, f"FAIL: Final-burst sample {idx} X value {x_val}g is outside plausible g-range."
        assert abs(y_val) <= 4.0, f"FAIL: Final-burst sample {idx} Y value {y_val}g is outside plausible g-range."
        assert abs(z_val) <= 4.0, f"FAIL: Final-burst sample {idx} Z value {z_val}g is outside plausible g-range."

        if max(abs(x_val), abs(y_val), abs(z_val)) > 0.25:
            peak_event_detected = True

    assert peak_event_detected, (
        "FAIL: Final-burst output appears trivial/constant; expected at least one clearly non-zero sample in the 8-sample burst."
    )
