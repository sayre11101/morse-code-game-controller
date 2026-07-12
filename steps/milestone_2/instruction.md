# Milestone 2

Implement this milestone in /app/main.c.

This milestone builds on the last milestone by locating the Morse Code Key and operator in a moving vehicle. During the test, the operator of the Morse Code key will be traveling in a car moving at a speed from 0 to 20 meters/second around turns and up and down hills with a minimum radius of 30 meters. The car has a maximum acceleration from 0-20 meters/second of 5 seconds. The car can brake without skidding at 0.9g on the surface of the road given the conditions. The road may be banked up to 15 degrees during turns. Because the road can be banked, the downward motion of the acceleration when the key is depressed and released will be split between the y and z axes. Left and right turns will be in the y axis and acceleration and braking will be in the x axis.

One major difference between M2 and M1 is that the accelerometer has a maximum absolute value of 10.0g. Any values below -10.0g are clipped to -10.0g. Any values higher than +10g are clipped to 10.0g. The timing and all other parameters for unit "dot" timing and the travel times for the key are the same as in milestone 1.

This milestone evaluates message decoding accuracy. After the key rests in the UP position for 5 seconds, print the decoded message to stdout terminated with '\n' and print nothing else. The messages have the same length and character restrictions as in milestone 1. The produced text must match the expected message exactly.
