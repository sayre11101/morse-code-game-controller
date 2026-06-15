import json
import re
import os

# --- UPDATED PATHS FOR THE SANDBOX ---
STATE_FILE = "/tmp/eval/state.json"
LOG_FILE = "/tmp/eval/agent_output.txt"


def test_hardware_configuration():
        """Verify I2C register state: offsets, FIFO reconfiguration to 8
        samples at 1.2g threshold, and that standby protocol was followed 
        before modifying control registers."""

        assert os.path.exists(STATE_FILE), "FAIL: state.json not found."

        with open(STATE_FILE, "r") as f:
            state = json.load(f)

        # Core Trap Verification
        assert state.get("STANDBY_VIOLATION") == 0, (
            "FATAL: Standby Violation! Agent modified THRESH_ACT or FIFO_CTL without placing POWER_CTL into Standby Mode (0x00) first."
        )
        assert state.get("STAGE") == 3, (
            f"FAIL: Agent stuck at Stage {state.get('STAGE')}. Did it forget to read INT_SOURCE to clear the hardware interrupt between batches?"
        )

        # --- NEW: Base Configuration Verification (Reviewer Fixes) ---
        assert state.get("BW_RATE") == 12, (
            f"BW_RATE Expected 0x0C (400Hz), Got {state.get('BW_RATE')}"
        )
        assert state.get("DATA_FORMAT") == 0, (
            f"DATA_FORMAT Expected 0x00 (+/- 2g), Got {state.get('DATA_FORMAT')}"
        )
        assert state.get("INITIAL_THRESH_ACT") == 24, (
            f"Initial THRESH_ACT Expected 0x18 (1.5g), Got {state.get('INITIAL_THRESH_ACT')}"
        )
        # -------------------------------------------------------------

        # Re-Configuration Verification (Stage 3)
        assert state.get("THRESH_ACT") == 19, (
            f"Final THRESH_ACT Expected 0x13 (1.2g), Got {state.get('THRESH_ACT')}"
        )
        assert state.get("FIFO_CTL") == 200, (
            f"FIFO_CTL Expected 0xC8 (8 samples), Got {state.get('FIFO_CTL')}"
        )

        # Offset Math Verification
        assert state.get("OFSX") == 5, f"OFSX Expected 5 (+5), Got {state.get('OFSX')}"
        assert state.get("OFSY") == 252, (
            f"OFSY Expected 252 (-4 in 8-bit Two's Complement), Got {state.get('OFSY')}"
        )
        assert state.get("OFSZ") == 2, f"OFSZ Expected 2 (+2), Got {state.get('OFSZ')}"


def test_math_and_stdout():
    """Verify stdout contains 10+ formatted gravity lines matching
    expected float values across all 3 trigger stages within 0.02g tolerance."""

    assert os.path.exists(LOG_FILE), "FAIL: agent_output.txt not found."
    assert os.path.exists(STATE_FILE), "FAIL: state.json not found."

    with open(STATE_FILE, "r") as f:
        state = json.load(f)

    stage1 = state.get("EXPECTED_STAGE1_X")
    stage2 = state.get("EXPECTED_STAGE2_X")
    stage3 = state.get("EXPECTED_STAGE3_X")

    assert stage1 and stage2 and stage3, "FAIL: Expected arrays missing from state.json"

    with open(LOG_FILE, "r") as f:
        stdout = f.read()

    pattern = r"X:\s*([+-]?\d+\.\d+)\s*g,\s*Y:\s*([+-]?\d+\.\d+)\s*g,\s*Z:\s*([+-]?\d+\.\d+)\s*g"
    matches = list(re.finditer(pattern, stdout))

    # We expect 1 from Stage 1, 1 from Stage 2, and 8 from Stage 3 = 10 total
    assert len(matches) >= 10, (
        f"FAIL: Expected at least 10 printed samples. Found {len(matches)}."
    )

    TOLERANCE = 0.02

    # 1. Grade Stage 1 (The 12th sample is at index 11)
    agent_s1_x = float(matches[0].group(1))
    expected_s1_x = stage1[11]
    assert abs(agent_s1_x - expected_s1_x) <= TOLERANCE, (
        f"Stage 1 X mismatch. Expected {expected_s1_x:.2f}, Got {agent_s1_x}"
    )

    # 2. Grade Stage 2 (The 12th sample is at index 11)
    agent_s2_x = float(matches[1].group(1))
    expected_s2_x = stage2[11]
    assert abs(agent_s2_x - expected_s2_x) <= TOLERANCE, (
        f"Stage 2 X mismatch. Expected {expected_s2_x:.2f}, Got {agent_s2_x}"
    )

    # 3. Grade Stage 3 (All 8 samples, starting at match index 2)
    for i in range(8):
        agent_x = float(matches[2 + i].group(1))
        expected_x = stage3[i]
        assert abs(agent_x - expected_x) <= TOLERANCE, (
            f"Stage 3 X mismatch at Sample {i + 1}. Expected {expected_x:.2f}, Got {agent_x}"
        )
