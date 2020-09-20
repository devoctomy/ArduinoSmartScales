void SleepWakeInterrupt()
{
  sleep_disable();
}

void Sleep(
  LiquidCrystal_I2C* lcd,
  int pinToWake)
{
  lcd->clear();
  lcd->print(F("Sleeping..."));
  Serial.print(F("Sleeping..."));
  _delay_ms(3000);

  lcd->clear();
  lcd->noBacklight();
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
  lcd->backlight();
}
