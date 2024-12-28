#include <SoftwareSerialParity.h>

// Define pins for RS485
#define RS485_TX 2
#define RS485_RX 3
#define RS485_EN 4

// Initialize software serial
SoftwareSerialParity rs485Serial(RS485_TX, RS485_RX);

// // States for the state machine
enum State {
  IDLE,
  RING,
  SPEAKING
};

State currentState = IDLE;
int unlockCount = 0;

const uint8_t H_DISCOVER[] = {0xA1, 0x5A, 0x00, 0x7B};
const uint8_t C_IMHERE[] = {0xB0, 0x5A, 0x00, 0x6A};
const uint8_t H_ACK[] = {0xA5, 0x5A, 0x00, 0x7F};
const uint8_t HHB[] = {0xA1, 0x41, 0x00, 0x60};
const uint8_t CHB[] = {0xB0, 0x41, 0x00, 0x71};
const uint8_t C_CALL[] = {0xB0, 0x39, 0x00, 0x09};
const uint8_t H_CALLACK[] = {0xA1, 0x39, 0x00, 0x18};
const uint8_t C_CALLHANGUP[] = {0xB0, 0x3F, 0x04, 0x0B};
const uint8_t H_CALLHANGUPACK[] = {0xA1, 0x3F, 0x04, 0x1A};
const uint8_t H_RING[] = {0xA1, 0x32, 0x00, 0x13}; 
const uint8_t C_RINGACK[] = {0xB0, 0x32, 0x00, 0x02};
const uint8_t C_PKUP[] = {0xB0, 0x36, 0x02, 0x04};
const uint8_t H_PKUPACK[] = {0xA1, 0x36, 0x02, 0x15};
const uint8_t C_SPKING[] = {0xB0, 0x42, 0x00, 0x72};
const uint8_t C_UNLOCK[] = {0xB0, 0x3B, 0x00, 0x0B};
const uint8_t H_UNLOCKACK[] = {0xA1, 0x3B, 0x00, 0x1A};


void setup() {
  // Initialize serial ports
  Serial.begin(9600);  // For debugging
  rs485Serial.begin(9600, EVEN);
  
  // Configure RS485 enable pin
  pinMode(RS485_EN, OUTPUT);
  digitalWrite(RS485_EN, LOW);  // Set to receive mode by default
  
  Serial.println("RS485 Doorbell System Started");
}

// Function to send data over RS485
void publishCmd(const uint8_t* cmd) {
  digitalWrite(RS485_EN, HIGH);  // Enable transmission
  delay(5);  // 5ms delay as in Python code
  rs485Serial.write(cmd, 4);
  rs485Serial.flush();  // Wait for transmission to complete
  digitalWrite(RS485_EN, LOW);   // Return to receive mode
  // Serial.print("Published command: ");
  // printHex(cmd, sizeof(cmd));
  // Serial.println();
}

void printHex(const uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (data[i] < 0x10) Serial.print('0'); // Add leading zero if needed
    Serial.print(data[i], HEX);
  }
}
void loop() {
  if (rs485Serial.available() >= 4) {  // Check if we have at least 4 bytes
    uint8_t message[4];  // Buffer for incoming message
    int bytesRead = rs485Serial.readBytes(message, 4);
    // Handle different messages
    if (strstr(message, H_DISCOVER)) {
      Serial.println("Received H_DISCOVER Sent C_IMHERE");
      publishCmd(C_IMHERE);
    }
    else if (strstr(message, H_RING)) {
      Serial.println("Received H_RING Sent C_RINGACK");
      publishCmd(C_RINGACK);
      currentState = RING;
    }
    else if (strstr(message, H_PKUPACK)) {
      Serial.println("Received H_PKUPACK Sent C_SPKING");
      currentState = SPEAKING;
      publishCmd(C_SPKING);
    }
    else if (strstr(message, H_UNLOCKACK)) {
      Serial.println("Received H_UNLOCKACK Sent C_SPKING");
      publishCmd(C_SPKING);
    }
    else if (strstr(message, H_CALLHANGUPACK)) {
      Serial.println("Received H_CALLHANGUPACK Sent CHB");
      currentState = IDLE;
      publishCmd(CHB);
    }
    else if (strstr(message, HHB)) {
      Serial.println("Received HHB");
      switch (currentState) {
        case IDLE:
          Serial.println("Sending CHB");
          publishCmd(CHB);
          break;
        case RING:
          Serial.println("Sending C_PKUP");
          publishCmd(C_PKUP);
          break;
        case SPEAKING:
          if (unlockCount < 5) {
            Serial.println("Sending C_UNLOCK");
            publishCmd(C_UNLOCK);
            unlockCount++;
          } else {
            Serial.println("Sending C_CALLHANGUP");
            publishCmd(C_CALLHANGUP);
            unlockCount = 0;
          }
          break;
      }
    }
    else if (strstr(message, H_ACK)) {
      Serial.println("Received H_ACK");
    }
    else {
      Serial.print("Unknown message: ");
      printHex(message, bytesRead);
      Serial.println();
      currentState = IDLE;
    }

    if (currentState != IDLE) {
      Serial.print("STATE: ");
      Serial.println(currentState);
    }
  }
}