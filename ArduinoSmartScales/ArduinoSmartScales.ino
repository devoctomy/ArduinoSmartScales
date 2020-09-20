#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <LiquidMenu.h>
#include <avr/sleep.h>
#include <EEPROM.h>
#include "HX711.h"
#include "PinHelpers.h"
#include "ButtonManager.h"
#include "MathsHelpers.h"
#include "ScaleHelpers.h"

#define DOUT A2
#define CLK  A3
#define INACTIVITY_TIME_BEFORE_SLEEP 30 * 1000
#define HOME_BUTTON_PIN 2
#define ENCODER_CLOCKWISE_PIN 3
#define ENCODER_ANTICLOCKWISE_PIN 4
#define ENCODER_BUTTON_PIN 5
#define BASELINEREADINGS 3
#define AVERAGESAMPLES 2
#define MIN_CALIBRATION_WEIGHT 10
#define MAX_CALIBRATION_WEIGHT 200

const char MENU_TEXT_OPTIONS[] PROGMEM = "Options";
const char MENU_TEXT_OPTIONS_CALIBRATE[] PROGMEM = "Calibrate";
const char MENU_TEXT_OPTIONS_CALIBRATE_WEIGHT[] PROGMEM = "Weight: ";
const char MENU_TEXT_OPTIONS_ROUNDING[] PROGMEM = "Rounding: ";
const char MENU_TEXT_BACK[] PROGMEM = "< Back";

//-------------------------------------------------------------------------------------
//Global variables
//-------------------------------------------------------------------------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);                       //lcd object
HX711 loadCell;                                           //scale object
float calibrationFactor = 429.24;                         //Default calibration factor
float calibrationWeight = 200.00;                         //Weight required for calibration
long baseline = 0;                                        //Baseline value
bool menuRequiresUpdate = true;                           //Causes menu to be refreshed
unsigned long lastActivityMillis = 0;                     //Millisecond count that last activity was recorded
unsigned long startMillis = 0;                            //Millisecond count that the system started
bool showingMenu = false;                                 //Signifies that the menu is currently being displayed
bool forceRefresh = false;                                //Causes scale readout to be refreshed, reguardless of it changing or not
bool requiresCalibration = false;                         //Calibration factor is not valid, calibration needs to be performed
bool enableRounding = true;                               //Enable / Disable rounding of samples
bool readSamples = true;                                  //Enable / Disable reading of samples during main program loop
unsigned int lastRounded = 999;                           //Last rounded sample recorded
float lastUnrounded = -1;                                 //Last unrounded sample recorded
float lastAverageSample = -1;                             //Last average sample recorded, used to calculate delta
float lastDelta = 0;                                      //Last delta

//-------------------------------------------------------------------------------------
//Menu setup
//-------------------------------------------------------------------------------------
LiquidLine mainMenu_Options_Line1(0, 0, MENU_TEXT_OPTIONS);
LiquidScreen mainMenu_Options(mainMenu_Options_Line1);

LiquidLine mainMenu_optionsMenu_Calibrate_Line1(0, 0, MENU_TEXT_OPTIONS_CALIBRATE);
LiquidScreen mainMenu_optionsMenu_Calibrate(mainMenu_optionsMenu_Calibrate_Line1);

LiquidLine mainMenu_optionsMenu_Calibrate_Weight_Line1(0, 0, MENU_TEXT_OPTIONS_CALIBRATE_WEIGHT, calibrationWeight);
LiquidScreen mainMenu_optionsMenu_Calibrate_Weight(mainMenu_optionsMenu_Calibrate_Weight_Line1);

LiquidLine mainMenu_optionsMenu_Rounding_Line1(0, 0, MENU_TEXT_OPTIONS_ROUNDING, enableRounding);
LiquidScreen mainMenu_optionsMenu_Rounding(mainMenu_optionsMenu_Rounding_Line1);

LiquidLine mainMenu_optionsMenu_Back_Line1(0, 0, MENU_TEXT_BACK);
LiquidScreen mainMenu_optionsMenu_Back(mainMenu_optionsMenu_Back_Line1);

LiquidMenu mainMenu(lcd);
LiquidMenu optionsMenu(lcd);
LiquidMenu calibrateMenu(lcd);

LiquidSystem menuSystem(0);
//-------------------------------------------------------------------------------------

void setup()
{
  RegisterActivity();
  Serial.begin(9600);
  Serial.println("Configuring lcd");  

  lcd.init();
  lcd.backlight();
  lcd.print(F("Please wait..."));

  Serial.println(F("Configuring menu"));
  mainMenu_Options_Line1.set_asProgmem(1);
  mainMenu_optionsMenu_Calibrate_Line1.set_asProgmem(1);
  mainMenu_optionsMenu_Calibrate_Weight_Line1.set_asProgmem(1);
  mainMenu_optionsMenu_Rounding_Line1.set_asProgmem(1);
  mainMenu_optionsMenu_Back_Line1.set_asProgmem(1);
  
  mainMenu.init();
  mainMenu.add_screen(mainMenu_Options);
  optionsMenu.init();
  optionsMenu.add_screen(mainMenu_optionsMenu_Calibrate);
  optionsMenu.add_screen(mainMenu_optionsMenu_Rounding);
  optionsMenu.add_screen(mainMenu_optionsMenu_Back);
  calibrateMenu.add_screen(mainMenu_optionsMenu_Calibrate_Weight);
  menuSystem.add_menu(mainMenu);
  menuSystem.add_menu(optionsMenu);
  menuSystem.add_menu(calibrateMenu);

  Serial.println(F("Configuring buttons"));
  AddManagedButton({
    F("H"),
    HOME_BUTTON_PIN,
    true,
    (ButtonCallbackDelegate)ManagedButtonCallback
  });

  Serial.println(F("Configuring encoder"));
  AddManagedEncoder({
    F("E"),
    ENCODER_CLOCKWISE_PIN,
    ENCODER_ANTICLOCKWISE_PIN,
    ENCODER_BUTTON_PIN,
    true,
    (ButtonCallbackDelegate)ManagedButtonCallback,
    (EncoderCallbackDelegate)ManagedEncoderCallback
  });

  ScaleInitResults initResults = InitialiseScale(&loadCell, DOUT, CLK, BASELINEREADINGS);
  baseline = initResults.Baseline;
  calibrationFactor = initResults.CalibrationFactor;
  if(calibrationFactor == 0)
  {
    requiresCalibration = true;
    readSamples = false;  
  }

  lcd.clear();
  lastActivityMillis = millis();
}

void loop()
{
  if(readSamples)
  {
    float averageSample = GetAveragedSample(&loadCell, AVERAGESAMPLES);
    if(lastAverageSample == -1) lastAverageSample = averageSample;
    lastDelta = averageSample - lastAverageSample;
    lastAverageSample = averageSample;
  }

  CheckManagedButtons();
  
  if(showingMenu)
  {
    if(menuRequiresUpdate)
    {
      menuSystem.update();
      menuRequiresUpdate = false;
    }
  }
  else
  {
    if(requiresCalibration)
    {
      if(menuRequiresUpdate)
      {
        lcd.clear();
        lcd.print(F("CALIBRATION"));
        lcd.setCursor(0,1);
        lcd.print(F("REQUIRED!"));
      }
    }
    else
    {
      bool updated = false;
      if(enableRounding)
      {
        unsigned int curRounded = (unsigned int)bsdRound(lastAverageSample);
        if(curRounded != lastRounded || forceRefresh)
        {
          lcd.clear();
          lcd.print(curRounded); 
          lcd.print(F("g"));
          lastRounded = curRounded;
          updated = true;
        }
      }
      else
      {
        if(lastAverageSample != lastUnrounded || forceRefresh)
        {
          lcd.clear();
          lcd.print(lastAverageSample); 
          lcd.print(F("g"));
          lastUnrounded = lastAverageSample;
          updated = true;
        }        
      }
      if(updated)
      {
        RegisterActivity();
        forceRefresh = false; 
      }
    }
  }

  int timeSinceLastActivity = (millis() - lastActivityMillis);
  if(timeSinceLastActivity >= INACTIVITY_TIME_BEFORE_SLEEP)
  {
    Serial.print(F("Time since last activity = "));
    Serial.println(timeSinceLastActivity);
    Sleep(HOME_BUTTON_PIN);
    RegisterActivity();
  }
}

void ManagedButtonCallback(String key, ButtonState buttonState)
{
  if(key == F("H") && buttonState == ButtonState::ButtonDepressed)
  {
      showingMenu = !showingMenu;
      readSamples = !showingMenu;
      if(showingMenu)
      {
        menuSystem.change_menu(mainMenu);
      }
      else
      {
        forceRefresh = true;
      }
  }
  else if(key == F("E.B") && buttonState == ButtonState::ButtonDepressed)
  {
      LiquidScreen* curScreen = menuSystem.get_currentScreen();
      if(curScreen == &mainMenu_Options)
      {
        menuSystem.change_menu(optionsMenu);
        menuSystem.change_screen(mainMenu_optionsMenu_Calibrate);
      }     
      else if(curScreen == &mainMenu_optionsMenu_Calibrate)
      {
        menuSystem.change_menu(calibrateMenu);
        //menuSystem.change_screen(mainMenu_optionsMenu_Calibrate_Weight);         
      }
      else if(curScreen == &mainMenu_optionsMenu_Calibrate_Weight)
      {
        CalibrateResults calibrateResults = CalibrateScale(
          &lcd,
          &loadCell,
          calibrationFactor,
          calibrationWeight,
          HOME_BUTTON_PIN,
          BASELINEREADINGS);

        baseline = calibrateResults.Baseline;
        calibrationFactor = calibrateResults.CalibrationFactor;
        showingMenu = false;
        readSamples = true;
        forceRefresh = true;             
      }
      else if(curScreen == &mainMenu_optionsMenu_Rounding)
      {
        enableRounding = !enableRounding;
        menuRequiresUpdate = true;
      }
      else if(curScreen == &mainMenu_optionsMenu_Back)
      {
        menuSystem.change_menu(mainMenu);
      }
  }
  RegisterActivity();
}

void ManagedEncoderCallback(String key, EncoderState encoderState)
{
  LiquidScreen* curScreen = menuSystem.get_currentScreen();
  if(encoderState == EncoderState::EncoderClockwise)
  {
    if(showingMenu)
    {      
      if(curScreen == &mainMenu_optionsMenu_Calibrate_Weight)
      {
        if(calibrationWeight < MAX_CALIBRATION_WEIGHT)
        {
          calibrationWeight += 5.0;
        }
        menuRequiresUpdate = true;
      }
      else
      {
        menuSystem.next_screen();
      }
    }
  }
  else
  {
    if(showingMenu)
    {
      if(curScreen == &mainMenu_optionsMenu_Calibrate_Weight)
      {
        if(calibrationWeight > MIN_CALIBRATION_WEIGHT)
        {
          calibrationWeight -= 5.0;
        }
        menuRequiresUpdate = true;
      }
      else
      {
        menuSystem.previous_screen();
      }
    }
  }
  menuRequiresUpdate = true;
  RegisterActivity();
}

void RegisterActivity()
{
  lastActivityMillis = millis();
}

void Sleep(int pinToWake)
{
  lcd.clear();
  lcd.print(F("Sleeping..."));
  Serial.print(F("Sleeping..."));
  delay(3000);

  lcd.clear();
  lcd.noBacklight();
  Serial.println(F("Attaching interrupt!"));
  int prevPinMode = GetPinMode(pinToWake);
  pinMode (pinToWake, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pinToWake), SleepWakeInterrupt, FALLING);
  
  Serial.flush();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();

  pinMode (pinToWake, prevPinMode);
  detachInterrupt(digitalPinToInterrupt(pinToWake));
  lcd.backlight();
}

void SleepWakeInterrupt()
{
  sleep_disable();
}
