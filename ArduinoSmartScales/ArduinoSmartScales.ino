#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <LiquidMenu.h>
#include <avr/sleep.h>
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

LiquidCrystal_I2C lcd(0x27, 16, 2);
HX711 scale;

int encoderButtonPin = 5;
int encoderClockwisePin = 3;
int encoderAntiClockwisePin = 4;
int homeButtonPin = 2;

float calibration_factor = 431;
long baseline = 0;
unsigned short analogReading = 0;
bool menuRequiresUpdate = true;
int lastActivityMillis = 0;
int inactivityTimeBeforeSleep = 5000;

LiquidLine welcome_line1(1, 0, "SmartScales ", SMARTSCALES_VERSION);
LiquidLine welcome_line2(1, 1, "");
LiquidScreen welcome_screen(welcome_line1, welcome_line2);

LiquidLine analogReading_line(0, 0, "Analog: ", analogReading);
LiquidScreen secondary_screen(analogReading_line);
LiquidMenu menu(lcd);

void setup()
{
  Serial.begin(9600);
  Serial.println("Configuring lcd");  

  lcd.init();
  lcd.backlight();
  lcd.print("Please wait...");

  Serial.println("Configuring menu");
  menu.init();
  menu.add_screen(welcome_screen);
  menu.add_screen(secondary_screen);   

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
  //scale.begin(DOUT, CLK);
  //scale.set_scale();

  Serial.println("Resetting");
  //scale.tare();

  Serial.println("Getting baseline");
  //baseline = scale.read_average();
  //Serial.print("Baseline: ");
  //Serial.println(baseline);

  lcd.clear();
  lastActivityMillis = millis();
}

float GetAveragedSamples(int sampleCount)
{
  float sample = scale.get_units(sampleCount);
  if(sample < 0) sample = 0;
  return sample;
}

void loop()
{
  /*
  scale.set_scale(calibration_factor);

  Serial.print("Reading: ");

  float averageSample = GetAveragedSamples(AVERAGESAMPLECOUNT);

  lcd.clear();
  lcd.print(averageSample);
  lcd.print("g");
  
  Serial.print(averageSample);
  Serial.print(" grams"); 
  Serial.print(" calibration_factor: ");
  Serial.print(calibration_factor);
  Serial.println();
  */

  CheckManagedButtons();
  
  if(menuRequiresUpdate)
  {
    menu.update();
    menuRequiresUpdate = false;
  }

  int timeSinceLastActivity = (millis() - lastActivityMillis);
  if(timeSinceLastActivity >= inactivityTimeBeforeSleep)
  {
    Sleep(homeButtonPin);
    RegisterActivity();
  }
  else
  {
    Serial.println(inactivityTimeBeforeSleep - timeSinceLastActivity);
  }
}

void ManagedButtonCallback(String key, ButtonState buttonState)
{
  if(key == "Home" && buttonState == ButtonState::ButtonDepressed)
  {
      menu.previous_screen();
  }
  else if(key == "Encoder.Button" && buttonState == ButtonState::ButtonDepressed)
  {
      menu.next_screen();
  }
}

void ManagedEncoderCallback(String key, EncoderState encoderState)
{
  if(encoderState == EncoderState::EncoderClockwise)
  {
    analogReading += 1;
  }
  else
  {
    analogReading -= 1;
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
