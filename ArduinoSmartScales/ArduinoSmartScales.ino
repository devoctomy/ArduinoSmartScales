#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <LiquidMenu.h>
#include <avr/sleep.h>
#include <EEPROM.h>
#include "HX711.h"
#include "ButtonManager.h"

#define DOUT A2
#define CLK  A3
#define AVERAGESAMPLECOUNT 7
#define BUZZER 8
#define MENU_BUTTON 0
#define SELECT_BUTTON 1
#define LEFT_BUTTON 2
#define RIGHT_BUTTON 3
#define SMARTSCALES_VERSION "1.0"
#define AVERAGE_SAMPLE_COUNT 2
#define INACTIVITY_TIME_BEFORE_SLEEP 60000

LiquidCrystal_I2C lcd(0x27, 16, 2);
HX711 scale;

int encoderButtonPin = 5;
int encoderClockwisePin = 3;
int encoderAntiClockwisePin = 4;
int homeButtonPin = 2;

float calibration_factor = 429.24;
long baseline = 0;
unsigned short analogReading = 0;
bool menuRequiresUpdate = true;
int lastActivityMillis = 0;
int startMillis = 0;
bool showingMenu = false;
bool forceRefresh = false;
bool requiresCalibration = false;

bool readSamples = true;           //Controls whether or not to read a sample during the main loop
int curSampleIndex = 0;;
int curSampleCount = 0;
float lastAverageSample = 0;
float lastDelta = 0;
float samples[AVERAGE_SAMPLE_COUNT];

LiquidLine mainMenu_Options_Line1(0, 0, "Options");
LiquidScreen mainMenu_Options(mainMenu_Options_Line1);

LiquidLine mainMenu_optionsMenu_Calibrate_Line1(0, 0, "Calibrate");
LiquidScreen mainMenu_optionsMenu_Calibrate(mainMenu_optionsMenu_Calibrate_Line1);

LiquidLine mainMenu_optionsMenu_Back_Line1(0, 0, "< Back");
LiquidScreen mainMenu_optionsMenu_Back(mainMenu_optionsMenu_Back_Line1);

LiquidMenu mainMenu(lcd);
LiquidMenu optionsMenu(lcd);

LiquidSystem menuSystem(0);

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
  optionsMenu.add_screen(mainMenu_optionsMenu_Back);

  menuSystem.add_menu(mainMenu);
  menuSystem.add_menu(optionsMenu);

  Serial.println("Configuring buttons");
  AddManagedButton({
    "Home",
    homeButtonPin,
    true,
    (ButtonCallbackDelegate)ManagedButtonCallback
  });

  Serial.println("Configuring encoder");
  AddManagedEncoder({
    "Encoder",
    encoderClockwisePin,
    encoderAntiClockwisePin,
    encoderButtonPin,
    true,
    (ButtonCallbackDelegate)ManagedButtonCallback,
    (EncoderCallbackDelegate)ManagedEncoderCallback
  });

  Serial.println("Initialising");
  scale.begin(DOUT, CLK);
  scale.set_scale();

  Serial.println("Resetting");
  scale.tare();

  Serial.println("Getting baseline");
  baseline = GetLargeBaseline(10); //scale.read_average();
  Serial.print("Baseline: ");
  Serial.println(baseline);
  EEPROM.get(0, calibration_factor);
  if(isnan(calibration_factor))
  {
    calibration_factor = 0;
    requiresCalibration = true;
    readSamples = false;
  }
  Serial.print("Calibration Factor: ");
  Serial.println(calibration_factor);
  scale.set_scale(calibration_factor);

  lcd.clear();
  lastActivityMillis = millis();
}

long GetLargeBaseline(int count)
{
  long total = 0;
  for(int curAverage = 0; curAverage < count; curAverage++)
  {
    total = scale.read_average(); 
  }
  return total / count;
}

float GetAveragedSample()
{
  float sample = scale.get_units(3);
  if(sample < 0) sample = 0;
  curSampleCount += 1;
  float averageSample = AddSampleAndGetAverage(sample); 
  return averageSample;
}

float AddSampleAndGetAverage(float sample)
{
  if(curSampleIndex < AVERAGE_SAMPLE_COUNT)
  {
    samples[curSampleIndex] = sample;
    curSampleIndex += 1;
    return sample;
  }
  else
  {
    for(int curIndex = 0; curIndex < (AVERAGE_SAMPLE_COUNT - 1); curIndex++)
    {
      samples[curIndex] = samples[curIndex + 1];
    }
    samples[AVERAGE_SAMPLE_COUNT - 1] = sample;
  }

  float curSampleAverage = 0;
  for(int curIndex = 0; curIndex < AVERAGE_SAMPLE_COUNT; curIndex++)
  {
    curSampleAverage += samples[curIndex];
  }
  curSampleAverage = curSampleAverage / AVERAGE_SAMPLE_COUNT; 
  return curSampleAverage;
}

void loop()
{
  // only read when in reading mode
  if(readSamples)
  {
    float averageSample = GetAveragedSample();
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
      if(lastDelta < -0.01 || lastDelta > 0.01 || forceRefresh)
      {
        lcd.clear();
        lcd.print(lastAverageSample);
        lcd.print("g");
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
    Sleep(homeButtonPin);
    RegisterActivity();
  }
}

void ManagedButtonCallback(String key, ButtonState buttonState)
{
  if(key == "Home" && buttonState == ButtonState::ButtonDepressed)
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
  else if(key == "Encoder.Button" && buttonState == ButtonState::ButtonDepressed)
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
      else if(curScreen == &mainMenu_optionsMenu_Back)
      {
        menuSystem.change_menu(mainMenu);
      }
  }
}

void Calibrate()
{
  char data[32];
  
  lcd.clear();
  lcd.print("Please wait...");
  delay(5000);
  lcd.clear();
  lcd.print("Clear scale...");
  delay(5000);
  scale.set_scale();
  scale.tare();
  scale.set_scale(0);
  baseline = GetLargeBaseline(10); //scale.read_average();
  lcd.clear();
  lcd.print("Place 50g");
  delay(5000);

  lcd.clear();
  lcd.print("Calibrating...");
  calibration_factor = 0;
  scale.set_scale(calibration_factor);
  float stepSize = 4.000;
  int unitCount = 3;
  float sample = scale.get_units(unitCount);
  if(sample < 0) sample = 0;
  while(sample != 50.00)
  {
    float delta = sample - 50.00;
    if(delta < 0.006 && delta > -0.006)
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
    String cfString = String(calibration_factor);

    sprintf(data, "%s / %s", sampleString.c_str(), cfString.c_str());
    Serial.println(data);
    lcd.setCursor(0,1);
    lcd.print(data);
    
    if(sample > 50.00)
    {
      calibration_factor += stepSize;
    }
    else
    {
      calibration_factor -= stepSize;
    }

    scale.set_scale(calibration_factor);
    sample = scale.get_units(unitCount);
    if(sample < 0) sample = 0;
  }

  Serial.print("***Final Sample: ");
  Serial.println(sample);
  Serial.print("***Final Calibration Factor: ");
  Serial.println(calibration_factor);

  EEPROM.put(0, calibration_factor);

  lcd.clear();
  lcd.print("Complete...");
  delay(5000);

  lcd.clear();
  lcd.print("Clear scale...");
  delay(5000);
  
  showingMenu = false;
  readSamples = true;
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
