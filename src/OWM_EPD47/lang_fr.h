#ifndef LANG_FR_H_
#define LANG_FR_H_

// French texts.
//
// All strings are const char* const: as const String each of these would have
// allocated on the heap before setup() even runs.

#define FONT(x) x##_tf


//Temperature - Humidity - Forecast
const char *const TXT_TEMPERATURE_C    = "Température (°C)";
const char *const TXT_HUMIDITY_PERCENT = "Humidité (%HR)";
const char *const TXT_HUMIDITY_SHORT   = "%HR";
const char *const TXT_FEELSLIKE        = "ressenti";

// Pressure
const char *const TXT_PRESSURE         = "Pression : ";
const char *const TXT_PRESSURE_HPA     = "Pression (hPa)";

// Precipitation
const char *const TEXT_SNOWFALL_CM       = "Neige fraîche (cm)";
const char *const TEXT_RAIN_SNOW_MM      = "Pluie + neige (mm)";
const char *const TEXT_PRECIPITATION_MM = "Précipitations (mm)";

//Moon
const char *const TXT_MOON_NEW             = "Nouvelle Lune";
const char *const TXT_MOON_WAXING_CRESCENT = "Premier Croissant";
const char *const TXT_MOON_FIRST_QUARTER   = "Premier Quartier";
const char *const TXT_MOON_WAXING_GIBBOUS  = "Gibbeuse Croissan.";
const char *const TXT_MOON_FULL            = "Pleine Lune";
const char *const TXT_MOON_WANING_GIBBOUS  = "Gibbeuse Décroiss.";
const char *const TXT_MOON_THIRD_QUARTER   = "Dernier Quartier";
const char *const TXT_MOON_WANING_CRESCENT = "Dernier Croissant";

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
const char *const TXT_SSW = "SSO";
const char *const TXT_SW  = "SO";
const char *const TXT_WSW = "OSO";
const char *const TXT_W   = "O";
const char *const TXT_WNW = "ONO";
const char *const TXT_NW  = "NO";
const char *const TXT_NNW = "NNO";

//Status messages
const char *const TXT_NO_WIFI  = "Aucune connexion WiFi disponible.";
const char *const TXT_NO_TIME  = "Synchronisation de l'heure impossible.";
const char *const TXT_NO_DATA  = "Données météo inaccessibles.";
const char *const TXT_RETRY_IN = "Prochaine tentative dans ";
const char *const TXT_MINUTES  = " minutes.";

//Upload mode
const char *const TXT_UPLOAD_MODE       = "MODE TÉLÉVERSEMENT";
const char *const TXT_UPLOAD_AWAKE_PRE  = "Appareil éveillé pendant ";
const char *const TXT_UPLOAD_AWAKE_POST = " secondes.";
const char *const TXT_UPLOAD_NOW        = "Téléversez le sketch maintenant.";
const char *const TXT_UPLOAD_AFTER      = "L'affichage se met ensuite à jour normalement.";

//Day of the week
static const char *const weekday_D[] = { "Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam" };

//Month
static const char *const month_M[] = { "Jan", "Fév", "Mar", "Avr", "Mai", "Jun", "Jul", "Aou", "Sep", "Oct", "Nov", "Déc" };

// Weather code texts per WMO WW as delivered by Open-Meteo.
// Source: https://open-meteo.com/en/docs -> "WMO Weather interpretation codes (WW)"
// These 28 codes are the only ones Open-Meteo documents; anything else falls
// through to the default branch on purpose.
static const char *weatherCodeToText(int weatherCode) {
  switch (weatherCode) {
    case  0: return "Ciel dégagé";                                 // Clear sky
    case  1: return "Principalement dégagé";                       // Mainly clear
    case  2: return "Partiellement nuageux";                       // Partly cloudy
    case  3: return "Couvert";                                     // Overcast
    case 45: return "Brouillard";                                  // Fog
    case 48: return "Brouillard givrant";                          // Depositing rime fog
    case 51: return "Bruine légère";                               // Drizzle: light
    case 53: return "Bruine modérée";                              // Drizzle: moderate
    case 55: return "Bruine dense";                                // Drizzle: dense
    case 56: return "Bruine verglaçante légère";                   // Freezing drizzle: light
    case 57: return "Bruine verglaçante dense";                    // Freezing drizzle: dense
    case 61: return "Pluie faible";                                // Rain: slight
    case 63: return "Pluie modérée";                               // Rain: moderate
    case 65: return "Pluie forte";                                 // Rain: heavy
    case 66: return "Pluie verglaçante faible";                    // Freezing rain: light
    case 67: return "Pluie verglaçante forte";                     // Freezing rain: heavy
    case 71: return "Neige faible";                                // Snow fall: slight
    case 73: return "Neige modérée";                               // Snow fall: moderate
    case 75: return "Neige forte";                                 // Snow fall: heavy
    case 77: return "Grains de neige";                             // Snow grains
    case 80: return "Averses faibles";                             // Rain showers: slight
    case 81: return "Averses modérées";                            // Rain showers: moderate
    case 82: return "Averses violentes";                           // Rain showers: violent
    case 85: return "Averses de neige faibles";                    // Snow showers: slight
    case 86: return "Averses de neige fortes";                     // Snow showers: heavy
    case 95: return "Orage";                                       // Thunderstorm: slight or moderate
    case 96: return "Orage, grêle légère";                         // Thunderstorm with slight hail
    case 99: return "Orage, grêle forte";                          // Thunderstorm with heavy hail
    default: return "Conditions inconnues";
  }
}

#endif /* ifndef LANG_FR_H_ */
