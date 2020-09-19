struct ScaleInitResults
{
  long Baseline;
  float CalibrationFactor;
};

long GetLargeBaseline(HX711* scale, int count)
{
  long total = 0;
  for(int curAverage = 0; curAverage < count; curAverage++)
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

  Serial.println("Getting baseline...");
  results.Baseline = GetLargeBaseline(scale, baselineReadings);
  Serial.print("Baseline: ");
  Serial.println(results.Baseline);

  Serial.print("Loading calibration factor...");
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
  Serial.print("Calibration Factor: ");
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
