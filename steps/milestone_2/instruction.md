# Milestone 2

Implement `adxl345_read_samples()` in `adxl345_read.c` using ADXL345 FIFO trigger behavior from the datasheet.

Use the ADXL345 datasheet at `/app/adxl345.pdf` to determine the relevant register names, register addresses, bit configurations, timing requirements, and trigger-reset/order-of-operations details required for this read sequence.

Requirements:

- Poll FIFO status until the trigger condition is reached.
- Check the datasheet for a required delay after trigger before reading
- Read 12 FIFO entries from the data registers, treating each FIFO entry as one complete 6-byte X/Y/Z sample.
- Each FIFO entry must be read with its own 6-byte DATAX0 transaction.
- Extract the last sample in that burst and convert raw values to gravity units.
- Print one line exactly in this format: `X: [val] g, Y: [val] g, Z: [val] g`.
- Clear interrupt source state.
- Make sure that the sensor is ready to receive another trigger. Look at the datasheet!

`main.c` calls this function twice. Both cycles must produce correctly formatted output and extract the correct sample from the FIFO buffer.
