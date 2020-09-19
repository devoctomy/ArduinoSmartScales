#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <LiquidMenu.h>
#include <avr/sleep.h>
#include <EEPROM.h>
#include "HX711.h"
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

//-------------------------------------------------------------------------------------
//Global variables
//-------------------------------------------------------------------------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);                       //lcd object
HX711 loadCell;                                           //scale object
float calibrationFactor = 429.24;                         //Default calibration factor
float calibrationWeight = 200.00;                         //Weight required for calibration
long baseline = 0;                                        //Baseline value
bool menuRequiresUpdate = true;                           //Causes menu to be refreshed
int lastActivityMillis = 0;                               //Millisecond count that last activity was recorded
int startMillis = 0;                                      //Millisecond count that the system started
bool showingMenu = false;                                 //Signifies that the menu is currently being displayed
bool forceRefresh = false;                                //Causes scale readout to be refreshed, reguardless of it changing or not
bool requiresCalibration = false;                         //Calibration factor is not valid, calibration needs to be performed
bool enableRounding = true;                               //Enable / Disable rounding of samples
bool readSamples = true;                                  //Enable / Disable reading of samples during main program loop
int lastRounded = -1;                                     //Last rounded sample recorded
float lastUnrounded = -1;                                 //Last unrounded sample recorded
float lastAverageSample = 0;                              //Last average sample recorded, used to calculate delta
float lastDelta = 0;                                      //Last delta
bool stopCalibrating = false;                             //Flag to abord calibration

//-------------------------------------------------------------------------------------
//Menu setup
//-------------------------------------------------------------------------------------
LiquidLine mainMenu_Options_Line1(0, 0, "Options");
LiquidScreen mainMenu_Options(mainMenu_Options_Line1);

LiquidLine mainMenu_optionsMenu_Calibrate_Line1(0, 0, "Calibrate");
LiquidScreen mainMenu_optionsMenu_Calibrate(mainMenu_optionsMenu_Calibrate_Line1);

LiquidLine mainMenu_optionsMenu_Rounding_Line1(0, 0, "Rounding: ", enableRounding);
LiquidScreen mainMenu_optionsMenu_Rounding(mainMenu_optionsMenu_Rounding_Line1);

LiquidLine mainMenu_optionsMenu_Back_Line1(0, 0, "< Back");
LiquidScreen mainMenu_optionsMenu_Back(mainMenu_optionsMenu_Back_Line1);

LiquidMenu mainMenu(lcd);
LiquidMenu optionsMenu(lcd);

LiquidSystem menuSystem(0);
//-------------------------------------------------------------------------------------

void setup()
{
  RegisterActivity();
  Serial.begin(9600);
  Serial.println("Configuring lcd");  

  lcd.init();
  lcd.backlight();
  lcd.print("Please wait...");

  Serial.println("Configuring menu");
  mainMenu.init();
  mainMenu.add_screen(mainMenu_Options);
  optionsMenu.init();
  optionsMenu.add_screen(mainMenu_optionsMenu_Calibrate);
  optionsMenu.add_screen(mainMenu_optionsMenu_Rounding);
  optionsMenu.add_screen(mainMenu_optionsMenu_Back);
  menuSystem.add_menu(mainMenu);
  menuSystem.add_menu(optionsMenu);

  Serial.println("Configuring buttons");
  AddManagedButton({
    "H",
    HOME_BUTTON_PIN,
    true,
    (ButtonCallbackDelegate)ManagedButtonCallback
  });

  Serial.println("Configuring encoder");
  AddManagedEncoder({
    "E",
    ENCODER_CLOCKWISE_PIN,
    ENCODER_ANTICLOCKWISE_PIN,
    ENCODER_BUTTON_PIN,
    true,
    (ButtonCallbackDelegate)ManagedButtonCallback,
    (EncoderCallbackDelegate)ManagedEncoderCallback
  });

  Serial.println("Initialising");
  loadCell.begin(DOUT, CLK);
  loadCell.set_scale();
  loadCell.tare();

  Serial.println("Getting baseline");
  baseline = GetLargeBaseline(loadCell, BASELINEREADINGS);
  Serial.print("Baseline: ");
  Serial.println(baseline);
  EEPROM.get(0, calibrationFactor);
  if(isnan(calibrationFactor))
  {
    calibrationFactor = 0;
    requiresCalibration = true;
    readSamples = false;
  }
  Serial.print("Calibration Factor: ");
  Serial.println(calibrationFactor);
  loadCell.set_scale(calibrationFactor);

  lcd.clear();
  lastActivityMillis = millis();
}

void loop()
{
  if(readSamples)
  {
    float averageSample = GetAveragedSample(loadCell, AVERAGESAMPLES);
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
        lcd.print("CALIBRATION");
        lcd.setCursor(0,1);
        lcd.print("REQUIRED!");
      }
    }
    else
    {
      bool updated = false;
      if(enableRounding)
      {
        int curRounded = (int)bsdRound(lastAverageSample);
        if(curRounded != lastRounded || forceRefresh)
        {
          lcd.clear();
          lcd.print(curRounded); 
          lcd.print("g");
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
          lcd.print("g");
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
    Serial.print("Time since last activity = ");
    Serial.println(timeSinceLastActivity);
    Sleep(HOME_BUTTON_PIN);
    RegisterActivity();
  }
}

void ManagedButtonCallback(String key, ButtonState buttonState)
{
  if(key == "H" && buttonState == ButtonState::ButtonDepressed)
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
  else if(key == "E.B" && buttonState == ButtonState::ButtonDepressed)
  {
      LiquidScreen* curScreen = menuSystem.get_currentScreen();
      if(curScreen == &mainMenu_Options)
      {
        menuSystem.change_menu(optionsMenu);
        menuSystem.change_screen(mainMenu_optionsMenu_Calibrate);
      }
      else if(curScreen == &mainMenu_optionsMenu_Calibrate)
      {
        Calibrate();
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
}

void Calibrate()
{
  int prevPinMode = GetPinMode(HOME_BUTTON_PIN);
  pinMode (HOME_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HOME_BUTTON_PIN), CalibrateStopInterrupt, FALLING);
  
  char data[32];
  
  lcd.clear();
  lcd.print("Please wait...");
  delay(5000);
  lcd.clear();
  lcd.print("Clear scale...");
  delay(5000);
  loadCell.set_scale();
  loadCell.tare();
  loadCell.set_scale(0);
  baseline = GetLargeBaseline(loadCell, BASELINEREADINGS);
  lcd.clear();
  lcd.print("Place 200g");
  delay(5000);

  lcd.clear();
  lcd.print("Calibrating...");
  stopCalibrating = false;
  float prevCalibrationFactor = calibrationFactor;
  calibrationFactor = 0;
  loadCell.set_scale(calibrationFactor);
  float stepSize = 4.000;
  int unitCount = 3;
  float sample = loadCell.get_units(unitCount);
  if(sample < 0) sample = 0;
  while(sample != calibrationWeight)
  {
    if(stopCalibrating)
    {
      detachInterrupt(digitalPinToInterrupt(HOME_BUTTON_PIN));
      pinMode (HOME_BUTTON_PIN, prevPinMode);
      lcd.clear();
      lcd.print("Aborting...");
      delay(5000);
      calibrationFactor = prevCalibrationFactor;
      loadCell.set_scale(calibrationFactor);
      showingMenu = false;
      readSamples = true;
      forceRefresh = true;
      return;
    }
    
    float delta = sample - calibrationWeight;
    if(delta <= 0.2 && delta >= -0.2)
    {
      break;
    }
    else if(delta < 2 && delta > -2 && stepSize > 0.001)
    {
      stepSize -= 0.001;
      unitCount = 10;
    }
    else if(delta < 5 && delta > -5 && stepSize > 0.01)
    {
       stepSize -= 0.010;
       unitCount = 7;
    }
    else if(delta < 10 && delta > -10 && stepSize != 1.0)
    {
      stepSize = 1.000;
      unitCount = 5;
    }
    else if(delta < 50 && delta > -50 && stepSize != 2.0)
    {
      stepSize = 2.000;
      unitCount = 4;
    }

    Serial.print("Step size: ");
    Serial.println(stepSize);
     
    memset(data, 0, sizeof(data));   
    String sampleString = String(sample);
    String cfString = String(calibrationFactor);

    sprintf(data, "%s / %s", sampleString.c_str(), cfString.c_str());
    Serial.println(data);
    lcd.setCursor(0,1);
    lcd.print(data);
    
    if(sample > calibrationWeight)
    {
      calibrationFactor += stepSize;
    }
    else
    {
      calibrationFactor -= stepSize;
    }

    loadCell.set_scale(calibrationFactor);
    sample = loadCell.get_units(unitCount);
    if(sample < 0) sample = 0;
  }

  EEPROM.put(0, calibrationFactor);

  lcd.clear();
  lcd.print("Complete,");
  lcd.setCursor(0,1);
  lcd.print("Clear scale...");
  delay(5000);
  
  showingMenu = false;
  readSamples = true;
  forceRefresh = true;
}

void ManagedEncoderCallback(String key, EncoderState encoderState)
{
  if(encoderState == EncoderState::EncoderClockwise)
  {
    if(showingMenu)
    {
      menuSystem.next_screen();
    }
  }
  else
  {
    if(showingMenu)
    {
      menuSystem.previous_screen();
    }
  }
  menuRequiresUpdate = true;
}

void RegisterActivity()
{
  lastActivityMillis = millis();
}

void Sleep(int pinToWake)
{
  lcd.clear();
  lcd.print("Sleeping...");
  Serial.print("Sleeping...");
  delay(3000);

  lcd.clear();
  lcd.noBacklight();
  Serial.println("Attaching interrupt!");
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

void CalibrateStopInterrupt()
{
  stopCalibrating = true;
}

int GetPinMode(uint8_t pin)
{
  if (pin >= NUM_DIGITAL_PINS) return (-1);

  uint8_t bit = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);
  volatile uint8_t *reg = portModeRegister(port);
  if (*reg & bit) return (OUTPUT);

  volatile uint8_t *out = portOutputRegister(port);
  return ((*out & bit) ? INPUT_PULLUP : INPUT);
}
