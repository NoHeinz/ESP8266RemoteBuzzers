
#define LED_STATUS      LED_BUILTIN     // Status LED
#define BTN_RESET       D1
#define BTN_READY       D8
const int BUFFER_SIZE = 10;
char buf[BUFFER_SIZE];
bool oneshot = true;
// Setup the controller
void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  while (!Serial) {};

  // Setup our I/O
  pinMode(LED_STATUS, OUTPUT);
  pinMode(BTN_RESET, INPUT_PULLUP);
  pinMode(BTN_READY, INPUT_PULLUP);
}

// Main loop
void loop() {
  if (digitalRead(BTN_RESET) == HIGH && oneshot) {                 // Reset button pressed
    oneshot = false;
    Serial.print("Reset");
  } else if (digitalRead(BTN_READY) == HIGH && oneshot) {                // Ready button pressed
    oneshot = false;
    Serial.print("Ready");
  }else{
    oneshot = true;
  }
  if (Serial.available() > 0) {
    // read the incoming bytes:
    int rlen = Serial.readBytes(buf, BUFFER_SIZE);

    // prints the received data
    for(int i = 0; i < rlen; i++)
      Serial.print(buf[i]);
  }

}
