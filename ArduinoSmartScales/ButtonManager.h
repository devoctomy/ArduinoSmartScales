enum ButtonState
{
  ButtonDepressed,
  ButtonPressed
};

enum EncoderState
{
  EncoderClockwise,
  EncoderAntiClockwise
};

typedef void (*ButtonCallbackDelegate) (String key, ButtonState buttonState);
typedef void (*EncoderCallbackDelegate) (String key, EncoderState encoderState);

struct EncoderDef
{
  String Key;
  byte ClockwiseDigitalPin;
  byte AntiClockwiseDigitalPin;
  byte ButtonDigitalPin;
  bool ButtonPullUp;
  ButtonCallbackDelegate ButtonCallback;
  EncoderCallbackDelegate EncoderCallback;  
  byte ClockwiseButtonIndex;
  byte AntiClockwiseButtonIndex;
  byte ButtonIndex;
  byte LastVal;
};

struct ButtonDef
{
  String Key;
  byte DigitalPin;
  bool PullUp;
  ButtonCallbackDelegate ButtonCallback;
  EncoderDef *EncoderParent;
  byte LastVal;
};

ButtonDef managedButtonDefs[4];
EncoderDef managedEncoderDefs[1];
byte managedButtonCount = 0;
byte managedEncoderCount = 0;

byte CheckButton(ButtonDef *buttonDef)
{
   byte buttonVal = digitalRead(buttonDef->DigitalPin);     //Read button value  
   byte lastVal = buttonDef->LastVal;                       //Get last value
   if(buttonVal != lastVal)                               //Compare new value against last value
   {
    buttonDef->LastVal = buttonVal;                        //Store current value as last value
    if(buttonDef->ButtonCallback != NULL)
    {
      buttonDef->ButtonCallback(buttonDef->Key, lastVal == 0 ?
        ButtonState::ButtonDepressed :
        ButtonState::ButtonPressed);
    }
    return buttonVal;
   }
}

void CheckEncoder(EncoderDef *encoderDef)
{
  ButtonDef clockwiseButton = managedButtonDefs[encoderDef->ClockwiseButtonIndex];
  ButtonDef antiClockwiseButton = managedButtonDefs[encoderDef->AntiClockwiseButtonIndex];
  byte clockwise = CheckButton(&clockwiseButton);
  if(encoderDef->LastVal == LOW && clockwise == HIGH)
  {
    byte antiClockwise = CheckButton(&antiClockwiseButton);
    encoderDef->EncoderCallback(encoderDef->Key, antiClockwise == HIGH ?
      EncoderState::EncoderClockwise :
      EncoderState::EncoderAntiClockwise);
  }
  encoderDef->LastVal = clockwise;
}

byte AddManagedButton(ButtonDef buttonDef)
{
  if(buttonDef.PullUp)
  {
    pinMode (buttonDef.DigitalPin, INPUT_PULLUP);
    buttonDef.LastVal = HIGH;
  }
  else
  {
    pinMode (buttonDef.DigitalPin, INPUT);
  }
  managedButtonDefs[managedButtonCount] = buttonDef;
  managedButtonCount += 1;
  return managedButtonCount-1;
}

void AddManagedEncoder(EncoderDef encoderDef)
{
  ButtonDef clocklwiseButton = {
    encoderDef.Key + F(".CW"),
    encoderDef.ClockwiseDigitalPin,
    false,
    NULL,
    &encoderDef
  };
  ButtonDef antiClocklwiseButton = {
    encoderDef.Key + F(".AC"),
    encoderDef.AntiClockwiseDigitalPin,
    false,
    NULL,
    &encoderDef
  };
  ButtonDef button = {
    encoderDef.Key + F(".B"),
    encoderDef.ButtonDigitalPin,
    true,
    encoderDef.ButtonCallback
  };

  encoderDef.ClockwiseButtonIndex = AddManagedButton(clocklwiseButton);
  encoderDef.AntiClockwiseButtonIndex = AddManagedButton(antiClocklwiseButton);
  encoderDef.ButtonIndex = AddManagedButton(button);

  managedEncoderDefs[managedEncoderCount] = encoderDef;
  managedEncoderCount += 1;
}

void CheckManagedButtons()
{
  for(byte i = 0; i < managedButtonCount; i++)
  {
    if(managedButtonDefs[i].EncoderParent == NULL)
    {
      CheckButton(&managedButtonDefs[i]);
    }
  }

  for(byte i = 0; i < managedEncoderCount; i++)
  {
    CheckEncoder(&managedEncoderDefs[i]);
  }
}
