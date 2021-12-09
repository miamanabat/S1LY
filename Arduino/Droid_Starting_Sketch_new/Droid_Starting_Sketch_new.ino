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
unsigned long pingTimerFront; // holds the next ping time

#define TRIGGER_PIN_LEFT 10
#define ECHO_PIN_LEFT 9
NewPing sonarLeft(TRIGGER_PIN_LEFT, ECHO_PIN_LEFT, MAX_DISTANCE);
unsigned long pingTimerLeft; // holds the next ping time

int randTime;
bool playSound;
bool randomSound;
bool integratedRoutine;
MP3Trigger MP3Trigger;



int MAXDRIVESPEED = 60;
int MAXTURNSPEED = 40;
int currSpeed = 0;

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
    pingTimerLeft = millis(); // start now
    pingTimerFront = millis() + 100; // start now

    randomSound = false;
    playSound = true;
    integratedRoutine = false;
    
    randomSeed(analogRead(0));
    randTime = (int)random(20000, 25000);
    MP3Trigger.setup(&Serial2);
    Serial2.begin(MP3Trigger::serialRate());
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

  
   //checkSound();
   //soundTrigger();
   MP3Trigger.update();
   
// make sure to change state of random and integrated if one button is pressed
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
  Serial.print("into echoCheckFront function");
  if (sonarFront.check_timer()) {
      if (iFront == numPrevValues) {
        iFront = 0;
        firstPassFront = false;
      }
    
      Serial.print("Ping: ");
      float sonarFrontValue = (sonarFront.ping_result / US_ROUNDTRIP_CM) * 0.39370079;
      prevSonarFront[iFront] = sonarFrontValue;
      Serial.print(sonarFrontValue);
      if (!firstPassFront) {
        float stdFront = getStdDev(prevSonarFront, numPrevValues);
        float meanFront = getMean(prevSonarFront, numPrevValues);
        if (sonarFrontValue - meanFront < stdFront) {
           Serial.print("passed");
           readingFront = sonarFrontValue;
        }
      }
      //Serial.print(sonarFrontValue);
      Serial.println("in");
      iFront = iFront + 1;
  }
  
}
float * prevSonarLeft = new float[numPrevValues];
int iLeft = 0;
bool firstPassLeft = true;

void echoCheckLeft() { // ping_timer interrupt calls this function every 24 uS
  Serial.print("into echoCheckLeft function");
  if (sonarLeft.check_timer()) {
      if (iLeft == numPrevValues) {
        iLeft = 0;
        firstPassLeft = false;
      }
      Serial.print("Ping: ");
      float sonarLeftValue = (sonarLeft.ping_result / US_ROUNDTRIP_CM) * 0.39370079;
      
      prevSonarLeft[iLeft] = sonarLeftValue;
      /*for (int i = 0; i < 20; i++) {
        Serial.print(prevSonarLeft[i]);
        Serial.print(" ");
      }*/
      Serial.print(sonarLeftValue);
      if (!firstPassLeft) {
        float stdLeft = getStdDev(prevSonarLeft, numPrevValues);
        float meanLeft = getMean(prevSonarLeft, numPrevValues);
        Serial.print("\nstd: ");
        Serial.print(stdLeft);
        Serial.print("\n");
        if (abs(sonarLeftValue - meanLeft) < 4*stdLeft + 2) { // absolute value
           Serial.print(" passed");                // here the value should be valid i think idk 
           readingLeft = sonarLeftValue;
        }                                           // not sure if this is too much computationally
      }
      //Serial.print(sonarLeftValue);
      Serial.println("in");
      iLeft = iLeft + 1;
  }
}



float getMean(float * val, int arrayCount) {          // got this code from online for mean and std using pointers in arduino 
                                                  // have not tested whether it works or not
  float total = 0;
  for (int i = 0; i < arrayCount; i++) {
    total = total + val[i];
  }
  float avg = total/(float)arrayCount;
  return avg;
}

float getStdDev(float * val, int arrayCount) {
  float avg = getMean(val, arrayCount);
  float total = 0;
  for (int i = 0; i < arrayCount; i++) {
    Serial.print(val[i]);
    Serial.print(" ");
    total = total + (val[i] - avg) * (val[i] - avg);
  }

  float variance = total/(float)arrayCount;
  float stdDev = sqrt(variance);
  return stdDev;
}
String songTitle[5] = {
  "Walk the Line", 
  "Blue Suede Shoes",
  "So Lonesome",
  "Folsom Prison",
  "Cheatin Heart"
};


/*
String songTitle[36] = {"Walk the Line",
                    "Ring of Fire",
                    "Blue Suede Shoes",
                    "So Lonesome",
                    "Folsom Prison",
                    "Cheatin Heart",
                    "Jolene",
                    "Big River",
                    "Blues Eyes Cryin",
                    "Imagine",
                    "Long Tall Sally",
                    "Pretty Woman",
                    "Peggy Sue",
                    "Everyday",
                    "La Bamba",
                    "Sweet Dreams",
                    "Desperado",
                    "The Twist",
                    "Respect",
                    "People Get Ready",
                    "Dock of the Bay",
                    "Dancing Streets",
                    "My Imagination",
                    "Stay Together",
                    "Papa New Bag",
                    "Stany By Me",
                    "Who Do You Love",
                    "My Generation",
                    "Yesterday",
                    "Mr Tambourine",
                    "Fighting Man",
                    "Paranoid",
                    "Highway to Hell",
                    "Roxanne",
                    "Lola",
                    "Love Rock N Roll"};
                    
int songTrack[36]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36};

long songLength[36]={166,157,136,169,165,163,162,168,140,184,129,180,150,128,124,211,121,156,148,159,162,158,181,198,128,178,150,199,127,150,139,173,208,195,250,175};

*/
int songTrack[5] = {1, 2, 3, 4, 5};
long songLength[5] = {166, 157, 136, 169, 165};

bool isPlaying = false;
bool volumeChanging = false;
int volumeMillis = 0;
bool volumeUp = false;
bool volumeFirstChange = true;
int volume = 80;

bool songChanging = false;
bool songUp = false;
bool songFirstChange = false;
bool startSong = false;
int songMillis = 0;
int percentage_song = 0;
int percentage_sound;
int song_index = 0;

/*
void soundTrigger() {

  if (songChanging){
    if (songFirstChange){
      songMillis = millis();
      songFirstChange = false;
    }
    if (songMillis + 25 < millis()){
      songMillis = millis();
      if (songUp){
         song_index = song_index + 1;
      } else{
        song_index = song_index - 1;
      }
    }
  }

  if (volumeChanging) {
    if (volumeFirstChange) {
      volumeMillis = millis();
      volumeFirstChange = false;
    } 
    if (volumeMillis + 250 < millis()) {
      volumeMillis = millis();
      if (volumeUp && volume > 5) {
        volume -= 5;
      } else if (volume < 250) {
        volume += 5;
      }
    }
  }

  
  if (isPlaying) {
    if (startSong) {
      MP3Trigger.trigger(song_index + 1);
      startSong = false;
      songMillis = millis();
    }
    percentage_song = (millis() - songMillis) / (1000 * songLength[song_index]);

    if (percentage_song >= 1) {
      songChanging = true;
      isPlaying = false;
      return;
    }
    song_display();
   
  } else {
    song_select_display();
  }

}

*/
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


void song_display(){
  String song2 = songTitle[(song_index+36)%36];
  display.begin(SSD1306_SWITCHCAPVCC, 0x3c);
  display.display();
  delay(2000);
  display.clearDisplay();

   display.setTextSize(1);
   display.setTextColor(WHITE);
   display.setCursor(0,0);
   display.println("Song Playing");
   display.println(song2);
   display.print("% Complete: ");
   display.println(percentage_song);
   display.display();
}

void song_select_display(){
  String song1 = songTitle[(song_index+35)%36];
  String song2 = songTitle[(song_index+36)%36];
  String song3 = songTitle[(song_index+37)%36];
  display.begin(SSD1306_SWITCHCAPVCC, 0x3c);
  display.display();
  delay(2000);
  display.clearDisplay();

   display.setTextSize(1);
   display.setTextColor(WHITE);
   display.setCursor(0,0);
   display.println(song1);
   display.print(">>" );
   display.println(song2);
   display.println(song3);
   display.display();
}




void sound_display(){
  display.begin(SSD1306_SWITCHCAPVCC, 0x3c);
  display.display();
  delay(2000);
  display.clearDisplay();

   display.setTextSize(2);
   display.setTextColor(WHITE);
   display.setCursor(0,0);
   display.print("Volume: ");
   percentage_sound = (255 - volume) / 255;
   display.print(percentage_sound);
   display.println("%");
   display.println("");
   display.print("% Complete: ");
   display.print(percentage_song);
   display.display();
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
            MP3Trigger.trigger(1);
            song_display();
           
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
            Serial.print("triangle pressed");
            MP3Trigger.stop();
            song_select_display();

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
     if (PS3Controller->PS3Connected && ((abs(PS3Controller->getAnalogHat(LeftHatY)-128) <= joystickDeadZoneRange) && (abs(PS3Controller->getAnalogHat(LeftHatX)-128) <= joystickDeadZoneRange)))
     {
            // stop the motors
            ST->stop();
            #ifdef SHADOW_DEBUG
               // strcat(output, "not moving");
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
