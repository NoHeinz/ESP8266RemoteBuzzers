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

/////////////////////////////////////////////
//           END RemoteXY include          //
/////////////////////////////////////////////



void setup() 
{
  RemoteXY_Init (); 
  
  
  // TODO you setup code
  
}

void loop() 
{ 
  RemoteXY_Handler ();
  
  
  // TODO you loop code
  // use the RemoteXY structure for data transfer
  // do not call delay(), use instead RemoteXY_delay() 


}