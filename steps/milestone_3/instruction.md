# Milesone 3

You must integrate the configuration and reading functions to respond to changing conditions. Open the main.c file and implement the primary control loop. First, execute your initialization and the two initial 12-sample reads. Next, you must reconfigure the sensor on the fly to detect a new activity threshold of 1.2g and change the FIFO watermark to 8 samples. Wait for the new trigger, download the 8 samples, and print all 8 samples on 8 separate lines using the exact same formatting as the previous reads.

Important. The recommended procedure is to put the device into low power standby before updating thresholds or other control registers. The test harness checks that you do this in between tests where the control registers are changed.
