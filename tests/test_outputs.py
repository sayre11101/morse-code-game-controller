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

    # --- Base Configuration Verification ---
    assert state.get("BW_RATE") == 12, (
        f"BW_RATE Expected 0x0C (400Hz), Got {state.get('BW_RATE')}"
    )
    assert state.get("DATA_FORMAT") == 0, (
        f"DATA_FORMAT Expected 0x00 (+/- 2g), Got {state.get('DATA_FORMAT')}"
    )
    assert state.get("INITIAL_THRESH_ACT") == 24, (
        f"Initial THRESH_ACT Expected 0x18 (1.5g), Got {state.get('INITIAL_THRESH_ACT')}"
    )

    # --- NEW: QA Reviewer Fixes ---
    initial_fifo_ctl = state.get("INITIAL_FIFO_CTL")
    assert initial_fifo_ctl == 204, (
        f"Initial FIFO_CTL Expected 0xCC (trigger mode, 12 samples, INT1), Got {initial_fifo_ctl}"
    )

    act_inact_ctl = state.get("ACT_INACT_CTL", 0)
    assert (act_inact_ctl & 0x70) != 0, (
        f"ACT_INACT_CTL must enable at least one axis for activity detection. Got {act_inact_ctl}"
    )

    # --- Re-Configuration Verification (Stage 3) ---
    assert state.get("THRESH_ACT") == 19, (
        f"Final THRESH_ACT Expected 0x13 (1.2g), Got {state.get('THRESH_ACT')}"
    )
    assert state.get("FIFO_CTL") == 200, (
        f"FIFO_CTL Expected 0xC8 (8 samples), Got {state.get('FIFO_CTL')}"
    )

    # --- Offset Math Verification ---
    assert state.get("OFSX") == 5, f"OFSX Expected 5 (+5), Got {state.get('OFSX')}"
    assert state.get("OFSY") == 252, (
        f"OFSY Expected 252 (-4 in 8-bit Two's Complement), Got {state.get('OFSY')}"
    )
    assert state.get("OFSZ") == 2, f"OFSZ Expected 2 (+2), Got {state.get('OFSZ')}"
    # --- Polling Verification ---
    total_polls = state.get("TOTAL_FIFO_POLLS", 0)
    assert total_polls >= 6, (
        f"FAIL: Agent did not poll the FIFO_STATUS register properly. "
        f"Expected at least 6 polls across 3 stages, but only saw {total_polls}."
    )


def test_math_and_stdout():
    """Verify stdout contains 10+ formatted gravity lines matching
    expected float values across all 3 trigger stages within 0.02g tolerance."""

    assert os.path.exists(LOG_FILE), "FAIL: agent_output.txt not found."
    assert os.path.exists(STATE_FILE), "FAIL: state.json not found."

    with open(STATE_FILE, "r") as f:
        state = json.load(f)

    # Retrieve X, Y, and Z expectations for all stages
    stage1_x = state.get("EXPECTED_STAGE1_X")
    stage1_y = state.get("EXPECTED_STAGE1_Y")
    stage1_z = state.get("EXPECTED_STAGE1_Z")

    stage2_x = state.get("EXPECTED_STAGE2_X")
    stage2_y = state.get("EXPECTED_STAGE2_Y")
    stage2_z = state.get("EXPECTED_STAGE2_Z")

    stage3_x = state.get("EXPECTED_STAGE3_X")
    stage3_y = state.get("EXPECTED_STAGE3_Y")
    stage3_z = state.get("EXPECTED_STAGE3_Z")

    assert all([stage1_x, stage1_y, stage1_z, stage2_x, stage3_x]), "FAIL: Expected arrays missing from state.json"

    with open(LOG_FILE, "r") as f:
        stdout = f.read()

    pattern = r"X:\s*([+-]?\d+\.\d+)\s*g,\s*Y:\s*([+-]?\d+\.\d+)\s*g,\s*Z:\s*([+-]?\d+\.\d+)\s*g"
    matches = list(re.finditer(pattern, stdout))

    # We expect 1 from Stage 1, 1 from Stage 2, and 8 from Stage 3 = 10 total
    assert len(matches) >= 10, (
        f"FAIL: Expected at least 10 printed samples. Found {len(matches)}."
    )

    TOLERANCE = 0.02

    # --- 1. Grade Stage 1 (The 12th sample is at index 11) ---
    agent_s1_x, agent_s1_y, agent_s1_z = map(float, matches[0].groups())
    
    assert abs(agent_s1_x - stage1_x[11]) <= TOLERANCE, f"Stage 1 X mismatch. Expected {stage1_x[11]:.2f}, Got {agent_s1_x}"
    assert abs(agent_s1_y - stage1_y[11]) <= TOLERANCE, f"Stage 1 Y mismatch. Expected {stage1_y[11]:.2f}, Got {agent_s1_y}"
    assert abs(agent_s1_z - stage1_z[11]) <= TOLERANCE, f"Stage 1 Z mismatch. Expected {stage1_z[11]:.2f}, Got {agent_s1_z}"

    # --- 2. Grade Stage 2 (The 12th sample is at index 11) ---
    agent_s2_x, agent_s2_y, agent_s2_z = map(float, matches[1].groups())
    
    assert abs(agent_s2_x - stage2_x[11]) <= TOLERANCE, f"Stage 2 X mismatch. Expected {stage2_x[11]:.2f}, Got {agent_s2_x}"
    assert abs(agent_s2_y - stage2_y[11]) <= TOLERANCE, f"Stage 2 Y mismatch. Expected {stage2_y[11]:.2f}, Got {agent_s2_y}"
    assert abs(agent_s2_z - stage2_z[11]) <= TOLERANCE, f"Stage 2 Z mismatch. Expected {stage2_z[11]:.2f}, Got {agent_s2_z}"

    # --- 3. Grade Stage 3 (All 8 samples, starting at match index 2) ---
    for i in range(8):
        agent_x, agent_y, agent_z = map(float, matches[2 + i].groups())
        
        assert abs(agent_x - stage3_x[i]) <= TOLERANCE, f"Stage 3 X mismatch at Sample {i + 1}. Expected {stage3_x[i]:.2f}, Got {agent_x}"
        assert abs(agent_y - stage3_y[i]) <= TOLERANCE, f"Stage 3 Y mismatch at Sample {i + 1}. Expected {stage3_y[i]:.2f}, Got {agent_y}"
        assert abs(agent_z - stage3_z[i]) <= TOLERANCE, f"Stage 3 Z mismatch at Sample {i + 1}. Expected {stage3_z[i]:.2f}, Got {agent_z}"