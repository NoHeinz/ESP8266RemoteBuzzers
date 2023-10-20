#include <RF24.h>

//  30 pin esp32
//  D4 -> LED to GND
//  D22 -> RESET
//  D15 -> READY
//  D4 -> CE  (nRF24)
//  D5 -> CSN (nRF24)
//  D23 -> MO  (nRF24)
//  D19 -> MI  (nRF24)
//  D18 -> SCK (nRF24)

// RemoteXY select connection mode and include library 
#define REMOTEXY_MODE__ESP32CORE_BLUETOOTH

#include <BluetoothSerial.h>

#include <RemoteXY.h>

// RemoteXY connection settings 
#define REMOTEXY_BLUETOOTH_NAME "RemoteXY"

// RemoteXY configurate  
#pragma pack(push, 1)
uint8_t RemoteXY_CONF[] =   // 50 bytes
  { 255,2,0,2,0,43,0,16,24,1,65,64,15,57,9,9,65,16,40,57,
  9,9,1,0,35,10,21,21,2,24,82,101,97,100,121,0,1,0,8,10,
  21,21,2,24,82,101,115,101,116,0 };
  
// this structure defines all the variables and events of your control interface 
struct {

    // input variables
  uint8_t ready_button; // =1 if button pressed, else =0 
  uint8_t reset_button; // =1 if button pressed, else =0 

    // output variables
  uint8_t led_1_r; // =0..255 LED Red brightness 
  uint8_t led_2_b; // =0..255 LED Blue brightness 

    // other variable
  uint8_t connect_flag;  // =1 if wire connected, else =0 

} RemoteXY;
#pragma pack(pop)

RF24 radio(4, 5); // CE, CSN

#define LED_STATUS      2     // Status LED




// ButtonStatus type
enum ButtonStatus : unsigned char { Disabled = 0, Enabled = 1, Answered = 2,  Flashing = 3};

// Status we want to share with the buttons
ButtonStatus buttonStatus[2]  = {Disabled, Disabled};
bool buttonConnected[2] = {false, false};
bool hasAnswered[2]     = {false, false};
unsigned long lastContact[2] = {0, 0};

// Last loop time
unsigned long lastLoopTime = 0;

// System status
bool isReady = false;

// Sends a new ACK payload to the transmitter
uint8_t button_status_to_led_output(ButtonStatus stat) {
  if (stat == Disabled){
    return 0;
  }else{
    return 255;
  }
}

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
  RemoteXY_Init ();
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
  RemoteXY_Handler ();
  if (RemoteXY.reset_button == 1) {                 // Reset button pressed
    // Turn all buttons off, Reset the hasAnswered statuses
    for (unsigned char button = 0; button < 2; button++) {
      buttonStatus[button] = Disabled;
      hasAnswered[button] = false;
    }
    isReady = false;
    digitalWrite(LED_STATUS, LOW);
  } else if (RemoteXY.ready_button == 1) {                // Ready button pressed
    // Make the buttons Enabled that haven't answered yet
    for (unsigned char button = 0; button < 2; button++) {
      buttonStatus[button] = hasAnswered[button] ? Disabled : Enabled;
    }
    isReady = true;
    digitalWrite(LED_STATUS, HIGH);
  }

  RemoteXY.led_1_r = button_status_to_led_output(buttonStatus[0]);
  RemoteXY.led_2_b = button_status_to_led_output(buttonStatus[1]);
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
