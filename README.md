# Arduino-RC-Plane-CG-Balancer
Arduino RC Plane CG Balancer

Three Load Cells are required. I used 5kg.
Each Load Cell requires calibration and the calibration value shall be input in the Arduino-RC-Plane-CG-Balancer Code.

It is necessary to enter the calibration value as shown below.
-----------------------------------------------------------------------
LoadCell_1.setCalFactor(218.64); // user set calibration value (float)
LoadCell_2.setCalFactor(215.65); // user set calibration value (float)
LoadCell_3.setCalFactor(218.36); // user set calibration value (float)
-----------------------------------------------------------------------
