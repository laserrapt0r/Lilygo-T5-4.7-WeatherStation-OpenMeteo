#ifndef WEATHERCODES_H_
#define WEATHERCODES_H_

// Maps a WMO weather code to an icon name.
//
// Source: Open-Meteo, "WMO Weather interpretation codes (WW)"
// https://open-meteo.com/en/docs  (retrieved 2026-08-20)
//
//   0            Clear sky
//   1, 2, 3      Mainly clear, partly cloudy, and overcast
//   45, 48       Fog and depositing rime fog
//   51, 53, 55   Drizzle: Light, moderate, and dense intensity
//   56, 57       Freezing Drizzle: Light and dense intensity
//   61, 63, 65   Rain: Slight, moderate and heavy intensity
//   66, 67       Freezing Rain: Light and heavy intensity
//   71, 73, 75   Snow fall: Slight, moderate, and heavy intensity
//   77           Snow grains
//   80, 81, 82   Rain showers: Slight, moderate, and violent
//   85, 86       Snow showers slight and heavy
//   95           Thunderstorm: Slight or moderate
//   96, 99       Thunderstorm with slight and heavy hail
//
// These 28 codes are the only ones Open-Meteo documents. Anything else falls
// through to the default branch on purpose and renders as "?".
//
// NOTE: the table used here previously came from the SYNOP ww scheme
// (WMO 4677) and did NOT line up with these codes - code 61 ("slight rain")
// was rendered as "heavy rain", for example.

static const char *weatherCodeToIcon(int weatherCode) {
  switch (weatherCode) {
    case 0:            return "sun";
    case 1:            return "fewclouds";
    case 2:            return "scatteredClouds";
    case 3:            return "brokenClouds";
    case 45:
    case 48:           return "mist";
    case 51 ... 57:    return "chanceRain";   // drizzle and freezing drizzle
    case 61 ... 67:    return "rain";         // rain and freezing rain
    case 71 ... 77:    return "snow";         // snow fall and snow grains
    case 80 ... 82:    return "chanceRain";   // rain showers
    case 85 ... 86:    return "snow";         // snow showers
    case 95 ... 99:    return "thunderstorm";
    default:           return "nA";
  }
}

#endif /* ifndef WEATHERCODES_H_ */
