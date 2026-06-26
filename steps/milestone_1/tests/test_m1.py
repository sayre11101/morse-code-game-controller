import subprocess
import pytest
import re
import os


BIN_PATH = "build/zephyr/zephyr.exe"
SOURCE_PATH = "main.c"


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


def _source_snapshot():
    try:
        with open(SOURCE_PATH, "r", encoding="utf-8", errors="replace") as source_file:
            return source_file.read()
    except OSError as exc:
        return f"<unable to read {SOURCE_PATH}: {exc}>"


def _with_source(message):
    return f"{message}\n\n{SOURCE_PATH}:\n{_source_snapshot()}"


def _validate_binary_or_fail():
    diag = _binary_diagnostics()
    if not diag["exists"]:
        pytest.fail(
            f"FAIL: Expected compiled binary at {BIN_PATH} but it was not found.\n"
            f"{SOURCE_PATH}:\n{_source_snapshot()}"
        )
    if not diag["executable"]:
        pytest.fail(
            f"FAIL: Compiled binary exists but is not executable ({BIN_PATH}). file: {diag['file_out']}\n"
            f"{SOURCE_PATH}:\n{_source_snapshot()}"
        )
    if "ELF" not in diag["file_out"]:
        pytest.fail(
            f"FAIL: Compiled binary is not ELF ({BIN_PATH}). file: {diag['file_out']}\n"
            f"{SOURCE_PATH}:\n{_source_snapshot()}"
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
        f"FAIL: Zephyr Build Failed:\n{build_res.stderr}\n\nSTDOUT:\n{build_res.stdout}\n\n"
        f"{SOURCE_PATH}:\n{_source_snapshot()}"
    )
    yield
# -----------------------------------

# Run the binary once and share output across tests.
@pytest.fixture(scope="module")
def zephyr_output(build_zephyr):
    _validate_binary_or_fail()
    try:
        result = subprocess.run(
            [BIN_PATH], 
            capture_output=True, 
            text=True, 
            timeout=300
        )
        return result.stdout
    except subprocess.TimeoutExpired:
        pytest.fail(
            "FAIL: The program timed out after 300s. Ensure there are no infinite loops.\n"
            f"{SOURCE_PATH}:\n{_source_snapshot()}"
        )
    except FileNotFoundError:
        pytest.fail(
            "FAIL: Could not find the compiled binary.\n"
            f"{SOURCE_PATH}:\n{_source_snapshot()}"
        )
    except OSError as exc:
        diag = _binary_diagnostics()
        pytest.fail(
            "FAIL: Could not execute compiled binary (OS-level execution error).\n"
            f"Details: {exc}\n"
            f"Binary: {BIN_PATH}\n"
            f"file: {diag['file_out']}\n"
            f"uname -m: {diag['uname_out']}\n\n"
            f"{SOURCE_PATH}:\n{_source_snapshot()}"
        )

STATE_LINE = re.compile(r"^(TRAVEL_DOWN|DOWN|TRAVEL_UP|UP)\s+([+-]?\d+(?:\.\d+)?)$")


def _parse_timeline(output_text):
    state_entries = []
    end_seen = False

    for raw_line in output_text.splitlines():
        line = raw_line.strip()
        if not line:
            continue

        if line == "END":
            end_seen = True
            continue

        match = STATE_LINE.match(line)
        if match is None:
            continue

        state_entries.append((match.group(1), float(match.group(2))))

    return state_entries, end_seen


def _drop_optional_leading_up(entries):
    if not entries:
        return entries
    if entries[0][0] == "UP" and len(entries) >= 2 and entries[1][0] == "TRAVEL_DOWN":
        return entries[1:]
    return entries


def test_state_lines_and_end_marker(zephyr_output):
    """Verify at least one state-duration line is printed and output ends with END."""
    entries, end_seen = _parse_timeline(zephyr_output)
    assert end_seen, _with_source("FAIL: Missing END marker in output.")
    assert entries, _with_source("FAIL: No state-duration lines were printed.")


def test_sensor_interaction_detected(zephyr_output):
    """Verify output shows ADXL345 interaction through expected read traces."""
    dat_reads = len(re.findall(r"^Read 0x32$", zephyr_output, re.MULTILINE))
    fifo_polls = len(re.findall(r"^Read 0x39$", zephyr_output, re.MULTILINE))
    int_reads = len(re.findall(r"^Read 0x30$", zephyr_output, re.MULTILINE))

    assert (dat_reads + fifo_polls + int_reads) > 0, (
        _with_source(
            "FAIL: No evidence of ADXL345 sensor reads was found. Expected DATAX0, FIFO_STATUS, or INT_SOURCE access."
        )
    )


def test_forbidden_fifo_or_link_configuration(zephyr_output):
    """Disallow FIFO mode and LINK configuration for this milestone."""
    write_re = re.compile(r"^Write 0x([0-9A-Fa-f]{2}) -> 0x([0-9A-Fa-f]{2})$", re.MULTILINE)

    for reg_hex, val_hex in write_re.findall(zephyr_output):
        reg = int(reg_hex, 16)
        val = int(val_hex, 16)

        # ADXL345 POWER_CTL (0x2D): LINK bit is bit 5.
        if reg == 0x2D and (val & 0x20):
            pytest.fail(
                _with_source(
                    "FAIL: LINK configuration is not allowed in milestone 1. "
                    "Clear POWER_CTL bit 5 (LINK)."
                )
            )

        # ADXL345 FIFO_CTL (0x38): mode bits [7:6] must remain Bypass (00).
        if reg == 0x38 and ((val >> 6) & 0x03) != 0:
            pytest.fail(
                _with_source(
                    "FAIL: FIFO mode is not allowed in milestone 1. "
                    "Keep FIFO_CTL mode bits [7:6] = 00 (Bypass)."
                )
            )


def test_required_state_sequence(zephyr_output):
    """Verify state transitions follow the expected two-cycle motion order."""
    entries, _ = _parse_timeline(zephyr_output)
    entries = _drop_optional_leading_up(entries)

    expected_prefix = [
        "TRAVEL_DOWN",
        "DOWN",
        "TRAVEL_UP",
        "UP",
        "TRAVEL_DOWN",
        "DOWN",
        "TRAVEL_UP",
        "UP",
    ]

    got_states = [name for name, _ in entries]
    assert len(got_states) >= len(expected_prefix), (
        _with_source(
            "FAIL: Incomplete state timeline. Expected at least two press/release cycles before final hold."
        )
    )
    assert got_states[: len(expected_prefix)] == expected_prefix, (
        _with_source(
            "FAIL: State order does not match expected key motion sequence.\n"
            f"Expected prefix: {expected_prefix}\n"
            f"Actual states:   {got_states}"
        )
    )


def test_duration_ranges(zephyr_output):
    """Verify travel/hold durations fall within expected timing windows."""
    entries, _ = _parse_timeline(zephyr_output)
    entries = _drop_optional_leading_up(entries)

    travel = [dur for state, dur in entries if state in ("TRAVEL_DOWN", "TRAVEL_UP")]
    down_holds = [dur for state, dur in entries if state == "DOWN"]
    up_holds = [dur for state, dur in entries if state == "UP"]

    assert len(travel) >= 4, "FAIL: Expected at least four travel segments across two cycles."
    assert len(travel) >= 4, _with_source("FAIL: Expected at least four travel segments across two cycles.")
    assert len(down_holds) >= 2, _with_source("FAIL: Expected at least two DOWN hold segments.")
    assert len(up_holds) >= 2, _with_source("FAIL: Expected at least two UP hold segments.")

    for dur in travel[:4]:
        assert 0.002 <= dur <= 0.020, (
            _with_source(
                f"FAIL: Travel duration {dur:.3f}s is outside the allowed 0.002..0.020s range."
            )
        )

    for dur in down_holds[:2]:
        assert 0.20 <= dur <= 1.20, (
            _with_source(
                f"FAIL: DOWN duration {dur:.3f}s is outside expected hold window."
            )
        )

    assert up_holds[-1] >= 4.90, (
        _with_source(
            f"FAIL: Final UP hold should be about 5s before END, got {up_holds[-1]:.3f}s."
        )
    )