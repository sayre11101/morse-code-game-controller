import pytest
import re
import os
import subprocess


def _extract_decoded_message_candidates(output_text):
    """Return candidate decoded-message lines from mixed App/mock output."""
    lines = [line.strip() for line in output_text.splitlines() if line.strip()]

    excluded_prefixes = (
        "[",
        "uart connected to pseudotty",
        "Write 0x",
        "Read 0x",
        "Starting",
    )

    return [
        line for line in lines
        if line.startswith(excluded_prefixes) is False
    ]


@pytest.fixture(scope="module")
def app_output():
    """Run the compiled binary and return stdout."""
    binary_path = "/app/app.out"
    if not os.path.exists(binary_path):
        pytest.fail(f"Could not find compiled binary at {binary_path}")
        
    result = subprocess.run([binary_path], capture_output=True, text=True, timeout=10)
    if result.returncode != 0 or not result.stdout:
        print(f"DEBUG STDERR: {result.stderr}")
        print(f"DEBUG STDOUT: {result.stdout}")
    return result.stdout


def test_output_exists(app_output):
    """Verify that the program produced some output."""
    candidates = _extract_decoded_message_candidates(app_output)
    msg_line = candidates[-1] if candidates else None
    assert msg_line is not None and len(msg_line) > 0, (
        "FAIL: No decoded message output found."
    )


def test_output_is_text(app_output):
    """Verify output is strict decoded text (A-Z only) and exactly one user line."""
    candidates = _extract_decoded_message_candidates(app_output)
    assert len(candidates) == 1, (
        "FAIL: Expected exactly one decoded-message line and no extra user-facing output. "
        f"Found {len(candidates)} candidate lines: {candidates}"
    )
    msg_line = candidates[0]
    assert bool(re.fullmatch(r'[A-Za-z]+', msg_line)), (
        f"FAIL: Output '{msg_line}' contains non-letter characters. "
        "Expected only alphabetic characters in the decoded message line."
    )


def test_output_length_reasonable(app_output):
    """Verify decoded message has reasonable length (not empty, not > 100 chars)."""
    candidates = _extract_decoded_message_candidates(app_output)
    assert len(candidates) == 1, "FAIL: Expected exactly one decoded-message line."
    msg_line = candidates[0]
    msg_len = len(msg_line)
    assert 3 <= msg_len <= 6, (
        f"FAIL: Output length {msg_len} is outside expected range (3-6 chars). "
        f"Output: '{msg_line}'"
    )


def test_decoded_message_matches_expected_word(app_output):
    """Verify decoded output matches the verifier-selected target word exactly."""
    candidates = _extract_decoded_message_candidates(app_output)
    assert len(candidates) == 1, "FAIL: Expected exactly one decoded-message line."
    msg_line = candidates[0]

    word_bank = ["SOS", "RADIO", "WAVE", "MORSE", "CODE", "PULSE", "SIGNAL", "LIGHT"]

    # Read index from verifier file
    try:
        with open("/tests/secret_word.txt", "r") as f:
            index_str = f.read().strip()
    except FileNotFoundError:
        index_str = "0"
    
    index = int(index_str) if index_str.isdigit() else 0
    if index < 0 or index >= len(word_bank):
        index = 0
        
    expected = word_bank[index]

    assert re.fullmatch(r"[A-Z]{3,6}", expected), (
        f"FAIL: Verifier expected word is invalid: '{expected}'."
    )

    actual = msg_line.strip().upper()
    assert actual == expected, (
        f"FAIL: Decoded message mismatch. Expected '{expected}', got '{actual}'."
    )
