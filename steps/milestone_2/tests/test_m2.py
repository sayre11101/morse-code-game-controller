import subprocess
import pytest
import re
import os


BIN_PATH = "build/zephyr/zephyr.exe"
SOURCE_PATH = "main.c"


def _binary_diagnostics():
    """Return file-type and architecture diagnostics for the compiled binary."""
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
    """Read and return the current contents of the agent source file."""
    try:
        with open(SOURCE_PATH, "r", encoding="utf-8", errors="replace") as source_file:
            return source_file.read()
    except OSError as exc:
        return f"<unable to read {SOURCE_PATH}: {exc}>"


def _validate_binary_or_fail():
    """Fail the test immediately if the compiled binary is missing, non-executable, or not ELF."""
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


def _normalize_timeout_output(raw_output):
    """Decode raw timeout output bytes or None into a plain string."""
    if raw_output is None:
        return ""
    if isinstance(raw_output, bytes):
        return raw_output.decode("utf-8", errors="replace")
    return str(raw_output)


def _tail_lines(text, line_count=40):
    """Return the last line_count lines of text as a single string."""
    lines = text.splitlines()
    if len(lines) <= line_count:
        return "\n".join(lines)
    return "\n".join(lines[-line_count:])


def _extract_decoded_message_candidates(output_text):
    """Return candidate decoded-message lines from mixed Zephyr/mock output."""
    lines = [line.strip() for line in output_text.splitlines() if line.strip()]

    # Drop known non-message log formats emitted by Zephyr and the mock sensor.
    excluded_prefixes = (
        "[",
        "uart connected to pseudotty",
        "*** Booting Zephyr",
        "Write 0x",
        "Read 0x",
    )

    return [
        line for line in lines
        if line.startswith(excluded_prefixes) is False
    ]

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

# Run the compiled binary once for all tests in this module
@pytest.fixture(scope="module")
def zephyr_output():
    """Run the compiled binary once and return its stdout for all tests to share."""
    _validate_binary_or_fail()
    try:
        result = subprocess.run(
            [BIN_PATH],
            capture_output=True,
            text=True,
            timeout=120,
        )
        return result.stdout
    except subprocess.TimeoutExpired as exc:
        partial_output = _normalize_timeout_output(exc.stdout or exc.output)
        pytest.fail(
            "FAIL: Program timed out after 120s.\n"
            "Expected: Morse-decoded message as plain text output.\n"
            f"Captured stdout tail:\n{_tail_lines(partial_output)}\n\n"
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

def test_output_exists(zephyr_output):
    """Verify that the program produced some output."""
    candidates = _extract_decoded_message_candidates(zephyr_output)
    msg_line = candidates[-1] if candidates else None
    assert msg_line is not None and len(msg_line) > 0, (
        "FAIL: No decoded message output found."
    )


def test_output_is_text(zephyr_output):
    """Verify output is strict decoded text (A-Z only) and exactly one user line."""
    candidates = _extract_decoded_message_candidates(zephyr_output)
    assert len(candidates) == 1, (
        "FAIL: Expected exactly one decoded-message line and no extra user-facing output. "
        f"Found {len(candidates)} candidate lines: {candidates}"
    )
    msg_line = candidates[0]
    assert bool(re.fullmatch(r'[A-Za-z]+', msg_line)), (
        f"FAIL: Output '{msg_line}' contains non-letter characters. "
        "Expected only alphabetic characters in the decoded message line."
    )


def test_output_length_reasonable(zephyr_output):
    """Verify decoded message has reasonable length (not empty, not > 100 chars)."""
    candidates = _extract_decoded_message_candidates(zephyr_output)
    assert len(candidates) == 1, "FAIL: Expected exactly one decoded-message line."
    msg_line = candidates[0]
    msg_len = len(msg_line)
    assert 3 <= msg_len <= 6, (
        f"FAIL: Output length {msg_len} is outside expected range (3-6 chars). "
        f"Output: '{msg_line}'"
    )


def test_decoded_message_matches_expected_word(zephyr_output):
    """Verify decoded output matches the verifier-selected target word exactly."""
    candidates = _extract_decoded_message_candidates(zephyr_output)
    assert len(candidates) == 1, "FAIL: Expected exactly one decoded-message line."
    msg_line = candidates[0]

    expected = os.environ.get("MORSE_TARGET_WORD", "").strip().upper()
    assert re.fullmatch(r"[A-Z]{3,6}", expected), (
        f"FAIL: Verifier expected word is invalid: '{expected}'."
    )

    actual = msg_line.strip().upper()
    assert actual == expected, (
        f"FAIL: Decoded message mismatch. Expected '{expected}', got '{actual}'."
    )
