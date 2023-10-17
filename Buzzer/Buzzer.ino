#include <RF24.h>

//  30 pin esp8266
//  D4 -> LED(Relay) 
//  D8 -> Button
//  D3 -> CE  (nRF24)
//  D2 -> CSN (nRF24)
//  D7 -> MO  (nRF24)
//  D6 -> MI  (nRF24)
//  D5 -> SCK (nRF24)

RF24 radio(D3, D2); // CE, CSN

#define PIN_BUTTON   D8
#define PIN_LED      LED_BUILTIN
#define COM_TIMEOUT  1000
#define ANSWERED_FLASHING_DURATION   2500
#define FLASH_PERIOD_LENGTH   100
#define FLASH_PERIOD_ON_LENGTH   50

// ButtonStatus options(Disabled = led off/button disabled, Enabled = led on/button enabled, Answered = led on(after flashing sequence)/button enabled)
enum ButtonStatus : unsigned char { Disabled = 0, Enabled = 1, Answered = 2 };

// Last loop start time
unsigned long lastLoopTime = 0;
// If this is in contact with the controller
bool isConnected = false;
// Last time we sent some status
unsigned long lastStatusSend = 0;
// When the button was pressed down
unsigned long buttonPressTime = 0;
// Status of the LED
ButtonStatus buttonState = Disabled;
// Which button number we are
unsigned char buttonNumber;

// Main setup function
void setup() {
  // put your setup code here, to run once:
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  Serial.begin(57600);
  while (!Serial) {};
  buttonNumber = 2; //change this to current number
  // Setup the radio device
  if (!radio.begin()) {
    Serial.write("RF24 device failed to begin\n");
  }
  radio.setPALevel(RF24_PA_LOW);     // RF24_PA_MIN = 0, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  radio.enableDynamicPayloads();
  radio.enableAckPayload();
  radio.setDataRate(RF24_1MBPS); //{ RF24_1MBPS = 0, RF24_2MBPS, RF24_250KBPS }
  //radio.setCRCLength(0); // { RF24_CRC_DISABLED = 0, RF24_CRC_8, RF24_CRC_16 }
  radio.setRetries(2, 2);//count(delay(<16),count(<16))
  radio.maskIRQ(false, false, false);  // not using the IRQs
    
   if (!radio.isChipConnected()) {
     Serial.write("RF24 device not detected.\n");
   } else {  
     Serial.write("RF24 device found\n");
   }

   // Configure the i/o
   char pipe[6] = "1QBTN";
   radio.openWritingPipe((uint8_t*)pipe);
   pipe[0] = '0';
   radio.openReadingPipe(1, (uint8_t*)pipe);
   radio.stopListening();
}

// Search for the button controller channel
bool findButtonController() {
   Serial.write("Searching for controller...\n");
   for (int a = 125; a > 0; a-=10) {
      radio.setChannel(a);
      delay(15);
      // Send a single byte for status
      if (sendButtonStatus(false)) {
        Serial.write("Quiz Controller found on channel ");
        char buffer[10];
        itoa(a,buffer,10);
        Serial.write(buffer);
        Serial.write("\n");
        return true;          
      }
      digitalWrite(PIN_LED, (millis() & 2047) > 2000);
   }
   // Add a 1.5 second pause before trying again (but still flash the LED)
   unsigned long m = millis();
   while (millis() - m < 1500) {
     digitalWrite(PIN_LED, (millis() & 2047) > 2000);
     delay(15);
   }
   
   return false;
}

// Attempt to send the sttaus of the button and receive what we shoudl be doing
bool sendButtonStatus(bool isDown) {
  unsigned char message = buttonNumber;
  if (isDown) message |= 128;//add button status to bit 8

  for (unsigned char retries=2; retries<=32; retries*=2) {  
    unsigned int randomDelayAmount = random(1,retries);
    if (radio.write(&message, 1)) {
      if (radio.available()) { //Test whether there are bytes available to be read.
       if (radio.getDynamicPayloadSize() == 2) {
          unsigned char tmp[2];
          radio.read(&tmp, 2);
          buttonState = (ButtonStatus)(tmp[buttonNumber-1]);
          return true;          
        } else {
          // Remove redundant data
          int total = radio.getDynamicPayloadSize();
          unsigned char tmp;
          while (total-- > 0) radio.read(&tmp, 1);
          Serial.write("Write OK, ACK wrong size\n");
          delay(randomDelayAmount);
        }
      } else {
          // Write ack recieve, but no custom ack recieved
          Serial.write("\nWrite OK, no ACK\n");
          return true;
      }
    } else {
      //no write ack recieved, collision assumed
      delay(randomDelayAmount);
    }
  }
  
  Serial.write("Write Failed\n");
  return false;
}

// Main loop
void loop() {
  lastLoopTime = millis();
  if (radio.isChipConnected()) {

    // If COM_TIMEOUT reached or not connected
    if ((lastLoopTime - lastStatusSend > COM_TIMEOUT) || (!isConnected)) {
        while (!findButtonController()) {};
        digitalWrite(PIN_LED, LOW);    
        isConnected = true;
        lastStatusSend = lastLoopTime;
    }  

    // If the button was pressed down (and its been 100ms since last check to prevent spamming)
    if ((digitalRead(PIN_BUTTON) == HIGH) && (buttonState == Enabled) && (lastLoopTime - buttonPressTime>100)) {
      // This ensures we get a random number sequence unique to this player.  The random number is used to prevent packet collision
      randomSeed(lastLoopTime);       
      // Send the DOWN state
      if (sendButtonStatus(true)) {
        buttonPressTime = lastLoopTime;
        lastStatusSend = lastLoopTime;
      }
    }else if (lastLoopTime-lastStatusSend > 100) {// General update every 100ms
      if (sendButtonStatus(false)) {
        lastStatusSend = lastLoopTime;
      } else delay(10); 
    }

    //LED controll logic
    if (buttonState == Enabled){//On while answering
      digitalWrite(PIN_LED,LOW);
    }else if (buttonState == Answered) {//flashing during time period after answering
      if (lastLoopTime - buttonPressTime<ANSWERED_FLASHING_DURATION){
        digitalWrite(PIN_LED,((lastLoopTime & FLASH_PERIOD_LENGTH)>FLASH_PERIOD_ON_LENGTH));
      }else{//On whenever else
        digitalWrite(PIN_LED,LOW);
      }
    }else{//led of when disabled
      digitalWrite(PIN_LED,HIGH);
    }

  } else {
     // Error flash sequence
     digitalWrite(PIN_LED, (lastLoopTime & 1023) < 100);
  }

  // Slow the main loop down
  delay(1);
}
