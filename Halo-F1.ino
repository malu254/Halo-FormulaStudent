const int MAX_FS_TEAMS = 50; // Max FS teams tracked/shown

// ESP32 Boards used v3.3.4

// Formula Student adaptation: shows FS team ELO rankings and event calendar
// Team data fetched from FSELO (fselo.get-racing.de) with hardcoded fallback
// Focuses on events in Germany (FSG), Spain (FSS), Czech Republic (FSCzech)

#define DISPLAY_TYPE DISPLAY_CYD_543
#define TOUCH_CAPACITIVE
#define TOUCH_SDA 8
#define TOUCH_SCL 4
#define TOUCH_INT 3
#define TOUCH_RST -1
#define TOUCH_MOSI 11
#define TOUCH_MISO 13
#define TOUCH_CLK 12
#define TOUCH_CS 38
#define TOUCH_MIN_X 1
#define TOUCH_MAX_X 480
#define TOUCH_MIN_Y 1
#define TOUCH_MAX_Y 272
#define SCREEN_WIDTH 272
#define SCREEN_HEIGHT 480

#ifdef TOUCH_CAPACITIVE
const String fw_version = "1.3.0-FS-beta";
#else
const String fw_version = "1.3.0-FS-R-beta";
#endif


#include <ArduinoJson.h>
#include <time.h>

// WIFI MANAGEMENT
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
WiFiManager wm;

// LVGL
#include <lvgl.h> // v9.3.0
#include "lv_conf.h"
LV_FONT_DECLARE(f1_symbols_28);
LV_FONT_DECLARE(weather_icons_12);
LV_FONT_DECLARE(montserrat_12);
LV_FONT_DECLARE(montserrat_14);
LV_FONT_DECLARE(montserrat_18);
LV_FONT_DECLARE(montserrat_20);
LV_FONT_DECLARE(montserrat_24);
LV_FONT_DECLARE(montserrat_38);


#define F1_SYMBOL_RANKING "\xEE\x95\xA1"
#define F1_SYMBOL_CHEQUERED_FLAG "\xEF\x84\x9E"
#define F1_SYMBOL_BARS "\xEF\x83\x89" 
#define F1_SYMBOL_GEAR "\xEF\x80\x93"
#define F1_SYMBOL_GEARS "\xEF\x82\x85"
#define F1_SYMBOL_SLIDERS "\xEF\x87\x9E"
#define F1_SYMBOL_WRENCH "\xEF\x82\xAD"
#define F1_SYMBOL_HAMMER "\xEF\x9B\xA3"

// Weather icon symbols (Font Awesome 7 Free Solid, font: weather_icons_16)
#define WX_SYMBOL_SUN           "\xEF\x86\x85"  // U+F185 fa-sun
#define WX_SYMBOL_CLOUD_SUN     "\xEF\x9B\x84"  // U+F6C4 fa-cloud-sun
#define WX_SYMBOL_CLOUD         "\xEF\x83\x82"  // U+F0C2 fa-cloud
#define WX_SYMBOL_SMOG          "\xEF\x9D\x9F"  // U+F75F fa-smog
#define WX_SYMBOL_DRIZZLE       "\xEF\x9C\xBD"  // U+F73D fa-cloud-rain
#define WX_SYMBOL_RAIN          "\xEF\x9D\x80"  // U+F740 fa-cloud-showers-heavy
#define WX_SYMBOL_SNOW          "\xEF\x8B\x9C"  // U+F2DC fa-snowflake
#define WX_SYMBOL_STORM         "\xEF\x83\xA7"  // U+F0E7 fa-bolt

#define HALO_COLOR_RED 0xFF1511

long UTCoffset;
int32_t UTCoffsetHours, UTCoffsetMinutes;

struct TimeRoller {
  lv_obj_t * hours;
  lv_obj_t * minutes;
};

TimeRoller nightModeStartRoller, nightModeStopRoller;

struct NightModeTimes {
  uint8_t start_hours;
  uint8_t start_minutes;
  uint8_t stop_hours;
  uint8_t stop_minutes;
};

NightModeTimes nightModeTimes = {23, 0, 8, 0};

bool nightModeActive = true;

bool timezoneOverrideActive = false;
lv_obj_t* timezone_override_switch = nullptr;
TimeRoller timezoneRoller = {nullptr, nullptr};

struct DriverStanding {
  String position;
  String points;   // ELO score for FS teams
  String number;   // team number (if any)
  String name;     // university short name
  String surname;  // team name
  String constructor;   // country
  String constructorId; // country code (for color)
};

struct TeamStandings {
  String position;
  String points;   // best ELO or cumulative
  String name;     // university full name
  String id;       // country code
};

struct SeasonStanding {
  String season;
  String round;
  DriverStanding driver_standings[50]; // FS team ELO rankings
  TeamStandings team_standings[30];    // University summary
  int driver_count;
  int team_count;
};

SeasonStanding current_season;

int driverStandingsCount = 0;

struct RaceSession {
    String name;
    String date;
    String time;
};

struct NextRaceInfo {
    String raceName;
    String circuitName;
    String country;
    float lat;
    float lon;
    bool isSprintWeekend;
    int sessionCount;
    RaceSession sessions[10]; // FS events typically have 5-7 public sessions
};

NextRaceInfo next_race;

// No live session results for Formula Student (no equivalent of OpenF1 API)
bool standings_loaded_once = false;

const int TOTAL_TEAMS = MAX_FS_TEAMS; // legacy UI naming compatibility

lv_display_t * disp;
lv_timer_t * clock_timer, * f1_api_timer, * standings_ui_timer, *news_timer, *statistics_timer, *notifications_timer;

lv_obj_t * sessions_container, * standings_container;

// Settings stuff
lv_obj_t * language_selector; // localized_text defined in localized_strings.h
lv_obj_t * no_spoiler_switch; bool noSpoilerModeActive = true;
lv_obj_t * brightness_slider, *night_brightness_slider; uint8_t brightness = 255, night_brightness = 30;

// No-Spoiler lift state (not a setting — temporary per-session override)
bool   noSpoilerLifted            = false; // true after user presses "Show"
String noSpoilerLiftedForSession  = "";    // session name that was active when lifted
String noSpoilerLastKnownSession  = "";    // updated each render cycle; read by the button callback
bool   noSpoilerWasStandings      = false; // true = was hiding standings; false = was hiding results

static int standings_offset = 0;
const int STANDINGS_PAGE_SIZE = 5;
// TOTAL_TEAMS is defined above

struct ScreenStruct {
  lv_obj_t * wifi;
  lv_obj_t * home;
  //lv_obj_t * settings;
};

ScreenStruct screen;

struct RaceTabLabelsStruct {
  lv_obj_t * clock;
  lv_obj_t * date;
  lv_obj_t * race_name;
};

RaceTabLabelsStruct racetab_labels;

lv_obj_t * home_tabs;

struct TabsStruct {
  lv_obj_t * race;
  lv_obj_t * standings;
  lv_obj_t * news;
  lv_obj_t * settings;
};

TabsStruct tabs;

// Standings tab state
lv_obj_t * standings_tab_list = nullptr;
bool standings_tab_showing_drivers = true;
lv_obj_t * standings_tab_btn_drivers = nullptr;
lv_obj_t * standings_tab_btn_constructors = nullptr;

// TRANSLATIONS
#include "localized_strings.h"

// PREFERENCES
#include <Preferences.h>
Preferences preferences;
#include "settings.h"

// LCD SCREEN
#include <bb_spi_lcd.h> // v2.7.1
#include "lv_bb_spi_lcd.h"
#include "touchscreen.h"

// FILES
#include "utils.h"
#include "audio.h"
#include "notifications.h"
#include "weather.h"
#include "ui.h"
#include "wifi_handler.h" // WiFiManager by Tzapu v2.0.17


void setup() {
  Serial.begin(115200);

  localized_text = &language_strings_en;

  // Initialise LVGL
  lv_init();
  lv_tick_set_cb([](){ 
    return (uint32_t) (esp_timer_get_time() / 1000ULL);
  });
  disp = lv_bb_spi_lcd_create(DISPLAY_TYPE);

#ifdef TOUCH_CAPACITIVE
  // Initialize touch screen
  bbct.init(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
#else
  lv_bb_spi_lcd_t* dsc = (lv_bb_spi_lcd_t *)lv_display_get_driver_data(disp);
  lcd = dsc->lcd;
  lcd->rtInit(TOUCH_MOSI, TOUCH_MISO, TOUCH_CLK, TOUCH_CS);
#endif

  // Register touch
  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);  
  lv_indev_set_read_cb(indev, touch_read);

  playNotificationSound();

  loadSettings(); //init preferences storage, check for saved settings and load them if present

  if(isNightTime() && nightModeActive) { // Adjust brightness after loading settings.
    adjustBrightness(night_brightness);
  } else {
    adjustBrightness(brightness);
  }

  create_ui_skeleton();

  setupWiFiManager(false);

  post_wifi_ui_creation();

  String uuid = getDeviceUUID();
  Serial.println("[Setup] Device UUID: " + uuid);
  Serial.println("[Setup] Device FW: " + fw_version);

  sendStatisticData(nullptr);
  
  lv_screen_load(screen.home);
  Serial.println("[Setup] Setup done");
}

void loop() {   
  lv_timer_periodic_handler();
  delay(5);
}
