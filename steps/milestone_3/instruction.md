# Milestone 3

Integrate setup and read behavior in `main.c` for a mid-run reconfiguration scenario.

Use the ADXL345 datasheet at `/app/adxl345.pdf` to determine the required register names, register addresses, bit configurations, timing requirements, and order of operations for reconfiguration, standby/measurement transitions, and FIFO reads.

Requirements:

- Run the existing initialization sequence and the two milestone-2 read cycles.
- Reconfigure for a new activity threshold of `1.2g` and FIFO trigger watermark of `8` samples.
- Before modifying THRESH_ACT or FIFO_CTL registers, follow the datasheet recommendation: place the device in low power standby mode.
- Wait for the new trigger condition, read 8 FIFO entries as complete 6-byte X/Y/Z samples, and print all 8 samples.
- After the trigger condition is reached check the datasheet for timing of the next I2C read
- Each FIFO entry must be read with its own 6-byte DATAX0 transaction. 
- Use the same per-line output format as milestone 2: `X: [val] g, Y: [val] g, Z: [val] g`.
