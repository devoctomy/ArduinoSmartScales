float bsdRound(float x)
{
  float t;
  if (!isfinite(x)) return (x);
  if (x >= 0.0)
  {
    t = floor(x);
    if (t - x <= -0.5)t += 1.0;
    return (t);
  }
  else
  {
    t = floor(-x);
    if (t + x <= -0.5) t += 1.0;
    return (-t);
  }
}
