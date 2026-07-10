# Milestone 2

Implement this milestone in /app/main.c.

This milestone builds on the last milestone by locating the Morse Code Key and operator in a moving vehicle. During the test, the operator of the Morse Code key will be traveling in a car moving at a speed from 0 to 20 meters/second around turns and up and down hills with a minimum radius of 30 meters. The car has a maximum acceleration from 0-20 meters/second of 5 seconds. The car can brake without skidding at 0.9g on the surface of the road given the conditions. The road will not be banked, meaning that the downward direction of the key will always be the Z axis. Left and right turns will be in the y axis and acceleration and braking will be in the x axis.

The timing and all other parameters for unit "dot" timing and the travel times for the key are the same as in milestone 1.

This milestone evaluates message decoding accuracy. After the key rests in the UP position for 5 seconds, print the decoded message to stdout terminate with '\n' The produced text must match the expected message exactly.
