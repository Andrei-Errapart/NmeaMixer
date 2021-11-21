Replace VTG in gps signal with the one from compass.


J4 = USB0
UART0:	Input
	GPS 38400 baud, 10Hz.

J3 = USB1
UART1:	Input
	Compass 19200 baud, 40Hz.

J2 = USB2
UART2:	Output
	GPS 38400 baud, copy of input except VTG.

J1 = USB3
UART3:	Input/Output
	Control 38400 baud.

// HEHDT = heading north-seeking gyro

$--HDT,x.x,T*hh

x.x = heading degrees, true.
T = true
checksum

