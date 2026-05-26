import sys
import argparse
import re
import os

def check_for_cheating(file_path):
    """Scans the agent's C code to ensure they didn't just hardcode the final answer."""
    if not os.path.exists(file_path):
        print(f"Helper Error: Could not find {file_path}")
        sys.exit(1)

    with open(file_path, 'r') as f:
        content = f.read()

    # Did they try to hardcode the exact float values instead of printing variables?
    if re.search(r'printf\([^%]*1\.00[^%]*\)', content):
        print("FAIL: Agent attempted to hardcode the final X gravity value.")
        sys.exit(1)
        
    # Did they forget to include the I2C API entirely?
    if "<zephyr/drivers/i2c.h>" not in content:
        print("FAIL: Agent did not include the required Zephyr I2C headers.")
        sys.exit(1)

    print("Pre-flight check passed: No obvious cheating detected.")
    sys.exit(0)

def main():
    parser = argparse.ArgumentParser(description="Harbor utility helper")
    parser.add_argument("action", choices=["preflight"], help="The helper method to run")
    parser.add_argument("--file", required=True, help="Target file to analyze")
    
    args = parser.parse_args()

    if args.action == "preflight":
        check_for_cheating(args.file)

if __name__ == "__main__":
    main()