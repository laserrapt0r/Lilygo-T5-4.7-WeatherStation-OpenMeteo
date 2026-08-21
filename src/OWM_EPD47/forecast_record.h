#ifndef FORECAST_RECORD_H_
#define FORECAST_RECORD_H_

#include <Arduino.h>

// One record holds either the current conditions (WxConditions[0]) or a
// three-hour block of the forecast (WxForecast[]).
//
// Icon and Description point at string literals from weathercodes.h and
// lang_*.h and are never copied, hence const char* rather than String - that
// saves roughly a hundred heap allocations across both arrays.
typedef struct {
  int         Dt;
  const char *Icon;
  const char *Description;
  float       PressureTrend;   // hPa over 6 h, positive means rising
  float       Temperature;
  float       FeelsLike;
  float       Humidity;
  float       High;
  float       Low;
  float       Winddir;
  float       Windspeed;
  float       Rainfall;
  float       Snowfall;        // cm of fresh snow, not water equivalent
  float       Precipitation;
  float       Pressure;
  int         Cloudcover;
  int         Visibility;
  int         Sunrise;
  int         Sunset;
  bool        IsDay;
} Forecast_record_type;

#endif /* ifndef FORECAST_RECORD_H_ */
