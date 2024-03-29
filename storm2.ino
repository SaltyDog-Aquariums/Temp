/*
 ################################################################################################
 #### Firmware for Storm LED Controller 10/8/2013 ###############################################
 #### Distributed under GNU GPL V3 http://www.gnu.org/licenses/gpl.html #########################
 ####                                                                                        ####
 #################################### Copyright 2013 Coralux LLC ################################
 #################################### Registration TXUOO1865413 #################################
 ################################################################################################
 # Much of the code here builds off of existing libraries contributed by the dedicated maker ####
 # community. Proper attribution is given to the best of my knowledge. Please contact me for ####
 # any oversights thereof. ######################################################################
 ################################################################################################
 
 #### REVISION HISTORY ##########################################################################
  
 10/6/2013 Switched from using AnalogWrite function to custom AnalogDirect using port manipulation
 10/7/2013 Added 25 KHz Fan Control to pins D9, D10, compatible with 4-wire PWM Fans with 
 separate fan power supply (typically 12V)
 10/7/2013 Completely reworked temperature sensing code for efficiency and compactness
 Added temperature display mode toggle for degrees C or F
  ################################################################################################
 
 ### CREDITS ####################################################################################
 # Rotary encoder handler for arduino.
 # Copyright 2011 Ben Buxton. Licenced under the GNU GPL Version 3.
 # Contact: bb@cactii.net
 # Quick implementation of rotary encoder routine.
 # More info: http://www.buxtronix.net/2011/10/rotary-encoders-done-properly.html
 ################################################################################################
 
 ################################################################################################
 # LCD Backlight Manager - Copyright (c) 2010 Andy Brown
 # http://andybrown.me.uk/wk/2010/11/28/lcd-backlight-and-contrast-manager/
 # This work is licensed under a Creative Commons
 # Attribution_ShareAlike 3.0 Unported License.
 # http://creativecommons.org/licenses/by_sa/3.0/
 ################################################################################################
 
 ################################################################################################
 # Sunrise Sunset functions handled by Swiftek TimeLord Arduino Library
 # http://www.swfltek.com/arduino/timelord.html
 ################################################################################################
 f
 ################################################################################################
 # Button bounce handling by Arduino Bounce Library
 # http://playground.arduino.cc/code/bounce
 ################################################################################################
 
 ################################################################################################
 # RTC Library https://github.com/adafruit/RTClib
 # Arduino OneWire http://playground.arduino.cc/Learning/OneWire
 
 ################################################################################################
 */

#include <avr/pgmspace.h>
//#include <util/delay.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <Bounce.h>
#include <EEPROM.h>
#include "RTClib.h"
#include "TimeLord.h"
#include <OneWire.h>

#include <rotary.h>

RTC_DS1307 RTC;

#define BUTTON 7
#define ENCODER_A 8
#define ENCODER_B 13
#define MAXBRIGHT 255
#define RESOLUTION 10
#define TEMP_READ_DELAY 1000
#define DS18S20_ID 0x10
#define DS18B20_ID 0x28
#define HALF_STEP

//long looptimer = 0;
//int loopcounter = 0;

unsigned long clocktimer = 0;

char p_buffer[17]; // buffer for using PROGMEM to store strings to save RAM
#define P(str) (strcpy_P(p_buffer, PSTR(str)), p_buffer) 

float lastTemperature = 0.0;
unsigned long lastTempRequest = 0;
byte addr[8]; // address of temperature sensor
byte sensorConnected = 1; 

#define ONE_WIRE_BUS 12 // Data wire is plugged into pin 2 on the Arduino

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire ds(ONE_WIRE_BUS);


// use these vars for pin interrupt encoder routine
Rotary r = Rotary(ENCODER_A, ENCODER_B);
volatile int position = 0;
int lastPosition = 0;

//byte channels[6] = { 3,5,6,9,10,11 };

// begin eeprom variables
boolean flash_counted[] = {
  0,0,0,0,0,0};
byte flash_duration[] = {
  0,0,0,0,0,0};
byte flash_max_count[] = {
  0,0,0,0,0,0};
byte flash_loop_counter[] = {
  0,0,0,0,0,0};
byte flash_loop_number[] = {
  0,0,0,0,0,0};
boolean main_flash[] = {
  0,0,0,0,0,0};
byte flash_count[] = {
  0,0,0,0,0,0};
long timer[] = {
  0,0,0,0,0,0};
//int counter = 0;

int value_advance(int setpoint,int minimum,int maximum,byte cursor_pos_A,byte cursor_pos_B, byte i=2, byte j=2); // bug workaround, must define primitive of a function with default parameters

byte transitionTime = 0; // duration (seconds) of Sunrise and Sunset transition
boolean auto_mode = 0;
// default time zone in San Francisco, CA
int latitude = 0;
int longitude = 0;
int gmt_offset = 0;
int moonphase = 0;
byte pwm_settings[5][6] = {
  {
    0,0,0,0,0,0                                                  }
  ,{
    0,0,0,0,0,0                                                  }
  ,{
    1,0,0,0,0,0                                                  }
  , {
    0,1,1,1,4,5                                      }
  , {
    0,0,0,0,0,0                }
};

// row 1 = day pwm settings
// row 2 = night pwm settings
// row 3 = delay settings (for separate sunrise/sunset delays)
// row 4 = cloud channel (for synchronizing certain banks of LEDs during atmospheric effects)
// row 5 - night pwm settings adjusted for moonphase

byte current_settings[] = {
  0, 0, 0, 0, 0, 0};

byte sunrise_time[2][2] = {
  {
    0,0                                                                                      }
  ,{
    0,0                                                                                      }
};
int brightness = 0; // manual brightness

//byte currentLimit = 100; // Limit current on all Buckpucks to currentLimit percent
byte dayStatus = 4; // day/night/sunrise/sunset indicator
byte toggleMode = 1; // user override mode to set day/off/night/program lighting
byte moonphase_toggle;
byte menu[7][2] = {
  {
    3,0                                                                                            }
  , {
    16,0                                                                                            }
  , {
    6,0                                                                                            }
  , {
    5,0                                                                                            }
  ,{
    1,0                                                                                            }
  ,{
    0,0                                                                                }
  ,{ 
    2,0                                                                         }
};


boolean cloud_last_mode = 1;
boolean last_mode = 1;
boolean prev_value = HIGH;
long hold_time = 0;
long timeDisp = 100;
boolean ynchoice = 1;
//byte subbars = 0;
byte remainder = 0;
//int button = 0;
byte lastPress = 1;

long cloudStart[] = {
  0,0,0,0,0,0};
byte cloudLength[] = {
  0,0,0,0,0,0};
byte cloudRamp[] = {
  2,2,2,2,2,2};
int cloud_adjustment[] = {
  0,0,0,0,0,0};
byte cloudDepth[] = {
  9,9,9,9,9,9};
byte cloud_max_adjustment[] = {
  0,0,0,0,0,0};
unsigned long cloudTimer[] = {
  0,0,0,0,0,0};
byte cloudMarker[] = {
  0,0,0,0,0,0};
byte cloud_switch = 0;
byte lightning_switch;
boolean dstMode; // (1) use Daylight Savings Time or (0) not
byte tempMode; // temperature mode C(0) or F(1)
byte fanMode; // fan control on [1] or off [0]
// lightning generator
unsigned long lastTime[] = {
  0,0,0,0,0,0};
int waitTime[] = {
  0,0,0,0,0,0};

//char* channels[] = {  "CH1 ", "CH2 ", "CH3 ", "CH4 ", "CH5 ", "CH6 "};

//initialize the time holder
unsigned int clock_settings[] = {
  1, 1, 1, 1, 1, 1};

byte x = 0;
byte y = 0;
//int foo=0;

byte increment = 0;

long sunriseTime = 0;
long sunsetTime = 0;
byte day[]  = {
  0, 0, 0, 0, 0, 0};
byte lastDay = 0;

Bounce bouncer = Bounce( BUTTON,5 ); // Instantiate a Bounce object with a 5 millisecond debounce time

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(2, 4, A3, A2, A1, A0);

// LCD special characters for [Manual] bar graph
/*
byte p1[8] = {
 0x10,
 0x10,
 0x10,
 0x10,
 0x10,
 0x10,
 0x10,
 0x10};
 
 byte p2[8] = {
 0x18,
 0x18,
 0x18,
 0x18,
 0x18,
 0x18,
 0x18,
 0x18};
 
 byte p3[8] = {
 0x1C,
 0x1C,
 0x1C,
 0x1C,
 0x1C,
 0x1C,
 0x1C,
 0x1C};
 
 byte p4[8] = {
 0x1E,
 0x1E,
 0x1E,
 0x1E,
 0x1E,
 0x1E,
 0x1E,
 0x1E};
 */

// function to determine when a button has been pressed
// differentiates between a short press and a long press based on hold_time
int buttonPress()
{
  bouncer.update ( ); // Update the debouncer
  byte value = bouncer.read(); // Get the update value
  if ( value == HIGH && prev_value == HIGH ) { // Encoder button is not pressed
    prev_value = value;
    return 0;
  } 
  else if ( value != HIGH && prev_value == HIGH ) { // Catch leading edge of encoder button press, start timing
    prev_value = value;
    hold_time = millis();
    return 0;
  } 
  else if ( value == HIGH && prev_value != HIGH ) { // Catch trailing edge of encoder button press
    prev_value = value;

    if ( millis() - hold_time <= 500 ) { 
      lastPress = 1;
      return 1; // Register a short button press
    }
  } 
  else if ( value != HIGH && prev_value != HIGH ) { // Encoder button is being held down
    if ( (millis() - hold_time > 500) && lastPress != 2 ) { 
      lastPress = 2;
      return 2; // Register a long button press
    } 
    else {
      return 0;
    }
  } 
  else {
    return 0;
  }

}

// subroutine for setting the user-defined PWM channel values
// i,j is the array address of the value to be set
void set_channel(int i,int j) {

  // this section just figures out the correct index for channels 1-3 vs 4-6 for lcd display purposes
  if ( j <= 2 ) {
    x = 5*j;
    y = 0;
  } 
  else {
    x = 5*(j - 3);
    y = 1;
  }

  lcd.blink();
  lcd.noCursor();
  byte m;
  if (i == 2) { 
    m = 2; 
  } 
  else { 
    m = 3; 
  }
  if ( i == 3 ) {
    value_display(pwm_settings[i][j],1,x+1,y);
  } 
  else {
    value_display(pwm_settings[i][j],m,x+1,y);
  }

  byte limit;
  byte lower_limit = 0;
  if (i==0 or i == 1) { 
    limit = MAXBRIGHT; 
  } 
  else if (i == 2) { 
    limit = transitionTime; 
  } 
  else if (i == 3) {
    limit = 6;
    lower_limit = 0;
  }

  pwm_settings[i][j] = value_advance(pwm_settings[i][j], lower_limit, limit, x+1, y, i, j);
  if (i == 0) { // day pwm settings
    EEPROM.write(j+7,pwm_settings[i][j]);
  } 
  else if (i == 1) { // night pwm settings
    EEPROM.write(j+13,pwm_settings[i][j]);
  } 
  else if (i == 2) { // ramp delays
    EEPROM.write(j+19,pwm_settings[i][j]);
  } 
  else if (i == 3) { // cloud channels
    EEPROM.write(j+25,pwm_settings[i][j]);
    flash_loop_number[pwm_settings[i][j]] = count_channels(pwm_settings[i][j]); // update the count of how many pwm strings are on this channel (for lightning optimization)
  }
  lcd.setCursor(x+1,y);
  lcd.print(P("CH"));
  lcd.print(j+1);
  lcd.noBlink();
  lcd.cursor();

}


// sets clock based on (current value to be modified -eg 16, low limit for the value -eg 0, 
// high limit for the value -eg 59, and screen position i,j on the lcd
int set_clock(int val, int low_limit, int limit, byte i, byte j) {

  lcd.blink();
  lcd.noCursor();
  //char buf[4];
  //clock_settings[menu[3][1]] = val;
  clock_settings[menu[3][1]] = value_advance(val,low_limit,limit,i,j);

  lcd.noBlink();
  lcd.cursor();
  for (byte i = 0; i<=5; i++) {
    cloudStart[i] = 0; // gotta reset the cloudStart time so that clouds still work if we've set the time back
  }
  last_mode = 1; // reset this so that the sunrise & sunset are recalculated after the clock change
  return clock_settings[menu[3][1]];
}


// subroutine for user-defined sunrise and sunset times
void sunrise_sunset(byte i) {
  lcd.noBlink();
  lcd.cursor();
  byte exitFlag = 0;

  while ( !exitFlag ) {

    lcd.setCursor(1,1);
    lcd.print(P(">"));
    while ( !exitFlag ) {

      menu_advance(4);

      switch (menu[4][1]) {
      case 0:
        lcd.setCursor(2,1);
        break;
      case 1:
        lcd.setCursor(5,1);
        break;

      }

      switch ( buttonPress() ) {
      case 1: // the button has been short-pressed! set the sunrise or sunset time
        switch (menu[4][1]) {
        case 0: // set the hour
          sunrise_time[i][0] = set_sunrise(i,menu[4][1],2,23);
          EEPROM.write(i*2+31,sunrise_time[i][0]);
          break;
        case 1: // set the minute
          sunrise_time[i][1] = set_sunrise(i,menu[4][1],5,59);
          EEPROM.write(i*2+32,sunrise_time[i][1]);
          break;
        }

        break;
      case 2: // long button press, exit the loop
        exitFlag = 1;
        break;
      }
    }
  }
  exitFlag = 0;
  lcd.setCursor(0,1);
}

// sets the sunrise-sunset times
// indices i,j of where to find the current value, cursor position k, maximum limit
int set_sunrise(byte i, byte j, byte k, int limit) {

  sunrise_time[i][j] = value_advance(sunrise_time[i][j],0,limit,k,1);

  last_mode = 1; // this trips the sunrise recalculation algorithm
  return sunrise_time[i][j];
}

void menu_advance(int i){
  if ( position > lastPosition ) {
    lastPosition = position;
    timeDisp = millis();
    if ( menu[i][1] < menu[i][0] ) {
      menu[i][1]++;
      switch (i) {
      case 2:
        break;
      case 3:
        break;
      case 4: 
        break;
      case 6:
        break;
      default:
        lcd.clear();
        break;
      }
    }
  }

  else if ( position < lastPosition) {
    lastPosition = position;
    timeDisp = millis();
    if ( menu[i][1] > 0 ) {
      menu[i][1]--;
      switch (i) {
      case 2:
        break;
      case 3:
        break;
      case 4: 
        break;
      case 6:
        break;
      default:
        lcd.clear();
        break;
      }
    }

  }
}

// subroutine for displaying and setting the values that one sees as the encoder is turned cw and ccw
int value_advance(int setpoint,int minimum,int maximum,byte cursor_pos_A,byte cursor_pos_B, byte i, byte j) {

  // i and j are specific to adjustments of the PWM settings for day or night
  // i tells the function whether it is day (0), night (1), or neither (2)
  // in the case of (0) or (1), it will update the brightness in real-time as the
  // user adjusts the value.

  // keep the value in bounds!
  if (setpoint < minimum) {
    setpoint = minimum;
  } 
  else if (setpoint > maximum) {
    setpoint = maximum;
  }

  while ( buttonPress() != 1 ) {

    if ( position > lastPosition ) {
      lastPosition = position;

      timeDisp = millis();
      if ( setpoint < maximum ) {

        setpoint+=1;

        lcd.setCursor(cursor_pos_A,cursor_pos_B);
        if ( minimum >= 0 ) {
          if ( setpoint < 10 ) { 
            lcd.print(P("0")); 
          }
          if ( maximum > 100 && setpoint < 100 ) { 
            lcd.print(P("0")); 
          }
        } 
        else if (setpoint >= 0 && minimum < 0) {
          lcd.print(P(" "));
        }
        lcd.print(setpoint);
        if ( minimum < 0 ) {
          lcd.print(P(" "));
        }
      }
    }

    else if ( position < lastPosition ) {
      lastPosition = position;

      timeDisp = millis();
      if ( setpoint > minimum ) {

        setpoint-=1;
        lcd.setCursor(cursor_pos_A,cursor_pos_B);
        if ( minimum >= 0 ) {
          if ( setpoint < 10 ) { 
            lcd.print(P("0")); 
          }
          if ( maximum > 100 && setpoint < 100 ) { 
            lcd.print(P("0")); 
          }
        } 
        else if (setpoint >= 0 && minimum < 0) {
          lcd.print(P(" "));
        }
        lcd.print(setpoint);
        if ( minimum < 0 ) {
          lcd.print(P(" "));
        }
      }


    }
    lcd.setCursor(cursor_pos_A,cursor_pos_B);

    if ( i == 0 || i == 1) {
      analogDirect(j,setpoint); // real-time 
    }

  }

  return setpoint;
}


// displays a set value the first time the menu loads
void value_display(int setpoint, byte digits, byte cursor_pos_A,byte cursor_pos_B) {
  lcd.setCursor(cursor_pos_A,cursor_pos_B);
  if (setpoint < 10 && digits > 1) {
    for (byte i=1; i<= digits-1; i++ ) {
      lcd.print(P("0"));
    }
  } 
  else if (setpoint < 100 && digits >2) {
    lcd.print(P("0"));
  }
  lcd.print(setpoint);
  lcd.print(P(" "));
  if (setpoint < 10 && digits == 1) {
    lcd.print(P(" "));
  }
}

// display the generic CH1 CH2 CH3 labels
void channel_display() {
  byte k = 0;
  for (byte i=0; i<=1; i++) {
    lcd.setCursor(0,i);
    for (byte j=1; j<=3; j++) {
      k++;
      lcd.print((char)0xA5);
      lcd.print(P("CH"));
      lcd.print(k);
      lcd.print(P(" "));
    }
  }
}

// menu system for scrolling through the different pwm channels 1-6
void pwm_config(int i) {
  lcd.noBlink();
  lcd.cursor();
  boolean exitFlag = 0;
  while ( !exitFlag ) {
    lcd.clear();
    channel_display();

    do_light(i);

    boolean f = 0;
    while ( !exitFlag ) {
      menu_advance(2);
      switch (menu[2][1]) {
      case 0:
        lcd.setCursor(0,0);
        break;
      case 1:
        lcd.setCursor(5,0);
        break;
      case 2:
        lcd.setCursor(10,0);
        break;
      case 3:
        lcd.setCursor(0,1);
        break;
      case 4:
        lcd.setCursor(5,1);
        break;
      case 5:
        if ( f == 1 ) {
          channel_display();
          lcd.cursor();
          f = 0;
        }
        lcd.setCursor(10,1);
        break;
      case 6:
        if ( f == 0 ) {
          lcd.clear();
          for (byte j=0;j<=5;j++) {
            if (j == 0){
              lcd.setCursor(0,0);
            }
            else if (j == 3) {
              lcd.setCursor(0,1);
            }
            char buffer[6];
            sprintf(buffer,"%c%3d ",0xA5,pwm_settings[i][j]);
            lcd.print(buffer);
          }
          lcd.noCursor();
          f = 1;
        }
        break;
      }
      switch ( buttonPress() ) {
      case 1:
        set_channel(i,menu[2][1]);
        break;
      case 2:
        exitFlag = 1;
        break;
      }
    }
    exitFlag = 1;
    break;
  }
  lcd.clear();
  exitFlag = 0;
}

void moonphase_adjust(byte foo) { // propagates the moonphase intensity through to the PWM settings for night
  for (byte i=0; i<=6;i++) {
    if (foo) { // use moonphase adjustment

      pwm_settings[4][i] = pwm_settings[1][i] * (moonphase/100.0);   // multiplies the night pwm value by 0 to 0.99 depending on moonphase

    } 
    else {
      pwm_settings[4][i] = pwm_settings[1][i]; // boring but necessary

    }
  }
  //  return 1;
}


byte make_cloud(byte i) {
  if (cloud_switch != 0 && pwm_settings[3][i] != 0 ) {
    DateTime now = RTC.now();
    cloud_max_adjustment[i] = pwm_settings[0][i]*cloudDepth[pwm_settings[3][i]]/10; // maximum brightness haircut taken for the cloud (90%)
    if (now.unixtime() > cloudStart[pwm_settings[3][i]] + cloudLength[pwm_settings[3][i]]) { // we're in a "clear" period and should calculate when the next cloud will come
      // do random generation of next cloud here
      cloudStart[pwm_settings[3][i]] = now.unixtime() + random((11-cloud_switch)*60); // when the next cloud will start
      cloudLength[pwm_settings[3][i]] = random(7,15);
      if (cloudStart[pwm_settings[3][i]] + cloudLength[pwm_settings[3][i]] > sunsetTime - transitionTime) { // tidy up all clouds before sunset
        cloudLength[pwm_settings[3][i]] = sunsetTime - transitionTime - cloudStart[pwm_settings[3][i]];
      }
      cloudRamp[pwm_settings[3][i]] = random(1,3); // ramp time for the cloud
      cloud_adjustment[pwm_settings[3][i]] = 0; // reset the cloud
      cloudMarker[pwm_settings[3][i]] = 0; // prepare for generation of cloud-specific constants below
    } 
    else if (now.unixtime() > cloudStart[pwm_settings[3][i]] && now.unixtime() - cloudStart[pwm_settings[3][i]] <= cloudRamp[pwm_settings[3][i]]) { // we are ramping up to a cloudy period
      if ( cloudMarker[pwm_settings[3][i]] == 0) { //these are constant for a given cloud, only recalculated once a cloud has finished
        cloudTimer[pwm_settings[3][i]] = millis(); // mark the start of the cloud in milliseconds
        cloudDepth[pwm_settings[3][i]] = random(5,10); // produces 50% to 90% decrease in pwm duty cycle (brightness)
        cloudMarker[pwm_settings[3][i]] = 1; 
      }
      cloud_adjustment[i] = cloud_max_adjustment[i]*(1-cos(3.14*(((float)(millis() - cloudTimer[pwm_settings[3][i]])/1000)/cloudRamp[pwm_settings[3][i]])))/2;
    } 
    else if (now.unixtime() > (cloudStart[pwm_settings[3][i]] + cloudLength[pwm_settings[3][i]] - cloudRamp[pwm_settings[3][i]]) && now.unixtime() <= cloudStart[pwm_settings[3][i]] + cloudLength[pwm_settings[3][i]] ) { // we are ramping down out of the cloud
      cloud_adjustment[i] = cloud_max_adjustment[i]*(1+cos(3.14*((((float)millis() - ((float)cloudTimer[pwm_settings[3][i]] + (float)cloudLength[pwm_settings[3][i]]*1000.0 - (float)cloudRamp[pwm_settings[3][i]]*1000.0))/1000.0)/(float)cloudRamp[pwm_settings[3][i]])))/2;
    } 
    else if (now.unixtime() > cloudStart[pwm_settings[3][i]] + cloudRamp[pwm_settings[3][i]] && now.unixtime() <= cloudStart[pwm_settings[3][i]] + cloudLength[pwm_settings[3][i]] - cloudRamp[pwm_settings[3][i]]) { // we are in the middle of a cloud
      // do lightning!

      if (lightning_switch != 0) {
        cloud_adjustment[i] = make_lightning(i);
      } 
      else {
        cloud_adjustment[i] = cloud_max_adjustment[i];
      }

    } 
    else {
      // do nothing
    }
  } 
  else { // if (cloud_switch == 0)
    cloud_adjustment[i] = 0;
  }
  return((byte)cloud_adjustment[i]);
}


byte make_lightning(byte i) {
  if (random(100*(55-cloudDepth[pwm_settings[3][i]] - 4*lightning_switch)) == 1 && main_flash[pwm_settings[3][i]] == 0 ) { // this creates lightning with probability porportional to cloud depth (the darker the cloud the more lightning!)
    main_flash[pwm_settings[3][i]] = 1;

    timer[pwm_settings[3][i]] = millis();
  } 
  else {
    if (main_flash[pwm_settings[3][i]] == 1) {
      cloud_adjustment[i] = pwm_settings[0][i]; // turn the channel completely off between sub-flashes to enhance the visual contrast of each flash
    } 
    else {
      cloud_adjustment[i] = cloud_max_adjustment[i];  // normal cloudy light level
    }
  }
  if ( flash_count[pwm_settings[3][i]] < flash_max_count[pwm_settings[3][i]] && main_flash[pwm_settings[3][i]] == 1) { // if we haven't used up the assigned number of flashes
    if (millis() - timer[pwm_settings[3][i]] > flash_duration[pwm_settings[3][i]]) { // this delay is how long each lightning strike is "on" and affects the perceived brightness via POV
      if ( flash_counted[pwm_settings[3][i]] == 0 ) { // do this stuff only once per cycle
        flash_counted[pwm_settings[3][i]] = 1;
        flash_count[pwm_settings[3][i]]++;
        timer[pwm_settings[3][i]] = millis();
      }
      if ( flash_loop_counter[pwm_settings[3][i]] == flash_loop_number[pwm_settings[3][i]] ) {
        flash_counted[pwm_settings[3][i]] = 0;
        flash_loop_counter[pwm_settings[3][i]] = 0;
      } 
      else {
        flash_loop_counter[pwm_settings[3][i]]++;
      }
      cloud_adjustment[i] = pwm_settings[0][i] - MAXBRIGHT; // flash full brightness
    }
  } 
  else {
    flash_count[pwm_settings[3][i]] = 0;
    main_flash[pwm_settings[3][i]] = 0;
    flash_duration[pwm_settings[3][i]] = random(43,63);
    flash_max_count[pwm_settings[3][i]] = random(6);
    cloud_adjustment[i] = cloud_max_adjustment[i];
  }


  return cloud_adjustment[i];
}


void do_light(byte j) {
  for (byte i=0; i<=5;i++) {
    if ( j == 0 ) { // day
      if ( fanMode == 1 && i == 3 ) { // do not subject the fans to clouds or lightning
        current_settings[i] = pwm_settings[0][i];
      } 
      else if ( fanMode == 1 && i == 4 ) { // do not subject the fans to clouds or lightning
        current_settings[i] = pwm_settings[0][i];
      } 
      else {
        current_settings[i] = pwm_settings[0][i] - make_cloud(i);
      }
    } 
    else if ( j == 1 ) { // night
      if ( fanMode == 1 && i == 3 ) { // do not subject the fans to moonphase adjustment
        current_settings[i] = pwm_settings[1][i];
      } 
      else if ( fanMode == 1 && i == 4 ) { // do not subject the fans to moonphase adjustment
        current_settings[i] = pwm_settings[1][i];
      } 
      else {
        if (moonphase_toggle) {
          current_settings[i] = pwm_settings[4][i]; // adjust the night lighting by the moon phase [0,0.99]
        } 
        else {
          current_settings[i] = pwm_settings[1][i];  
        }
      }
    } 
    else if ( j == 2 ) { // off
      current_settings[i] = 0;

    }
  }

  for (byte i=0; i<=5;i++) {
    analogDirect(i, current_settings[i]);
  }
}


ISR(PCINT0_vect)
{
  unsigned char result = r.process();
  if (result) {
    result == DIR_CW ? position -= 1 : position += 1;
  }
} 

byte count_channels(byte i) {
  byte result = 0;
  for ( byte j=1; j<=5; j++ ) {
    if ( i == pwm_settings[3][j] ) {
      result++;
    }
  }
  return result;
}

void disp_clock() { // displays the current date and time on the LCD
  if ( millis() - clocktimer >= 1000 ) {
    clocktimer = millis();
  DateTime now = RTC.now();
  char buff[15];                               //create a character long variable called buff to hold the time display
  sprintf(buff,"%02d/%02d %02d:%02d:%02d",now.month(),now.day(),now.hour(),now.minute(),now.second());   //build a 7 char long string from the hour minute and second   
  lcd.setCursor(0,1);               //on the last row
  lcd.print(buff);                  //print the time
  lcd.print(P("  "));
  }
}


void analogDirect(byte ch,byte val) {
  // replaces analogWrite with direct port manipulation for faster performance
  switch ( ch ) {
  case 0:
    OCR2B = val;
    break;
  case 1:

    if(val == 0){ 
      DDRD = DDRD & B11011111; // clamp output
    }
    else{
      DDRD = DDRD | B00100000; // release output
    }

    OCR0A = val;
    break;
  case 2:
    if(val == 0){ 
      DDRD = DDRD & B10111111; // clamp output
    }
    else{
      DDRD = DDRD | B01000000; // release output
    }
    OCR0B = val;
    break;
  case 3:
    if ( fanMode == 1 ) {
      OCR1A = val*1.2549019; // quotient of 320/255 to achieve full scale at 25KHz
    } 
    else {
      OCR1A = val;
    }
    break;
  case 4:
    if ( fanMode == 1 ) {
      OCR1B = val*1.2549019; // quotient of 320/255 to achieve full scale at 25KHz
    } 
    else {
      OCR1B = val;
    }
    break;
  case 5:
    OCR2A = val;
    break;
  }
}

boolean getTemperature(){
  byte data[ONE_WIRE_BUS];
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);
  ds.reset();
  ds.select(addr);
  ds.write(0xBE);
  for ( byte i = 0; i < 9; i++) {
    data[i] = ds.read();
  }
  // Calculate the temperature
  lastTemperature = ( (data[1] << 8) + data[0] )*0.0625; // Temperature in C
  if ( tempMode == 1 ) {
    lastTemperature = lastTemperature * 1.8 + 32.0; // Temperature in F
  }
  return true;
}

int main(void) {
  init();
  // void setup() {
  {
    //Serial.begin(9600);
    Wire.begin();
    RTC.begin();

    // DS18B20 Temperature Sensor Setup
    //find a Temperature Sensor device
    if (!ds.search(addr)) {
      ds.reset_search();
      sensorConnected = 0;
    }
    if (OneWire::crc8( addr, 7) != addr[7]) {
      sensorConnected = 0;
    } 
    if (addr[0] != DS18S20_ID && addr[0] != DS18B20_ID) {
      sensorConnected = 0;
    }

    // start EEPROM variables - only need this the first time the sketch is written to each chip
    // comment it out to save 298 bytes of FLASH space
    /*
  if (EEPROM.read(511) != 111) {
     EEPROM.write(0,50); // sunrise/sunset transition duration
     EEPROM.write(1,1); // auto or manual mode selector
     
     // default Time Zone in San Francisco, CA
     EEPROM.write(2,90+38); // latitude [-90,90], normalized to +90
     EEPROM.write(3,highByte(58)); // highByte of longitude [-180,180] , normalized to +180
     EEPROM.write(4,lowByte(58)); // lowByte of longitude [-180,180] , normalized to +180
     EEPROM.write(5,4); // GMT offset [-12,12] normalized to +12
     EEPROM.write(6,0); // cloud mode toggle
     
     
     // default brightness for each of 6 PWM channels
     for (byte i=7; i<=18; i++) {
     EEPROM.write(i,3);
     }
     
     // default ramp delay for each of 6 PWM channels
     for (byte i=19;i<=24;i++) {
     EEPROM.write(i,2);
     }
     
     // default cloud channels for each of the 6 PWM channels
     for (byte i=25; i<= 30; i++) {
     EEPROM.write(i,1);
     }
     
     // Sunrise & Sunset Manual Mode Settings
     EEPROM.write(31,7); // sunrise time hour
     EEPROM.write(32,11); // sunrise time minute
     EEPROM.write(33,17); // sunset time hour
     EEPROM.write(34,21); // sunset time minute
     EEPROM.write(35,0); // lightning switch
     EEPROM.write(36,1); // Daylight Savings Mode
     EEPROM.write(37,0); // moon phase adjustment off by default
     EEPROM.write(38,0); // temperature mode C[0] or F[1]
     EEPROM.write(39,0); // fan mode on [1] or off [0]
     EEPROM.write(511,111); // stops this routine from running except the first time the sketch is ever loaded
     }
     */

    // fill in these values from those stored in EEPROM
    transitionTime = EEPROM.read(0); // duration (seconds) of Sunrise and Sunset transition
    auto_mode = EEPROM.read(1); // toggle [0 or 1] for [Manual or Auto] selection
    latitude = EEPROM.read(2)-90; // latitude of tank
    longitude = (EEPROM.read(3)*256 + EEPROM.read(4) - 180); // longitude of tank
    gmt_offset = EEPROM.read(5)-12; // GMT offset of tank
    cloud_switch = EEPROM.read(6); // cloud mode toggle

    // fill in day/night PWM settings from EEPROM
    for ( byte i = 7; i<=12; i++ ) {
      pwm_settings[0][i-7] = EEPROM.read(i);
    }
    for ( byte j = 13; j<=18; j++ ) {
      pwm_settings[1][j-13] = EEPROM.read(j);
    }
    for ( byte k = 19; k<=24; k++ ) {
      pwm_settings[2][k-19] = EEPROM.read(k);
    }
    for ( byte l = 25; l<=30; l++ ) {
      pwm_settings[3][l-25] = EEPROM.read(l);
    }
    // fill in sunrise/sunset times from EEPROM
    sunrise_time[0][0] = EEPROM.read(31);
    sunrise_time[0][1] = EEPROM.read(32);
    sunrise_time[1][0] = EEPROM.read(33);
    sunrise_time[1][1] = EEPROM.read(34);

    lightning_switch = EEPROM.read(35); // lightning mode toggle
    dstMode = EEPROM.read(36); // Daylight Savings Time setting
    moonphase_toggle = EEPROM.read(37);
    tempMode = EEPROM.read(38); // temperature mode C[0] or F[1]
    fanMode = EEPROM.read(39); // fan mode on [1] or off [0]
    // end eeprom variables


    // use direct port mainpulation to set INPUT/OUTPUT ports
    // see http://www.arduino.cc/en/Reference/PortManipulation

    DDRD = B01111110; // set D0,D7 as input, all other as output
    DDRB = B0001110; // set D9,D10,D11 as output, all other as input
    DDRC = B00111111; // set A6 & A7 as input (these pins cannot function as digital outputs) , all other A0-A5 as digital output

    //  pinMode(ENCODER_B,INPUT);
    //  pinMode(ENCODER_A,INPUT);
    //  pinMode(BUTTON,INPUT);
    //  for (byte i=0; i<=5;i++) {
    //    pinMode(channels[i],OUTPUT);
    //  }


    // PWM Direct Port Access Setup

    // Timer0 Setup - Don't mess with this as it affects millis() and delay()
    TCCR0A |= (1 << WGM00) | (1 << WGM01); // fast pwm Timer0 (D5,D6)
    TCCR0B = _BV(CS00) | _BV(CS01); // F_CPU / (64 * 256)  = 1KHz PWM on D5,D6
    TCCR0A |= (1 << COM0A1); // clear on compare match (D6)
    TCCR0A |= (1 << COM0B1); // clear on compare match (D5)


    // Timer1 Setup
    if (fanMode == 0) { // Standard 490 Hz PWM
      TCCR1A |= (1 << WGM10) | (1 << WGM12); // 8-bit fast pwm Timer1 (D9,D10)
      TCCR1B = _BV(CS10) | _BV(CS11);// F_CPU / (128 * 256)  = 490 Hz PWM on D9,D10
      TCCR1A |= (1 << COM1A1); // clear on compare match (D9)
      TCCR1A |= (1 << COM1B1); // clear on compare match (D10)
    } 
    else if (fanMode == 1) { // 
      // Timer1 Initialization for 25 KHz PWM on D9, D10 for Fan Control
      // Commandeer Timer1 to achieve 25 KHz PWM, vs the default frequency of 490 Hz
      TCCR1A = _BV(COM1A1) | _BV(COM1B1);
      TCCR1B = _BV(WGM13) | _BV(CS10);
      ICR1 = 320; // gives us 25 KHz PWM
    }


    // Timer2 Setup
    TCCR2A |= (1 << WGM20); // phase correct PWM Timer2 (D3,D11)
    TCCR2B = _BV(CS22) ; // F_CPU / (128 * 256) = 490 Hz PWM on D3,D11
    TCCR2A |= (1 << COM2A1); // clear on compare match (D3)
    TCCR2A |= (1 << COM2B1); // clear on compare match (D11)

    // End PWM Direct Port Access Setup


    //internal pullups not ncessary if using harware debounce RC circuit
    //digitalWrite(ENCODER_B, HIGH); // activate internal 20k pullup
    //digitalWrite(ENCODER_A, HIGH); // activate internal 20k pullup

    // enable pin change interrupts on those two pins:
    PCICR |= (1 << PCIE0); 
    PCMSK0 |= (1 << PCINT5) | (1 << PCINT0); // set digital pins 13 and 8 as interrupt pins
    sei(); // enable interrupts

    /*
  lcd.createChar(0, p1);
     lcd.createChar(1, p2);
     lcd.createChar(2, p3);
     lcd.createChar(3, p4);
     */
    lcd.begin(16, 2);
    /*
  if (! RTC.isrunning()) {
     lcd.clear();
     lcd.setCursor(0, 0);
     lcd.print(P("RTC Error!"));
     RTC.adjust(DateTime(__DATE__, __TIME__)); // this line sets the datetime to the PC datetime at compile time
     }*/

  }


  while(1) {
    // void loop() {
/*      
      loopcounter+=1;
if ( millis() - looptimer > 1000 ) {
lcd.print(loopcounter);
looptimer = millis();
loopcounter = 0;
} */
      
    if ( sensorConnected != 0 ) {
      if (millis() - lastTempRequest >= TEMP_READ_DELAY) {
        getTemperature();
        lastTempRequest = millis();
      }
    }

    disp_clock();

    DateTime now = RTC.now();

    if ( now.day() != lastDay || auto_mode != last_mode ) { // Calculate Sunrise and Sunset once a day
      last_mode = auto_mode;
      TimeLord myLord;
      myLord.Position(latitude, longitude); // set global position based on degrees latitude and longitude
      myLord.TimeZone(gmt_offset*60); // set GMT time offset


      byte day_temp[]  = {
        0,0,0, now.day(), now.month(), now.year()                                                                                                                                                                                        };

      // sunrise sunset calculations

      switch ( auto_mode ) {
      case 0:
        {
          DateTime srt (now.year(),now.month(),now.day(),sunrise_time[0][0],sunrise_time[0][1],0);
          //char buffer[15];
          //sprintf(buffer,"%0004d/%02d/%02d %02d:%02d", srt.year(),srt.month(),srt.day(),srt.hour(),srt.minute());
          sunriseTime = srt.unixtime();
          DateTime sst (now.year(),now.month(),now.day(),sunrise_time[1][0],sunrise_time[1][1],0);
          sunsetTime = sst.unixtime();
        }
        break;
      case 1:

        for (byte i=0;i<=1;i++) {
          if ( i == 0 ) {
            myLord.SunRise(day_temp); // Calculate SunRise
          } 
          else {
            myLord.SunSet(day_temp); // Calculate SunSet
            moonphase = 100*myLord.MoonPhase(day_temp); // Calculate Moon Phase [0.0, 0.99]
            moonphase = 50 - moonphase;
            moonphase = 100 - 2*abs(moonphase); // [0.0 = New Moon, 0.5 = First Quarter, 0.99 = Full Moon, 0.5 = Last Quarter, 0.01 = Almost New]
            moonphase_adjust(moonphase_toggle);
          }
          byte day[] = {
            day_temp[tl_second],day_temp[tl_minute],day_temp[tl_hour],day_temp[tl_day],day_temp[tl_month],day_temp[tl_year]                                                                                                                                      };
          if ( dstMode ) {
            myLord.DstRules(3,2,11,1, 60); // Daylight Savings Time
            myLord.DST(day);
          }
          sunrise_time[i][0] = day[tl_hour]; // set sunset hour
          sunrise_time[i][1] = day[tl_minute]; // set sunset minute

          long time_temp = now.unixtime() + ((long) day[tl_hour] - (long) now.hour())*3600L + ( day[tl_minute]-now.minute())*60 + ( day[tl_second]-now.second());
          if ( i == 0 ) {
            sunriseTime = time_temp;
          } 
          else {
            sunsetTime = time_temp;
          }
        }
        /*
      {
         myLord.SunRise(day_temp); // Calculate SunRise
         byte day[] = {
         day_temp[tl_second],day_temp[tl_minute],day_temp[tl_hour],day_temp[tl_day],day_temp[tl_month],day_temp[tl_year]                                  };
         myLord.DST(day);
         sunrise_time[0][0] = day[tl_hour]; // set sunset hour
         sunrise_time[0][1] = day[tl_minute]; // set sunset minute
         sunriseTime = now.unixtime() + ((long) day[tl_hour] - (long) now.hour())*3600L + ( day[tl_minute]-now.minute())*60 + ( day[tl_second]-now.second());
         
         }
         { 
         myLord.SunSet(day_temp); // Calculate SunSet
         byte day[] = {
         day_temp[tl_second],day_temp[tl_minute],day_temp[tl_hour],day_temp[tl_day],day_temp[tl_month],day_temp[tl_year]                                    };
         myLord.DST(day);
         sunrise_time[1][0] = day[tl_hour]; // set sunset hour
         sunrise_time[1][1] = day[tl_minute]; // set sunset minute
         sunsetTime = now.unixtime() + ((long) day[tl_hour] - (long) now.hour())*3600L + ((int) day[tl_minute]-now.minute())*60 + ((int) day[tl_second]-now.second());
         
         }*/
        break;
      }
      lastDay = day[tl_day];
    }


    /*
  if ( (millis() - timeDisp > 2000) && (menu[0][1] == 1)) {
     lcd.setCursor(0,1);               //on the last row
     lcd.print(buff);                  //print the time
     lcd.print(P("  "));
     } 
     else if ( menu[0][1] != 1)  {
     lcd.setCursor(0,1);               //on the last row
     lcd.print(buff);                  //print the time
     lcd.print(P("  "));
     }
     */

    lcd.setCursor(0, 0);
    switch (buttonPress()) {
    case 0: 

      // Encoder button is not pressed

      break;
    case 1: // Only catch leading edge of encoder button press
      menu[0][1]++;

      if ( menu[0][1] > menu[0][0] ) { 
        menu[0][1] = 0; 
      }
      break;
    case 2: // Encoder button is being held down

      lcd.clear();
      lcd.noCursor();
      lcd.setCursor(0,0);
      lcd.print(P("Enter Setup? Y/N"));

      lcd.setCursor(13,0);

      int exitFlag = 0;
      while ( !exitFlag ) {


        if ( position > lastPosition ) {
          lcd.setCursor(15,0);
          ynchoice = 0;
          lastPosition = position;
        }

        else if ( position < lastPosition ) {
          lcd.setCursor(13,0);
          ynchoice = 1;
          lastPosition = position;
        }

        if ( (ynchoice == 0) && (buttonPress() == 1) ) {
          exitFlag = 1;
          ynchoice = 1; // reset to default for next cycle
          lcd.clear();
        }  
        else if ( (ynchoice == 1) && (buttonPress() == 1)) {
          exitFlag = 0;

          lcd.clear();
          menu[1][1] = 0;
          while ( exitFlag != 1 ) {

            menu_advance(1);
            lcd.setCursor(0,0);

            switch ( menu[1][1] ) {
            case 0:
              lcd.print(P("Set PWM (Day)"));
              break;
            case 1:
              lcd.print(P("Set PWM (Night)"));
              break;
            case 2: 
              lcd.print(P("Set Sunrise Mode"));

              lcd.setCursor(1,1);

              if (auto_mode == 1) {
                lcd.print(P("    [Auto]  "));
              } 
              else {
                lcd.print(P("   [Manual] "));
              }

              break;
            case 3:
              lcd.print(P("Set Sunrise Time"));
              lcd.setCursor(0,1);
              char buffs[8];
              sprintf(buffs,"  %02d:%02d",sunrise_time[0][0],sunrise_time[0][1]);
              lcd.print(buffs);
              if ( auto_mode == 1 ) { 
                lcd.print(P(" Auto")); 
              } 
              else {
                lcd.print(P(" Manual"));
                //lcd.print(P("al"));
              }
              break;
            case 4:
              lcd.print(P("Set Sunset Time "));
              lcd.setCursor(0,1);
              char buffn[8];
              sprintf(buffn,"  %02d:%02d",sunrise_time[1][0],sunrise_time[1][1]);
              lcd.print(buffn);
              if ( auto_mode == 1) { 
                lcd.print(P(" Auto")); 
              } 
              else {
                lcd.print(P(" Manual"));
              }
              break;
            case 5:
              lcd.print(P("Set Ramp Time "));
              lcd.setCursor(1,1);
              if (transitionTime < 10) { 
                lcd.print(P(" "));
              }
              if (transitionTime < 100 ) {
                lcd.print(P(" "));
              }
              lcd.print(transitionTime);
              lcd.print(P(" Minutes"));
              break;
            case 6:
              lcd.print(P("Set Ramp Delay"));
              break;

            case 7:
              lcd.print(P("Set Cloud Freq"));

              lcd.setCursor(1,1);
              if (cloud_switch < 10) { 
                lcd.print(P(" "));
              }
              lcd.print(cloud_switch);

              break;
            case 8:
              lcd.print(P("Cloud Channels"));
              break;

            case 9:
              lcd.print(P("Lightning Freq"));

              lcd.setCursor(1,1);

              if (lightning_switch < 10) { 
                lcd.print(P(" "));
              }
              lcd.print(lightning_switch);

              break;

            case 10:
              lcd.print(P("Moonphase Adjust"));

              lcd.setCursor(4,1);

              if (moonphase_toggle) {
                lcd.print(P(" [On]  "));
              } 
              else {
                lcd.print(P("[Off] "));
              }
              break;

            case 11: 

              lcd.print(P("Set Lat/Long/GMT"));
              lcd.setCursor(4,1);
              lcd.print(latitude);
              lcd.print(P("/"));
              lcd.print(longitude);
              lcd.print(P("/"));
              lcd.print(gmt_offset);
              break;

            case 12:

              lcd.print(P("Temp Disp"));
              lcd.setCursor(10,0);

              if (tempMode == 1) {
                lcd.print(P("[F]"));
              } 
              else {
                lcd.print(P("[C]"));
              }
              break;

            case 13:

              lcd.print(P("CH4 CH5 Fan?"));
              lcd.setCursor(5,1);

              if (fanMode == 1) {
                lcd.print(P("[Y]"));
              } 
              else {
                lcd.print(P("[N]"));
              }
              break;

            case 14:
              lcd.print(P("Daylight Savings"));

              lcd.setCursor(4,1);

              if (dstMode) {
                lcd.print(P(" [On]  "));
              } 
              else {
                lcd.print(P("[Off] "));
              }
              break;


            case 15:
              lcd.print(P("Set Clock"));


              {
                disp_clock();
              }
              break;
            case 16:
              lcd.print(P("Exit Setup"));
              break;
            }

            if (buttonPress() == 1) {
              switch ( menu[1][1] ) {
              case 0:
                pwm_config(0);
                break;
              case 1:
                pwm_config(1);              
                if (moonphase_toggle) {
                  moonphase_adjust(moonphase_toggle);
                }
                break;

              case 2: // set sun mode
                lcd.setCursor(1,1);
                //last_mode = auto_mode;
                //auto_mode = ~auto_mode;

                if (auto_mode == 0) {
                  auto_mode = 1;
                  //lcd.print(P(" [Automatic]  "));
                  last_mode = 0;
                } 
                else {
                  auto_mode = 0;
                  //lcd.print(P(" [Manual] "));
                  last_mode = 1;
                }

                EEPROM.write(1,auto_mode);
                lcd.clear();
                exitFlag = 0;

                break;
              case 3: // set sunrise
                if ( auto_mode == 0 ) {
                  sunrise_sunset(0);
                }
                break;
              case 4:
                if ( auto_mode == 0 ) {
                  sunrise_sunset(1);
                }
                break;
              case 5: // set ramp time

                lcd.setCursor(0,1);
                lcd.print(P(">"));

                transitionTime = value_advance(transitionTime, 0, 180, 1, 1);
                EEPROM.write(0,transitionTime);

                lcd.clear();
                //menu[2][1] = 3;
                exitFlag = 0;

                break;
              case 6: // set ramp delay
                pwm_config(2);
                break;

              case 7: // cloud mode toggle
                lcd.setCursor(0,1);
                lcd.print(P(">"));

                cloud_switch = value_advance(cloud_switch, 0, 10, 1, 1);
                EEPROM.write(6,cloud_switch);

                lcd.clear();
                //menu[2][1] = 3;
                exitFlag = 0;
                break;

              case 8: // set cloud channel
                pwm_config(3);
                break;

              case 9: // Lightning freq toggle
                lcd.setCursor(0,1);
                lcd.print(P(">"));

                lightning_switch = value_advance(lightning_switch, 0, 10, 1, 1);

                EEPROM.write(35,lightning_switch);
                lcd.clear();
                exitFlag = 0;
                break;

              case 10: // moonphase toggle
                lcd.setCursor(1,1);

                moonphase_toggle = ~moonphase_toggle;
                if (moonphase_toggle) {
                  moonphase_adjust(moonphase_toggle);
                }
                /*
                if ( lightning_switch == 1 ) {
                 lightning_switch = 0;
                 } else {
                 lightning_switch = 1;
                 }*/

                EEPROM.write(37,moonphase_toggle);
                lcd.clear();
                exitFlag = 0;
                break;


              case 11: // set lat/long/gmt

                lcd.blink();
                lcd.noCursor();
                while ( !exitFlag ) {

                  lcd.clear();
                  lcd.print(P("Set Lat/Long/GMT"));
                  lcd.setCursor(4,0);

                  while ( !exitFlag ) {

                    menu_advance(6);

                    switch (menu[6][1]) {
                    case 0:
                      lcd.setCursor(4,0);
                      break;
                    case 1:
                      lcd.setCursor(8,0);
                      break;
                    case 2:
                      lcd.setCursor(13,0);
                      break;
                    }

                    switch ( buttonPress() ) {
                    case 1:

                      switch (menu[6][1]) {
                      case 0:
                        lcd.setCursor(0,0);
                        lcd.print(P("Set Latitude    "));
                        if ( latitude < 0 ) {
                          lcd.setCursor(0,1);
                        } 
                        else {
                          lcd.setCursor(1,1);
                        }
                        lcd.print(latitude);
                        lcd.setCursor(3,1);
                        lcd.print(P(" Deg"));

                        latitude = value_advance(latitude,-90,90,0,1);
                        EEPROM.write(2,latitude+90);
                        break;
                      case 1:
                        lcd.setCursor(0,0);
                        lcd.print(P("Set Longitude   "));
                        if ( longitude < 0 ) {
                          lcd.setCursor(0,1);
                        } 
                        else {
                          lcd.setCursor(1,1);
                        }
                        lcd.print(longitude);
                        lcd.setCursor(4,1);
                        lcd.print(P(" Deg"));
                        longitude = value_advance(longitude,-180,180,0,1);
                        EEPROM.write(3,highByte(longitude+180));
                        EEPROM.write(4,lowByte(longitude+180));
                        break;
                      case 2:
                        lcd.setCursor(0,0);
                        lcd.print(P("Set GMT Offset  "));
                        if ( gmt_offset < 0 ) {
                          lcd.setCursor(0,1);
                        } 
                        else {
                          lcd.setCursor(1,1);
                        }
                        lcd.print(gmt_offset);
                        lcd.setCursor(4,1);
                        lcd.print(P("Hours"));
                        gmt_offset = value_advance(gmt_offset,-12,12,0,1);
                        EEPROM.write(5,gmt_offset+12);
                        break;
                      }
                      lcd.clear();
                      lcd.setCursor(0,0);                  
                      lcd.print(P("Set Lat/Long/GMT"));
                      last_mode = 0;
                      break;
                    case 2:

                      exitFlag = 1;
                      break;
                    }
                  }

                }
                lcd.clear();
                exitFlag = 0;

                break;
              case 12:
                //lcd.setCursor(1,1);
                if ( tempMode == 1 ) {

                  tempMode = 0;
                } 
                else {
                  tempMode = 1;
                }

                EEPROM.write(38,tempMode);
                lcd.clear();
                exitFlag = 0;
                break;

              case 13:
                //lcd.setCursor(1,1);
                if ( fanMode == 1 ) {
                  fanMode = 0;
                  TCCR1A |= (1 << WGM10) | (1 << WGM12); // 8-bit fast pwm Timer1 (D9,D10)
                  TCCR1B = _BV(CS10) | _BV(CS11);// F_CPU / (128 * 256)  = 490 Hz PWM on D9,D10
                  TCCR1A |= (1 << COM1A1); // clear on compare match (D9)
                  TCCR1A |= (1 << COM1B1); // clear on compare match (D10)
                } 
                else {
                  fanMode = 1;
                  // Timer1 Initialization for 25 KHz PWM on D9, D10 for Fan Control
                  // Commandeer Timer1 to achieve 25 KHz PWM, vs the default frequency of 490 Hz
                  TCCR1A = _BV(COM1A1) | _BV(COM1B1);
                  TCCR1B = _BV(WGM13) | _BV(CS10);
                  ICR1 = 320; // gives us 25 KHz PWM
                }

                EEPROM.write(39,fanMode);
                lcd.clear();
                exitFlag = 0;
                break;

              case 14: // DST mode toggle
                //lcd.setCursor(1,1);

                dstMode = ~dstMode;

                EEPROM.write(36,dstMode);
                lcd.clear();
                exitFlag = 0;
                break;

              case 15: // set clock
                lcd.noBlink();
                lcd.cursor();
                while ( !exitFlag ) {

                  lcd.clear();

                  DateTime now = RTC.now();
                  int clock_settings[] = {
                    now.month(),now.day(),now.year(),now.hour(),now.minute(),now.second()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    };
                  char dset[17];
                  sprintf(dset,"Date: %02d/%02d/%04d",now.month(),now.day(),now.year()); 
                  char tset[16];
                  sprintf(tset,"Time:  %02d:%02d:%02d",now.hour(),now.minute(),now.second()); 
                  lcd.setCursor(0,0);
                  lcd.print(dset);
                  lcd.setCursor(0,1);
                  lcd.print(tset);


                  while ( !exitFlag ) {

                    menu_advance(3);

                    switch (menu[3][1]) {
                    case 0:
                      lcd.setCursor(6,0);
                      break;
                    case 1:
                      lcd.setCursor(9,0);
                      break;
                    case 2:
                      lcd.setCursor(12,0);
                      break;
                    case 3:
                      lcd.setCursor(7,1);
                      break;
                    case 4:
                      lcd.setCursor(10,1);
                      break;
                    case 5:
                      lcd.setCursor(13,1);
                      break;
                    }

                    switch ( buttonPress() ) {
                    case 1:

                      switch (menu[3][1]) {
                      case 0:
                        clock_settings[menu[3][1]] = set_clock(now.month(),1,12,6,0);
                        break;
                      case 1:
                        clock_settings[menu[3][1]] = set_clock(now.day(),1,31,9,0);
                        break;
                      case 2:
                        clock_settings[menu[3][1]] = set_clock(now.year(),2000,2099,12,0);
                        break;
                      case 3:
                        clock_settings[menu[3][1]] = set_clock(now.hour(),0,23,7,1);
                        break;
                      case 4:
                        clock_settings[menu[3][1]] = set_clock(now.minute(),0,59,10,1);
                        break;
                      case 5:
                        clock_settings[menu[3][1]] = set_clock(now.second(),0,59,13,1);
                        break;
                      }

                      break;
                    case 2:
                      // exiting from setting time, create new dateobj and save it to the RTC
                      DateTime dt0 (clock_settings[2],clock_settings[0],clock_settings[1],clock_settings[3],clock_settings[4],clock_settings[5]);
                      RTC.adjust(dt0.unixtime());
                      exitFlag = 1;
                      break;
                    }
                  }
                }
                lcd.clear();
                exitFlag = 0;
                break;


              case 16: // exit setup
                exitFlag = 1;
                break;
              }
            }
          }
          lcd.clear();
        }
        lcd.blink();
      }
      break;
    }

    switch (menu[0][1]) {
    case 0:
      lcd.noBlink();

      if ( sensorConnected != 0 ) {
        //getTemperature();

        lcd.print(int(lastTemperature));
        lcd.print(P("."));
        lcd.print(int(100*(lastTemperature-int(lastTemperature))));

        lcd.print((char)223);
        if ( tempMode == 0 ) {
          lcd.print(P("C "));
        } 
        else {
          lcd.print(P("F "));
        }
      }

      if ( now.unixtime() > sunsetTime || now.unixtime() <= sunriseTime ) {
        dayStatus = 1;
        lcd.print(P("Night  "));
      } 
      else if ( now.unixtime() > sunsetTime - transitionTime*60 && now.unixtime() <= sunsetTime ) {
        dayStatus = 2;
        lcd.print(P("Sunset "));
      } 
      else if ( now.unixtime() > sunriseTime + transitionTime*60 && now.unixtime() <= sunsetTime - transitionTime*60 ) {
        dayStatus = 3;
        lcd.print(P("Day    "));
      } 
      else if ( now.unixtime() > sunriseTime && now.unixtime() <= sunriseTime + transitionTime*60 ) {
        dayStatus = 4;
        lcd.print(P("Sunrise"));
      } 
      else {
        lcd.print(P("Error"));
        dayStatus = 1;
      }

      switch (dayStatus) {
        byte intensity;
      case 1: // Night
        //do_night();
        do_light(1);
        break;
      case 2: // Sunset
        for (byte i=0;i<=5;i++) {

          if (now.unixtime() < sunsetTime - pwm_settings[2][i]*60) { // assign each channel brightness in accordance with sunset profile
            // linear ramp algorithm DO NOT ERASE
            //analogWrite(channels[i], pwm_settings[0][i] - (now.unixtime() - (sunsetTime - transitionTime*60))*(pwm_settings[0][i] - pwm_settings[4][i]) / ((transitionTime-pwm_settings[2][i])*60));
            intensity = pwm_settings[0][i] - ((pwm_settings[0][i] - pwm_settings[4][i]) >> 1)*(1-cos(3.14*(now.unixtime() - (sunsetTime-transitionTime*60))/((transitionTime-pwm_settings[2][i])*60)));
            //intensity = pwm_settings[0][i] - (now.unixtime() - (sunsetTime - transitionTime*60))*(pwm_settings[0][i] - pwm_settings[4][i]) / ((transitionTime-pwm_settings[2][i])*60);

          } 
          else {
            intensity = pwm_settings[1][i];

          }
          analogDirect(i, intensity);
        }

        break;
      case 3: // Day
        do_light(0);
        break;
      case 4: // Sunrise
        for (byte i=0;i<=5;i++) {
          if (now.unixtime() > sunriseTime + pwm_settings[2][i]*60) { // assign each channel brightness in accordance with sunrise profile

            // linear ramp algorithm DO NOT ERASE
            //analogWrite(channels[i], pwm_settings[4][i] + (now.unixtime() - (sunriseTime + pwm_settings[2][i]*60))*(pwm_settings[0][i] - pwm_settings[4][i]) / ((transitionTime-pwm_settings[2][i])*60));

            // trigonometric ramp algorithm
            intensity = pwm_settings[4][i] + ((pwm_settings[0][i] - pwm_settings[4][i]) >> 1)*(1+cos(3.14*((transitionTime-pwm_settings[2][i])*60 - (now.unixtime() - sunriseTime - pwm_settings[2][i]*60))/((transitionTime-pwm_settings[2][i])*60)));
            //intensity = pwm_settings[4][i] + (now.unixtime() - (sunriseTime + pwm_settings[2][i]*60))*(pwm_settings[0][i] - pwm_settings[4][i]) / ((transitionTime-pwm_settings[2][i])*60);
          } 
          else {
            intensity = pwm_settings[1][i];
          }
          analogDirect(i, intensity);
        }
        break;
      }
      break;
    case 1:

      lcd.print(P("Manual ("));
      lcd.print(brightness*100/25500);
      lcd.print(P("%)     "));

      if ( position > lastPosition ) {
        timeDisp = millis();
        increment = 1;
        lastPosition = position;
      }
      else if (position < lastPosition ) {
        timeDisp = millis();
        increment = 2;
        lastPosition = position;
      }
      else {
        increment = 0;
      }
      byte subbars;
      switch ( increment ) {
      case 0:
        break;
      case 1:
        if ( brightness < 25500 ) {
          brightness+=MAXBRIGHT;
        }

        /*
      lcd.setCursor(0,1);
         subbars = brightness/318; // the 318 is a normalizing value that maxes out the bars as we hit 100%
         
         remainder = subbars % 5;
         for ( int i = 1; i <= subbars/5; i++ ) {
         lcd.print((char)0xFF);
         }
         
         if ( remainder > 0 ) { 
         lcd.write(remainder-1); 
         }
         for ( int i = 1; i <= 15-subbars/5; i++ ) {
         lcd.print(P(" "));
         }*/
        /*
        for (byte i=0;i<=5;i++) {
         analogDirect(i, brightness/100);
         }
         */
        break;
      case 2:
        if ( brightness > 0 ) { 
          brightness-=MAXBRIGHT;
        }

        /*
      lcd.setCursor(0,1);
         subbars = brightness/318;
         remainder = subbars % 5;
         for ( int i = 1; i <= subbars/5; i++ ) {
         lcd.print((char)0xFF);
         }
         if ( remainder > 0 ) { 
         lcd.write(remainder-1); 
         }
         
         for ( int i = 1; i <= 15-subbars/5; i++ ) {
         lcd.print(P(" "));
         }*/
        /*
        for (byte i=0;i<=5;i++) {
         analogDirect(i, brightness/100);
         }
         */
        break;
      default:
        break;
      }
      for (byte i=0;i<=5;i++) {
        analogDirect(i, brightness/100);
      }
      break;
    case 2:
      lcd.print(P("Dark            "));
      for (byte i=0; i<=5; i++) {
        analogDirect(i, pwm_settings[1][i]);
      }
      break;
    case 3:
      lcd.print(P("Off             "));
      do_light(2);
      /*
for (byte i=0; i<=5; i++) {
       analogDirect(i,0);
       }
       */

      break;
    default:
      break;
    }
  }
}











