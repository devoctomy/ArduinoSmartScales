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
  int ClockwiseDigitalPin;
  int AntiClockwiseDigitalPin;
  int ButtonDigitalPin;
  bool ButtonPullUp;
  ButtonCallbackDelegate ButtonCallback;
  EncoderCallbackDelegate EncoderCallback;  
  int ClockwiseButtonIndex;
  int AntiClockwiseButtonIndex;
  int ButtonIndex;
  int LastVal;
};

struct ButtonDef
{
  String Key;
  int DigitalPin;
  bool PullUp;
  ButtonCallbackDelegate ButtonCallback;
  EncoderDef *EncoderParent;
  int LastVal;
};

ButtonDef managedButtonDefs[4];
EncoderDef managedEncoderDefs[1];
int managedButtonCount = 0;
int managedEncoderCount = 0;

int CheckButton(ButtonDef *buttonDef)
{
   int buttonVal = digitalRead(buttonDef->DigitalPin);     //Read button value  
   int lastVal = buttonDef->LastVal;                       //Get last value
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
  int clockwise = CheckButton(&clockwiseButton);
  if(encoderDef->LastVal == LOW && clockwise == HIGH)
  {
    int antiClockwise = CheckButton(&antiClockwiseButton);

    encoderDef->EncoderCallback(encoderDef->Key, antiClockwise == HIGH ?
      EncoderState::EncoderClockwise :
      EncoderState::EncoderAntiClockwise);
  }
  encoderDef->LastVal = clockwise;
}

int AddManagedButton(ButtonDef buttonDef)
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
    encoderDef.Key + ".Clockwise",
    encoderDef.ClockwiseDigitalPin,
    false,
    NULL,
    &encoderDef
  };
  ButtonDef antiClocklwiseButton = {
    encoderDef.Key + ".AntiClockwise",
    encoderDef.AntiClockwiseDigitalPin,
    false,
    NULL,
    &encoderDef
  };
  ButtonDef button = {
    encoderDef.Key + ".Button",
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
  int i = 0;
  for(i = 0; i < managedButtonCount; i++)
  {
    if(managedButtonDefs[i].EncoderParent == NULL)
    {
      CheckButton(&managedButtonDefs[i]);
    }
  }

  for(i = 0; i < managedEncoderCount; i++)
  {
    CheckEncoder(&managedEncoderDefs[i]);
  }
}
