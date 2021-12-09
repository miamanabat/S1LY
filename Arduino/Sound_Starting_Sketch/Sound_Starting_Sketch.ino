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
//    Used for MP3Trigger/Sound/Display
// ---------------------------------------------------------------------------------------
MP3Trigger MP3Trigger;
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// state variables
boolean song_playing = false;
boolean song_started = false;
boolean song_change = true;

boolean song_scroll = false;
boolean volume_scroll = false;
boolean volume_change = false;

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

    MP3Trigger.setup(&Serial2);
    Serial2.begin(MP3Trigger::serialRate());

    display.begin(SSD1306_SWITCHCAPVCC, 0X3C);
    display.display();
    delay(2000);
    display.clearDisplay();
    song_select_display();

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

   if (song_playing) {
      play_song();
   } else { //if (song_change) {
      //song_change = false;
      if (song_scroll) {
        check_scroll();
      }
   }
   MP3Trigger.update();
   printOutput(output);

   
}
//************************************************
// ND Jukebox
//************************************************

//************************************************
// Juke Box Variables
//************************************************
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
// ---------------------------------------------------------------------------------------
//    Song Sound/Display
// ---------------------------------------------------------------------------------------
int song_index=0;
long start_of_song = 0;
long soundMillis = 0;
long song_length = 0;
long song_percentage = 0;

int song_change_value = 1;
long song_change_millis = 0;

int volume_change_value = 1;
int volume_value = 50;
long volume_percentage = ((255-50) * 100)/255;
long volume_change_millis = 0;

void play_song() {
  if (volume_scroll)
    check_volume_scroll();
  if (!song_started) {
    song_started = true;
    start_of_song = millis();
    soundMillis = millis();
    song_percentage = 0;
    song_length = songLength[song_index];
    MP3Trigger.trigger(songTrack[song_index]);
    //MP3Trigger.trigger(songTrack[song_index]);
    //MP3Trigger.trigger(songTrack[song_index]);


    if (!volume_scroll)
      display_song();
  } else { // if the song is already playing
    if (soundMillis + 400 < millis()) {
      soundMillis = millis();
      song_percentage = (millis() - start_of_song) / (song_length*10);
      if (song_percentage >= 100) {  // check to see if the song ended
        song_playing = false;
        song_change = true;
        song_started = false;
        song_select_display(); 
        return;
      }
      if (!volume_scroll)
        display_song();
    }
  }
}

void check_scroll() {
  if (song_change_millis + 100 < millis()) {
      song_change_millis = millis();
      if (song_index < 35 and song_change_value == 1) {
        song_index += 1;
      } else if (song_index > 0 and song_change_value == -1) {
        song_index -= 1;
      }
      if (!volume_scroll)
        song_select_display();
    }
}

void check_volume_scroll(){
  if (volume_change_millis + 100 < millis()){
    volume_change_millis = millis();
    if (volume_value < 250 and volume_change_value == 1){
      volume_value += 5;
    } else if (volume_value > 5 and volume_change_value == -1){
       volume_value -= 5;
    }
    printOutput("volume value");
    Serial.println(volume_value);
    MP3Trigger.setVolume(255-volume_value);
    //MP3Trigger.setVolume(1);
    display_volume();
  }
}


void display_song() {
  String song2 = songTitle[song_index];
  display.begin(SSD1306_SWITCHCAPVCC, 0x3c);
  //delay(2000); this messed up the stopping before
  display.clearDisplay();

   display.setTextSize(1);
   display.setTextColor(WHITE);
   display.setCursor(0,0);
   display.println();
   display.println();
   display.println("Song Playing");
   display.println(song2);
   display.print("% Complete: ");
   display.print(song_percentage);  
   display.println("%");
   display.display();

}

void display_volume() {
    display.begin(SSD1306_SWITCHCAPVCC, 0x3c);
    volume_percentage = (volume_value*100)/255;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println();
    display.println();
    display.print("Volume: ");
    display.print(volume_percentage);
    display.println("%");
    display.println();
    display.print("% Complete: ");
    display.print(song_percentage);
    display.println("%");
    display.display();
}

void song_select_display(){
  String song1 = "";
  String song3 = "";
  if (song_index != 0) {
    song1 = songTitle[song_index - 1];
  }
  String song2 = songTitle[song_index];
  if (song_index != 35) {
    song3 = songTitle[song_index + 1];
  }
  display.begin(SSD1306_SWITCHCAPVCC, 0x3c);
  //delay(2000);
  display.clearDisplay();

   display.setTextSize(1);
   display.setTextColor(WHITE);
   display.setCursor(0,0);
   display.println();
   display.println();
   display.println(song1);
   display.print(">>" );
   display.println(song2);
   display.println(song3);
   display.display();
}

// =======================================================================================
//          Check Controller Function to show all PS3 Controller inputs are Working
// =======================================================================================
void checkController()
{
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

            song_playing = true;
            song_started = false;
              
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

            if (song_playing) {
              MP3Trigger.stop();
            }

            song_playing = false;
            song_change = true;
            song_started = false;
            song_select_display(); 

              
     }
     
     if (PS3Controller->PS3Connected && ((abs(PS3Controller->getAnalogHat(LeftHatY)-128) > joystickDeadZoneRange) || (abs(PS3Controller->getAnalogHat(LeftHatX)-128) > joystickDeadZoneRange)))
     {
            
            int currentValueY = PS3Controller->getAnalogHat(LeftHatY) - 128;
            int currentValueX = PS3Controller->getAnalogHat(LeftHatX) - 128;
            
            char yString[5];
            itoa(currentValueY, yString, 10);

            char xString[5];
            itoa(currentValueX, xString, 10);

            #ifdef SHADOW_DEBUG
                strcat(output, "LEFT Joystick Y Value: ");
                strcat(output, yString);
                strcat(output, "\r\n");
                strcat(output, "LEFT Joystick X Value: ");
                strcat(output, xString);
                strcat(output, "\r\n");
            #endif

            if (currentValueY >= 20 && !volume_scroll) {
              volume_scroll = true;
              volume_change = true;
              volume_change_millis = millis();
              volume_change_value = -1;
            } else if (currentValueY <= -20 && !volume_scroll) {
              volume_scroll = true;
              volume_change = true;
              volume_change_millis = millis();
              volume_change_value = 1;
            }  else if (currentValueY < 20 && currentValueY > -20) {
              volume_scroll = false;
              volume_change = false;
            } else if (volume_scroll) {
              volume_change = true;
            }
     } else {
            volume_scroll = false;
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

            if (currentValueY >= 20 && !song_scroll) {
              song_scroll = true;
              song_change = true;
              song_change_millis = millis();
              song_change_value = 1;
            } else if (currentValueY <= -20 && !song_scroll) {
              song_scroll = true;
              song_change = true;
              song_change_millis = millis();
              song_change_value = -1;
            }  else if (currentValueY < 20 && currentValueY > -20) {
              song_scroll = false;
              song_change = false;
            } else if (song_scroll) {
              song_change = true;
            }
     } else {
            song_scroll = false;
     }

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
