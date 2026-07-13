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


def get_num_words():
    try:
        with open("/tests/num_words.txt") as f:
            return int(f.read().strip())
    except Exception:
        return 8

@pytest.fixture(scope="function", params=range(get_num_words()))
def app_output():
    """Run the compiled binary and return stdout."""
    binary_path = "/app/app.out"
    if not os.path.exists(binary_path):
        pytest.fail(f"Could not find compiled binary at {binary_path}")
        
    result = subprocess.run([binary_path], capture_output=True, text=True, timeout=10)
    
    assert result.returncode == 0, f"FAIL: Binary exited with non-zero return code {result.returncode}"
    return result.stdout


class TestMilestone1:
    def test_output_exists(self, app_output):
        """Verify that the program produced some output."""
        candidates = _extract_decoded_message_candidates(app_output)
        msg_line = candidates[-1] if candidates else None
        assert msg_line is not None and len(msg_line) > 0, (
            "FAIL: No decoded message output found."
        )
        
    def test_get_sample_stru_calls(self, app_output):
        """Verify that main loop requested at least roughly appropriate mock readings."""
        try:
            with open("/app/get_sample_stru_calls.txt", "r") as f:
                content = f.read().strip().split()
                actual_calls = int(content[0])
                expected_min_calls = int(content[1]) if len(content) > 1 else 20
        except Exception:
            pytest.fail("FAIL: Agent did not call get_sample_stru correctly or log file is unreadable.")
        
        assert actual_calls >= expected_min_calls, (
            f"FAIL: The user application bypassed the IO polling entirely or sampled too slowly. Expected at least {expected_min_calls} calls, got {actual_calls}."
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

        WORDS = [
            "SOS SEND HELP NOW PLEASE",
            "RADIO WAVES ARE COOL",
            "MORSE CODE IS VERY OLD",
            "PHYSICS AND SOFTWARE",
            "ACCELEROMETER READS G",
            "SOLVE THE PUZZLE FAST",
            "THE CAR IS DRIVING NOW",
            "WAVES TRAVEL FAST FAR"
        ]

        expected = "SOS SEND HELP NOW PLEASE"
        try:
            with open("/app/index.txt", "r") as f:
                # the C mock advances index + 1 before using, so to match we fetch the current index - 1
                curr_idx = int(f.read().strip())
                # if idx was originally 0, C mock set index.txt to 1
                word_index = (curr_idx - 1) % len(WORDS)
                if word_index < 0:
                    word_index = 0
                expected = WORDS[word_index]
        except Exception:
            pass

        assert re.fullmatch(r"[A-Z ]{13,50}", expected), (
            f"FAIL: Verifier expected word is invalid: '{expected}'."
        )

        actual = msg_line.strip().upper()
        assert actual == expected, (
            f"FAIL: Decoded message mismatch. Expected '{expected}', got '{actual}'."
        )
