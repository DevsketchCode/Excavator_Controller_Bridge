#include <RF24.h>
#include <RF24_config.h>
#include <nRF24L01.h>
#include <printf.h>
#include <Bluepad32.h>
#include <SPI.h>

// Initialize NRF24 on Pins 4 (CE) and 5 (CSN)
RF24 radio(4, 5); 

// Pointer to store controller data from Bluepad32
ControllerPtr myControllers[BP32_MAX_CONTROLLERS];

// Data packet array for the SY-E571 Excavator
byte data[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Forward declarations for Bluepad32 callbacks
void onConnectedController(ControllerPtr ctl);
void onDisconnectedController(ControllerPtr ctl);

void setup() {
  Serial.begin(115200);
  
  // Initialize Radio
  if (radio.begin()) {
    Serial.println("NRF24L01+ initialized successfully!");
    radio.setPALevel(RF24_PA_LOW);
    
    // Double E toys typically use a specific channel (e.g., channel 76 or 40)
    radio.setChannel(76);

    // Set data rate to 1MBPS or 2MBPS (Most toys use 1MBPS)
    radio.setDataRate(RF24_1MBPS);

    // Open the writing pipe with the correct target address array
    const uint64_t pipeAddress = 0xE8E8F0F0E1LL; 
    radio.openWritingPipe(pipeAddress);
    
    radio.stopListening();
  } else {
    Serial.println("NRF24L01+ failed to initialize! Check wiring.");
  }

  // Initialize Bluepad32 with required connection/disconnection callbacks
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys(); // Clears old cache to prevent pairing locks
}

void loop() {
  // Update controller states
  bool dataUpdated = BP32.update();
  if (dataUpdated) {
    processControllers();
  }
  vTaskDelay(1); // Yield to ESP32 background tasks
}

// Callback when a controller connects
void onConnectedController(ControllerPtr ctl) {
  bool foundSlot = false;
  for (int i = 0; i < BP32_MAX_CONTROLLERS; i++) {
    if (myControllers[i] == nullptr) {
      myControllers[i] = ctl;
      foundSlot = true;
      Serial.printf("Controller connected, index=%d\n", i);
      break;
    }
  }
  if (!foundSlot) {
    Serial.println("Controller connected, but maximum slots reached!");
  }
}

// Callback when a controller disconnects
void onDisconnectedController(ControllerPtr ctl) {
  bool foundSlot = false;
  for (int i = 0; i < BP32_MAX_CONTROLLERS; i++) {
    if (myControllers[i] == ctl) {
      myControllers[i] = nullptr;
      foundSlot = true;
      Serial.printf("Controller disconnected from index=%d\n", i);
      break;
    }
  }
  if (!foundSlot) {
    Serial.println("Controller disconnected, but not found in slots.");
  }
}

void processControllers() {
  for (auto myController : myControllers) {
    if (myController && myController->isConnected() && myController->hasData()) {
      
      // Reset packet array on every loop
      for(int i=0; i<7; i++) data[i] = 0x00;

      // Read Left Joystick Y-Axis (Forward / Backward)
      int throttle = myController->axisY(); // Usually ranges from -512 to 512
      
      if (throttle < -200) {
        data[0] = 0x01; // Command Hex for Forward
        Serial.println("Action: Moving Forward");
      } 
      else if (throttle > 200) {
        data[0] = 0x02; // Command Hex for Backward
        Serial.println("Action: Moving Backward");
      }

      // Transmit the packet via NRF24L01+
      radio.write(&data, sizeof(data));
    }
  }
}