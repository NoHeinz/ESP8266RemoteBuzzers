#include <RF24.h>

//  30 pin esp8266
//  D4 -> LED to GND
//  D1 -> RESET
//  D8 -> READY
//  D3 -> CE  (nRF24)
//  D2 -> CSN (nRF24)
//  D7 -> MO  (nRF24)
//  D6 -> MI  (nRF24)
//  D5 -> SCK (nRF24)


RF24 radio(D3, D2); // CE, CSN

#define LED_STATUS      LED_BUILTIN     // Status LED
#define BTN_RESET       D1
#define BTN_READY       D8




// ButtonStatus type
enum ButtonStatus : unsigned char { Disabled = 0, Enabled = 1, Answered = 2 };

// Status we want to share with the buttons
ButtonStatus buttonStatus[2]  = {Disabled, Disabled};
bool buttonConnected[2] = {false, false};
bool hasAnswered[2]     = {false, false};
unsigned long lastContact[2] = {0, 0};

// Last loop time
unsigned long lastLoopTime = 0;

// System status
bool isReady = false;


// searches the radio spectrum for a quiet channel
bool findEmptyChannel() {
  Serial.write("Scanning for empty channel...\n");
  char buffer[10];
  // Scan all channels looking for a quiet one.  We skip every 10
  for (int channel = 125; channel > 0; channel -= 10) {
    radio.setChannel(channel);
    delay(20);
    unsigned int inUse = 0;
    unsigned long testStart = millis();
    // Check for 400 ms per channel
    while (millis() - testStart < 400) {
      digitalWrite(LED_STATUS, millis() % 500 > 400);
      if ((radio.testCarrier()) || (radio.testRPD())) inUse++;
      delay(1);
    }
    // Low usage if inUse < 10 tests true in 400ms
    if (inUse < 10) {
      itoa(channel, buffer, 10);
      Serial.write("Channel ");
      Serial.write(buffer);
      Serial.write(" selected\n");
      return true;
    }
  }
  return false;
}

// Sends a new ACK payload to the transmitter
void setupACKPayload() {
  // Update the ACK for the next payload, prevents radio from stalling waiting on ack payload
  unsigned char payload[2];
  for (unsigned char button=0; button<2; button++)
      payload[button] = buttonStatus[button];
  radio.writeAckPayload(1, &payload, 2);
}

// Check for messages from the buttons
void checkRadioMessageReceived() {
  // Check if data is available
  if (radio.available()) {
    unsigned char buffer;
    radio.read(&buffer, 1);

    // Grab the button number from the data
    unsigned char buttonNumber = buffer & 0x7F; // Get the button number
    if ((buttonNumber >= 1) && (buttonNumber <= 2)) {
      buttonNumber--; //zero index

      // Update the last contact time for this button
      lastContact[buttonNumber] = lastLoopTime;

      // And that it's connected
      if (!buttonConnected[buttonNumber]){
        buttonConnected[buttonNumber] = true;
      }
      // If the button was pressed, was enabled, hasn't answered and the system is ready for button presses
      if ((buffer & 128) && (buttonStatus[buttonNumber] == Enabled) && (!hasAnswered[buttonNumber]) && (isReady)) {
        // No longer ready
        isReady = false;
        // Signal the button was pressed
        hasAnswered[buttonNumber] = true;

        // Change button status
        for (unsigned char btn = 0; btn < 2; btn++)
          buttonStatus[btn] = Disabled;
        buttonStatus[buttonNumber] = Answered;

        // Turn off the ready light
        digitalWrite(LED_STATUS, HIGH);
      }
    }

    setupACKPayload();
  }
}

// Setup the controller
void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  while (!Serial) {};
  // Setup the radio device
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.enableDynamicPayloads();
  radio.enableAckPayload();
  radio.setDataRate(RF24_1MBPS);
  radio.setRetries(4, 8);
  radio.maskIRQ(false, false, false);  // not using the IRQs

  // Setup our I/O
  pinMode(LED_STATUS, OUTPUT);
  pinMode(BTN_RESET, INPUT_PULLUP);
  pinMode(BTN_READY, INPUT_PULLUP);


  if (!radio.isChipConnected()) {
    Serial.write("RF24 device not detected.\n");
  } else {
    Serial.write("RF24 detected.\n");

    // Trun off the LED
    digitalWrite(LED_STATUS, HIGH);

    // Now setup the pipes for the four buttons
    char pipe[6] = "0QBTN";
    radio.openWritingPipe((uint8_t*)pipe);
    pipe[0] = '1';
    radio.openReadingPipe(1, (uint8_t*)pipe);

    // Start listening for messages
    radio.startListening();

    // Find an empty channel to run on
    while (!findEmptyChannel()) {};

    // Start listening for messages
    radio.startListening();
    
    // Ready
    digitalWrite(LED_STATUS, LOW);

    setupACKPayload();
  }
}

// Main loop
void loop() {
  lastLoopTime = millis();


  if (digitalRead(BTN_RESET) == HIGH) {                 // Reset button pressed
    // Turn all buttons off, Reset the hasAnswered statuses
    for (unsigned char button = 0; button < 2; button++) {
      buttonStatus[button] = Disabled;
      hasAnswered[button] = false;
    }
    isReady = false;
    digitalWrite(LED_STATUS, LOW);
  } else if (digitalRead(BTN_READY) == HIGH) {                // Ready button pressed
    // Make the buttons Enabled that haven't answered yet
    for (unsigned char button = 0; button < 2; button++) {
      buttonStatus[button] = hasAnswered[button] ? Disabled : Enabled;
    }
    isReady = true;
    digitalWrite(LED_STATUS, HIGH);
  }

  // monitor for Buttons that are out of contact
  for (unsigned char button = 0; button < 2; button++) {
    // If the button is connected
    if (buttonConnected[button]) {
      // If its been 1 second since we heard from it
      if (lastLoopTime - lastContact[button] > 1000) {
        // Disconnect it
        Serial.write("button disconnected.\n");
        buttonConnected[button] = false;
      } 
    } 
  }
  checkRadioMessageReceived();//recieve button data and send button statuses
}
