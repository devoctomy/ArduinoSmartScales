long GetLargeBaseline(HX711 scale, int count)
{
  long total = 0;
  for(int curAverage = 0; curAverage < count; curAverage++)
  {
    total = scale.read_average(); 
  }
  return total / count;
}

float GetAveragedSample(HX711 scale, int count)
{
  float sample = scale.get_units(count);
  if(sample < 0) sample = 0;
  return sample;
}
