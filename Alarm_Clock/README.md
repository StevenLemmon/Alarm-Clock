This is still an ongoing project that I am working on in my free time.

This part of the project uses a STM32F4 Discovery board, which uses a Cortex
M4 processor.  What it does is simply uses the STM32 RTC library to create an alarm 
for a certain time and sends a message using UART to an ESP8266 connect to 
pins PB6(TX) and PB7(RX).

Currently the alarm time is hardcoded and the board only starts to keep track 
of time on start up and always starts at the same time.
