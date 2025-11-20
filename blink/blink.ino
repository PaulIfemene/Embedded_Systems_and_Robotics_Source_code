#include <HardwareSerial.h>

HardwareSerial mySerial(2); // UART2

void setup() {
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, 16, 17); // RX=16, TX=17
  Serial.println("\n--- AS608 Basic Serial Test ---");
  Serial.println("If you see any data after touching the sensor, it's connected correctly.");
}

void loop() {
  // Data from sensor → PC
  if (mySerial.available()) {
    Serial.write(mySerial.read());
  }

  // Data from PC → sensor
  if (Serial.available()) {
    mySerial.write(Serial.read());
  }
}



#include <HardwareSerial.h>

HardwareSerial mySerial(2); // UART2

void setup() {
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, 16, 17); // RX=16, TX=17
  Serial.println("\n--- AS608 Basic Serial Test ---");
  Serial.println("If you see any data after touching the sensor, it's connected correctly.");
}

void loop() {
  // Data from sensor → PC
  if (mySerial.available()) {
    Serial.write(mySerial.read());
  }

  // Data from PC → sensor
  if (Serial.available()) {
    mySerial.write(Serial.read());
  }
}




#include <HardwareSerial.h>

HardwareSerial mySerial(2); // UART2

void setup() {
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, 16, 17); // RX=16, TX=17
  Serial.println("\n--- AS608 Basic Serial Test ---");
  Serial.println("If you see any data after touching the sensor, it's connected correctly.");
}

void loop() {
  // Data from sensor → PC
  if (mySerial.available()) {
    Serial.write(mySerial.read());
  }

  // Data from PC → sensor
  if (Serial.available()) {
    mySerial.write(Serial.read());
  }
}
