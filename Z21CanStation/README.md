# Z21CanStation

Provides a z21 server with additional ZCan Interface to handle communication with Roco 10818 feedback decoder or similar modules.
CAN Tx - Gpio4
CAN Rx - Gpio5 

Provides DCC control using DccInterfaceMaster Library.
Bridge pwm pin 1 GPIO25
Bridge pwm pin 2 GPIO26
Bridge enable pin 1 GPIO27
Bridge enable pin 2 GPIO16
Current Sense Pin GPIO34
=> Designed for be used with ADS712 5A. Output pin has resistor devider with two 3.3kOhm. Results in 0A voltage of 0,94V.
