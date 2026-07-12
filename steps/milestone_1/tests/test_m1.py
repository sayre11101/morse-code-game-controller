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
    return result.stdout


class TestMilestone1:
    def test_output_exists(self, app_output):
        """Verify that the program produced some output."""
        candidates = _extract_decoded_message_candidates(app_output)
        msg_line = candidates[-1] if candidates else None
        assert msg_line is not None and len(msg_line) > 0, (
            "FAIL: No decoded message output found."
        )

    def test_output_is_text(self, app_output):
        """Verify output is strictly one single line in stdout with alpha uppercase characters and spaces ending with \n."""
        assert bool(re.fullmatch(r'[A-Z ]+\n', app_output)), (
            "FAIL: Output must be exactly one single line ending with \\n containing only UPPERCASE alphabetic characters and spaces. "
            f"Got: {repr(app_output)}"
        )

    def test_output_length_reasonable(self, app_output):
        """Verify decoded message has reasonable length (not empty, not > 100 chars)."""
        candidates = _extract_decoded_message_candidates(app_output)
        assert len(candidates) == 1, "FAIL: Expected exactly one decoded-message line."
        msg_line = candidates[0]
        msg_len = len(msg_line)
        assert 13 <= msg_len <= 50, (
            f"FAIL: Output length {msg_len} is outside expected reasonable bounds format length (13-50 chars). "
            f"Output: '{msg_line}'"
        )

    def test_decoded_message_matches_expected_word(self, app_output):
        """Verify decoded output matches the verifier-selected target phrase exactly."""
        candidates = _extract_decoded_message_candidates(app_output)
        assert len(candidates) == 1, "FAIL: Expected exactly one decoded-message line."
        msg_line = candidates[0]

        expected = "SOS SEND HELP NOW PLEASE"
        try:
            with open("/tests/secret_word.txt", "r") as f:
                expected = f.read().strip()
        except FileNotFoundError:
            pass

        assert re.fullmatch(r"[A-Z ]{13,50}", expected), (
            f"FAIL: Verifier expected word is invalid: '{expected}'."
        )

        actual = msg_line.strip().upper()
        assert actual == expected, (
            f"FAIL: Decoded message mismatch. Expected '{expected}', got '{actual}'."
        )
