#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>

// --- Hardware & Protocol Setup ---
// WARNING: Pins 4 and 5 conflict with JTAG on the ESP32-C3! 
// Change these back to 10 and 8 if the camera stops moving.
#define TX_PIN 10  // Safe pin for ESP32-C3
#define RX_PIN 8   // Safe pin for ESP32-C3
HardwareSerial CamSerial(1); 

#define CHANNEL   0x60
#define CMD_LEFT  0xCC
#define CMD_RIGHT 0xC3
#define CMD_UP    0xCA
#define CMD_DOWN  0xC5

#define PAN_FAST  0xA8
#define TILT_FAST 0x58

// --- Camera Shutter Setup ---
#define FOCUS_PIN 0    // Controls Transistor 1 (Black Wire)
#define SHUTTER_PIN 2  // Controls Transistor 2 (Red Wire)
// --- Camera Focus and shutter - White + Black = focus ----- White + Black + Red = Photo 

// --- Wi-Fi Setup ---
const char* ssid = "Corbans_Camera";   
const char* password = "geek1234"; 

WebServer server(80);
byte activeCommand = 0x00; 

// --- The HTML & CSS Webpage ---
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>Zifon Control</title>
  <style>
    /* Lock the page exactly to the visible window, stopping Chrome from pushing it down */
    html, body {
      height: 100%;
      width: 100%;
      margin: 0;
      padding: 0;
    }
    
    body { 
      display: flex; 
      flex-direction: column; 
      align-items: center; 
      justify-content: center; 
      background-color: #121212; 
      font-family: sans-serif; 
      overflow: hidden; /* Prevent accidental scrolling */
      touch-action: none; /* Block all browser gestures */
    }
    
    .d-pad { 
      display: grid; 
      grid-template-columns: repeat(3, 24vmin); 
      grid-template-rows: repeat(3, 24vmin); 
      gap: 2vmin; 
    }
    
    .btn { 
      box-sizing: border-box; /* Forces the 4px border inside the 24vmin size */
      background-color: #333333; 
      color: #ffffff; 
      border: 4px solid #555555; 
      border-radius: 20px; 
      font-size: 11vmin; 
      display: flex; 
      align-items: center; 
      justify-content: center; 
      user-select: none; 
      -webkit-user-select: none; 
      cursor: pointer; 
      box-shadow: 0 10px 20px rgba(0,0,0,0.5); 
      transition: transform 0.1s, box-shadow 0.1s, background-color 0.1s; 
    }
    
    /* Default blue press for directional arrows */
    .btn:active { 
      background-color: #007bff; 
      border-color: #0056b3;
      transform: scale(0.92); 
      box-shadow: 0 4px 8px rgba(0,0,0,0.5); 
    }

    /* Custom red press for the camera shutter */
    .center:active {
      background-color: #dc3545; 
      border-color: #a71d2a;
    }

    /* Refresh and Focus Button Styling */
    .small-btn { font-size: 8vmin; background-color: #222; border-color: #444; font-weight: bold;}
    .small-btn:active { background-color: #555; border-color: #777; }
    
    .focus:active { background-color: #28a745; border-color: #1e7e34; } /* Turns green when focusing */
    
    /* Grid Placement */
    .up { grid-column: 2; grid-row: 1; }
    .left { grid-column: 1; grid-row: 2; }
    .center { grid-column: 2; grid-row: 2; } 
    .right { grid-column: 3; grid-row: 2; }
    .refresh { grid-column: 1; grid-row: 3; } 
    .down { grid-column: 2; grid-row: 3; }
    .focus { grid-column: 3; grid-row: 3; } 
  </style>
  <script>
    function startMove(dir) { fetch('/move?dir=' + dir); }
    function stopMove() { fetch('/stop'); }
    
    function startFocus() { fetch('/focus?state=on'); }
    function stopFocus() { fetch('/focus?state=off'); }
    function takePicture() { fetch('/snap'); }
    
    document.addEventListener('contextmenu', event => event.preventDefault());
  </script>
</head>
<body>
  <div class="d-pad">
    <div class="btn up" onpointerdown="startMove('up')" onpointerup="stopMove()" onpointerleave="stopMove()">&#9650;</div>
    <div class="btn left" onpointerdown="startMove('left')" onpointerup="stopMove()" onpointerleave="stopMove()">&#9664;</div>
    
    <div class="btn center" onpointerdown="takePicture()">&#128247;</div>
    
    <div class="btn right" onpointerdown="startMove('right')" onpointerup="stopMove()" onpointerleave="stopMove()">&#9654;</div>
    
    <div class="btn small-btn refresh" onpointerdown="location.reload()">&#8635;</div>
    
    <div class="btn down" onpointerdown="startMove('down')" onpointerup="stopMove()" onpointerleave="stopMove()">&#9660;</div>
    
    <div class="btn small-btn focus" onpointerdown="startFocus()" onpointerup="stopFocus()" onpointerleave="stopFocus()">AF</div>
  </div>
</body>
</html>
)rawliteral";

// --- Web Server Endpoints ---

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleMove() {
  String dir = server.arg("dir");
  if (dir == "left") activeCommand = CMD_LEFT;
  else if (dir == "right") activeCommand = CMD_RIGHT;
  else if (dir == "up") activeCommand = CMD_UP;
  else if (dir == "down") activeCommand = CMD_DOWN;
  
  pinMode(TX_PIN, OUTPUT);
  CamSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); 
  
  server.send(200, "text/plain", "Moving " + dir);
}

void handleStop() {
  activeCommand = 0x00; 
  delay(10); 
  CamSerial.end(); 
  pinMode(TX_PIN, INPUT); 
  server.send(200, "text/plain", "Stopped");
}

void handleFocus() {
  String state = server.arg("state");
  if (state == "on") {
    digitalWrite(FOCUS_PIN, HIGH); // Engage Transistor 1
    Serial.println("Focusing...");
  } else {
    digitalWrite(FOCUS_PIN, LOW);  // Release Transistor 1
    Serial.println("Focus Released.");
  }
  server.send(200, "text/plain", "Focus " + state);
}

void handleSnap() {
  Serial.println("Picture button pressed! Sequencing...");
  
  // 1. Half-press the button (Engage Focus)
  digitalWrite(FOCUS_PIN, HIGH);   
  
  // Wait 800 milliseconds to give the Nikon lens time to actually lock focus
  // (If you are in a dark room or the subject is far, you might even need to increase this to 1000)
  delay(2000); 
  
  // 2. Full-press the button (Engage Shutter while keeping Focus held down)
  digitalWrite(SHUTTER_PIN, HIGH); 
  
  // Hold the shutter down for 200 milliseconds to ensure the camera registers the shot
  delay(2000); 
  
  // 3. Release both
  digitalWrite(SHUTTER_PIN, LOW); 
  digitalWrite(FOCUS_PIN, LOW); 
  
  server.send(200, "text/plain", "Snap!");
}

void setup() {
  Serial.begin(115200);

  // Initialize Camera Pins
  pinMode(FOCUS_PIN, OUTPUT);
  digitalWrite(FOCUS_PIN, LOW);
  pinMode(SHUTTER_PIN, OUTPUT);
  digitalWrite(SHUTTER_PIN, LOW);

  // Initialize Speed Settings
  CamSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(3000); 
  uint8_t panSpeed[3] = {CHANNEL, PAN_FAST, CHANNEL};
  CamSerial.write(panSpeed, 3);
  delay(100); 
  uint8_t tiltSpeed[3] = {CHANNEL, TILT_FAST, CHANNEL};
  CamSerial.write(tiltSpeed, 3);
  delay(100);
  
  CamSerial.end();
  pinMode(TX_PIN, INPUT); 

  // Start Wi-Fi Access Point
  Serial.println("Starting Wi-Fi Access Point...");
  WiFi.softAP(ssid, password);
  
  // *** SOFTWARE FIX FOR BROWNOUTS ***
  WiFi.setTxPower(WIFI_POWER_8_5dBm); 
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP); 

  // Start Web Server
  server.on("/", handleRoot);
  server.on("/move", handleMove);
  server.on("/stop", handleStop);
  server.on("/focus", handleFocus);
  server.on("/snap", handleSnap);
  server.begin();
  Serial.println("Web server started!");
}

void loop() {
  server.handleClient();
  
  if (activeCommand != 0x00) {
    if (CamSerial.availableForWrite() >= 3) {
      uint8_t packet[3] = {CHANNEL, activeCommand, CHANNEL};
      CamSerial.write(packet, 3);
    }
  }
}
