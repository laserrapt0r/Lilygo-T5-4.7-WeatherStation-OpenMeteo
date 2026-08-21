// ============================================================================
//  Weather station for the LilyGo T5-S3 4.7" e-paper display (ESP32-S3)
//
//  Data source: Open-Meteo (https://open-meteo.com) - no API key required.
//  Based on the OpenWeatherMap weather station by G6EJD.
//
//  Configuration: user_settings.h (copy it from user_settings.h.example)
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <strings.h>            // strcasecmp
#include <esp_sleep.h>          // esp_deep_sleep_start, esp_sleep_enable_timer_wakeup
#include <driver/rtc_io.h>      // keeps the wake button pulled up in deep sleep
#include "epd_driver.h"

#include "user_settings.h"      // also pulls in lang_*.h
#include "forecast_record.h"
#include "weathercodes.h"
#include "MoonRise.h"          // https://github.com/signetica/MoonRise - is the moon above the horizon?

// Fonts & Bitmaps
#include "OpenSans8B.h"
#include "OpenSans10B.h"
#include "OpenSans12B.h"
#include "OpenSans18B.h"
#include "OpenSans24B.h"
#include "moon.h"
#include "sunrise.h"
#include "sunset.h"

#define SCREEN_WIDTH   EPD_WIDTH
#define SCREEN_HEIGHT  EPD_HEIGHT

enum alignment { LEFT, RIGHT, CENTER };

#define White         0xFF
#define LightGrey     0xBB
#define Grey          0x88
#define DarkGrey      0x44
#define Black         0x00

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

const bool LargeIcon = true;
const bool SmallIcon = false;
#define Large  20
#define Small  10

#define max_readings 24         // 24 blocks of 3 hours = 72 hours

Forecast_record_type WxConditions[1];
Forecast_record_type WxForecast[max_readings];

float pressure_readings[max_readings]      = {0};
float temperature_readings[max_readings]   = {0};
float humidity_readings[max_readings]      = {0};
float precipitation_readings[max_readings] = {0};
float liquid_readings[max_readings]        = {0};   // precipitation minus the snow share

String Time_str = "--:--:--";
String Date_str = "-- --- ----";
int    wifi_signal   = 0;
int    CurrentHour   = 0;
int    CurrentMin    = 0;
int    CurrentSec    = 0;
bool   TimeIsValid   = false;
long   SleepDuration = SLEEP_ACTIVE_MIN;
long   StartTime     = 0;

// Survives deep sleep - drives the NTP and full-refresh intervals.
RTC_DATA_ATTR uint32_t bootCount = 0;
uint32_t cycle = 0;   // bootCount - 1, so 0 on the very first boot

// Start mode, derived from the wake-up cause and the button state
bool g_serviceMode  = false;   // button held at boot -> upload window
bool g_manualUpdate = false;   // button tapped -> refresh right away

GFXfont  currentFont;
uint8_t *framebuffer = NULL;

// Holds the patch of framebuffer underneath an icon while it is being measured
// - see DrawIconCentred. Big enough for the largest of those patches: the
// large icon's band plus its margin, 266 px wide and 247 rows tall at 4 bpp.
#define ICON_PATCH_BYTES  36864
uint8_t *iconPatch = NULL;

// How much sky an icon leaves visible. Decides what replaces the sun at night:
// a clear sky can show a whole constellation, a covered one only a few stars.
enum SkyKind { SKY_CLEAR, SKY_CLOUDY };

// One star of an asterism: position normalised around the centre, x to the
// right and y down, plus the radius as a fraction of the field size.
typedef struct { float x, y, mag; } StarPos;
typedef struct { uint8_t a, b; } StarLink;

// ---- Forward declarations ---------------------------------------------------
//  The Arduino builder generates prototypes automatically, but not reliably for
//  every construct, so they are all spelled out here. For the same reason none
//  of these functions uses default arguments: a generated prototype would
//  collide with the declaration.

// Flow
void  DetermineStartMode();
void  EnterServiceMode();
bool  UpdateLocalTime(uint32_t timeout_ms);
bool  SetupTime();
bool  IsAwakeWindow(int hour);
long  SecondsUntilWakeupHour();
void  BeginSleep();
void  BeginPanel();
void  EndPanel();
void  ShowMessage(const String &line1, const String &line2);

// Data
bool  obtainWeatherDataOpenMeteo(WiFiClient &client);
bool  DecodeWeatherOpenMeteo(JsonDocument &doc);

// Helpers
String ConvertUnixTime(int unix_time);
String TitleCase(String text);
String formatNumber(unsigned int value);
int    JulianDate(int d, int m, int y);
double NormalizedMoonPhase(int d, int m, int y);

// Display
void  DisplayWeather();
void  DisplayStatusSection(int x, int y, int rssi);
void  DisplayGeneralInfoSection();
void  DisplayDisplayWindSection(int x, int y, float angle, float windspeed, int Cradius);
void  DisplayAstronomySection(int x, int y);
void  DisplayMainWeatherSection(int x, int y);
void  DisplayWeatherIcon(int cx, int bandTop, int bandBottom);
bool  InkExtent(int x0, int x1, int y0, int y1, int *top, int *bottom);
bool  PatchSave(int x0, int y0, int x1, int y1);
void  PatchRestore();
void  DrawIconCentred(int cx, int x0, int x1, int bandTop, int bandBottom,
                      const char *icon, bool IconSize, bool isDay, long timestamp);
void  DisplayTempHumiPressSection(int x, int y);
void  DisplayForecastTextSection(int x, int y);
void  DisplayVisiCCoverSection(int x, int y);
void  DisplayForecastWeather(int x, int y, int index, int fwidth);
void  DisplayForecastSection(int x, int y);
void  DisplayGraphSection();
void  DisplayConditionsSection(int x, int y, const char *IconName, bool IconSize, bool isDay,
                               long timestamp);
bool  isMoonUp(long timestamp);
int   stringWidth(String text);
void  DrawPressureAndTrend(int x, int y, float pressure);
void  DrawRSSI(int x, int y, int rssi);
void  DrawBattery(int x, int y);
void  DrawMoon(int x, int y, int diameter, int dd, int mm, int yy, const char *hemisphere);
void  DrawMoonImage(int x, int y);
void  DrawSunriseImage(int x, int y);
void  DrawSunsetImage(int x, int y);
void  DrawGraph(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max,
                const char *title, float DataArray[], float LowerArray[], int readings,
                bool auto_scale, bool barchart_mode);
const char *WindDegToOrdinalDirection(float winddirection);
const char *MoonPhase(int d, int m, int y, const char *hemisphere);

// Icons
void  addcloud(int x, int y, int scale, int linesize);
void  addrain(int x, int y, bool IconSize);
void  addsnow(int x, int y, bool IconSize);
void  addtstorm(int x, int y, int scale);
void  addsun(int x, int y, int scale);
void  addmoon(int cx, int cy, int scale);
void  addstar(int cx, int cy, int r);
void  addMoonIndicator(int x, int y, bool IconSize);
void  addstars(int cx, int cy, int scale);
void  addconstellation(int cx, int cy, int scale);
void  drawAsterism(int cx, int cy, int f, const StarPos *stars, int nStars,
                   const StarLink *links, int nLinks);
void  addsunormoon(int x, int y, int scale, bool night, bool moonUp, SkyKind sky, bool IconSize);
void  addfog(int x, int y, int scale, int linesize);
void  DrawAngledLine(int x, int y, int x1, int y1, int size, int color);
void  ClearSky(int x, int y, bool IconSize, bool night, bool moonUp);
void  BrokenClouds(int x, int y, bool IconSize, bool night, bool moonUp);
void  FewClouds(int x, int y, bool IconSize, bool night, bool moonUp);
void  ScatteredClouds(int x, int y, bool IconSize, bool night, bool moonUp);
void  Rain(int x, int y, bool IconSize);
void  ChanceRain(int x, int y, bool IconSize, bool night, bool moonUp);
void  Thunderstorms(int x, int y, bool IconSize);
void  Snow(int x, int y, bool IconSize);
void  Mist(int x, int y, bool IconSize, bool night, bool moonUp);
void  CloudCover(int x, int y, int CloudCover);
void  Nodata(int x, int y, bool IconSize);
void  Visibility(int x, int y, String VisibilityText);
void  arrow(int x, int y, int asize, float aangle, int pwidth, int plength);

// Drawing primitives
void  drawString(int32_t x, int32_t y, String text, alignment align);
void  setFont(GFXfont const &font);
void  fillCircle(int x, int y, int r, uint8_t color);
void  drawFastHLine(int16_t x0, int16_t y0, int length, uint16_t color);
void  drawFastVLine(int16_t x0, int16_t y0, int length, uint16_t color);
void  drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void  drawCircle(int x0, int y0, int r, uint8_t color);
void  drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void  fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void  fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
void  drawPixel(int x, int y, uint8_t color);

// ============================================================================
//  System, WiFi, time, sleep
// ============================================================================

void InitialiseSystem() {
  StartTime = millis();
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n--- Starting up ---");
  Serial.printf("Boot #%lu\n", (unsigned long)bootCount);

  // Pre-fill the pointers so an error path never dereferences NULL.
  WxConditions[0].Icon        = "nA";
  WxConditions[0].Description = "";
  for (int i = 0; i < max_readings; i++) {
    WxForecast[i].Icon        = "nA";
    WxForecast[i].Description = "";
  }

  epd_init();
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  iconPatch   = (uint8_t *)ps_calloc(sizeof(uint8_t), ICON_PATCH_BYTES);
  if (!framebuffer) {
    Serial.println("!!! ERROR: no memory for the framebuffer !!!");
  } else {
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    Serial.println("Framebuffer allocated.");
  }
}

// The button pulls to GND, so LOW means pressed (same as the LilyGo example).
//   tapped, or woke the device      -> refresh immediately
//   held down while booting         -> upload window
void DetermineStartMode() {
#if ENABLE_BUTTON_WAKEUP
  pinMode(WAKE_BUTTON_PIN, INPUT_PULLUP);
  bool wokeByButton = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0);

  if (digitalRead(WAKE_BUTTON_PIN) != LOW) {
    g_manualUpdate = wokeByButton;
    return;
  }
  unsigned long held = millis();
  while (digitalRead(WAKE_BUTTON_PIN) == LOW) {
    if (millis() - held >= SERVICE_HOLD_MS) { g_serviceMode = true; return; }
    delay(20);
  }
  g_manualUpdate = true;
#endif
}

// Keeps the device awake long enough to accept an upload. Without this it goes
// back to sleep within seconds - and at night for hours at a time.
void EnterServiceMode() {
  Serial.println("\n=== UPLOAD MODE ===");
  Serial.printf("Staying awake for %d s. Upload the sketch now.\n", SERVICE_MODE_SECONDS);
  if (framebuffer) {
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    BeginPanel();
    setFont(OpenSans24B);
    drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 90, TXT_UPLOAD_MODE, CENTER);
    setFont(OpenSans12B);
    drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 20,
               String(TXT_UPLOAD_AWAKE_PRE) + SERVICE_MODE_SECONDS + TXT_UPLOAD_AWAKE_POST, CENTER);
    drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20, TXT_UPLOAD_NOW, CENTER);
    drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 60,
               TXT_UPLOAD_AFTER, CENTER);
    EndPanel();
  }
  for (int s = SERVICE_MODE_SECONDS; s > 0; s--) {
    if (s % 10 == 0 || s <= 5) Serial.printf("  %d s left\n", s);
    delay(1000);
  }
  Serial.println("=== Upload mode finished ===");
}

uint8_t StartWiFi() {
  Serial.println("\nConnecting to " + String(ssid));
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

#if USE_STATIC_IP
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Static IP configuration failed, falling back to DHCP");
  }
#endif

  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS) {
    delay(100);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifi_signal = WiFi.RSSI();
    Serial.println("WiFi connected, IP " + WiFi.localIP().toString() + ", RSSI " + String(wifi_signal));
  } else {
    Serial.println("WiFi connection FAILED");
  }
  return WiFi.status();
}

void StopWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi switched off");
}

bool UpdateLocalTime(uint32_t timeout_ms) {
  struct tm timeinfo;
  char day_output[40], time_output[40];

  if (!getLocalTime(&timeinfo, timeout_ms)) {
    Serial.println("Could not obtain the time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin  = timeinfo.tm_min;
  CurrentSec  = timeinfo.tm_sec;
  Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");

  snprintf(day_output, sizeof(day_output), "%s, %02u %s %04u",
           weekday_D[timeinfo.tm_wday], timeinfo.tm_mday,
           month_M[timeinfo.tm_mon], (unsigned)(timeinfo.tm_year + 1900));
  strftime(time_output, sizeof(time_output), "%H:%M:%S", &timeinfo);

  Date_str = day_output;
  Time_str = time_output;
  return true;
}

bool SetupTime() {
  // TZ has to be set again after every wake-up; the RTC itself keeps running.
  setenv("TZ", Timezone, 1);
  tzset();

  if ((cycle % NTP_SYNC_EVERY) != 0 && UpdateLocalTime(200)) {
    Serial.println("Time taken from the RTC, no NTP needed");
    TimeIsValid = true;
    return true;
  }

  configTzTime(Timezone, ntpServer, "time.nist.gov");
  TimeIsValid = UpdateLocalTime(8000);
  return TimeIsValid;
}

bool IsAwakeWindow(int hour) {
  if (WAKEUP_HOUR > SLEEP_HOUR) return (hour >= WAKEUP_HOUR || hour <= SLEEP_HOUR);
  return (hour >= WAKEUP_HOUR && hour <= SLEEP_HOUR);
}

// Seconds until the next WAKEUP_HOUR, so the night is slept through in one
// stretch instead of ~21 pointless wake-ups.
long SecondsUntilWakeupHour() {
  long nowSec  = CurrentHour * 3600L + CurrentMin * 60L + CurrentSec;
  long wakeSec = (long)WAKEUP_HOUR * 3600L;
  long delta   = wakeSec - nowSec;
  if (delta <= 0) delta += 24L * 3600L;
  return delta + SLEEP_DELTA_SEC;
}

void BeginSleep() {
  epd_poweroff_all();

  long sleepSeconds;
  if (TimeIsValid && UpdateLocalTime(200)) {
    if (!IsAwakeWindow(CurrentHour)) {
      sleepSeconds = SecondsUntilWakeupHour();
      Serial.printf("Sleeping through the night until %02d:00\n", WAKEUP_HOUR);
    } else {
      sleepSeconds = SleepDuration * 60L - ((CurrentMin % SleepDuration) * 60L + CurrentSec) + SLEEP_DELTA_SEC;
    }
  } else {
    // Without a valid clock the wake-up cannot be aligned to the hour.
    sleepSeconds = SleepDuration * 60L + SLEEP_DELTA_SEC;
  }
  if (sleepSeconds < 30) sleepSeconds = 30;

  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000LL);
#if ENABLE_BUTTON_WAKEUP
  // Extra wake source: without it the device is deaf for hours at night.
  // ext0 rather than ext1, because the ext1 mode constants differ between
  // IDF 4.4 and 5.x while ext0 is identical on both core generations.
  rtc_gpio_pullup_en((gpio_num_t)WAKE_BUTTON_PIN);
  rtc_gpio_pulldown_dis((gpio_num_t)WAKE_BUTTON_PIN);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_BUTTON_PIN, 0);   // 0 = LOW = pressed
#endif
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + " s");
  Serial.println("Sleeping  : " + String(sleepSeconds) + " s");
  Serial.printf("Free heap (min): %u bytes\n", ESP.getMinFreeHeap());
  Serial.println("Entering deep sleep...");
  Serial.flush();
  esp_deep_sleep_start();
}

// ============================================================================
//  Flow
// ============================================================================

void loop() {
  // Stays empty - the flow always ends in deep sleep.
}

void setup() {
  bootCount++;
  cycle = bootCount - 1;
  DetermineStartMode();          // first of all, so the button is read immediately
  InitialiseSystem();
  if (g_serviceMode) EnterServiceMode();

  bool wifiOk = (StartWiFi() == WL_CONNECTED);
  bool timeOk = wifiOk && SetupTime();
  // A button press forces a refresh even outside the display window, and so
  // does upload mode - otherwise its notice would stay on the panel.
  bool forced   = g_manualUpdate || g_serviceMode;
  bool inWindow = timeOk && (IsAwakeWindow(CurrentHour) || forced);
  bool dataOk = false;

  if (inWindow) {
    WiFiClient client;
    for (int attempt = 1; attempt <= MAX_FETCH_RETRIES && !dataOk; attempt++) {
      Serial.printf("\n--- Data request attempt %d of %d ---\n", attempt, MAX_FETCH_RETRIES);
      dataOk = obtainWeatherDataOpenMeteo(client);
      if (!dataOk && attempt < MAX_FETCH_RETRIES) {
        Serial.println("Incomplete data, pausing briefly before the next attempt...");
        delay(RETRY_DELAY_MS);
      }
    }
  }

  StopWiFi();   // on every path, including at night and on errors

  if (!wifiOk) {
    SleepDuration = SLEEP_ERROR_MIN;
    ShowMessage(TXT_NO_WIFI, String(TXT_RETRY_IN) + SleepDuration + TXT_MINUTES);
  } else if (!timeOk) {
    SleepDuration = SLEEP_ERROR_MIN;
    ShowMessage(TXT_NO_TIME, String(TXT_RETRY_IN) + SleepDuration + TXT_MINUTES);
  } else if (!IsAwakeWindow(CurrentHour) && !forced) {
    // At night the image is deliberately left as is - no refresh, no request.
    Serial.println("Outside the display window, panel left unchanged.");
  } else if (dataOk) {
    bool weatherIsMoving = (WxConditions[0].Rainfall > 0) ||
                           (WxForecast[0].Rainfall > 0) ||
                           (fabsf(WxConditions[0].PressureTrend) >= PRESSURE_TREND_THRESHOLD);
    SleepDuration = weatherIsMoving ? SLEEP_ACTIVE_MIN : SLEEP_STABLE_MIN;
    Serial.printf("Pressure trend %+.1f hPa/6h -> weather %s, interval %ld minutes\n",
                  WxConditions[0].PressureTrend,
                  weatherIsMoving ? "on the move" : "stable", SleepDuration);

    if (framebuffer) {
      BeginPanel();
      DisplayWeather();
      EndPanel();
    }
  } else {
    SleepDuration = SLEEP_ERROR_MIN;
    ShowMessage(TXT_NO_DATA, String(TXT_RETRY_IN) + SleepDuration + TXT_MINUTES);
  }

  BeginSleep();
}

// Power up and clear the panel BEFORE anything is drawn.
//
// This ordering matters: DrawMoonImage(), DrawSunriseImage() and
// DrawSunsetImage() push their bitmaps straight to the panel rather than into
// the framebuffer. They only survive if the panel is already powered and
// cleared. EndPanel() then blits the framebuffer on top - and because
// epd_draw_grayscale_image() only ever darkens, white framebuffer pixels leave
// the panel untouched and the bitmaps stay visible.
void BeginPanel() {
  if (!framebuffer) return;
  epd_poweron();
  // epd_clear() wipes the panel and prevents ghosting. FULL_REFRESH_EVERY can
  // thin it out - see user_settings.h for the trade-off.
  if ((cycle % FULL_REFRESH_EVERY) == 0) epd_clear();
}

void EndPanel() {
  if (!framebuffer) return;
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  epd_poweroff_all();
}

void ShowMessage(const String &line1, const String &line2) {
  if (!framebuffer) return;
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  BeginPanel();
  DisplayStatusSection(600, 20, wifi_signal);
  setFont(OpenSans18B);
  drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 30, line1, CENTER);
  if (line2.length()) drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20, line2, CENTER);
  EndPanel();
}

// ============================================================================
//  Open-Meteo data retrieval
// ============================================================================

bool DecodeWeatherOpenMeteo(JsonDocument &doc) {
  JsonObject current = doc["current"];
  JsonObject hourly  = doc["hourly"];
  JsonObject daily   = doc["daily"];

  if (current.isNull() || hourly.isNull() || daily.isNull()) {
    Serial.println("Response is missing current/hourly/daily");
    return false;
  }

  // Resolve the arrays once instead of looking up the key on every access.
  JsonArray hTime  = hourly["time"];
  JsonArray hTemp  = hourly["temperature_2m"];
  JsonArray hHum   = hourly["relative_humidity_2m"];
  JsonArray hCode  = hourly["weather_code"];
  JsonArray hPress = hourly["pressure_msl"];
  JsonArray hRain  = hourly["rain"];
  JsonArray hSnow  = hourly["snowfall"];
  JsonArray hPrec  = hourly["precipitation"];
  JsonArray hVis   = hourly["visibility"];
  JsonArray hIsDay = hourly["is_day"];

  // Block r reads the hours 3r, 3r+1 and 3r+2.
  const size_t neededHours = (size_t)max_readings * 3;
  if (hTime.size() < neededHours) {
    Serial.printf("Only %u hourly values received, %u are required\n",
                  (unsigned)hTime.size(), (unsigned)neededHours);
    return false;
  }

  WxConditions[0].Dt          = current["time"].as<long>();
  WxConditions[0].Temperature = current["temperature_2m"].as<float>();
  WxConditions[0].Pressure    = current["pressure_msl"].as<float>();
  WxConditions[0].Humidity    = current["relative_humidity_2m"].as<float>();
  WxConditions[0].FeelsLike   = current["apparent_temperature"].as<float>();
  WxConditions[0].Cloudcover  = current["cloud_cover"].as<int>();
  WxConditions[0].Windspeed   = current["wind_speed_10m"].as<float>();
  WxConditions[0].Winddir     = current["wind_direction_10m"].as<float>();
  WxConditions[0].IsDay       = current["is_day"].as<int>() != 0;
  WxConditions[0].Visibility  = hVis[0].as<int>();
  // Read by setup() to decide the sleep interval.
  WxConditions[0].Rainfall    = current["rain"].as<float>();

  WxConditions[0].Sunrise = daily["sunrise"][0].as<long>();
  WxConditions[0].Sunset  = daily["sunset"][0].as<long>();
  WxConditions[0].High    = daily["temperature_2m_max"][0].as<float>();
  WxConditions[0].Low     = daily["temperature_2m_min"][0].as<float>();

  int weatherCode = current["weather_code"].as<int>();
  WxConditions[0].Icon        = weatherCodeToIcon(weatherCode);
  WxConditions[0].Description = weatherCodeToText(weatherCode);

  Serial.printf("Now: %.1f C, %.0f%% RH, %.1f hPa, code %d (%s)\n",
                WxConditions[0].Temperature, WxConditions[0].Humidity,
                WxConditions[0].Pressure, weatherCode, WxConditions[0].Description);
  Serial.printf("Sunrise %s / sunset %s\n",
                ConvertUnixTime(WxConditions[0].Sunrise).substring(0, 5).c_str(),
                ConvertUnixTime(WxConditions[0].Sunset).substring(0, 5).c_str());

  for (int r = 0; r < max_readings; r++) {
    const int i = r * 3;

    float t0 = hTemp[i].as<float>();
    float t1 = hTemp[i + 1].as<float>();
    float t2 = hTemp[i + 2].as<float>();

    WxForecast[r].Dt          = hTime[i].as<long>();
    WxForecast[r].Temperature = t0;
    WxForecast[r].Low         = min(t0, min(t1, t2));
    WxForecast[r].High        = max(t0, max(t1, t2));
    WxForecast[r].Pressure    = hPress[i].as<float>();
    WxForecast[r].Humidity    = hHum[i].as<float>();
    WxForecast[r].IsDay       = hIsDay[i].as<int>() != 0;

    // Highest code of the three hours = "worst" weather in the block.
    int code = hCode[i].as<int>();
    int c1   = hCode[i + 1].as<int>();
    int c2   = hCode[i + 2].as<int>();
    if (c1 > code) code = c1;
    if (c2 > code) code = c2;
    WxForecast[r].Icon        = weatherCodeToIcon(code);
    WxForecast[r].Description = weatherCodeToText(code);

    WxForecast[r].Rainfall      = hRain[i].as<float>() + hRain[i + 1].as<float>() + hRain[i + 2].as<float>();
    WxForecast[r].Snowfall      = hSnow[i].as<float>() + hSnow[i + 1].as<float>() + hSnow[i + 2].as<float>();
    WxForecast[r].Precipitation = hPrec[i].as<float>() + hPrec[i + 1].as<float>() + hPrec[i + 2].as<float>();
  }

  // Pressure trend: later minus now, so rising pressure comes out positive.
  float trend = WxForecast[2].Pressure - WxForecast[0].Pressure;
  WxConditions[0].PressureTrend = roundf(trend * 10.0f) / 10.0f;

  return true;
}

bool obtainWeatherDataOpenMeteo(WiFiClient &client) {
  client.stop();
  HTTPClient http;

  String uri = String("/v1/forecast?latitude=") + Latitude + "&longitude=" + Longitude
             + "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,"
               "wind_speed_10m,wind_direction_10m,weather_code,cloud_cover,pressure_msl,rain"
               "&hourly=temperature_2m,relative_humidity_2m,weather_code,pressure_msl,"
               "rain,snowfall,precipitation,visibility,is_day"
               "&daily=sunrise,sunset,temperature_2m_max,temperature_2m_min"
               "&models=best_match&timeformat=unixtime&temporal_resolution=hourly_1&past_hours=0"
               "&timezone=" OM_TIMEZONE
             + "&forecast_days="  + OM_FORECAST_DAYS
             + "&forecast_hours=" + OM_FORECAST_HOURS;

  Serial.println("Connecting: " + String(serverOpenMeteo) + uri);
  http.begin(client, serverOpenMeteo, 80, uri);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Connection failed, HTTP code %i %s\n",
                  httpCode, http.errorToString(httpCode).c_str());
    http.end();
    client.stop();
    return false;
  }

  // Keep only the fields that actually get drawn.
  JsonDocument filter;
  filter["current"] = true;
  filter["daily"]   = true;
  JsonObject h = filter["hourly"].to<JsonObject>();
  h["time"]                 = true;
  h["temperature_2m"]       = true;
  h["relative_humidity_2m"] = true;
  h["weather_code"]         = true;
  h["pressure_msl"]         = true;
  h["rain"]                 = true;
  h["snowfall"]             = true;
  h["precipitation"]        = true;
  h["visibility"]           = true;
  h["is_day"]               = true;

  // ArduinoJson 7: JsonDocument grows on demand, no fixed capacity needed.
  JsonDocument doc;

  // With a known Content-Length, parse straight from the stream - that saves a
  // full String copy of the body. For chunked transfers (getSize() == -1)
  // getStream() would hand out the raw chunk headers, so getString() has to
  // decode the transfer instead.
  DeserializationError error;
  if (http.getSize() > 0) {
    error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  } else {
    error = deserializeJson(doc, http.getString(), DeserializationOption::Filter(filter));
  }

  http.end();
  client.stop();

  if (error) {
    Serial.printf("deserializeJson() failed: %s\n", error.c_str());
    return false;
  }
  return DecodeWeatherOpenMeteo(doc);
}

// ============================================================================
//  Helpers
// ============================================================================

String ConvertUnixTime(int unix_time) {
  time_t tm = unix_time;
  struct tm *now_tm = localtime(&tm);
  char output[40];
  strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm);
  return output;
}

String TitleCase(String text) {
  if (text.length() > 0) {
    String temp_text = text.substring(0, 1);
    temp_text.toUpperCase();
    return temp_text + text.substring(1);
  }
  return text;
}

// Thousands separators, e.g. 24000 -> "24.000"
String formatNumber(unsigned int value) {
  String result = "";
  int count = 0;
  do {
    int digit = value % 10;
    value = value / 10;
    result = (char)('0' + digit) + result;
    count++;
    if (count % 3 == 0 && value > 0) result = '.' + result;
  } while (value > 0);
  return result;
}

int JulianDate(int d, int m, int y) {
  int mm, yy, k1, k2, k3, j;
  yy = y - (int)((12 - m) / 10);
  mm = m + 9;
  if (mm >= 12) mm = mm - 12;
  k1 = (int)(365.25 * (yy + 4712));
  k2 = (int)(30.6 * mm + 0.5);
  k3 = (int)((int)((yy / 100) + 49) * 0.75) - 38;
  j = k1 + k2 + d + 59;
  if (j > 2299160) j = j - k3;
  return j;
}

double NormalizedMoonPhase(int d, int m, int y) {
  int j = JulianDate(d, m, y);
  double Phase = (j + 4.867) / 29.53059;
  return (Phase - (int)Phase);
}

// ============================================================================
//  Display
// ============================================================================

void DisplayWeather() {
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  DisplayStatusSection(600, 20, wifi_signal);
  DisplayGeneralInfoSection();
  DisplayDisplayWindSection(137, 150, WxConditions[0].Winddir, WxConditions[0].Windspeed, 100);
  DisplayAstronomySection(5, 252);
  DisplayMainWeatherSection(320, 110);
  // Band between the pressure row above and the forecast times below.
  DisplayWeatherIcon(835, 82, 248);
  DisplayForecastSection(285, 220);
  DisplayGraphSection();
}

void DisplayGeneralInfoSection() {
  setFont(OpenSans10B);
  drawString(5, 2, City, LEFT);
  setFont(OpenSans8B);
  drawString(500, 2, Date_str + "  @   " + Time_str, LEFT);
}

// Vertical ink extent inside a framebuffer region. Returns false if it is blank.
bool InkExtent(int x0, int x1, int y0, int y1, int *top, int *bottom) {
  *top = -1; *bottom = -1;
  if (x0 < 0) x0 = 0;
  if (x1 >= EPD_WIDTH) x1 = EPD_WIDTH - 1;
  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      uint8_t byte = framebuffer[y * EPD_WIDTH / 2 + x / 2];
      uint8_t nib  = (x & 1) ? (byte >> 4) : (byte & 0x0F);
      if (nib < 0x0C) {                 // anything clearly darker than white
        if (*top < 0) *top = y;
        *bottom = y;
        break;
      }
    }
  }
  return (*top >= 0);
}

// Copies a rectangle of the framebuffer aside, so a trial drawing can be taken
// back afterwards. x is rounded outwards to whole bytes - two pixels share one
// byte at 4 bpp - which is why the caller may get back slightly more than it
// asked for. Returns false if the rectangle does not fit the buffer; the caller
// then has to manage without.
static int patchX, patchY, patchW, patchH;

bool PatchSave(int x0, int y0, int x1, int y1) {
  if (!iconPatch) return false;
  x0 = (x0 < 0) ? 0 : (x0 & ~1);
  y0 = (y0 < 0) ? 0 : y0;
  x1 = (x1 >= EPD_WIDTH)  ? EPD_WIDTH  - 1 : (x1 | 1);
  y1 = (y1 >= EPD_HEIGHT) ? EPD_HEIGHT - 1 : y1;
  if (x1 < x0 || y1 < y0) return false;

  patchX = x0; patchY = y0;
  patchW = x1 - x0 + 1; patchH = y1 - y0 + 1;
  if ((long)(patchW / 2) * patchH > ICON_PATCH_BYTES) return false;

  for (int r = 0; r < patchH; r++)
    memcpy(iconPatch + (long)r * (patchW / 2),
           framebuffer + (long)(patchY + r) * (EPD_WIDTH / 2) + patchX / 2,
           patchW / 2);
  return true;
}

void PatchRestore() {
  for (int r = 0; r < patchH; r++)
    memcpy(framebuffer + (long)(patchY + r) * (EPD_WIDTH / 2) + patchX / 2,
           iconPatch + (long)r * (patchW / 2),
           patchW / 2);
}

// Centres an icon vertically in a band by what it actually draws.
//
// The icons differ far too much in height for one fixed offset to suit them
// all - a plain cloud covers 120 px, one with rain streaks 153. Rather than
// carry a table of extents that has to be kept in step with the drawing code,
// the icon is drawn once, measured, taken back and drawn again where it
// belongs.
//
// Taking it back means restoring the pixels, not painting a white rectangle
// over the column it was measured in: the sun's outermost ray and the moon's
// companion stars reach up to nine pixels further left than that, into the
// neighbouring tile. A rectangle left those fragments standing, and the second
// drawing then added its own next to them - a hook on the sun's left ray, and
// five or six stars where there should be three.
//
// The moon indicator is drawn afterwards, and anchored to the band rather than
// to the symbol. It has to stay out of the measurement - it sits above and to
// the right, so counting it would drop the symbol by ~13 px on precisely those
// hours where the moon is up, and the same icon would end up at two different
// heights depending on the moon. Anchoring it to the band keeps it clear of
// the time above and puts it in the same corner in every tile.
void DrawIconCentred(int cx, int x0, int x1, int bandTop, int bandBottom,
                     const char *icon, bool IconSize, bool isDay, long timestamp) {
  const int probe  = (bandTop + bandBottom) / 2;
  const int margin = (IconSize == LargeIcon) ? 40 : 24;   // measured spill is 9 px

  // Without somewhere to put the patch there is no way to undo the trial
  // drawing, so draw the icon once and leave it uncentred rather than leave
  // fragments behind.
  if (!PatchSave(x0 - margin, bandTop - margin, x1 + margin, bandBottom + margin)) {
    DisplayConditionsSection(cx, probe, icon, IconSize, isDay, timestamp);
    if (isMoonUp(timestamp)) addMoonIndicator(cx, probe, IconSize);
    return;
  }

  DisplayConditionsSection(cx, probe, icon, IconSize, isDay, timestamp);

  int top, bottom, shift = 0;
  if (InkExtent(x0, x1, bandTop, bandBottom, &top, &bottom))
    shift = probe - (top + bottom) / 2;

  PatchRestore();
  DisplayConditionsSection(cx, probe + shift, icon, IconSize, isDay, timestamp);

  // Last, so no cloud can paint over it, and at the band centre rather than at
  // the symbol, so it marks "moon above the horizon" from the same corner in
  // every tile - independently of the weather and of whether it is day.
  if (isMoonUp(timestamp)) addMoonIndicator(cx, probe, IconSize);
}

void DisplayWeatherIcon(int cx, int bandTop, int bandBottom) {
  DrawIconCentred(cx, cx - 100, cx + 110, bandTop, bandBottom,
                  WxConditions[0].Icon, LargeIcon, WxConditions[0].IsDay, WxConditions[0].Dt);
}

void DisplayMainWeatherSection(int x, int y) {
  setFont(OpenSans8B);
  // All three blocks share the same left text edge at x - 30. The description
  // used to sit 5 px further right, which read as a stray leading space.
  DisplayTempHumiPressSection(x, y - 60);
  DisplayForecastTextSection(x - 30, y + 45);
  DisplayVisiCCoverSection(x - 10, y + 95);
}

void DisplayTempHumiPressSection(int x, int y) {
  setFont(OpenSans18B);
  drawString(x - 30, y, String(WxConditions[0].Temperature, 1) + "°C " +
                        String(WxConditions[0].Humidity, 0) + TXT_HUMIDITY_SHORT, LEFT);
  setFont(OpenSans12B);
  int Yoffset = 42;

  DrawPressureAndTrend(x + 250, y, WxConditions[0].Pressure);

  // apparent_temperature comes straight from Open-Meteo, so there is no reason
  // to hide it when the wind happens to be zero - which is what used to happen.
  drawString(x - 30, y + Yoffset, String(WxConditions[0].FeelsLike, 1) + " °C " + TXT_FEELSLIKE, LEFT);
  Yoffset += 30;
  drawString(x - 30, y + Yoffset, String(WxConditions[0].High, 0) + "°C max | " +
                                  String(WxConditions[0].Low, 0) + "°C min", LEFT);
}

// Pressure reading plus a 6-hour mini graph (now, +3 h, +6 h).
void DrawPressureAndTrend(int x, int y, float pressure) {
  String label = String(TXT_PRESSURE) + String(pressure, 0) + "hPa";
  drawString(x, y, label, LEFT);

  const int trendWidth  = 24;
  const int trendHeight = 16;
  // Measured rather than a fixed offset: "Druck: " is short, "Pressure: " and
  // "Pression : " are not, and a fixed 200 px put the trend on top of the unit.
  int gx = x + stringWidth(label) + 12;
  int gy = y;

  float p0 = WxForecast[0].Pressure;
  float p1 = WxForecast[1].Pressure;
  float p2 = WxForecast[2].Pressure;

  float pMin = min(p0, min(p1, p2));
  float pMax = max(p0, max(p1, p2));

  // Widen the scale artificially when pressure is very stable (also guards /0).
  if (pMax - pMin < 2.0f) { pMax += 1.0f; pMin -= 1.0f; }

  int y0 = gy + trendHeight - (int)((p0 - pMin) / (pMax - pMin) * trendHeight);
  int y1 = gy + trendHeight - (int)((p1 - pMin) / (pMax - pMin) * trendHeight);
  int y2 = gy + trendHeight - (int)((p2 - pMin) / (pMax - pMin) * trendHeight);

  drawLine(gx, y0, gx + trendWidth / 2, y1, Black);
  drawLine(gx, y0 - 1, gx + trendWidth / 2, y1 - 1, Black);
  drawLine(gx + trendWidth / 2, y1, gx + trendWidth, y2, Black);
  drawLine(gx + trendWidth / 2, y1 - 1, gx + trendWidth, y2 - 1, Black);

  fillCircle(gx, y0, 2, Black);
  fillCircle(gx + trendWidth, y2, 2, Black);
}

void DisplayForecastTextSection(int x, int y) {
  const int lineWidth = 34;
  setFont(OpenSans12B);

  // Amount for the next three hours. While it is snowing that is centimetres of
  // fresh snow, not the couple of millimetres of meltwater the rain field would
  // report - which is what this used to print during a snowfall.
  String text = WxConditions[0].Description;
  if (WxForecast[0].Snowfall > 0)      text += " (" + String(WxForecast[0].Snowfall, 1) + " cm)";
  else if (WxForecast[0].Rainfall > 0) text += " (" + String(WxForecast[0].Rainfall, 1) + " mm)";

  // Break at the last space before lineWidth *characters*. UTF-8 continuation
  // bytes (umlauts) do not count, otherwise the line breaks too early.
  int lastSpace = -1, chars = 0, breakAt = -1;
  for (int i = 0; i < (int)text.length(); i++) {
    char c = text.charAt(i);
    if ((c & 0xC0) == 0x80) continue;
    if (c == ' ') lastSpace = i;
    if (++chars > lineWidth && lastSpace >= 0) { breakAt = lastSpace; break; }
  }

  String line1 = (breakAt < 0) ? text : text.substring(0, breakAt);
  String line2 = (breakAt < 0) ? String("") : text.substring(breakAt + 1);

  drawString(x, y + 5, TitleCase(line1), LEFT);
  if (line2.length()) drawString(x, y + 30, line2, LEFT);
}

void DisplayVisiCCoverSection(int x, int y) {
  setFont(OpenSans12B);
  Visibility(x + 5, y, formatNumber(WxConditions[0].Visibility) + " m");
  CloudCover(x + 175, y, WxConditions[0].Cloudcover);
}

void DisplayForecastWeather(int x, int y, int index, int fwidth) {
  x = x + fwidth * index;
  setFont(OpenSans10B);
  drawString(x + fwidth / 2, y + 30, ConvertUnixTime(WxForecast[index].Dt).substring(0, 5), CENTER);

  // Centred in the band between the time above and the temperatures below.
  // Clouds therefore sit a little differently from icon to icon - a rain icon
  // carries its streaks below the cloud - but each symbol as a whole is
  // centred, and the same symbol always lands in the same place.
  DrawIconCentred(x + fwidth / 2 - 5, x + 1, x + fwidth - 1, y + 52, y + 128,
                  WxForecast[index].Icon, SmallIcon, WxForecast[index].IsDay, WxForecast[index].Dt);

  // addrain() and addsnow() draw their streaks as text and leave OpenSans8B
  // behind, so the temperatures below need the font set again.
  setFont(OpenSans10B);
  drawString(x + fwidth / 2, y + 130, String(WxForecast[index].High, 0) + "°/" +
                                      String(WxForecast[index].Low, 0) + "°", CENTER);
}

void DisplayForecastSection(int x, int y) {
  for (int f = 0; f < 8; f++) DisplayForecastWeather(x, y, f, 82);
}

void DisplayGraphSection() {
  // The precipitation bars are stacked: liquid at the bottom, snow on top.
  // Over three days both can fall, so neither is hidden.
  //
  // Open-Meteo folds snow into `precipitation` at 7 cm of fresh snow per 10 mm
  // of meltwater - verified against live data: a block of 0.30 mm rain plus
  // 0.42 cm snow came back as 0.90 mm, and 0.42 / 0.7 is exactly the 0.60 mm
  // difference. That conversion always gives the liquid share; how the snow
  // share is scaled is up to SNOW_AS_FALLEN_DEPTH.
  float snowSum = 0;
  for (int r = 0; r < max_readings; r++) {
    snowSum += WxForecast[r].Snowfall;
    pressure_readings[r]    = WxForecast[r].Pressure;
    temperature_readings[r] = WxForecast[r].Temperature;
    humidity_readings[r]    = WxForecast[r].Humidity;

    float liquid = WxForecast[r].Precipitation - WxForecast[r].Snowfall / 0.7f;
    if (liquid < 0) liquid = 0;
#if SNOW_AS_FALLEN_DEPTH
    float snowPart = WxForecast[r].Snowfall * 10.0f;   // cm of snow -> mm of snow
#else
    float snowPart = WxForecast[r].Snowfall / 0.7f;    // cm of snow -> mm of meltwater
#endif
    liquid_readings[r]        = liquid;
    precipitation_readings[r] = liquid + snowPart;
  }

  int gwidth = 175, gheight = 100;
  int gx  = (SCREEN_WIDTH - gwidth * 4) / 5 + 8;
  int gy  = (SCREEN_HEIGHT - gheight - 30);
  int gap = gwidth + gx;

  DrawGraph(gx + 0 * gap,     gy, gwidth, gheight,  10,   30, TXT_TEMPERATURE_C,     temperature_readings,   NULL,            max_readings, autoscale_on,  barchart_off);
  DrawGraph(gx + 1 * gap,     gy, gwidth, gheight,   0,  100, TXT_HUMIDITY_PERCENT,  humidity_readings,      NULL,            max_readings, autoscale_off, barchart_off);
  // Only call it rain + snow when snow is actually in the forecast - in August
  // the title would otherwise promise snow that is nowhere in the data. With
  // the water-equivalent scaling the bar totals the precipitation either way,
  // so that mode keeps its usual title.
#if SNOW_AS_FALLEN_DEPTH
  const char *precipTitle = (snowSum > 0) ? TEXT_RAIN_SNOW_MM : TEXT_PRECIPITATION_MM;
#else
  const char *precipTitle = TEXT_PRECIPITATION_MM;
#endif
  DrawGraph(gx + 2 * gap + 5, gy, gwidth, gheight,   0,   30, precipTitle,           precipitation_readings, liquid_readings, max_readings, autoscale_on,  barchart_on);
  DrawGraph(gx + 3 * gap,     gy, gwidth, gheight, 900, 1050, TXT_PRESSURE_HPA,      pressure_readings,      NULL,            max_readings, autoscale_on,  barchart_off);
}

void DisplayAstronomySection(int x, int y) {
  setFont(OpenSans10B);
  time_t now = time(NULL);
  struct tm *now_utc = gmtime(&now);
  drawString(x + 5, y + 102, MoonPhase(now_utc->tm_mday, now_utc->tm_mon + 1,
                                       now_utc->tm_year + 1900, Hemisphere), LEFT);
  DrawMoonImage(x + 10, y + 23);
  DrawMoon(x - 28, y - 15, 75, now_utc->tm_mday, now_utc->tm_mon + 1,
           now_utc->tm_year + 1900, Hemisphere);
  drawString(x + 115, y + 40, ConvertUnixTime(WxConditions[0].Sunrise).substring(0, 5), LEFT);
  drawString(x + 115, y + 80, ConvertUnixTime(WxConditions[0].Sunset).substring(0, 5), LEFT);
  DrawSunriseImage(x + 180, y + 20);
  DrawSunsetImage(x + 180, y + 60);
}

void DrawMoon(int x, int y, int diameter, int dd, int mm, int yy, const char *hemisphere) {
  double Phase = NormalizedMoonPhase(dd, mm, yy);
  if (strcasecmp(hemisphere, "south") == 0) Phase = 1 - Phase;
  fillCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, DarkGrey);
  const int number_of_lines = 90;
  for (double Ypos = 0; Ypos <= number_of_lines / 2; Ypos++) {
    double Xpos = sqrt(number_of_lines / 2 * number_of_lines / 2 - Ypos * Ypos);
    double Rpos = 2 * Xpos;
    double Xpos1, Xpos2;
    if (Phase < 0.5) {
      Xpos1 = -Xpos;
      Xpos2 = Rpos - 2 * Phase * Rpos - Xpos;
    } else {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2 * Phase * Rpos + Rpos;
    }
    double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW1y = (number_of_lines - Ypos)  / number_of_lines * diameter + y;
    double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW2y = (number_of_lines - Ypos)  / number_of_lines * diameter + y;
    double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW3y = (Ypos + number_of_lines)  / number_of_lines * diameter + y;
    double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW4y = (Ypos + number_of_lines)  / number_of_lines * diameter + y;
    drawLine(pW1x, pW1y, pW2x, pW2y, White);
    drawLine(pW3x, pW3y, pW4x, pW4y, White);
  }
  drawCircle(x + diameter - 1, y + diameter, diameter / 2, Black);
}

const char *MoonPhase(int d, int m, int y, const char *hemisphere) {
  int c, e;
  double jd;
  int b;
  if (m < 3) { y--; m += 12; }
  ++m;
  c   = 365.25 * y;
  e   = 30.6  * m;
  jd  = c + e + d - 694039.09;
  jd /= 29.53059;
  b   = jd;
  jd -= b;
  b   = jd * 8 + 0.5;
  b   = b & 7;
  if (strcasecmp(hemisphere, "south") == 0) b = 7 - b;
  switch (b) {
    case 0:  return TXT_MOON_NEW;
    case 1:  return TXT_MOON_WAXING_CRESCENT;
    case 2:  return TXT_MOON_FIRST_QUARTER;
    case 3:  return TXT_MOON_WAXING_GIBBOUS;
    case 4:  return TXT_MOON_FULL;
    case 5:  return TXT_MOON_WANING_GIBBOUS;
    case 6:  return TXT_MOON_THIRD_QUARTER;
    default: return TXT_MOON_WANING_CRESCENT;
  }
}

void DisplayDisplayWindSection(int x, int y, float angle, float windspeed, int Cradius) {
  arrow(x, y, Cradius - 22, angle, 18, 33);
  setFont(OpenSans8B);
  int dxo, dyo, dxi, dyi;
  drawCircle(x, y, Cradius, Black);
  drawCircle(x, y, Cradius + 1, Black);
  drawCircle(x, y, Cradius * 0.7, Black);
  for (float a = 0; a < 360; a = a + 22.5) {
    dxo = Cradius * cos((a - 90) * PI / 180);
    dyo = Cradius * sin((a - 90) * PI / 180);
    if (a == 45)  drawString(dxo + x + 15, dyo + y - 18, TXT_NE, CENTER);
    if (a == 135) drawString(dxo + x + 20, dyo + y - 2,  TXT_SE, CENTER);
    if (a == 225) drawString(dxo + x - 20, dyo + y - 2,  TXT_SW, CENTER);
    if (a == 315) drawString(dxo + x - 15, dyo + y - 18, TXT_NW, CENTER);
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    drawLine(dxo + x, dyo + y, dxi + x, dyi + y, Black);
    dxo = dxo * 0.7;
    dyo = dyo * 0.7;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    drawLine(dxo + x, dyo + y, dxi + x, dyi + y, Black);
  }
  drawString(x, y - Cradius - 20,     TXT_N, CENTER);
  drawString(x, y + Cradius + 10,     TXT_S, CENTER);
  drawString(x - Cradius - 15, y - 5, TXT_W, CENTER);
  drawString(x + Cradius + 10, y - 5, TXT_E, CENTER);
  drawString(x + 3, y + 50, String(angle, 0) + "°", CENTER);
  setFont(OpenSans12B);
  drawString(x, y - 50, WindDegToOrdinalDirection(angle), CENTER);
  setFont(OpenSans24B);
  drawString(x + 3, y - 18, String(windspeed, 1), CENTER);
  setFont(OpenSans12B);
  drawString(x, y + 25, "km/h", CENTER);
}

const char *WindDegToOrdinalDirection(float winddirection) {
  if (winddirection >= 348.75 || winddirection < 11.25)  return TXT_N;
  if (winddirection <  33.75)  return TXT_NNE;
  if (winddirection <  56.25)  return TXT_NE;
  if (winddirection <  78.75)  return TXT_ENE;
  if (winddirection < 101.25)  return TXT_E;
  if (winddirection < 123.75)  return TXT_ESE;
  if (winddirection < 146.25)  return TXT_SE;
  if (winddirection < 168.75)  return TXT_SSE;
  if (winddirection < 191.25)  return TXT_S;
  if (winddirection < 213.75)  return TXT_SSW;
  if (winddirection < 236.25)  return TXT_SW;
  if (winddirection < 258.75)  return TXT_WSW;
  if (winddirection < 281.25)  return TXT_W;
  if (winddirection < 303.75)  return TXT_WNW;
  if (winddirection < 326.25)  return TXT_NW;
  return TXT_NNW;
}

void DisplayStatusSection(int x, int y, int rssi) {
  setFont(OpenSans8B);
  DrawRSSI(x + 305, y + 15, rssi);
  DrawBattery(x + 150, y);
}

void DrawRSSI(int x, int y, int rssi) {
  int WIFIsignal = 0;
  int xpos = 1;
  for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20) {
    if (_rssi <= -20)  WIFIsignal = 30;
    if (_rssi <= -40)  WIFIsignal = 24;
    if (_rssi <= -60)  WIFIsignal = 18;
    if (_rssi <= -80)  WIFIsignal = 12;
    if (_rssi <= -100) WIFIsignal = 6;
    if (rssi != 0) fillRect(x + xpos * 8, y - WIFIsignal, 6, WIFIsignal, Black);
    else           drawRect(x + xpos * 8, y - WIFIsignal, 6, WIFIsignal, Black);
    xpos++;
  }
  if (rssi == 0) drawString(x + 28, y - 18, "x", LEFT);
}

// Battery reading via analogReadMilliVolts(): the calibration lives inside the
// core function and works on ESP32 as well as ESP32-S3. The esp_adc_cal API
// used before no longer exists in arduino-esp32 3.x.
void DrawBattery(int x, int y) {
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);

  uint32_t mv_sum = 0;
  for (int i = 0; i < 8; i++) mv_sum += analogReadMilliVolts(BATTERY_ADC_PIN);
  float voltage = (mv_sum / 8.0f) * BATTERY_DIVIDER / 1000.0f;

  if (voltage <= 1.0f) {   // no battery connected
    Serial.println("No battery voltage measurable (USB powered?)");
    return;
  }

  int percentage;
  if      (voltage >= 4.20f) percentage = 100;
  else if (voltage <= 3.20f) percentage = 0;
  else percentage = (int)(2836.9625f * powf(voltage, 4) - 43987.4889f * powf(voltage, 3)
                        + 255233.8134f * powf(voltage, 2) - 656689.7123f * voltage + 632041.7303f);
  percentage = constrain(percentage, 0, 100);

  Serial.printf("Battery: %.2f V -> %d %%\n", voltage, percentage);

  drawRect(x + 25, y - 14, 40, 15, Black);
  fillRect(x + 65, y - 10, 4, 7, Black);
  fillRect(x + 27, y - 12, 36 * percentage / 100.0, 11, Black);
  drawString(x + 85, y - 14, String(percentage) + "%  " + String(voltage, 1) + "v", LEFT);
}

// ============================================================================
//  Weather icons
// ============================================================================

void DisplayConditionsSection(int x, int y, const char *IconName, bool IconSize, bool isDay,
                              long timestamp) {
  if (!IconName) IconName = "nA";
  bool night  = !isDay;
  bool moonUp = isMoonUp(timestamp);

  if      (!strcmp(IconName, "sun"))             ClearSky(x, y, IconSize, night, moonUp);
  else if (!strcmp(IconName, "fewclouds"))       FewClouds(x, y, IconSize, night, moonUp);
  else if (!strcmp(IconName, "scatteredClouds")) ScatteredClouds(x, y, IconSize, night, moonUp);
  else if (!strcmp(IconName, "brokenClouds"))    BrokenClouds(x, y, IconSize, night, moonUp);
  else if (!strcmp(IconName, "chanceRain"))      ChanceRain(x, y, IconSize, night, moonUp);
  else if (!strcmp(IconName, "rain"))            Rain(x, y, IconSize);
  else if (!strcmp(IconName, "thunderstorm"))    Thunderstorms(x, y, IconSize);
  else if (!strcmp(IconName, "snow"))            Snow(x, y, IconSize);
  else if (!strcmp(IconName, "mist"))            Mist(x, y, IconSize, night, moonUp);
  else                                           Nodata(x, y, IconSize);

}

void addcloud(int x, int y, int scale, int linesize) {
  // The cloud is drawn as filled shapes with smaller white ones punched back
  // out of them. Callers scale the cloud but pass a fixed outline width, and
  // the second cloud of ScatteredClouds comes out at radius 4 in a forecast
  // tile - the white circles then get a negative radius and the cloud renders
  // as a solid lump. Keep at least two pixels of hollow; this leaves every
  // cloud of radius 7 or more exactly as it was.
  if (linesize > scale - 2) linesize = scale - 2;
  if (linesize < 1)         linesize = 1;

  fillCircle(x - scale * 3, y, scale, Black);
  fillCircle(x + scale * 3, y, scale, Black);
  fillCircle(x - scale, y - scale, scale * 1.4, Black);
  fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, Black);
  fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, Black);
  fillCircle(x - scale * 3, y, scale - linesize, White);
  fillCircle(x + scale * 3, y, scale - linesize, White);
  fillCircle(x - scale, y - scale, scale * 1.4 - linesize, White);
  fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, White);
  fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, White);
}

void addrain(int x, int y, bool IconSize) {
  if (IconSize == SmallIcon) {
    setFont(OpenSans8B);
    drawString(x - 25, y + 12, "///////", LEFT);
  } else {
    setFont(OpenSans18B);
    drawString(x - 60, y + 25, "///////", LEFT);
  }
}

void addsnow(int x, int y, bool IconSize) {
  if (IconSize == SmallIcon) {
    setFont(OpenSans8B);
    drawString(x - 25, y + 15, "* * * *", LEFT);
  } else {
    setFont(OpenSans18B);
    drawString(x - 60, y + 30, "* * * *", LEFT);
  }
}

void addtstorm(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 1; i < 5; i++) {
    drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1, Black);
    drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, Black);
    drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, Black);
  }
}

void addsun(int x, int y, int scale) {
  // Same reasoning as addcloud: ScatteredClouds draws its sun at 0.7 of an
  // already small scale, and a fixed 5 px outline left a two-pixel hole in a
  // seven-pixel disc, with rays as thick as the disc itself. Half the radius
  // is what the other icons already come out at, so nothing else changes.
  int linesize = 5;
  if (linesize > scale / 2) linesize = scale / 2;
  if (linesize < 1)         linesize = 1;
  fillRect(x - scale * 2, y, scale * 4, linesize, Black);
  fillRect(x, y - scale * 2, linesize, scale * 4, Black);
  DrawAngledLine(x + scale * 1.4, y + scale * 1.4, (x - scale * 1.4), (y - scale * 1.4), linesize * 1.5, Black);
  DrawAngledLine(x - scale * 1.4, y + scale * 1.4, (x + scale * 1.4), (y - scale * 1.4), linesize * 1.5, Black);
  fillCircle(x, y, scale * 1.3, White);
  fillCircle(x, y, scale, Black);
  fillCircle(x, y, scale - linesize, White);
}

// Extra crescent offset into the corner of the icon: marks that the moon is
// currently above the horizon - regardless of whether it is daytime.
// Geometry taken unchanged from the original addmoon().
void addMoonIndicator(int x, int y, bool IconSize) {
  int xOffset = 65;
  int yOffset = 12;
  if (IconSize == LargeIcon) {
    xOffset = 105;   // 130 pushed the indicator past the right screen edge
    yOffset = -40;
  }
  fillCircle(x - 28 + xOffset, y - 37 + yOffset, uint16_t(Small * 1.0), Black);
  fillCircle(x - 16 + xOffset, y - 37 + yOffset, uint16_t(Small * 1.6), White);
}

// Four-pointed sparkle. Stays legible down to a couple of pixels, unlike a
// filled disc which just turns into a grey dot.
void addstar(int cx, int cy, int r) {
  if (r < 2) r = 2;
  int w = r / 3 + 1;
  fillTriangle(cx, cy - r, cx - w, cy, cx + w, cy, Black);
  fillTriangle(cx, cy + r, cx - w, cy, cx + w, cy, Black);
  fillTriangle(cx - r, cy, cx, cy - w, cx, cy + w, Black);
  fillTriangle(cx + r, cy, cx, cy - w, cx, cy + w, Black);
}

// Three sparkles, kept to the left of the moon and deliberately low: further
// up they would collide with the time label above each forecast tile.
void addstars(int cx, int cy, int scale) {
  addstar(cx - scale * 1.25, cy - scale * 0.70, scale * 0.24);
  addstar(cx - scale * 0.25, cy - scale * 1.05, scale * 0.18);
  addstar(cx - scale * 1.45, cy + scale * 0.50, scale * 0.15);
}

// Asterisms drawn on a clear moonless night, where the icon would otherwise be
// empty. Coordinates are normalised around the centre, x to the right, y down,
// derived from the stars' real right ascension and declination. mag is the
// star radius as a fraction of the field size, ranked by apparent brightness.
// Ursa Major, northern hemisphere.
static const StarPos BIG_DIPPER[7] = {
  { -1.00f, -0.30f, 0.17f },   // Alkaid
  { -0.60f, -0.14f, 0.19f },   // Mizar
  { -0.24f,  0.00f, 0.19f },   // Alioth
  {  0.04f,  0.14f, 0.15f },   // Megrez
  {  0.14f,  0.56f, 0.19f },   // Phecda
  {  0.66f,  0.62f, 0.21f },   // Merak
  {  0.72f,  0.16f, 0.23f },   // Dubhe
};
static const StarLink BIG_DIPPER_LINKS[7] = {
  {0,1},{1,2},{2,3},   // handle
  {3,4},{4,5},{5,6},{6,3},   // bowl
};

// Crux, southern hemisphere. The long axis runs from Gacrux down to Acrux and
// points towards the south celestial pole; Mimosa and Imai form the crossbar.
// Epsilon Crucis is the faint fifth star inside the cross and stays unlinked.
// Coordinates are scaled up by a quarter against the true angular spread, so
// the cross covers about as much of the icon as the Big Dipper does - Crux is
// a far more compact constellation and would otherwise look undersized.
static const StarPos SOUTHERN_CROSS[5] = {
  {  0.11f,  0.70f, 0.24f },   // Acrux    - brightest, south end
  { -0.51f, -0.08f, 0.22f },   // Mimosa   - east arm
  { -0.03f, -0.66f, 0.21f },   // Gacrux   - north end
  {  0.43f, -0.29f, 0.17f },   // Imai     - west arm
  {  0.26f,  0.09f, 0.13f },   // Epsilon  - faint, inside the cross
};
static const StarLink SOUTHERN_CROSS_LINKS[2] = {
  {2,0},   // Gacrux - Acrux, the long axis
  {1,3},   // Mimosa - Imai, the crossbar
};

void drawAsterism(int cx, int cy, int f, const StarPos *stars, int nStars,
                  const StarLink *links, int nLinks) {
  // Joining lines only where they would not merge into a blob. Grey, so the
  // stars themselves stay the brightest thing in the icon.
  if (f >= 13) {
    for (int i = 0; i < nLinks; i++) {
      const StarPos *a = &stars[links[i].a];
      const StarPos *b = &stars[links[i].b];
      drawLine(cx + a->x * f, cy + a->y * f, cx + b->x * f, cy + b->y * f, Grey);
    }
  }
  for (int i = 0; i < nStars; i++)
    addstar(cx + stars[i].x * f, cy + stars[i].y * f, stars[i].mag * f);
}

void addconstellation(int cx, int cy, int scale) {
  // Neither cloud nor moon is in the way, so the field can use the whole icon.
  // Small icons need the larger multiplier: their `scale` is a third of the
  // large one, but the tile they sit in is only half the size, not a third.
  int f = scale * (scale >= 24 ? 1.55 : 2.20);

  // The Big Dipper never rises across most of the southern hemisphere, and
  // Crux never rises across most of the northern one - so follow Hemisphere.
  if (strcasecmp(Hemisphere, "south") == 0)
    drawAsterism(cx, cy, f, SOUTHERN_CROSS, 5, SOUTHERN_CROSS_LINKS, 2);
  else
    drawAsterism(cx, cy, f, BIG_DIPPER, 7, BIG_DIPPER_LINKS, 7);
}

// Solid crescent. Chosen over a full disc or the photographic texture because
// it is the only shape that still reads at the 20 px a forecast tile allows.
void addmoon(int cx, int cy, int scale) {
  int r = scale;
  fillCircle(cx, cy, r, Black);
  fillCircle(cx + r * 0.55, cy - r * 0.42, r * 0.95, White);
}

// What replaces the sun after sunset:
//   day                  -> sun
//   night, moon up       -> crescent plus stars
//   night, moon below    -> stars only, so a clear moonless night still reads
//                           as night rather than as missing data
// When a cloud is drawn over the icon afterwards the moon is pushed further
// out, otherwise the cloud swallows it.
void addsunormoon(int x, int y, int scale, bool night, bool moonUp, SkyKind sky, bool IconSize) {
  bool behindCloud = (sky != SKY_CLEAR);
  // Small icons draw their cloud at roughly 0.75 scale, but every glyph offset
  // is a multiple of `scale` itself. Measured against the cloud, the glyph
  // therefore ends up about a third further out on small icons than on large
  // ones. Pull it back so both read the same.
  int pull = (IconSize == LargeIcon) ? 0 : scale * 0.65;

  if (!night) {
    // Behind a cloud the sun keeps its distance: its rays reach two radii and
    // must not cross the cloud's lower edge, or it reads as being in front.
    if (behindCloud) addsun(x - scale * 0.60 + pull, y - scale * 0.65, scale);
    else             addsun(x, y, scale);
    return;
  }
  // The moon has no rays, so it can sit closer in and be drawn larger. It is
  // placed lower than the sun on purpose: higher up it would run into the time
  // label above each forecast tile.
  int mx = behindCloud ? x - scale * 0.55 + pull : x;
  int my = behindCloud ? y + scale * 0.45 : y;
  int ms = behindCloud ? scale * 1.60 : scale;
  // Wherever a sun would stand during the day, something stands at night: the
  // moon if it is up, stars if it is not. Icons that never draw a sun - rain,
  // snow, thunderstorm - get nothing either way.
  if (moonUp) {
    addmoon(mx, my, ms);
    addstars(mx, my, ms);
  } else if (sky == SKY_CLEAR) {
    // Clear sky and no moon: a constellation is exactly what you would see.
    addconstellation(mx, my, ms);
  } else {
    addstars(mx, my, ms);
  }
}

void addfog(int x, int y, int scale, int linesize) {
  fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, Black);
  fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, Black);
  fillRect(x - scale * 3, y + scale * 2.5, scale * 6, linesize, Black);
}

void DrawAngledLine(int x, int y, int x1, int y1, int size, int color) {
  int dx = (size / 2.0) * (x - x1) / sqrt(sq(x - x1) + sq(y - y1));
  int dy = (size / 2.0) * (y - y1) / sqrt(sq(x - x1) + sq(y - y1));
  fillTriangle(x + dx, y - dy, x - dx,  y + dy,  x1 + dx, y1 - dy, color);
  fillTriangle(x - dx, y + dy, x1 - dx, y1 + dy, x1 + dx, y1 - dy, color);
}

void ClearSky(int x, int y, bool IconSize, bool night, bool moonUp) {
  int scale = (IconSize == LargeIcon) ? Large : Small;
  y += (IconSize ? 0 : 10);
  addsunormoon(x, y, scale * (IconSize ? 1.7 : 1.2), night, moonUp, SKY_CLEAR, IconSize);
}

void BrokenClouds(int x, int y, bool IconSize, bool night, bool moonUp) {
  int scale = (IconSize == LargeIcon) ? Large : Small;
  int linesize = 5;
  y += 15;
  addsunormoon(x - scale * 1.8, y - scale * 1.8, scale, night, moonUp, SKY_CLOUDY, IconSize);
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
}

void FewClouds(int x, int y, bool IconSize, bool night, bool moonUp) {
  int scale = (IconSize == LargeIcon) ? Large : Small;
  int linesize = 5;
  y += 15;
  // Sun/moon first, cloud on top - anything drawn after the cloud would sit in
  // front of it. FewClouds and ScatteredClouds had this the wrong way round.
  addsunormoon((x + (IconSize ? 10 : 0)) - scale * 1.8, y - scale * 1.6, scale, night, moonUp, SKY_CLOUDY, IconSize);
  addcloud(x + (IconSize ? 10 : 0), y, scale * (IconSize ? 0.9 : 0.8), linesize);
}

void ScatteredClouds(int x, int y, bool IconSize, bool night, bool moonUp) {
  int scale = (IconSize == LargeIcon) ? Large : Small;
  int linesize = 5;
  y += 15;
  // Sun/moon first, both clouds on top - see FewClouds.
  addsunormoon(x - scale * 2.2, y - scale * 1.8, scale * 0.7, night, moonUp, SKY_CLOUDY, IconSize);
  // The small second cloud used to be placed at y * 0.75, a multiplication
  // rather than an offset - its distance from the main cloud then depended on
  // where the icon happened to sit on the panel: 52 px apart on the large icon,
  // 22 px in a forecast tile. Now a plain offset, so it scales with the icon.
  // Up and to the right: the left is taken by the sun or moon, and directly
  // above the main cloud the two merge into a single lump.
  addcloud(x + scale * 2.00, y - scale * 1.90, scale / 2.2, linesize);
  addcloud(x, y, scale * 0.9, linesize);
}

void Rain(int x, int y, bool IconSize) {
  int scale = (IconSize == LargeIcon) ? Large : Small;
  int linesize = 5;
  y += 15;
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addrain(x, y, IconSize);
}

void ChanceRain(int x, int y, bool IconSize, bool night, bool moonUp) {
  int scale = (IconSize == LargeIcon) ? Large : Small;
  int linesize = 5;
  y += 15;
  addsunormoon(x - scale * 1.8, y - scale * 1.8, scale, night, moonUp, SKY_CLOUDY, IconSize);
  addcloud(x, y, scale * (IconSize ? 1 : 0.65), linesize);
  addrain(x, y, IconSize);
}

void Thunderstorms(int x, int y, bool IconSize) {
  int scale = (IconSize == LargeIcon) ? Large : Small;
  int linesize = 5;
  // The cloud shrinks to 0.75 on a small icon but the lightning used to keep
  // full scale, so it hung far lower than any other icon's precipitation. Both
  // scale together now, and y+=15 lines the cloud up with the rest - it was 5.
  int s = scale * (IconSize ? 1 : 0.75);
  y += 15;
  addcloud(x, y, s, linesize);
  addtstorm(x, y, s);
}

void Snow(int x, int y, bool IconSize) {
  int scale = (IconSize == LargeIcon) ? Large : Small;
  int linesize = 5;
  y += 15;   // was missing, leaving the snow cloud 15 px above every other one
  addcloud(x, y, scale * (IconSize ? 1 : 0.75), linesize);
  addsnow(x, y, IconSize);
}

void Mist(int x, int y, bool IconSize, bool night, bool moonUp) {
  int scale = (IconSize == LargeIcon) ? Large : Small;
  int linesize = (IconSize == SmallIcon) ? 3 : 5;
  // Used to draw the sun unconditionally, so a foggy night showed sunshine.
  // Same rule as the cloud icons: sun by day, moon if it is up, stars if not.
  int s = scale * (IconSize ? 1 : 0.75);
  if (!night)      addsun(x, y, s);
  else if (moonUp) addmoon(x, y, s);
  else             addstars(x, y, s);
  addfog(x, y, scale, linesize);
}

void CloudCover(int x, int y, int CloudCover) {
  addcloud(x - 9, y,     Small * 0.3, 2);
  addcloud(x + 3, y - 2, Small * 0.3, 2);
  addcloud(x, y + 15,    Small * 0.6, 2);
  drawString(x + 30, y, String(CloudCover) + "%", LEFT);
}

void Nodata(int x, int y, bool IconSize) {
  if (IconSize == LargeIcon) setFont(OpenSans24B); else setFont(OpenSans12B);
  drawString(x - 3, y - 10, "?", CENTER);
}

void Visibility(int x, int y, String VisibilityText) {
  float start_angle = 0.52, end_angle = 2.61, Offset = 10;
  int r = 14;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    drawPixel(x + r * cos(i), y - r / 2 + r * sin(i) + Offset, Black);
    drawPixel(x + r * cos(i), 1 + y - r / 2 + r * sin(i) + Offset, Black);
  }
  start_angle = 3.61; end_angle = 5.78;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    drawPixel(x + r * cos(i), y + r / 2 + r * sin(i) + Offset, Black);
    drawPixel(x + r * cos(i), 1 + y + r / 2 + r * sin(i) + Offset, Black);
  }
  fillCircle(x, y + Offset, r / 4, Black);
  drawString(x + 20, y, VisibilityText, LEFT);
}

void arrow(int x, int y, int asize, float aangle, int pwidth, int plength) {
  float dx = (asize - 10) * cos((aangle - 90) * PI / 180) + x;
  float dy = (asize - 10) * sin((aangle - 90) * PI / 180) + y;
  float x1 = 0;           float y1 = plength;
  float x2 = pwidth / 2;  float y2 = pwidth / 2;
  float x3 = -pwidth / 2; float y3 = pwidth / 2;
  float angle = aangle * PI / 180 - 135;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, Black);
}

bool isMoonUp(long timestamp) {
  MoonRise mr;
  mr.calculate(atof(Latitude), atof(Longitude), (time_t)timestamp);
  return mr.isVisible;
}

void DrawMoonImage(int x, int y) {
  Rect_t area = { .x = x, .y = y, .width = moon_width, .height = moon_height };
  epd_draw_grayscale_image(area, (uint8_t *)moon_data);
}

void DrawSunriseImage(int x, int y) {
  Rect_t area = { .x = x, .y = y, .width = sunrise_width, .height = sunrise_height };
  epd_draw_grayscale_image(area, (uint8_t *)sunrise_data);
}

void DrawSunsetImage(int x, int y) {
  Rect_t area = { .x = x, .y = y, .width = sunset_width, .height = sunset_height };
  epd_draw_grayscale_image(area, (uint8_t *)sunset_data);
}

// ============================================================================
//  Graph
// ============================================================================

// LowerArray is optional and only used for bar charts: the part of each bar it
// covers is filled black and the remainder grey, so one bar can show two
// components without a second axis. Pass NULL for a plain graph.
void DrawGraph(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max,
               const char *title, float DataArray[], float LowerArray[], int readings,
               bool auto_scale, bool barchart_mode) {
#define y_minor_axis 5
#define number_of_dashes 20
  setFont(OpenSans10B);

  if (auto_scale) {
    // Min/max as float and starting at index 0 - this used to round to int and
    // skip the first reading, which clipped values at the edges of the graph.
    float dmin = DataArray[0], dmax = DataArray[0];
    for (int i = 1; i < readings; i++) {
      if (DataArray[i] > dmax) dmax = DataArray[i];
      if (DataArray[i] < dmin) dmin = DataArray[i];
    }
    Y1Max = ceilf(dmax);
    Y1Min = barchart_mode ? 0.0f : floorf(dmin);
    if (Y1Max - Y1Min < 1.0f) Y1Max = Y1Min + 1.0f;   // also guards against /0
  }

  float x2, y2;
  int last_x = x_pos + 1;
  int last_y = y_pos + (Y1Max - constrain(DataArray[0], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight;

  // One decimal when the step between two labels is small enough that rounding
  // would mislead: a -6..0 axis steps by 1.2, and printing that as whole
  // numbers would label 1.2 apart as 1 apart.
  int decimals = (((Y1Max - Y1Min) / y_minor_axis) < 2.0f) ? 1 : 0;

  drawRect(x_pos, y_pos, gwidth + 3, gheight + 2, Grey);
  // Centred on the frame, not 20 px to its left - a long title reached that far
  // into the y-axis labels, which sit right-aligned just left of the frame.
  // Lifted a little too: 3 px between title and topmost label read as touching.
  drawString(x_pos + gwidth / 2, y_pos - 33, title, CENTER);

  for (int gx = 0; gx < readings; gx++) {
    x2 = x_pos + gx * gwidth / (readings - 1) - 1;
    y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
    if (barchart_mode) {
      int bw = (gwidth / readings) - 1;
      if (LowerArray) {
        // Whole bar grey, then the lower component painted black over it.
        fillRect(last_x + 2, y2, bw, y_pos + gheight - y2 + 2, Grey);
        float yl = y_pos + (Y1Max - constrain(LowerArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
        if (yl < y_pos + gheight) fillRect(last_x + 2, yl, bw, y_pos + gheight - yl + 2, Black);
      } else {
        fillRect(last_x + 2, y2, bw, y_pos + gheight - y2 + 2, Black);
      }
    } else {
      drawLine(last_x, last_y - 1, x2, y2 - 1, Black);
      drawLine(last_x, last_y, x2, y2, Black);
    }
    last_x = x2;
    last_y = y2;
  }

  for (int spacing = 0; spacing <= y_minor_axis; spacing++) {
    for (int j = 0; j < number_of_dashes; j++) {
      if (spacing < y_minor_axis)
        drawFastHLine((x_pos + 3 + j * gwidth / number_of_dashes),
                      y_pos + (gheight * spacing / y_minor_axis),
                      gwidth / (2 * number_of_dashes), Grey);
    }
    // Decimals chosen once for the whole axis, not per label - otherwise a
    // scale of 0..32 printed "32, 26, 19, 13, 6, 0.0", with a decimal on the
    // last value only.
    float label = Y1Max - (Y1Max - Y1Min) / y_minor_axis * spacing;
    drawString(x_pos - (decimals ? 10 : 7), y_pos + gheight * spacing / y_minor_axis - 5,
               String(label + 0.01, decimals), RIGHT);
  }

  for (int i = 0; i < 3; i++) {
    drawString(20 + x_pos + gwidth / 3 * i, y_pos + gheight + 10, String(i) + "d", LEFT);
    if (i < 2) drawFastVLine(x_pos + gwidth / 3 * i + gwidth / 3, y_pos, gheight, LightGrey);
  }
}

// ============================================================================
//  Drawing primitives (wrappers around the EPD driver)
// ============================================================================

// Width of a string in the current font, in pixels.
int stringWidth(String text) {
  char *data = const_cast<char *>(text.c_str());
  int32_t x1, y1, w, h, xx = 0, yy = 0;
  get_text_bounds(&currentFont, data, &xx, &yy, &x1, &y1, &w, &h, NULL);
  return w;
}

void drawString(int32_t x, int32_t y, String text, alignment align) {
  char *data = const_cast<char *>(text.c_str());
  int32_t x1, y1, w, h;
  int32_t xx = x, yy = y;
  get_text_bounds(&currentFont, data, &xx, &yy, &x1, &y1, &w, &h, NULL);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  int32_t cursor_y = y + h;
  write_string(&currentFont, data, &x, &cursor_y, framebuffer);
}

void fillCircle(int x, int y, int r, uint8_t color)                       { epd_fill_circle(x, y, r, color, framebuffer); }
void drawFastHLine(int16_t x0, int16_t y0, int length, uint16_t color)    { epd_draw_hline(x0, y0, length, color, framebuffer); }
void drawFastVLine(int16_t x0, int16_t y0, int length, uint16_t color)    { epd_draw_vline(x0, y0, length, color, framebuffer); }
void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) { epd_write_line(x0, y0, x1, y1, color, framebuffer); }
void drawCircle(int x0, int y0, int r, uint8_t color)                     { epd_draw_circle(x0, y0, r, color, framebuffer); }
void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { epd_draw_rect(x, y, w, h, color, framebuffer); }
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { epd_fill_rect(x, y, w, h, color, framebuffer); }
void drawPixel(int x, int y, uint8_t color)                               { epd_draw_pixel(x, y, color, framebuffer); }
void setFont(GFXfont const &font)                                         { currentFont = font; }

void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  int16_t x2, int16_t y2, uint16_t color) {
  epd_fill_triangle(x0, y0, x1, y1, x2, y2, color, framebuffer);
}
