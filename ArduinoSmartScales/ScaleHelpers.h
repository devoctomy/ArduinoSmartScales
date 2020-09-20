struct ScaleInitResults
{
  long Baseline;
  float CalibrationFactor;
};

struct CalibrateResults
{
  long Baseline;
  float CalibrationFactor;
};

bool stopCalibrating = false;

long GetLargeBaseline(HX711* scale, int count)
{
  long total = 0;
  for(byte curAverage = 0; curAverage < count; curAverage++)
  {
    total = scale->read_average(); 
  }
  return total / count;
}

ScaleInitResults InitialiseScale(
  HX711* scale,
  int dout,
  int clk,
  int baselineReadings)
{
  ScaleInitResults results = { 0, 0 };
  
  scale->begin(dout, clk);
  scale->set_scale();
  scale->tare();

  Serial.println(F("Getting baseline..."));
  results.Baseline = GetLargeBaseline(scale, baselineReadings);
  Serial.print(F("Baseline: "));
  Serial.println(results.Baseline);

  Serial.print(F("Loading calibration factor..."));
  float loadedCalibrationFactor;
  EEPROM.get(0, loadedCalibrationFactor);
  if(isnan(loadedCalibrationFactor))
  {
    results.CalibrationFactor = 0;
  }
  else
  {
    results.CalibrationFactor = loadedCalibrationFactor;
  }
  Serial.print(F("Calibration Factor: "));
  Serial.println(results.CalibrationFactor);
  
  scale->set_scale(results.CalibrationFactor);
  return results;
}

float GetAveragedSample(HX711* scale, int count)
{
  float sample = scale->get_units(count);
  if(sample < 0) sample = 0;
  return sample;
}

void CalibrateStopInterrupt()
{
  stopCalibrating = true;
}

CalibrateResults CalibrateScale(
  LiquidCrystal_I2C* lcd,
  HX711* scale,
  float curCalibrationFactor,
  float calibrationWeight,
  int homeButtonPin,
  int baselineReadings)
{
  CalibrateResults results = { 0, 0 };
  
  int prevPinMode = GetPinMode(homeButtonPin);
  pinMode (homeButtonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(homeButtonPin), CalibrateStopInterrupt, FALLING);
  
  char data[24];
  
  lcd->clear();
  lcd->print(F("Clear scale..."));
  delay(5000);
  scale->set_scale();
  scale->tare();
  scale->set_scale(0);
  results.Baseline = GetLargeBaseline(scale, baselineReadings);
  lcd->clear();
  String weight = String(calibrationWeight);
  memset(data, 0, sizeof(data));
  sprintf(data, "Place %sg", weight.c_str());
  lcd->print(data);
  delay(5000);

  lcd->clear();
  lcd->print(F("Calibrating..."));
  stopCalibrating = false;
  float prevCalibrationFactor = curCalibrationFactor;
  results.CalibrationFactor = 1;
  scale->set_scale(results.CalibrationFactor);
  float stepSize = 4.000;
  int unitCount = 3;
  float scaleTop = calibrationWeight + 50.00;
  float sample = scale->get_units(unitCount);
  while(sample != calibrationWeight)
  {
    if(stopCalibrating)
    {
      detachInterrupt(digitalPinToInterrupt(homeButtonPin));
      pinMode (homeButtonPin, prevPinMode);
      lcd->clear();
      lcd->print(F("Aborting..."));
      delay(5000);
      results.CalibrationFactor = prevCalibrationFactor;
      scale->set_scale(results.CalibrationFactor);  
      return results;
    }

    float delta = sample - calibrationWeight;
    float positiveDelta = delta;
    if(positiveDelta < 0) positiveDelta = positiveDelta * -1;
    float pc = positiveDelta / 1000;
    if(pc > 1) pc = 1;
    stepSize = 20 * pc;

    Serial.print(F("Positive delta: "));
    Serial.println(positiveDelta, 4);
    
    if(positiveDelta <= 0.2)
    {
      break;
    }
     
    memset(data, 0, sizeof(data));   
    String sampleString = String(sample);
    String cfString = String(results.CalibrationFactor);

    sprintf(data, "%s / %s", sampleString.c_str(), cfString.c_str());
    lcd->setCursor(0,1);
    lcd->print(data);
    
    if(sample > calibrationWeight)
    {
      results.CalibrationFactor += stepSize;
    }
    else
    {
      results.CalibrationFactor -= stepSize;
    }

    scale->set_scale(results.CalibrationFactor);
    sample = scale->get_units(unitCount);
    if(sample < 0) sample = 0;
  }

  EEPROM.put(0, results.CalibrationFactor);

  lcd->clear();
  lcd->print(F("Calibration"));
  lcd->setCursor(0,1);
  lcd->print(F("Complete"));
  delay(2000);

  return results;
}
