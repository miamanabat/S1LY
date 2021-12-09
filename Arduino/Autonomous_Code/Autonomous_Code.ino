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
//    Used for Autonomous Drive portion
float readingLeft  = 50;
float readingFront = 50;
bool isAutonomous = false;

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


// ---------------------------------------------------------------------------------------
//    Sonar Sensor
// ---------------------------------------------------------------------------------------
#define TRIGGER_PIN_FRONT 12
#define ECHO_PIN_FRONT 11
#define MAX_DISTANCE 400 // in centimeters
NewPing sonarFront(TRIGGER_PIN_FRONT, ECHO_PIN_FRONT, MAX_DISTANCE);
unsigned int pingSpeed = 100; // frequency of ping (in ms)


#define TRIGGER_PIN_LEFT 10
#define ECHO_PIN_LEFT 9
NewPing sonarLeft(TRIGGER_PIN_LEFT, ECHO_PIN_LEFT, MAX_DISTANCE);

int randTime;
bool playSound;
bool randomSound;
bool integratedRoutine;
MP3Trigger MP3Trigger;



int MAXDRIVESPEED = 60;
int MAXTURNSPEED = 40;
int currSpeed = 0;


// AUTO COURSE TUNING VARIABLES
int initialPauseTime = 3000;
int turnRightTime = 1000;
int turnRightSpeed = 60;
int straightSpeed = 60;
int leftAdjustSpeed = 20;
float adjustTurnDetectDistance = .5;
int numSonarReadValues = 9;
float frontDistanceDataPoints[8];
float leftDistanceDataPoints[8];
int takeSonarValue = 4;
long unsigned initialPauseTimer;
long unsigned straightTimerStart;
long unsigned pingTimer_front;
long unsigned pingTimer_left;

long unsigned turnRightStartTime;

bool autoCourseMode = false;
bool leftCheckComplete = false;
bool goStraight = false;
bool turnRight = false;
bool goBackwards = false;

int numStraight = 1;
int tapeOffsetValue = 0;
int currentDistanceFront = 120;
int currentDistanceLeft = 120;
int frontDistanceDataCounter = 0;
int leftDistanceDataCounter = 0;

int timeStraight1 = 0;
int timeStraight2 = 0;
int timeStraight3 = 0;
int timeStraight4 = 0;
int adjustLeftValue = 0;

// =======================================================================================
//                                 Servo and Lights
// =======================================================================================

Servo myServo;
int servoPos = 90;
int servo_dir = 1;

void moveServo(){
  servoPos = servoPos + (1 * servo_dir);
  if (servoPos > 100){
    servo_dir = -1;
  }
  if (servoPos < -100){
    servo_dir = 1;
  }
  myServo.write(servoPos);
  
}

#define latch 4
#define clock 5
#define data 6
Adafruit_TLC5947 LEDControl = Adafruit_TLC5947(1, clock, data, latch);
int ledMaxBright = 4000; // 4095 is MAX brightness
bool on = false;
size_t checkTiming = 0;

void lightsFlash(){
  checkTiming += 1;
  if (checkTiming%100000 == 0){
    if (on){
      LEDControl.setPWM(0, 0);
      LEDControl.setPWM(1, 0);
      LEDControl.setPWM(2, 0);
      LEDControl.setPWM(3, 0);
    }
    else{
      LEDControl.setPWM(0, ledMaxBright);
      LEDControl.setPWM(1, ledMaxBright);
      LEDControl.setPWM(2, ledMaxBright);
      LEDControl.setPWM(3, ledMaxBright);
    }
    LEDControl.write();
    on = !on;
    checkTiming = 0;
  }
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


    //Setup for motors (saber 2x12)
    Serial1.begin(9600);
    ST->autobaud();
    ST->setTimeout(200);
    ST->setDeadband(driveDeadBandRange);

    //Setup for sonar sensor    
    pingTimer_left = millis(); // start now
    pingTimer_front = millis() + 100; // start now

    randomSound = false;
    playSound = true;
    integratedRoutine = false;
    
    randomSeed(analogRead(0));
    randTime = (int)random(20000, 25000);
    MP3Trigger.setup(&Serial2);
    Serial2.begin(MP3Trigger::serialRate());

    myServo.attach(2);
    LEDControl.begin();
}

// =======================================================================================
//           Main Program Loop - This is the recurring check loop for entire sketch
// =======================================================================================
long soundMillis = millis();
bool soundWait = false;
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

   moveServo();
   lightsFlash();
  
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

  ST->turn(40);
  ST->drive(40);
   check_autonomous();
  
   //checkSound();
   //soundTrigger();
   //MP3Trigger.update();
   
// make sure to change state of random and integrated if one button is pressed
}

void check_autonomous() {
  checkAutoCourseToggle();

  if (autoCourseMode) {

    if (millis() >= pingTimer_front) {
      pingTimer_front = millis() + pingSpeed;
      sonarFront.ping_timer(echoCheckFront);
    }
    if (millis() >= pingTimer_left) {
      pingTimer_left = millis() + pingSpeed;
      sonarLeft.ping_timer(echoCheckLeft);
    }
    if (!leftCheckComplete && ((initialPauseTimer + initialPauseTime) < millis())) {
      tapeOffsetValue = currentDistanceLeft;
      leftCheckComplete = true;
      goStraight = true;
      straightTimerStart = millis();
    }
    if (goStraight && !goBackwards) {
      adjustLeft();
      goStraightFunction();
    }
    if (turnRight && !goBackwards) {
      turnRightFunction();
    }
    if(goStraight && goBackwards) {
      adjustLeft();
      //goStraightBackwardsFunction();
    }
  } 
}

// i think this is actually supposed to include a controller check to see if it is being called
void checkAutoCourseToggle() {
  if (PS3Controller->PS3Connected && PS3Controller->getButtonPress(TRIANGLE) && !extraClicks)
     {
            #ifdef SHADOW_DEBUG
                strcat(output, "Button: TRIANGLE Selected.\r\n");
            #endif       
            
            previousMillis = millis();
            extraClicks = true;
     
        if (!autoCourseMode) {
            autoCourseMode = true;
            leftCheckComplete = false;
            goStraight = false;
            turnRight = false;
            goBackwards = false;
            numStraight = 1;
            tapeOffsetValue = 0;
            currentDistanceFront = 120;
            currentDistanceLeft = 120;
            frontDistanceDataCounter = 0;
            leftDistanceDataCounter = 0;
            pingTimer_front = millis();
            pingTimer_left = millis() + 50;
            initialPauseTimer = millis();
            timeStraight1 = 0;
            timeStraight2 = 0;
            timeStraight3 = 0;
            timeStraight4 = 0;
            adjustLeftValue = 0;
        } else {
            autoCourseMode = false;
        }
     }
}

void adjustLeft() {
  float currentOffset = abs(tapeOffsetValue - currentDistanceLeft);

  if (currentOffset > adjustTurnDetectDistance) {

    if (tapeOffsetValue > currentDistanceLeft) {
      adjustLeftValue = leftAdjustSpeed;
    }

    if (tapeOffsetValue < currentDistanceLeft) {
      adjustLeftValue = leftAdjustSpeed * -1;
    }
  } else {
    adjustLeftValue = 0;
  }
  
}

void goStraightFunction() {
  if (currentDistanceFront > tapeOffsetValue) {
    ST->turn(adjustLeftValue);
    ST->drive(straightSpeed);
  } else {
    if (numStraight < 4) {
      goStraight = false;
      turnRight = true;
      turnRightStartTime = millis();
      if (numStraight == 1) {
        timeStraight1 = millis() - straightTimerStart;
      }
      if (numStraight == 2) {
        timeStraight2 = millis() - straightTimerStart;
      }
      if (numStraight == 3) {
        timeStraight3 = millis() - straightTimerStart;
      }
    } else {
      goBackwards = true;
      timeStraight4 = millis() - straightTimerStart;
    }
  }
  
}
/*
void goStraightBackwardsFunction() {
  
  if (goStraight) { // need to actually change this to timing
    ST->turn(adjustLeftValue);
    ST->drive(-straightSpeed);
  } else {
    if (numStraight < 4) {
      goStraight = false;
      turnRight = true;
      turnRightStartTime = millis();
      if (numStraight == 1) {
        timeStraight1 = millis() - straightTimerStart;
      }
      if (numStraight == 2) {
        timeStraight2 = millis() - straightTimerStart;
      }
      if (numStraight == 3) {
        timeStraight3 = millis() - straightTimerStart;
      }
    } else {
      goBackwards = true;
      timeStraight4 = millis() - straightTimerStart;
    }
  }
}
*/

void turnRightFunction() {
  if (currentDistanceFront > tapeOffsetValue) {
    ST->turn(20);
    ST->turn(1);
  } else {
    turnRight = false;
    goStraight = true;
  }

}

int sort_desc(const void *cmp1, const void *cmp2) {
  // need to cast the void * to int *
  float a = *((float*)cmp1);
  float b = *((float *)cmp2);

  // the comparison
  return a > b ? -1 : (a < b ? 1 : 0);
  
}

void checkSound() {
  if (randomSound) {
      // this is for the random sound routine
     if (soundMillis + randTime < millis()) {
        Serial.print("play sound");
        soundMillis = millis();
        if (!soundWait) { // sound will be waited on later
          MP3Trigger.stop();
          randTime = (int)random(5000, 7000);
          soundWait = true;
        } else {
          playSound = true;
          randTime = (int)random(10000, 12000);
          soundWait = false;
        }
     }

    if (playSound) {
      Serial.print("sound is playing\n");
      int randNum = (int)random(1, 6);
      playSound = false;
      Serial.print(randNum);
      MP3Trigger.trigger(randNum);
    } 

     
  } else if (integratedRoutine) {
      if (currSpeed < MAXDRIVESPEED) {
        // trigger stop sound
        MP3Trigger.setVolume(70);
        MP3Trigger.trigger(4);
        soundMillis = 0;    /// if used in the randomroutine might not work might have to set to millis
      } else {
        if (soundMillis = 0) {
          soundMillis = millis();
        } else if (soundMillis + 2000 < millis()) { // two seconds
          // increase volume min vol 70 or 80
          MP3Trigger.setVolume(40);
          
        } else if (soundMillis + 3000 < millis()) { // three seconds
          // increase volume
          MP3Trigger.setVolume(20);
        }
      }
    
  }

}


// =======================================================================================
//          Check for echo - sonar sensor
// =======================================================================================

int numPrevValues = 20; // number of values stored for std and mean, can be adjusted

float * prevSonarFront = new float[numPrevValues]; // values of prev numPrevValues for front sensor stored in array
int iFront = 0;   // indexing for storing in prevSonarFront
bool firstPassFront = true; // flag so stdev only calculated after prevSonarFront is filled

void echoCheckFront() { // ping_timer interrupt calls this function every 24 uS
  float sonarValue = 120;
  if (sonarFront.check_timer()) {
      sonarValue = (sonarFront.ping_result / US_ROUNDTRIP_CM) * 0.39370079;
  }

  frontDistanceDataPoints[frontDistanceDataCounter] = sonarValue;
  frontDistanceDataCounter++;

  if (frontDistanceDataCounter = numSonarReadValues) {
    int lt_length = sizeof(frontDistanceDataPoints) / sizeof(frontDistanceDataPoints[0]);
    qsort(frontDistanceDataPoints, lt_length, sizeof(frontDistanceDataPoints[0]), sort_desc);
    currentDistanceFront = frontDistanceDataPoints[takeSonarValue];
    frontDistanceDataCounter = 0;    
  } 
}


void echoCheckLeft() { // ping_timer interrupt calls this function every 24 uS
  float sonarValue = 120;
  if (sonarLeft.check_timer()) {
      sonarValue = (sonarLeft.ping_result / US_ROUNDTRIP_CM) * 0.39370079;
  }

  leftDistanceDataPoints[leftDistanceDataCounter] = sonarValue;
  leftDistanceDataCounter++;

  if (leftDistanceDataCounter = numSonarReadValues) {
    int lt_length = sizeof(leftDistanceDataPoints) / sizeof(leftDistanceDataPoints[0]);
    qsort(leftDistanceDataPoints, lt_length, sizeof(leftDistanceDataPoints[0]), sort_desc);
    currentDistanceLeft = leftDistanceDataPoints[takeSonarValue];
    leftDistanceDataCounter = 0;    
  } 
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


            if (randomSound) {
              randomSound = false;
            } else {
              randomSound = true;
            }
   
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

            if (integratedRoutine) {
              integratedRoutine = false;
            } else {
              integratedRoutine = true;
            }     
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

     
     if (PS3Controller->PS3Connected && ((abs(PS3Controller->getAnalogHat(LeftHatY)-128) > joystickDeadZoneRange) || (abs(PS3Controller->getAnalogHat(LeftHatX)-128) > joystickDeadZoneRange)))
     {
            
            int currentValueY = PS3Controller->getAnalogHat(LeftHatY) - 128;
            int currentValueX = PS3Controller->getAnalogHat(LeftHatX) - 128;
            currentValueY = currentValueY * -1;
            currentValueX = currentValueX * -1;

            char yString[5];
            itoa(currentValueY, yString, 10);

            char xString[5];
            itoa(currentValueX, xString, 10);
 
            moveDroid(currentValueY, currentValueX);
           
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
            ST->stop();
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
boolean criticalFaultDetect(){
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
        
    } else if (!isFootMotorStopped){
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
