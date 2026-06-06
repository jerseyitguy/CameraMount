#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>

// --- Hardware & Protocol Setup ---
#define TX_PIN 4
#define RX_PIN 5
HardwareSerial CamSerial(1); 

#define CHANNEL   0x60
#define CMD_LEFT  0xCC
#define CMD_RIGHT 0xC3
#define CMD_UP    0xCA
#define CMD_DOWN  0xC5

#define PAN_FAST  0xA8
#define TILT_FAST 0x58

// --- Wi-Fi Setup ---
const char* ssid = "Corbans_Camera";   // The Wi-Fi Network Name
const char* password = "geek1234"; // The Wi-Fi Password (min 8 characters)

WebServer server(80);
byte activeCommand = 0x00; // Tracks which direction is currently being held

// --- The HTML & CSS Webpage ---
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>Zifon Control</title>
  <style>
    body { 
      display: flex; 
      flex-direction: column; 
      align-items: center; 
      justify-content: center; 
      height: 100vh; 
      background-color: #121212; 
      margin: 0; 
      font-family: sans-serif; 
      overflow: hidden; /* Prevent accidental scrolling */
      touch-action: none; /* Block all browser gestures */
    }
    
    .d-pad { 
      display: grid; 
      /* Increased to 30vmin since the title is gone, maximizing the 4:3 screen */
      grid-template-columns: repeat(3, 30vmin); 
      grid-template-rows: repeat(3, 30vmin); 
      gap: 3vmin; 
    }
    
    .btn { 
      background-color: #333333; 
      color: #ffffff; 
      border: 4px solid #555555; /* High contrast boundary */
      border-radius: 20px; 
      font-size: 14vmin; /* Massive directional arrows */
      display: flex; 
      align-items: center; 
      justify-content: center; 
      user-select: none; 
      -webkit-user-select: none; /* iOS/Safari compatibility */
      cursor: pointer; 
      box-shadow: 0 10px 20px rgba(0,0,0,0.5); /* Make buttons pop off the screen */
      transition: transform 0.1s, box-shadow 0.1s; /* Smooth animation */
    }
    
    .btn:active { 
      background-color: #007bff; 
      border-color: #0056b3;
      transform: scale(0.92); /* Button physically "presses" down */
      box-shadow: 0 4px 8px rgba(0,0,0,0.5); 
    }
    
    /* Grid Placement */
    .up { grid-column: 2; grid-row: 1; }
    .left { grid-column: 1; grid-row: 2; }
    .right { grid-column: 3; grid-row: 2; }
    .down { grid-column: 2; grid-row: 3; }
  </style>
  <script>
    // Send command when touch starts
    function startMove(dir) { fetch('/move?dir=' + dir); }
    // Send stop command when touch ends
    function stopMove() { fetch('/stop'); }
    
    // Completely disable right-click menus and long-press text selection
    document.addEventListener('contextmenu', event => event.preventDefault());
  </script>
</head>
<body>
  <div class="d-pad">
    <div class="btn up" onpointerdown="startMove('up')" onpointerup="stopMove()" onpointerleave="stopMove()">&#9650;</div>
    <div class="btn left" onpointerdown="startMove('left')" onpointerup="stopMove()" onpointerleave="stopMove()">&#9664;</div>
    <div class="btn right" onpointerdown="startMove('right')" onpointerup="stopMove()" onpointerleave="stopMove()">&#9654;</div>
    <div class="btn down" onpointerdown="startMove('down')" onpointerup="stopMove()" onpointerleave="stopMove()">&#9660;</div>
  </div>
</body>
</html>
)rawliteral";
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleMove() {
  String dir = server.arg("dir");
  if (dir == "left") activeCommand = CMD_LEFT;
  else if (dir == "right") activeCommand = CMD_RIGHT;
  else if (dir == "up") activeCommand = CMD_UP;
  else if (dir == "down") activeCommand = CMD_DOWN;
  
  // Re-attach the TX pin to the Serial controller
  pinMode(TX_PIN, OUTPUT);
  CamSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); 
  
  server.send(200, "text/plain", "Moving " + dir);
}

void handleStop() {
  activeCommand = 0x00; // Stop the loop from sending bytes
  
  delay(10); // Give the final bytes a moment to clear the hardware buffer
  
  // Detach the Serial controller and set the pin to High-Impedance (INPUT)
  CamSerial.end(); 
  pinMode(TX_PIN, INPUT); 
  
  server.send(200, "text/plain", "Stopped");
}

void setup() {
  Serial.begin(115200);

  // 1. Initialize Speed Settings first
  CamSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(3000); 
  uint8_t panSpeed[3] = {CHANNEL, PAN_FAST, CHANNEL};
  CamSerial.write(panSpeed, 3);
  delay(100); 
  uint8_t tiltSpeed[3] = {CHANNEL, TILT_FAST, CHANNEL};
  CamSerial.write(tiltSpeed, 3);
  delay(100);
  
  CamSerial.end();
  pinMode(TX_PIN, INPUT); // Release to High-Z until a button is pressed

  // 2. Start the Wi-Fi Access Point
  Serial.println("Starting Wi-Fi Access Point...");
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP); // By default, this is usually 192.168.4.1

  // 3. Start the Web Server
  server.on("/", handleRoot);
  server.on("/move", handleMove);
  server.on("/stop", handleStop);
  server.begin();
  Serial.println("Web server started!");
}

void loop() {
  // Listen for incoming web requests from your smartphone
  server.handleClient();
  
  // If a button is currently being held down on the webpage...
  if (activeCommand != 0x00) {
    // Check if the ESP32's hardware serial buffer has room. 
    // This allows us to spam the camera continuously WITHOUT blocking the web server!
    if (CamSerial.availableForWrite() >= 3) {
      uint8_t packet[3] = {CHANNEL, activeCommand, CHANNEL};
      CamSerial.write(packet, 3);
    }
  }
}
