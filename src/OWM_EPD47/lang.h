#ifndef LANG_H_
#define LANG_H_

// English texts.
//
// All strings are const char* const: as const String each of these would have
// allocated on the heap before setup() even runs.

#define FONT(x) x##_tf


//Temperature - Humidity - Forecast
const char *const TXT_TEMPERATURE_C    = "Temperature (°C)";
const char *const TXT_HUMIDITY_PERCENT = "Humidity (%RH)";
const char *const TXT_HUMIDITY_SHORT   = "%RH";
const char *const TXT_FEELSLIKE        = "feels like";

// Pressure
const char *const TXT_PRESSURE         = "Pressure: ";
const char *const TXT_PRESSURE_HPA     = "Pressure (hPa)";

// Precipitation
const char *const TEXT_SNOWFALL_CM       = "Snowfall (cm)";
const char *const TEXT_RAIN_SNOW_MM      = "Rain + snow (mm)";
const char *const TEXT_PRECIPITATION_MM = "Precipitation (mm)";

//Moon
const char *const TXT_MOON_NEW             = "New";
const char *const TXT_MOON_WAXING_CRESCENT = "Waxing Crescent";
const char *const TXT_MOON_FIRST_QUARTER   = "First Quarter";
const char *const TXT_MOON_WAXING_GIBBOUS  = "Waxing Gibbous";
const char *const TXT_MOON_FULL            = "Full";
const char *const TXT_MOON_WANING_GIBBOUS  = "Waning Gibbous";
const char *const TXT_MOON_THIRD_QUARTER   = "Third Quarter";
const char *const TXT_MOON_WANING_CRESCENT = "Waning Crescent";

//Wind
const char *const TXT_N   = "N";
const char *const TXT_NNE = "NNE";
const char *const TXT_NE  = "NE";
const char *const TXT_ENE = "ENE";
const char *const TXT_E   = "E";
const char *const TXT_ESE = "ESE";
const char *const TXT_SE  = "SE";
const char *const TXT_SSE = "SSE";
const char *const TXT_S   = "S";
const char *const TXT_SSW = "SSW";
const char *const TXT_SW  = "SW";
const char *const TXT_WSW = "WSW";
const char *const TXT_W   = "W";
const char *const TXT_WNW = "WNW";
const char *const TXT_NW  = "NW";
const char *const TXT_NNW = "NNW";

//Status messages
const char *const TXT_NO_WIFI  = "No WiFi connection available.";
const char *const TXT_NO_TIME  = "Time synchronisation failed.";
const char *const TXT_NO_DATA  = "Weather data unreachable.";
const char *const TXT_RETRY_IN = "Next attempt in ";
const char *const TXT_MINUTES  = " minutes.";

//Upload mode
const char *const TXT_UPLOAD_MODE       = "UPLOAD MODE";
const char *const TXT_UPLOAD_AWAKE_PRE  = "Staying awake for ";
const char *const TXT_UPLOAD_AWAKE_POST = " seconds.";
const char *const TXT_UPLOAD_NOW        = "Upload the sketch now.";
const char *const TXT_UPLOAD_AFTER      = "The display refreshes normally afterwards.";

//Day of the week
static const char *const weekday_D[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

//Month
static const char *const month_M[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

// Weather code texts per WMO WW as delivered by Open-Meteo.
// Source: https://open-meteo.com/en/docs -> "WMO Weather interpretation codes (WW)"
// These 28 codes are the only ones Open-Meteo documents; anything else falls
// through to the default branch on purpose.
static const char *weatherCodeToText(int weatherCode) {
  switch (weatherCode) {
    case  0: return "Clear sky";                                   // Clear sky
    case  1: return "Mainly clear";                                // Mainly clear
    case  2: return "Partly cloudy";                               // Partly cloudy
    case  3: return "Overcast";                                    // Overcast
    case 45: return "Fog";                                         // Fog
    case 48: return "Rime fog";                                    // Depositing rime fog
    case 51: return "Light drizzle";                               // Drizzle: light
    case 53: return "Moderate drizzle";                            // Drizzle: moderate
    case 55: return "Dense drizzle";                               // Drizzle: dense
    case 56: return "Light freezing drizzle";                      // Freezing drizzle: light
    case 57: return "Dense freezing drizzle";                      // Freezing drizzle: dense
    case 61: return "Slight rain";                                 // Rain: slight
    case 63: return "Moderate rain";                               // Rain: moderate
    case 65: return "Heavy rain";                                  // Rain: heavy
    case 66: return "Light freezing rain";                         // Freezing rain: light
    case 67: return "Heavy freezing rain";                         // Freezing rain: heavy
    case 71: return "Slight snow fall";                            // Snow fall: slight
    case 73: return "Moderate snow fall";                          // Snow fall: moderate
    case 75: return "Heavy snow fall";                             // Snow fall: heavy
    case 77: return "Snow grains";                                 // Snow grains
    case 80: return "Slight rain showers";                         // Rain showers: slight
    case 81: return "Moderate rain showers";                       // Rain showers: moderate
    case 82: return "Violent rain showers";                        // Rain showers: violent
    case 85: return "Slight snow showers";                         // Snow showers: slight
    case 86: return "Heavy snow showers";                          // Snow showers: heavy
    case 95: return "Thunderstorm";                                // Thunderstorm: slight or moderate
    case 96: return "Thunderstorm, slight hail";                   // Thunderstorm with slight hail
    case 99: return "Thunderstorm, heavy hail";                    // Thunderstorm with heavy hail
    default: return "Unknown conditions";
  }
}

#endif /* ifndef LANG_H_ */
