#include <HardwareSerial.h>

#define TX_PIN 4
#define RX_PIN 5

HardwareSerial CamSerial(1); 

// Protocol constants
#define CHANNEL   0x60
#define CMD_LEFT  0xCC
#define CMD_RIGHT 0xC3
#define CMD_UP    0xCA
#define CMD_DOWN  0xC5

void setup() {
  Serial.begin(115200);
  
  // Initialize the dummy RX pin, TX is handled dynamically in the loop
  CamSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  
  delay(3000); 
  Serial.println("Starting Full Axis Test...");
}

void loop() {
  // Cycle through all 4 directions
  moveCamera(CMD_LEFT, "Left");
  moveCamera(CMD_RIGHT, "Right");
  moveCamera(CMD_UP, "Up");
  moveCamera(CMD_DOWN, "Down");
  
  Serial.println("Restarting sequence in 3 seconds...");
  Serial.println("-------------------------");
  delay(3000); 
}

void moveCamera(byte command, const char* dirName) {
  Serial.print("Moving: ");
  Serial.println(dirName);
  
  // 1. Re-attach the TX pin to the Serial controller
  pinMode(TX_PIN, OUTPUT);
  CamSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); 

  // Pack the 3 bytes
  uint8_t packet[3] = {CHANNEL, command, CHANNEL};
  
  unsigned long startTime = millis();
  
  // 2. Spam the hardware buffer continuously for 1 second (ZERO delay)
  while (millis() - startTime < 1000) {
    CamSerial.write(packet, 3);
  }
  
  // Wait a tiny fraction of a second to ensure the final bytes clear the buffer
  delay(10); 
  
  // 3. Detach the Serial controller and set the pin to High-Impedance (INPUT)
  CamSerial.end(); 
  pinMode(TX_PIN, INPUT); 
  
  Serial.println("TX Pin Released to High-Z");
  
  // Wait 1 second before firing the next directional command
  delay(1000); 
}
