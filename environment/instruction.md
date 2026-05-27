# ADXL345 accelerometer

This project is to read the manual adxl345.pdf (supplied in the container) to determine the correct i2c commands to send to the chip.
The environment is a Zephyr native_sim with an i2c driver. Send i2c commands to i2c0 to:

- Set the X offset to +5, the Y offset to -4 and the Z offset to +2
- Configure the sensor for a 400Hz sample rate
- Configure the FIFO to trigger and store 12 samples on the INT1 pin
- Configure activity to detect a 1.5g threshold
- Configure the data format for +/- 8g range - 10 bit resolution (read the manual!!)
- Poll the FIFO status until a trigger
- Once triggered, download 12 samples
- Output the 12th sample converted to floating point in units of "g" (gravity)
- Print the final gravity values to standard output in this format:
  - X: [val] g, Y: [val] g, Z: [val] g on one line
- Wait for another trigger, download the 12 samples and print out the 12th one like before
- Change the threshold to 1.2g and the number of samples to 8
- Wait for another trigger, download the 8 samples and print all samples on 8 lines in the same format as the 1 sample from before
- CAUTION - you must read the manual to determine the correct sequence of writing registers including control registers!!
