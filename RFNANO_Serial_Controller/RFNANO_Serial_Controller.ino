#include <RF24.h>

//  30 pin esp8266
//  D4 -> LED to GND
//  D1 -> RESET
//  D8 -> READY



RF24 radio(7, 8); // CE, CSN

#define LED_STATUS      LED_BUILTIN     // Status LED
#define SERIAL_UPDATE_RATE 500



// ButtonStatus type
enum ButtonStatus : char { Disabled = 0x01, Enabled = 0x02, Answered = 0x03};

// Status we want to share with the buttons
ButtonStatus buttonStatus[4]  = {Disabled, Disabled, Disabled, Disabled};
bool buttonConnected[4] = {false, false, false, false};
bool hasAnswered[4]     = {false, false, false, false};
unsigned long lastContact[4] = {0, 0, 0, 0};

// Last loop time
unsigned long lastLoopTime = 0;
unsigned long lastSerialSendTime;
// System status
bool isReady = false;


// searches the radio spectrum for a quiet channel
bool findEmptyChannel() {
  Serial.write("scanning for empty channel...\n");
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
      Serial.write("channel ");
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
  unsigned char payload[4];
  for (unsigned char button=0; button<4; button++)
      payload[button] = buttonStatus[button];
  radio.writeAckPayload(1, &payload, 4);
}

// Check for messages from the buttons
void checkRadioMessageReceived() {
  // Check if data is available
  if (radio.available()) {
    unsigned char buffer;
    radio.read(&buffer, 1);

    // Grab the button number from the data
    unsigned char buttonNumber = buffer & 0x7F; // Get the button number
    if ((buttonNumber >= 1) && (buttonNumber <= 4)) {
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
        for (unsigned char btn = 0; btn < 4; btn++)
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
    //while (!findEmptyChannel()) {};
    radio.setChannel(125);
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
  while (Serial.available() > 0){
    char inByte = Serial.read();
    if (inByte == 0x01) {                 // Reset button pressed
      // Turn all buttons off, Reset the hasAnswered statuses
      for (unsigned char button = 0; button < 4; button++) {
        buttonStatus[button] = Disabled;
        hasAnswered[button] = false;
      }
      isReady = false;
      digitalWrite(LED_STATUS, LOW);
    } else if (inByte == 0x02) {                // Ready button pressed
      // Make the buttons Enabled that haven't answered yet
      for (unsigned char button = 0; button < 4; button++) {
        buttonStatus[button] = hasAnswered[button] ? Disabled : Enabled;
      }
      isReady = true;
      digitalWrite(LED_STATUS, HIGH);
    } else if (inByte == 0x03) {                // disable button pressed
      // Make the buttons Enabled that haven't answered yet
      for (unsigned char button = 0; button < 4; button++) {
        buttonStatus[button] = Disabled;
      }
      isReady = false;
      digitalWrite(LED_STATUS, LOW);
    } else if (inByte == 0x04) {                // set button answer status from serial
      while (Serial.available() < 2){}//wait for next bytes
      char button_num = Serial.read();
      char button_answer_status = Serial.read();
      if (button_answer_status > 0){
        hasAnswered[button_num] = true;
      }else{
        hasAnswered[button_num] = false;
      }
      if (isReady){ //reset state based on new has answered
        buttonStatus[button_num] = hasAnswered[button_num] ? Disabled : Enabled;
      }
    }
  }
  // monitor for Buttons that are out of contact
  for (unsigned char button = 0; button < 4; button++) {
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
  if ((lastLoopTime - lastSerialSendTime > SERIAL_UPDATE_RATE) && (Serial.availableForWrite()>5)){
      char payload[6];
      for (unsigned char button=0; button<4; button++){
        payload[button+1] = buttonStatus[button];
        if (buttonConnected[button]) payload[button+1] |= 0x80;
        if (hasAnswered[button]) payload[button+1] |= 0x40;
      }
      payload[0] = 'S';
      payload[5] = '\n';
      Serial.write(payload);
      lastSerialSendTime = lastLoopTime;
  }
  checkRadioMessageReceived();//recieve button data and send button statuses
}
