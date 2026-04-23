//use gps.c and gps.h to get data from module to serial2 on esp32
#include <gps.h>
#include <HardwareSerial.h>

//rx2=16, tx2=17
HardwareSerial MySerial(2);

void setup(){
Serial.begin(115200);
MySerial.begin(9600, SERIAL_8N1, 16, 17);
Serial.println("UART2 Initialized");
}
