#ifndef LANG_DE_H_
#define LANG_DE_H_

// German texts.
//
// All strings are const char* const: as const String each of these would have
// allocated on the heap before setup() even runs.

#define FONT(x) x##_tf


//Temperature - Humidity - Forecast
const char *const TXT_TEMPERATURE_C    = "Temperatur (°C)";
const char *const TXT_HUMIDITY_PERCENT = "Luftfeuchtigkeit (%rF)";
const char *const TXT_HUMIDITY_SHORT   = "%rF";
const char *const TXT_FEELSLIKE        = "gefühlt";

// Pressure
const char *const TXT_PRESSURE         = "Druck: ";
const char *const TXT_PRESSURE_HPA     = "Luftdruck (hPa)";

// Precipitation
const char *const TEXT_SNOWFALL_CM       = "Neuschnee (cm)";
const char *const TEXT_RAIN_SNOW_MM      = "Regen + Schnee (mm)";
const char *const TEXT_PRECIPITATION_MM = "Niederschlag (mm)";

//Moon
const char *const TXT_MOON_NEW             = "Neumond";
const char *const TXT_MOON_WAXING_CRESCENT = "Zunehmend";
const char *const TXT_MOON_FIRST_QUARTER   = "Erstes Viertel";
const char *const TXT_MOON_WAXING_GIBBOUS  = "Zunehmender Dreiviertel";
const char *const TXT_MOON_FULL            = "Vollmond";
const char *const TXT_MOON_WANING_GIBBOUS  = "Schwindender Dreiviertel";
const char *const TXT_MOON_THIRD_QUARTER   = "Drittes Viertel";
const char *const TXT_MOON_WANING_CRESCENT = "Abnehmender Mond";

//Wind
const char *const TXT_N   = "N";
const char *const TXT_NNE = "NNO";
const char *const TXT_NE  = "NO";
const char *const TXT_ENE = "ONO";
const char *const TXT_E   = "O";
const char *const TXT_ESE = "OSO";
const char *const TXT_SE  = "SO";
const char *const TXT_SSE = "SSO";
const char *const TXT_S   = "S";
const char *const TXT_SSW = "SSW";
const char *const TXT_SW  = "SW";
const char *const TXT_WSW = "WSW";
const char *const TXT_W   = "W";
const char *const TXT_WNW = "WNW";
const char *const TXT_NW  = "NW";
const char *const TXT_NNW = "NNW";

//Status messages
const char *const TXT_NO_WIFI  = "Keine WLAN-Verbindung möglich.";
const char *const TXT_NO_TIME  = "Keine Zeitsynchronisation möglich.";
const char *const TXT_NO_DATA  = "Wetterdaten nicht erreichbar.";
const char *const TXT_RETRY_IN = "Nächster Versuch in ";
const char *const TXT_MINUTES  = " Minuten.";

//Upload mode
const char *const TXT_UPLOAD_MODE       = "UPLOAD-MODUS";
const char *const TXT_UPLOAD_AWAKE_PRE  = "Gerät bleibt ";
const char *const TXT_UPLOAD_AWAKE_POST = " Sekunden wach.";
const char *const TXT_UPLOAD_NOW        = "Jetzt den Sketch hochladen.";
const char *const TXT_UPLOAD_AFTER      = "Danach aktualisiert sich das Display normal.";

//Day of the week
static const char *const weekday_D[] = { "Son", "Mon", "Die", "Mit", "Don", "Fre", "Sam" };

//Month
static const char *const month_M[] = { "Jan", "Feb", "Mär", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez" };

// Weather code texts per WMO WW as delivered by Open-Meteo.
// Source: https://open-meteo.com/en/docs -> "WMO Weather interpretation codes (WW)"
// These 28 codes are the only ones Open-Meteo documents; anything else falls
// through to the default branch on purpose.
static const char *weatherCodeToText(int weatherCode) {
  switch (weatherCode) {
    case  0: return "Klar";                                        // Clear sky
    case  1: return "Überwiegend klar";                            // Mainly clear
    case  2: return "Teilweise bewölkt";                           // Partly cloudy
    case  3: return "Bedeckt";                                     // Overcast
    case 45: return "Nebel";                                       // Fog
    case 48: return "Reifnebel";                                   // Depositing rime fog
    case 51: return "Leichter Nieselregen";                        // Drizzle: light
    case 53: return "Mäßiger Nieselregen";                         // Drizzle: moderate
    case 55: return "Dichter Nieselregen";                         // Drizzle: dense
    case 56: return "Leichter gefrierender Nieselregen";           // Freezing drizzle: light
    case 57: return "Dichter gefrierender Nieselregen";            // Freezing drizzle: dense
    case 61: return "Leichter Regen";                              // Rain: slight
    case 63: return "Mäßiger Regen";                               // Rain: moderate
    case 65: return "Starker Regen";                               // Rain: heavy
    case 66: return "Leichter gefrierender Regen";                 // Freezing rain: light
    case 67: return "Starker gefrierender Regen";                  // Freezing rain: heavy
    case 71: return "Leichter Schneefall";                         // Snow fall: slight
    case 73: return "Mäßiger Schneefall";                          // Snow fall: moderate
    case 75: return "Starker Schneefall";                          // Snow fall: heavy
    case 77: return "Schneegriesel";                               // Snow grains
    case 80: return "Leichte Regenschauer";                        // Rain showers: slight
    case 81: return "Mäßige Regenschauer";                         // Rain showers: moderate
    case 82: return "Heftige Regenschauer";                        // Rain showers: violent
    case 85: return "Leichte Schneeschauer";                       // Snow showers: slight
    case 86: return "Starke Schneeschauer";                        // Snow showers: heavy
    case 95: return "Gewitter";                                    // Thunderstorm: slight or moderate
    case 96: return "Gewitter mit leichtem Hagel";                 // Thunderstorm with slight hail
    case 99: return "Gewitter mit starkem Hagel";                  // Thunderstorm with heavy hail
    default: return "Wetterlage unbekannt";
  }
}

#endif /* ifndef LANG_DE_H_ */
