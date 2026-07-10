# Milestone 1

Implement this milestone in /app/main.c.

In this challenge you will observe the motion of a Morse Code key by reading the acceleration of the key in 3 dimensions in units of g (9.8 m/s/s). By observing the key through a defined API in mock_sensor.h, you must decode a message that is being sent by a mock operator.

The essence of this challenge is to read and understand the Morse Code from a pdf files, morse-code-sheet.pdf and to monitor the operation of a Morse Code key from acceleration data from a mock sensor on the key with a given API. From the Morse Code sheet you should be able to understand the timing required to receive messages in Morse Code. In this challenge you will have access to acceleration readings from a Morse Code Key using the API defined in mock_sensor.h. You will call the mock_sensor API to get the sample number and the 3 acceleration values. You need to calculate the acceleration of the key in the z axis using the size of the gap as 2.0mm and the transit time either up or down in the range of 3-6 msec. When the key reaches the DOWN position it stops within 0.1 msec. When the key reaches the UP position it stops within 0.2 msec. The polarity of the z axis acceleration is negative for downward (in the same direction as gravity).

You will call the mock sensor API to get readings from mock accelerometer. This will return true if a new reading if available. The structure defined in mock_sensor.h will be filled in. You can inspect the sample rate and the sample number and the x,y, and z acceleration. x and y are perpendicular to the motion of the key. The z acceleration is in the upward and downward direction traveled by the key.

Note that in the data sheet morse-code-sheet.pdf the times are in units of the time of the dot. The speed of the Morse Code keying in the test will set that unit time between 100 and 250 msec. This time will not vary over the message. It will be set at the beginning and will be consistent throughout the message. The travel time of the key will still be in the range of 3-6 msec irrespective of the speed of transmission.

The message will end when the key remains in the UP position (the mock operator is not pressing on the key) for a duration of 5 seconds. At that point you should output the decoded message followed by a '\n' to stdout.
