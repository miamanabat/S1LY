// =======================================================================================
//                     PS3 Starting Sketch for Notre Dame Droid Class
// =======================================================================================
//                          Last Revised Date: 02/06/2021
//                             Revised By: Prof McLaughlin
// =======================================================================================
// ---------------------------------------------------------------------------------------
//                          Libraries
// ---------------------------------------------------------------------------------------
#include <PS3BT.h>
#include <usbhub.h>
#include <Sabertooth.h>
#include <Adafruit_TLC5947.h>
#include <MP3Trigger.h>
#include <Servo.h> 
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NewPing.h>

// ---------------------------------------------------------------------------------------
//                       Debug - Verbose Flags
// ---------------------------------------------------------------------------------------
#define SHADOW_DEBUG       //uncomment this for console DEBUG output

// ---------------------------------------------------------------------------------------
//                 Setup for USB, Bluetooth Dongle, & PS3 Controller
// ---------------------------------------------------------------------------------------
USB Usb;
BTD Btd(&Usb);
PS3BT *PS3Controller=new PS3BT(&Btd);

// ---------------------------------------------------------------------------------------
//    Output String for Serial Monitor Output
// ---------------------------------------------------------------------------------------
char output[300];

// ---------------------------------------------------------------------------------------
//    Deadzone range for joystick to be considered at nuetral
// ---------------------------------------------------------------------------------------
byte joystickDeadZoneRange = 15;

// ---------------------------------------------------------------------------------------
//    Used for PS3 Fault Detection
// ---------------------------------------------------------------------------------------
uint32_t msgLagTime = 0;
uint32_t lastMsgTime = 0;
uint32_t currentTime = 0;
uint32_t lastLoopTime = 0;
int badPS3Data = 0;

boolean isPS3ControllerInitialized = false;
boolean mainControllerConnected = false;
boolean WaitingforReconnect = false;
boolean isFootMotorStopped = true;

// ---------------------------------------------------------------------------------------
//    Used for PS3 Controller Click Management
// ---------------------------------------------------------------------------------------
long previousMillis = millis();
boolean extraClicks = false;

// ---------------------------------------------------------------------------------------
//    Used for Pin 13 Main Loop Blinker
// ---------------------------------------------------------------------------------------
long blinkMillis = millis();
boolean blinkOn = false;

// ---------------------------------------------------------------------------------------
//    User Motor Control
// ---------------------------------------------------------------------------------------
byte driveDeadBandRange = 10;
#define SABERTOOTH_ADDR 128
Sabertooth *ST=new Sabertooth(SABERTOOTH_ADDR, Serial1);

int MAXDRIVESPEED = 60;
int MAXTURNSPEED = 40;
int currSpeed = 0;

// =======================================================================================
//                                 Servo and Lights
// =======================================================================================

Servo myServo;
int servoPos = 0;
int servo_dir = 1;

void moveServo(){
  servoPos = servoPos + (1 * servo_dir);
  if (servoPos > 70){
    servo_dir = -1;
  }
  if (servoPos < -70){
    servo_dir = 1;
  }
  myServo.write(servoPos);
}

#define latch 6
#define clock 5
#define data 4
Adafruit_TLC5947 LEDControl = Adafruit_TLC5947(1, clock, data, latch);
int ledMaxBright = 4000; // 4095 is MAX brightness
bool on = false;
long checkTiming = 0;
long lightsMillis = 0;
void lightsFlash(){
  if (lightsMillis == 0){
    lightsMillis = millis();
  }
  if (lightsMillis + 4000 < millis()){
    lightsMillis = millis();
    Serial.print(F("lights change! \n"));
    for (int i = 0; i < 12; i++) {
      if (on) {
        if (i % 2 == 0) {
           LEDControl.setPWM(i, 0);
        } else {
          LEDControl.setPWM(i, ledMaxBright);
        }
      } else {
        if (i % 2 == 0) {
          LEDControl.setPWM(i, ledMaxBright);
        } else {
          LEDControl.setPWM(i, 0);
        }
      }
    }
    LEDControl.write();

    on = !on;
    
  }
}


// =======================================================================================
//                                 Display Function
// =======================================================================================
/*
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
String words = "";
void display_words(){
    display.begin(SSD1306_SWITCHCAPVCC, 0x3c);
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println();
    display.println(words);
    display.display();
}
*/

// =======================================================================================
//                                 Integrated Routines
// =======================================================================================
MP3Trigger MP3Trigger;
long integratedMoveMillis = 0;
bool moveForwardIntegrated = true;
int val = 10;
bool playing_song = false;
bool displaying_int1 = false;
long integrated1Start = 0;
/* to repeat
 *  int counterroutine = 0;
 *  each time the timer stops, reapeat until counter is whatever
 */
 int playcount = 0;
bool integratedRoutine1;

void integrated1() {
  if (!playing_song && integratedRoutine1 && playcount ==0){
    integrated1Start = millis();
    MP3Trigger.trigger(6);
    playcount = 1;
    playing_song = true;
  }
  /*
  if (!displaying_int1){
    words = "meow";
    display_words();
    displaying_int1 = true;
  }
  */
  
  moveServo();
  lightsFlash();
  

  // if the integrated routine is over (30 seconds)
  if (integrated1Start + 30000 < millis()){
    MP3Trigger.stop();
    MP3Trigger.update();
    //playing_song = false;
    integratedRoutine1 = false;
    integrated1Start = millis();
    MP3Trigger.trigger(11);
    MP3Trigger.update();
    return;
  }
  if (integratedMoveMillis == 0) {
    integratedMoveMillis = millis();
  }
  if (integratedMoveMillis + 2000 < millis()) {
    moveForwardIntegrated = !moveForwardIntegrated;
    integratedMoveMillis = millis();
  }
  if (moveForwardIntegrated) {
    //Serial.print(F("trying to move forward"));
    val = 40;
  } else {
    //Serial.print(F("trying to move backwards"));
    val = -40;
    }

  ST->drive(0);
  ST->turn(val);

  // check for stopping routine
  // when routine stops, clear display
}


// =======================================================================================
//                                 Main Program
// =======================================================================================
// =======================================================================================
//                          Initialize - Setup Function
// =======================================================================================
void setup()
{
  
    //Debug Serial for use with USB Debugging
    Serial.begin(115200);
    while (!Serial);
    
    if (Usb.Init() == -1)
    {
        Serial.print(F("\r\nOSC did not start"));
        while (1); //halt
    }

    strcpy(output, "");
    
    Serial.print(F("\r\nBluetooth Library Started"));
    
    //Setup for PS3 Controller
    PS3Controller->attachOnInit(onInitPS3Controller); // onInitPS3Controller is called upon a new connection

    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);

    // Setup for servo and lights
    myServo.attach(2);
    LEDControl.begin();

/*
    // Setup for Display
    display.begin(SSD1306_SWITCHCAPVCC, 0X3C);
    display.display();
    delay(2000);
    display.clearDisplay();
*/

    // Setup for Sound
    MP3Trigger.setup(&Serial2);
    Serial2.begin(MP3Trigger::serialRate());

    //Setup for motors (saber 2x12)
    Serial1.begin(9600);
    ST->autobaud();
    ST->setTimeout(200);
    ST->setDeadband(driveDeadBandRange);


    for (int i = 0; i < 12; i++) {
      LEDControl.setPWM(i, 0);
      LEDControl.setPWM(i, 0);
    }
    LEDControl.write();
    
    integratedRoutine1 = false;


    

}

// =======================================================================================
//           Main Program Loop - This is the recurring check loop for entire sketch
// =======================================================================================
void loop()
{   
   // Make sure the Bluetooth Dongle is working - skip main loop if not
   if ( !readUSB() )
   {
     //We have a fault condition that we want to ensure that we do NOT process any controller data
     printOutput(output);
     return;
   }
    
   // Check and output PS3 Controller inputs
   checkController();
  
   // Ignore extra inputs from the PS3 Controller
   if (extraClicks)
   {
      if ((previousMillis + 500) < millis())
      {
          extraClicks = false;
      }
   }
  
   // Control the Main Loop Blinker
   if ((blinkMillis + 500) < millis()) {
      if (blinkOn) {
        digitalWrite(13, LOW);
        blinkOn = false;
      } else {
        digitalWrite(13, HIGH);
        blinkOn = true;
      }
      blinkMillis = millis();
   }

   printOutput(output);

   if (integratedRoutine1) {
    integrated1();
   }

   MP3Trigger.update();
}

// =======================================================================================
//          Check Controller Function to show all PS3 Controller inputs are Working
// =======================================================================================
void checkController()
{
       if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(UP) && !extraClicks)
     {              
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: UP Selected.\r\n");
            #endif
            
            previousMillis = millis();
            extraClicks = true;
            
     }
  
     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(DOWN) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: DOWN Selected.\r\n");
            #endif                     
            
            previousMillis = millis();
            extraClicks = true;
       
     }

     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(LEFT) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: LEFT Selected.\r\n");
            #endif  

            previousMillis = millis();
            extraClicks = true;

            
            integratedRoutine1 = !integratedRoutine1;

     }
     
     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(RIGHT) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: RIGHT Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
                     
     }
     
     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(CIRCLE) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: CIRCLE Selected.\r\n");
            #endif      
            
            previousMillis = millis();
            extraClicks = true;
           
     }

     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(CROSS) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: CROSS Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
              
     }
     
     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(TRIANGLE) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: TRIANGLE Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
              
     }
     

     if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(SQUARE) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: SQUARE Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
              
     }
     
     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(L1))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: LEFT 1 Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(L2))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: LEFT 2 Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(R1))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: RIGHT 1 Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(R2))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: RIGHT 2 Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(SELECT))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: SELECT Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(START))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: START Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && !extraClicks && PS3Controller->getButtonPress(PS))
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: PS Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     }

     if (PS3Controller->PS3Connected && ((abs(PS3Controller->getAnalogHat(LeftHatY)-128) > joystickDeadZoneRange) || (abs(PS3Controller->getAnalogHat(LeftHatX)-128) > joystickDeadZoneRange)))
     {
            
            int currentValueY = PS3Controller->getAnalogHat(LeftHatY) - 128;
            int currentValueX = PS3Controller->getAnalogHat(LeftHatX) - 128;
            
            char yString[5];
            itoa(currentValueY, yString, 10);

            char xString[5];
            itoa(currentValueX, xString, 10);

            if (!integratedRoutine1) {
              moveDroid(-currentValueY, -currentValueX);
            }
           
            #ifdef SHADOW_DEBUG
                strcat(output, "LEFT Joystick Y Value: ");
                strcat(output, yString);
                strcat(output, "\r\n");
                strcat(output, "LEFT Joystick X Value: ");
                strcat(output, xString);
                strcat(output, "\r\n");
            #endif       
     } else{
            // stop the motors
            if (!integratedRoutine1){
              ST->stop();
            }
            #ifdef SHADOW_DEBUG
               // strcat(output, "not moving");
            #endif 
     }

     if (PS3Controller->PS3Connected && ((abs(PS3Controller->getAnalogHat(RightHatY)-128) > joystickDeadZoneRange) || (abs(PS3Controller->getAnalogHat(RightHatX)-128) > joystickDeadZoneRange)))
     {
            int currentValueY = PS3Controller->getAnalogHat(RightHatY) - 128;
            int currentValueX = PS3Controller->getAnalogHat(RightHatX) - 128;

            char yString[5];
            itoa(currentValueY, yString, 10);

            char xString[5];
            itoa(currentValueX, xString, 10);

            #ifdef SHADOW_DEBUG
                strcat(output, "RIGHT Joystick Y Value: ");
                strcat(output, yString);
                strcat(output, "\r\n");
                strcat(output, "RIGHT Joystick X Value: ");
                strcat(output, xString);
                strcat(output, "\r\n");
            #endif       
     }
}

// =======================================================================================
//           Movement Functions
// =======================================================================================
void moveDroid(int footDriveSpeed,int turnnum) {

//idk why theyre switched
  if (turnnum > MAXTURNSPEED) {
    turnnum = MAXTURNSPEED;
  } else if (turnnum < -MAXTURNSPEED) {
    turnnum = -MAXTURNSPEED;
  }
  if (footDriveSpeed > MAXDRIVESPEED) {
    footDriveSpeed = MAXDRIVESPEED;
  } else if (footDriveSpeed < -MAXDRIVESPEED) {
    footDriveSpeed = -MAXDRIVESPEED;
  }

  currSpeed = footDriveSpeed;

  Serial.print(turnnum);
  Serial.print("\n footspeed: ");
  Serial.print(footDriveSpeed);
  
  ST->drive(turnnum);
  ST->turn(footDriveSpeed);
  
            #ifdef SHADOW_DEBUG
                strcat(output, "moving?");
            #endif       
     
}



// =======================================================================================
//           PPS3 Controller Device Mgt Functions
// =======================================================================================
// =======================================================================================
//           Initialize the PS3 Controller Trying to Connect
// =======================================================================================
void onInitPS3Controller()
{
    PS3Controller->setLedOn(LED1);
    isPS3ControllerInitialized = true;
    badPS3Data = 0;

    mainControllerConnected = true;
    WaitingforReconnect = true;

    #ifdef SHADOW_DEBUG
       strcat(output, "\r\nWe have the controller connected.\r\n");
       Serial.print("\r\nDongle Address: ");
       String dongle_address = String(Btd.my_bdaddr[5], HEX) + ":" + String(Btd.my_bdaddr[4], HEX) + ":" + String(Btd.my_bdaddr[3], HEX) + ":" + String(Btd.my_bdaddr[2], HEX) + ":" + String(Btd.my_bdaddr[1], HEX) + ":" + String(Btd.my_bdaddr[0], HEX);
       Serial.println(dongle_address);
    #endif
}

// =======================================================================================
//           Determine if we are having connection problems with the PS3 Controller
// =======================================================================================
boolean criticalFaultDetect()
{
    if (PS3Controller->PS3Connected)
    {
        
        currentTime = millis();
        lastMsgTime = PS3Controller->getLastMessageTime();
        msgLagTime = currentTime - lastMsgTime;            
        
        if (WaitingforReconnect)
        {
            
            if (msgLagTime < 200)
            {
             
                WaitingforReconnect = false; 
            
            }
            
            lastMsgTime = currentTime;
            
        } 
        
        if ( currentTime >= lastMsgTime)
        {
              msgLagTime = currentTime - lastMsgTime;
              
        } else
        {

             msgLagTime = 0;
        }
        
        if (msgLagTime > 300 && !isFootMotorStopped)
        {
            #ifdef SHADOW_DEBUG
              strcat(output, "It has been 300ms since we heard from the PS3 Controller\r\n");
              strcat(output, "Shut down motors and watching for a new PS3 message\r\n");
            #endif
            
//          You would stop all motors here
            isFootMotorStopped = true;
        }
        
        if ( msgLagTime > 10000 )
        {
            #ifdef SHADOW_DEBUG
              strcat(output, "It has been 10s since we heard from Controller\r\n");
              strcat(output, "\r\nDisconnecting the controller.\r\n");
            #endif
            
//          You would stop all motors here
            isFootMotorStopped = true;
            
            PS3Controller->disconnect();
            WaitingforReconnect = true;
            return true;
        }

        //Check PS3 Signal Data
        if(!PS3Controller->getStatus(Plugged) && !PS3Controller->getStatus(Unplugged))
        {
            //We don't have good data from the controller.
            //Wait 15ms - try again
            delay(15);
            Usb.Task();   
            lastMsgTime = PS3Controller->getLastMessageTime();
            
            if(!PS3Controller->getStatus(Plugged) && !PS3Controller->getStatus(Unplugged))
            {
                badPS3Data++;
                #ifdef SHADOW_DEBUG
                    strcat(output, "\r\n**Invalid data from PS3 Controller. - Resetting Data**\r\n");
                #endif
                return true;
            }
        }
        else if (badPS3Data > 0)
        {

            badPS3Data = 0;
        }
        
        if ( badPS3Data > 10 )
        {
            #ifdef SHADOW_DEBUG
                strcat(output, "Too much bad data coming from the PS3 Controller\r\n");
                strcat(output, "Disconnecting the controller and stop motors.\r\n");
            #endif
            
//          You would stop all motors here
            isFootMotorStopped = true;
            
            PS3Controller->disconnect();
            WaitingforReconnect = true;
            return true;
        }
    }
    else if (!isFootMotorStopped)
    {
        #ifdef SHADOW_DEBUG      
            strcat(output, "No PS3 controller was found\r\n");
            strcat(output, "Shuting down motors and watching for a new PS3 message\r\n");
        #endif
        
//      You would stop all motors here
        isFootMotorStopped = true;
        
        WaitingforReconnect = true;
        return true;
    }
    
    return false;
}

// =======================================================================================
//           USB Read Function - Supports Main Program Loop
// =======================================================================================
boolean readUSB()
{
  
     Usb.Task();
     
    //The more devices we have connected to the USB or BlueTooth, the more often Usb.Task need to be called to eliminate latency.
    if (PS3Controller->PS3Connected) 
    {
        if (criticalFaultDetect())
        {
            //We have a fault condition that we want to ensure that we do NOT process any controller data!!!
            printOutput(output);
            return false;
        }
        
    } else if (!isFootMotorStopped)
    {
        #ifdef SHADOW_DEBUG      
            strcat(output, "No controller was found\r\n");
            strcat(output, "Shuting down motors, and watching for a new PS3 foot message\r\n");
        #endif
        
//      You would stop all motors here
        isFootMotorStopped = true;
        
        WaitingforReconnect = true;
    }
    
    return true;
}

// =======================================================================================
//          Print Output Function
// =======================================================================================

void printOutput(const char *value)
{
    if ((strcmp(value, "") != 0))
    {
        if (Serial) Serial.println(value);
        strcpy(output, ""); // Reset output string
    }
}
