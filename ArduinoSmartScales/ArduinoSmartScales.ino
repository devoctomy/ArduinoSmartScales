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
#include "SleepHelpers.h"

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

enum ScalesMode
{
  Normal,
  Coffee
};

enum CoffeeModeStep
{
  None,
  Start,
  PlaceEmptyCarafe,
  PlaceFilterPaper,
  WetFilterPaper,
  EmptyCarafe,
  AddGrounds,
  FirstPour,
  Bloom,
  AddWater,
  Brew,
  Drink
};

const char MENU_TEXT_MODE[] PROGMEM = "Mode";
const char MENU_TEXT_MODE_NORMAL[] PROGMEM = "Normal";
const char MENU_TEXT_MODE_COFFEE[] PROGMEM = "Coffee";
const char MENU_TEXT_OPTIONS[] PROGMEM = "Options";
const char MENU_TEXT_OPTIONS_CALIBRATE[] PROGMEM = "Calibrate";
const char MENU_TEXT_OPTIONS_CALIBRATE_WEIGHT[] PROGMEM = "Weight: ";
const char MENU_TEXT_OPTIONS_ROUNDING[] PROGMEM = "Rounding: ";
const char MENU_TEXT_BACK[] PROGMEM = "< Back";

//-------------------------------------------------------------------------------------
//Global variables
//-------------------------------------------------------------------------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);                             //lcd object
HX711 loadCell;                                                 //scale object
float calibrationFactor = 429.24;                               //Default calibration factor
float calibrationWeight = 200.00;                               //Weight required for calibration
long baseline = 0;                                              //Baseline value
bool menuRequiresUpdate = true;                                 //Causes menu to be refreshed
unsigned long lastActivityMillis = 0;                           //Millisecond count that last activity was recorded
unsigned long startMillis = 0;                                  //Millisecond count that the system started
bool showingMenu = false;                                       //Signifies that the menu is currently being displayed
bool forceRefresh = false;                                      //Causes scale readout to be refreshed, reguardless of it changing or not
bool requiresCalibration = false;                               //Calibration factor is not valid, calibration needs to be performed
bool enableRounding = true;                                     //Enable / Disable rounding of samples
bool readSamples = true;                                        //Enable / Disable reading of samples during main program loop
unsigned int lastRounded = 999;                                 //Last rounded sample recorded
float lastUnrounded = -1;                                       //Last unrounded sample recorded
float lastAverageSample = -1;                                   //Last average sample recorded, used to calculate delta
float lastUnroundedOffsetSample = -1;                           //Last offset value, that being the desired weight - current unrounded weight
float lastRoundedOffsetSample = -1;                             //Last offset value, that being the desired weight - current rounded weight
float lastDelta = 0;                                            //Last delta
ScalesMode scalesMode = ScalesMode::Normal;                     //Scales mode
CoffeeModeStep coffeeModeStep = CoffeeModeStep::Start;          //Coffee mode step
CoffeeModeStep lastCoffeeModeStep = CoffeeModeStep::None;       //Last coffee mode step
bool ignoreHome = false;                                        //Ignore the next press of the home button
float curCoffeeModeStepSampleOffset = 0.0;                      //Ammount to offset the sample readout by
float curCoffeeModeStepDesiredWeight = 0.0;                     //Desired weight, this causes the scales to display how much is to be added until the desired weight is reached
bool curCoffeeModeStepRequiresNextClick = false;                //Determines if the current coffee mode step requires that next be clicked (push down on encoder)

//-------------------------------------------------------------------------------------
//Menu setup
//-------------------------------------------------------------------------------------
LiquidLine mainMenu_Mode_Line1(0, 0, MENU_TEXT_MODE);
LiquidScreen mainMenu_Mode(mainMenu_Mode_Line1);

LiquidLine mainMenu_modeMenu_Normal_Line1(0, 0, MENU_TEXT_MODE_NORMAL);
LiquidScreen mainMenu_modeMenu_Normal(mainMenu_modeMenu_Normal_Line1);

LiquidLine mainMenu_modeMenu_Coffee_Line1(0, 0, MENU_TEXT_MODE_COFFEE);
LiquidScreen mainMenu_modeMenu_Coffee(mainMenu_modeMenu_Coffee_Line1);

LiquidLine mainMenu_modeMenu_Back_Line1(0, 0, MENU_TEXT_BACK);
LiquidScreen mainMenu_modeMenu_Back(mainMenu_modeMenu_Back_Line1);

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
LiquidMenu modeMenu(lcd);
LiquidMenu optionsMenu(lcd);
LiquidMenu calibrateMenu(lcd);

LiquidSystem menuSystem(0);
//-------------------------------------------------------------------------------------

void setup()
{
  RegisterActivity();
  Serial.begin(9600);
  Serial.println(F("Configuring lcd"));  

  lcd.init();
  lcd.backlight();
  lcd.print(F("Please wait..."));

  Serial.println(F("Configuring menu"));
  mainMenu_Mode_Line1.set_asProgmem(1);
  mainMenu_modeMenu_Normal_Line1.set_asProgmem(1);
  mainMenu_modeMenu_Coffee_Line1.set_asProgmem(1);
  mainMenu_modeMenu_Back_Line1.set_asProgmem(1);
  mainMenu_Options_Line1.set_asProgmem(1);
  mainMenu_optionsMenu_Calibrate_Line1.set_asProgmem(1);
  mainMenu_optionsMenu_Calibrate_Weight_Line1.set_asProgmem(1);
  mainMenu_optionsMenu_Rounding_Line1.set_asProgmem(1);
  mainMenu_optionsMenu_Back_Line1.set_asProgmem(1);
  
  mainMenu.init();
  mainMenu.add_screen(mainMenu_Mode);
  mainMenu.add_screen(mainMenu_Options);

  modeMenu.init();
  modeMenu.add_screen(mainMenu_modeMenu_Normal);
  modeMenu.add_screen(mainMenu_modeMenu_Coffee);
  modeMenu.add_screen(mainMenu_modeMenu_Back);
  
  optionsMenu.init();
  optionsMenu.add_screen(mainMenu_optionsMenu_Calibrate);
  optionsMenu.add_screen(mainMenu_optionsMenu_Rounding);
  optionsMenu.add_screen(mainMenu_optionsMenu_Back);

  calibrateMenu.init();
  calibrateMenu.add_screen(mainMenu_optionsMenu_Calibrate_Weight);
  
  menuSystem.add_menu(mainMenu);
  menuSystem.add_menu(modeMenu);
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
      
      if(scalesMode == ScalesMode::Normal)
      {
        updated = displaySample(
          true,
          0,
          0,
          0,
          false);
      }
      else
      {
        bool stepChanged = lastCoffeeModeStep != coffeeModeStep;
        switch(coffeeModeStep)
        {
          case CoffeeModeStep::Start:
          {
            if(stepChanged || forceRefresh)
            {
              lcd.clear();
              lcd.print(F("Coffee Mode"));
              lcd.setCursor(0,1);
              lcd.print(F("Start         "));
              lcd.print((char)62);
              updated = true;
            }
            break;
          }
          case CoffeeModeStep::PlaceEmptyCarafe:
          {
            Serial.println(F("Place carafe"));
            
            if(stepChanged || forceRefresh)
            {
              lcd.clear();
              lcd.print(F("Place carafe"));
            }
            updated = displaySample(
              false,
              1,
              curCoffeeModeStepSampleOffset,
              curCoffeeModeStepDesiredWeight,
              true);
            
            break;
          }
          case CoffeeModeStep::PlaceFilterPaper:
          {
            Serial.println(F("Place filter paper"));

            if(stepChanged || forceRefresh)
            {
              lcd.clear();
              lcd.print(F("Place filter"));
            }
            updated = displaySample(
              false,
              1,
              curCoffeeModeStepSampleOffset,
              curCoffeeModeStepDesiredWeight,
              true);
            
            break;
          }
          case CoffeeModeStep::WetFilterPaper:
          {
            Serial.println(F("Wet filter paper"));
            
            if(stepChanged || forceRefresh)
            {
              lcd.clear();
              lcd.print(F("Wet paper"));
              lcd.setCursor(0, 1);
              lcd.print(F("Next          "));
              lcd.print((char)62);
              updated = true;
            }
            
            break;
          }
          case CoffeeModeStep::EmptyCarafe:
          {
            Serial.println(F("Empty carafe"));
            
            if(stepChanged || forceRefresh)
            {
              lcd.clear();
              lcd.print(F("Empty carafe"));
              lcd.setCursor(0, 1);
              lcd.print(F("Next          "));
              lcd.print((char)62);
              updated = true;
            }
            
            break;               
          }
          case CoffeeModeStep::AddGrounds:
          {
            Serial.println(F("Add grounds"));
            
            if(stepChanged || forceRefresh)
            {
              lcd.clear();
              lcd.print(F("Add grounds"));
            }
            updated = displaySample(
              false,
              1,
              curCoffeeModeStepSampleOffset,
              curCoffeeModeStepDesiredWeight,
              true);

            if(DesiredWeightReached())
            {
              InitCoffeeModeStep((CoffeeModeStep)((int)coffeeModeStep + 1));
            }
                          
            break;               
          }   
          case CoffeeModeStep::FirstPour:
          {
            Serial.println(F("First pour"));
            
            if(stepChanged || forceRefresh)
            {
              lcd.clear();
              lcd.print(F("First pour"));
            }             
            updated = displaySample(
              false,
              1,
              curCoffeeModeStepSampleOffset,
              curCoffeeModeStepDesiredWeight,
              true);              

            if(DesiredWeightReached())
            {
              InitCoffeeModeStep((CoffeeModeStep)((int)coffeeModeStep + 1));
            }

            break;
          }
          case CoffeeModeStep::Bloom:
          {
            Serial.println(F("Bloom"));
            
            PauseWithCountdown(F("Blooming..."), 45000);
            InitCoffeeModeStep((CoffeeModeStep)((int)coffeeModeStep + 1));

            break;
          }
          case CoffeeModeStep::AddWater:
          {
            Serial.println(F("Add water"));
            
            if(stepChanged || forceRefresh)
            {
              lcd.clear();
              lcd.print(F("Add water"));
            }             
            updated = displaySample(
              false,
              1,
              curCoffeeModeStepSampleOffset,
              curCoffeeModeStepDesiredWeight,
              true);

            if(DesiredWeightReached())
            {
              InitCoffeeModeStep((CoffeeModeStep)((int)coffeeModeStep + 1));
            }

            break;
          }
          case CoffeeModeStep::Brew:
          {
            Serial.println(F("Brew"));
            
            PauseWithCountdown(F("Brewing"), 120000);
            InitCoffeeModeStep((CoffeeModeStep)((int)coffeeModeStep + 1)); 

            break;
          }
          case CoffeeModeStep::Drink:
          {
            Serial.println(F("Drink"));
            
            lcd.clear();
            lcd.print(F("All done!"));
            lcd.setCursor(0, 1);
            lcd.print(F("Enjoy"));
            delay(5000);
            scalesMode = ScalesMode::Normal;
            showingMenu = false;
            readSamples = true;
            forceRefresh = true;

            break;
          }
        }

        lastCoffeeModeStep = coffeeModeStep;     
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
    ignoreHome = true;
    Sleep(&lcd, HOME_BUTTON_PIN);
    RegisterActivity();
    forceRefresh = true;
  }
}

bool DesiredWeightReached()
{
  if(enableRounding)
  {
    return lastRoundedOffsetSample <= 0;
  }
  else
  {
    return lastUnroundedOffsetSample <= 0;
  }
}

void PauseWithCountdown(
  String title,
  unsigned long pauseMillis)
{
  lcd.clear();
  lcd.print(title);
  lcd.setCursor(0, 1);
  lcd.print(F("Please wait..."));
  delay(2000);
  unsigned long start = millis();
  unsigned long elapsed = millis() - start;
  while(elapsed < (pauseMillis - 2000))
  {
    lcd.setCursor(0, 1);
    unsigned long remaining = bsdRound(((pauseMillis - 2000) - elapsed) / 1000);    //remaining time in whole seconds
    String remainingString = String(remaining > 0 ? remaining : 0);
    remainingString += F("s");
    while(remainingString.length() < 15)
    {
      remainingString += F(" ");
    }
    lcd.print(remainingString.c_str());
    delay(1000);
    RegisterActivity();
    elapsed = millis() - start;
  }
}

bool displaySample(
  bool clearScreen,
  int line,
  float offset,
  float desiredWeight,
  bool addNextGlyph)
{
  bool updated;
  if(enableRounding)
  {
    updated = displayRoundedSample(
      clearScreen,
      line,
      offset,
      desiredWeight,
      addNextGlyph);
  }
  else
  {
    updated = displayUnroundedSample(
      clearScreen,
      line,
      offset,
      desiredWeight,
      addNextGlyph);       
  }
  return updated;
}

bool displayUnroundedSample(
  bool clearScreen,
  int line,
  float offset,
  float desiredWeight,
  bool addNextGlyph)
{
  bool updated = false;
  if(lastAverageSample != lastUnrounded || forceRefresh)
  {
    if(clearScreen) lcd.clear();
    lcd.setCursor(0, line);
    lastUnroundedOffsetSample = desiredWeight > 0 ? desiredWeight - (lastAverageSample - offset) : (lastAverageSample - offset);
    String sampleString = String(lastUnroundedOffsetSample);
    sampleString += F("g");
    while(sampleString.length() < 15)
    {
      sampleString += F(" ");
    }
    if(addNextGlyph) sampleString[14] = (char)62;
    lcd.print(sampleString.c_str());
    lastUnrounded = lastAverageSample;
    updated = true;
  }
  return updated;
}

bool displayRoundedSample(
  bool clearScreen,
  int line,
  float offset,
  float desiredWeight,
  bool addNextGlyph)
{
  bool updated = false;
  unsigned int curRounded = (unsigned int)bsdRound(lastAverageSample);
  if(curRounded != lastRounded || forceRefresh)
  {
    if(clearScreen) lcd.clear();
    lcd.setCursor(0, line);

    lastRoundedOffsetSample = desiredWeight > 0 ? desiredWeight - (curRounded - (unsigned int)bsdRound(offset)) : (curRounded - (unsigned int)bsdRound(offset));
    String sampleString = String((unsigned int)lastRoundedOffsetSample);
    sampleString += F("g");
    while(sampleString.length() < 15)
    {
      sampleString += F(" ");
    }
    if(addNextGlyph) sampleString[14] = (char)62;
    lcd.print(sampleString.c_str());
    lastRounded = curRounded;
    updated = true;
  }
  return updated;
}

void ManagedButtonCallback(String key, ButtonState buttonState)
{
  if(key == F("H") && buttonState == ButtonState::ButtonDepressed)
  {
    Serial.println("Home depressed!");
    if(ignoreHome)
    {
      ignoreHome = false;
      return;  
    }
    
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
      if(showingMenu)
      {
        LiquidScreen* curScreen = menuSystem.get_currentScreen();
  
        //--------------------------
        //Mode
        //--------------------------
        if(curScreen == &mainMenu_Mode)
        {
          menuSystem.change_menu(modeMenu);
          menuSystem.change_screen(mainMenu_modeMenu_Normal);
        }
        else if(curScreen == &mainMenu_modeMenu_Normal)
        {
          scalesMode = ScalesMode::Normal;
          showingMenu = false;
          readSamples = true;
          forceRefresh = true;
        }
        else if(curScreen == &mainMenu_modeMenu_Coffee)
        {
          scalesMode = ScalesMode::Coffee;
          curCoffeeModeStepRequiresNextClick = true;
          showingMenu = false;
          readSamples = true;
          forceRefresh = true;
        }
        //--------------------------
        //Options
        //--------------------------
        else if(curScreen == &mainMenu_Options)
        {
          menuSystem.change_menu(optionsMenu);
          menuSystem.change_screen(mainMenu_optionsMenu_Calibrate);
        }     
        else if(curScreen == &mainMenu_optionsMenu_Calibrate)
        {
          menuSystem.change_menu(calibrateMenu);       
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
        //--------------------------
        //Back buttons
        //--------------------------
        else if(curScreen == &mainMenu_optionsMenu_Back ||
                curScreen == &mainMenu_modeMenu_Back)
        {
          menuSystem.change_menu(mainMenu);
        }
      }      
      else
      {
        if(scalesMode == ScalesMode::Coffee)
        {
          if(curCoffeeModeStepRequiresNextClick)
          {
            InitCoffeeModeStep((CoffeeModeStep)((int)coffeeModeStep + 1));
          }
          forceRefresh = true;
        }
      }
  }
  RegisterActivity();
}

void InitCoffeeModeStep(CoffeeModeStep desiredCoffeeModeStep)
{
  if(coffeeModeStep == desiredCoffeeModeStep)
  {
    return;  
  }

  coffeeModeStep = desiredCoffeeModeStep;
  curCoffeeModeStepRequiresNextClick = false;
  lcd.clear();
  switch(coffeeModeStep)
  {
    case CoffeeModeStep::PlaceEmptyCarafe:
    {
      curCoffeeModeStepRequiresNextClick = true;
      curCoffeeModeStepSampleOffset = 0;
      break;
    }
    case CoffeeModeStep::PlaceFilterPaper:
    {
      curCoffeeModeStepRequiresNextClick = true;
      curCoffeeModeStepSampleOffset = lastAverageSample;                                      //Ack weight of carafe from previous step
      break;
    }
    case CoffeeModeStep::WetFilterPaper:
    {
      curCoffeeModeStepRequiresNextClick = true;
      curCoffeeModeStepSampleOffset += (lastAverageSample - curCoffeeModeStepSampleOffset);   //Ack weight of dry filter paper from previous step
      break;
    }
    case CoffeeModeStep::EmptyCarafe:
    {
      curCoffeeModeStepRequiresNextClick = true;
      break;
    }
    case CoffeeModeStep::AddGrounds:
    {
      curCoffeeModeStepDesiredWeight = 15;                                                    //Set the weight of grounds we want to measure out
      curCoffeeModeStepSampleOffset += (lastAverageSample - curCoffeeModeStepSampleOffset);   //Ack weight of filter paper being wet from 2 steps ago
      break;
    }
    case CoffeeModeStep::FirstPour:
    {
      curCoffeeModeStepDesiredWeight = 60;                                                    //Set the weight of water we want to measure out
      curCoffeeModeStepSampleOffset += (lastAverageSample - curCoffeeModeStepSampleOffset);   //Ack weight of grounds from previous step
      break;
    }
    case CoffeeModeStep::Bloom:
    {
      curCoffeeModeStepSampleOffset += (lastAverageSample - curCoffeeModeStepSampleOffset);   //Ack weight of water from previous step
      break;
    } 
    case CoffeeModeStep::AddWater:
    {
      curCoffeeModeStepDesiredWeight = 240;
      break;
    }
    case CoffeeModeStep::Brew:
    {
      curCoffeeModeStepSampleOffset += (lastAverageSample - curCoffeeModeStepSampleOffset);   //Ack weight of water from previous step
      break;      
    }
  }  
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
