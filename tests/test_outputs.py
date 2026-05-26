import json
import re
import os

STATE_FILE = '/app/state.json'
LOG_FILE = '/app/agent_output.txt'

def test_hardware_configuration():
    assert os.path.exists(STATE_FILE), "FAIL: state.json not found."
    
    with open(STATE_FILE, 'r') as f:
        state = json.load(f)

    assert state.get("OFSX") == 5, f"OFSX Expected 0x05, Got {state.get('OFSX')}"
    assert state.get("OFSY") == 252, f"OFSY Expected 0xFC, Got {state.get('OFSY')}"
    assert state.get("OFSZ") == 2, f"OFSZ Expected 0x02, Got {state.get('OFSZ')}"
    assert state.get("BW_RATE") == 12, f"BW_RATE Expected 0x0C, Got {state.get('BW_RATE')}"
    assert state.get("DATA_FORMAT") == 2, f"DATA_FORMAT Expected 0x02, Got {state.get('DATA_FORMAT')}"
    assert state.get("POWER_CTL") == 8, f"POWER_CTL Expected 0x08, Got {state.get('POWER_CTL')}"
    assert state.get("THRESH_ACT") == 24, f"THRESH_ACT Expected 0x18, Got {state.get('THRESH_ACT')}"
    assert state.get("INT_ENABLE") in (16, 18), f"INT_ENABLE Expected 0x10 or 0x12, Got {state.get('INT_ENABLE')}"

    # Fixed the syntax error here:
    assert state.get("FIFO_CTL") == 204, f"FIFO_CTL Expected 0xCC, Got {state.get('FIFO_CTL')}"

def test_math_and_stdout():
    assert os.path.exists(LOG_FILE), "FAIL: agent_output.txt not found."
    assert os.path.exists(STATE_FILE), "FAIL: state.json not found."
    
    with open(STATE_FILE, 'r') as f:
        state = json.load(f)
        
    expected_x = state.get("EXPECTED_X")
    expected_y = state.get("EXPECTED_Y")
    expected_z = state.get("EXPECTED_Z")

    assert expected_x is not None, "FAIL: Expected X missing from state.json"
    
    with open(LOG_FILE, 'r') as f:
        stdout = f.read()

    pattern = r"X:\s*([+-]?\d+\.\d+)\s*g,\s*Y:\s*([+-]?\d+\.\d+)\s*g,\s*Z:\s*([+-]?\d+\.\d+)\s*g"
    match = re.search(pattern, stdout)
    
    assert match is not None, f"FAIL: Could not find the correctly formatted output string. Stdout was:\n{stdout}"

    agent_x = float(match.group(1))
    agent_y = float(match.group(2))
    agent_z = float(match.group(3))

    TOLERANCE = 0.02 

    assert abs(agent_x - expected_x) <= TOLERANCE, f"X mismatch. Expected {expected_x:.2f}, Got {agent_x}"
    assert abs(agent_y - expected_y) <= TOLERANCE, f"Y mismatch. Expected {expected_y:.2f}, Got {agent_y}"
    assert abs(agent_z - expected_z) <= TOLERANCE, f"Z mismatch. Expected {expected_z:.2f}, Got {agent_z}"