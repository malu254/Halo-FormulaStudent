// Tries once to fetch the latest news
bool fetchLatestNews(String &title, String &link, String &desc) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, "https://www.the-race.com/category/formula-1/rss/");

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[News] HTTP error: %d\n", httpCode);
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();

  String line;
  bool inItem = false;
  int lineNum = 0, itemLine = 0;
  int itemPos = -1;

  title = "";
  link = "";
  desc = "";

  bool title_parsed = false, link_parsed = false, desc_parsed = false;

  while (stream->connected() && stream->available()) {
    line = stream->readStringUntil('\n');
    lineNum++;
    itemPos = 0;

    Serial.printf("[News] Line %d: %s\n", lineNum, line.c_str());

    if (line.indexOf("<item>") >= 0) {
      inItem = true;
      itemLine = lineNum;
      itemPos = line.indexOf("<item>");
      Serial.printf("[News] Line of Item: %d, Index Of Item: %d\n", itemLine, itemPos);
    }

    if (inItem) {
      if (line.indexOf("<title>", itemPos) >= 0 && title.length() == 0) {
        int start = line.indexOf("<title><![CDATA[", itemPos) + 16;
        int end   = line.indexOf("]]></title>", itemPos);
        if (end > start) title = line.substring(start, end);
        title_parsed = true;
        Serial.println("[News] Title parsed");
      }

      if (line.indexOf("<description>", itemPos) >= 0 && desc.length() == 0) {
        int start = line.indexOf("<description><![CDATA[", itemPos) + 22;
        int end   = line.indexOf("]]></description>", itemPos);
        if (end > start) {
          desc = line.substring(start, end);
          if (desc.length() > 300) desc = desc.substring(0, 300) + "...";
        }
        desc_parsed = true;
        Serial.println("[News] Desc parsed");
      }

      if (line.indexOf("<link>", itemPos) >= 0 && link.length() == 0) {
        int start = line.indexOf("<link>", itemPos) + 6;
        int end   = line.indexOf("</link>", itemPos);
        if (end > start) link = line.substring(start, end);
        link_parsed = true;
        Serial.println("[News] Link parsed");
      }

      if (link_parsed && desc_parsed && title_parsed) {
        break; // Stop after the first item
      }

    }
  }

  http.end();

  if (link == "") return false;
  if (title == "" && desc == "") return false;

  Serial.println("[News] Title: " + title);
  Serial.println("[News] Link: " + link);
  Serial.println("[News] Desc: " + desc);

  return true;
}

// Tries for 30 seconds to fetch the latest news (sometimes it takes more tries to get a reliable connection)
// @TODO -- find a better way, maybe with a timer instead of while loop because it is the only blocking code we are doing
bool getLatestNews(String &title, String &link, String &desc) {
  unsigned long long startTimestamp = millis();
  Serial.println("[News] Fetching News");

  while (!fetchLatestNews(title, link, desc)) {
    Serial.println("[News] Another cycle of news fetching");
    delay(1000);
    if (startTimestamp + 30000 < millis()) return false;
  }

  return true;
}

// ── Hardcoded fallback FS Electric team ELO rankings (2024 season approximate)
// Update this list each season or when FSELO fetch fails.
static void load_fs_hardcoded_rankings() {
  struct FSTeamEntry { const char* name; const char* university; const char* country; const char* countryCode; int elo; };
  static const FSTeamEntry fsTeams[] = {
    {"AMZ Racing",           "ETH Zurich",        "Switzerland",     "SUI", 2450},
    {"KA-RaceIng",           "KIT",               "Germany",         "GER", 2380},
    {"TU Graz Racing",       "TU Graz",           "Austria",         "AUT", 2310},
    {"DHBW Engineering",     "DHBW Stuttgart",    "Germany",         "GER", 2280},
    {"municHMotorsport",     "TU Munich",         "Germany",         "GER", 2250},
    {"High Octane MS",       "HS Esslingen",      "Germany",         "GER", 2200},
    {"UAS Racing",           "HFT Stuttgart",     "Germany",         "GER", 2170},
    {"GET Low Racing",       "Univ. Stuttgart",   "Germany",         "GER", 2150},
    {"RIOT Racing",          "Loughborough Univ", "Great Britain",   "GBR", 2120},
    {"Revolve NTNU",         "NTNU",              "Norway",          "NOR", 2100},
    {"FaSTTUBe",             "TU Berlin",         "Germany",         "GER", 2080},
    {"TUfast",               "TU Munich",         "Germany",         "GER", 2060},
    {"SAY Racing",           "FH Salzburg",       "Austria",         "AUT", 2040},
    {"Ecurie Aix",           "RWTH Aachen",       "Germany",         "GER", 2020},
    {"STUBA Green Team",     "STU Bratislava",    "Slovakia",        "SVK", 2000},
    {"e-Ting Racing",        "Univ. Heidelberg",  "Germany",         "GER", 1980},
    {"TU Wien Racing",       "TU Wien",           "Austria",         "AUT", 1960},
    {"Edinburgh Univ. FS",   "Univ. Edinburgh",   "Great Britain",   "GBR", 1940},
    {"Celeritas Dynamics",   "TU Eindhoven",      "Netherlands",     "NLD", 1920},
    {"EPFL Racing Team",     "EPFL",              "Switzerland",     "SUI", 1900},
    {"mrt",                  "TU Munich",         "Germany",         "GER", 1880},
    {"Green Formula",        "HS Offenburg",      "Germany",         "GER", 1860},
    {"Weiss Racing",         "HS Esslingen",      "Germany",         "GER", 1840},
    {"ITK Racing Team",      "TU Budapest",       "Hungary",         "HUN", 1820},
    {"RUSTy",                "Ruhr-Univ. Bochum", "Germany",         "GER", 1800},
  };

  int count = sizeof(fsTeams) / sizeof(fsTeams[0]);
  if (count > 50) count = 50;
  current_season.driver_count = count;
  current_season.season = "2024";
  current_season.round  = "0";

  for (int i = 0; i < count; i++) {
    current_season.driver_standings[i].position      = String(i + 1);
    current_season.driver_standings[i].points        = String(fsTeams[i].elo);
    current_season.driver_standings[i].number        = "";
    current_season.driver_standings[i].name          = fsTeams[i].university;
    current_season.driver_standings[i].surname       = fsTeams[i].name;
    current_season.driver_standings[i].constructor   = fsTeams[i].country;
    current_season.driver_standings[i].constructorId = fsTeams[i].countryCode;
  }

  // Build university standings from team list
  struct UniEntry { const char* name; const char* code; int bestElo; int rank; };
  static UniEntry unis[30];
  int uniCount = 0;
  for (int i = 0; i < count; i++) {
    bool found = false;
    for (int j = 0; j < uniCount; j++) {
      if (String(unis[j].code) == String(fsTeams[i].countryCode)) {
        found = true;
        break;
      }
    }
    if (!found && uniCount < 30) {
      unis[uniCount].name    = fsTeams[i].country;
      unis[uniCount].code    = fsTeams[i].countryCode;
      unis[uniCount].bestElo = fsTeams[i].elo;
      unis[uniCount].rank    = uniCount + 1;
      uniCount++;
    }
  }
  current_season.team_count = uniCount;
  for (int j = 0; j < uniCount; j++) {
    current_season.team_standings[j].position = String(unis[j].rank);
    current_season.team_standings[j].points   = String(unis[j].bestElo);
    current_season.team_standings[j].name     = unis[j].name;
    current_season.team_standings[j].id       = unis[j].code;
  }

  standings_loaded_once = true;
}

// Try to parse FSELO JSON response into current_season
// Expected format: array of objects with "pos","team","university","country","elo" fields
static bool parse_fselo_json(const String& payload) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.printf("[FSELO] JSON parse error: %s\n", error.c_str());
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  if (arr.isNull() || arr.size() == 0) {
    // Try object with "teams" array
    arr = doc["teams"].as<JsonArray>();
    if (arr.isNull() || arr.size() == 0) return false;
  }

  int count = 0;
  for (JsonObject obj : arr) {
    if (count >= 50) break;
    String teamName = obj["team"] | obj["name"] | obj["team_name"] | "";
    String university = obj["university"] | obj["school"] | obj["uni"] | "";
    String country = obj["country"] | obj["nation"] | "";
    String countryCode = obj["country_code"] | obj["code"] | country.substring(0, 3);
    String eloStr = obj["elo"] | obj["rating"] | obj["score"] | "0";
    String posStr = obj["pos"] | obj["rank"] | obj["position"] | String(count + 1);

    if (teamName.length() == 0 && university.length() == 0) continue;

    current_season.driver_standings[count].position      = posStr;
    current_season.driver_standings[count].points        = eloStr;
    current_season.driver_standings[count].number        = "";
    current_season.driver_standings[count].name          = university;
    current_season.driver_standings[count].surname       = teamName;
    current_season.driver_standings[count].constructor   = country;
    current_season.driver_standings[count].constructorId = countryCode;
    count++;
  }

  if (count == 0) return false;

  current_season.driver_count = count;
  current_season.season = "2025";
  current_season.round  = "0";
  standings_loaded_once = true;
  Serial.printf("[FSELO] Parsed %d teams from FSELO\n", count);
  return true;
}

// Fetch FS Electric team ELO rankings from FSELO website (with hardcoded fallback)
bool fetch_fs_team_rankings() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[FSELO] WiFi not connected");
    load_fs_hardcoded_rankings();
    return true;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  HTTPClient http;
  http.setTimeout(8000);

  // Try a few known URL patterns for FSELO data
  const char* urls[] = {
    "https://fselo.get-racing.de/elo_website_electric/data/elo_table_electric.json",
    "https://fselo.get-racing.de/elo_website_electric/data/elo.json",
    "https://fselo.get-racing.de/elo_website_electric/data/data.json",
    "https://fselo.get-racing.de/elo_website_electric/data/electric.json",
  };

  for (int u = 0; u < 4; u++) {
    http.begin(secureClient, urls[u]);
    int code = http.GET();
    if (code == 200) {
      String payload = http.getString();
      http.end();
      if (parse_fselo_json(payload)) {
        Serial.printf("[FSELO] Successfully fetched from %s\n", urls[u]);
        return true;
      }
    } else {
      http.end();
    }
  }

  Serial.println("[FSELO] All API attempts failed, using hardcoded rankings");
  load_fs_hardcoded_rankings();
  return true;
}

// ── Formula Student 2025 Event Calendar ───────────────────────────────────────
// Events in Germany (FSG), Spain (FSS), Czech Republic (FSCzech)
// Dates are approximate — verify at formulastudent.de and fsevents.de each year.

struct FSEventDef {
  const char* name;
  const char* circuit;
  const char* country;
  float lat;
  float lon;
  // 5 session slots: Scrutineering, Static Events, Skid Pad & Acc., Autocross, Endurance
  const char* sessionNames[5];
  const char* sessionDates[5];
  const char* sessionTimes[5];
};

// 2025 FS calendar – Electric class (dates approximate, update as confirmed)
static const FSEventDef fs_events_2025[] = {
  {
    "FS Czech Republic 2025",
    "Autodrom Most",
    "Czech Republic",
    50.5127f, 13.6368f,
    {"Scrutineering", "Static Events", "Skid Pad & Acc.", "Autocross", "Endurance"},
    {"2025-07-24",   "2025-07-25",    "2025-07-26",       "2025-07-27", "2025-07-27"},
    {"08:00:00Z",    "08:00:00Z",     "08:00:00Z",        "08:00:00Z",  "12:00:00Z"},
  },
  {
    "Formula Student Germany 2025",
    "Hockenheimring",
    "Germany",
    49.3278f, 8.5660f,
    {"Scrutineering", "Static Events", "Skid Pad & Acc.", "Autocross", "Endurance"},
    {"2025-08-06",   "2025-08-07",    "2025-08-08",       "2025-08-09", "2025-08-10"},
    {"08:00:00Z",    "08:00:00Z",     "08:00:00Z",        "08:00:00Z",  "08:00:00Z"},
  },
  {
    "Formula Student Spain 2025",
    "Circuit de Barcelona",
    "Spain",
    41.5701f, 2.2614f,
    {"Scrutineering", "Static Events", "Skid Pad & Acc.", "Autocross", "Endurance"},
    {"2025-08-19",   "2025-08-20",    "2025-08-21",       "2025-08-22", "2025-08-23"},
    {"08:00:00Z",    "08:00:00Z",     "08:00:00Z",        "08:00:00Z",  "08:00:00Z"},
  },
};

static const int fs_events_count = sizeof(fs_events_2025) / sizeof(fs_events_2025[0]);

// Fills next_race with the next upcoming (or currently active) FS event from the calendar.
// Falls back to the last event if all are past.
void getNextFSEvent(NextRaceInfo &info) {
  time_t now = time(nullptr);

  const FSEventDef* selected = &fs_events_2025[fs_events_count - 1]; // default: last event

  for (int i = 0; i < fs_events_count; i++) {
    // Use the Endurance session (last session) end date as event end proxy
    // Consider an event "active or upcoming" if Endurance hasn't started yet
    const FSEventDef& ev = fs_events_2025[i];
    struct tm tmUTC = {};
    String endDatetime = String(ev.sessionDates[4]) + "T" + String(ev.sessionTimes[4]);
    // add 8 hours for endurance finish
    endDatetime.remove(endDatetime.length() - 1); // strip Z
    strptime(endDatetime.c_str(), "%Y-%m-%dT%H:%M:%S", &tmUTC);
    time_t endEpoch = timegm(&tmUTC) + 8 * 3600; // ~8 h for the session
    if (now < endEpoch) {
      selected = &fs_events_2025[i];
      break;
    }
  }

  info.raceName    = selected->name;
  info.circuitName = selected->circuit;
  info.country     = selected->country;
  info.lat         = selected->lat;
  info.lon         = selected->lon;
  info.isSprintWeekend = false;
  info.sessionCount    = 5;

  for (int s = 0; s < 5; s++) {
    info.sessions[s].name = selected->sessionNames[s];
    info.sessions[s].date = selected->sessionDates[s];
    info.sessions[s].time = selected->sessionTimes[s];
  }

  Serial.println("[FS Event] Next event: " + info.raceName);
}

// Runs with a lvgl timer, fetches FS team rankings and next event info
void update_f1_api(lv_timer_t *timer) {
  if (!fetch_fs_team_rankings()) {
    Serial.println("[FS API] Rankings fetch failed, keeping existing data");
  }

  getNextFSEvent(next_race);

  Serial.println("[FS Event] Event: " + next_race.raceName);
  Serial.println("[FS Event] Circuit: " + next_race.circuitName);
  Serial.println("[FS Event] Country: " + next_race.country);

  for (int i = 0; i < next_race.sessionCount; i++) {
    String has_started = "No";
    if (hasSessionStarted(next_race.sessions[i].date, next_race.sessions[i].time)) has_started = "Yes";
    Serial.printf("[FS Event] %s - %s %s - Started: %s\n",
                  next_race.sessions[i].name.c_str(),
                  next_race.sessions[i].date.c_str(),
                  next_race.sessions[i].time.c_str(),
                  has_started.c_str());
  }

  // Weather forecast for each session
  fetchWeatherForRace(next_race);
}

  getNextFSEvent(next_race);

  Serial.println("[FS Event] Event: " + next_race.raceName);
  Serial.println("[FS Event] Circuit: " + next_race.circuitName);
  Serial.println("[FS Event] Country: " + next_race.country);

  for (int i = 0; i < next_race.sessionCount; i++) {
    String has_started = "No";
    if (hasSessionStarted(next_race.sessions[i].date, next_race.sessions[i].time)) has_started = "Yes";
    Serial.printf("[FS Event] %s - %s %s - Started: %s\n",
                  next_race.sessions[i].name.c_str(),
                  next_race.sessions[i].date.c_str(),
                  next_race.sessions[i].time.c_str(),
                  has_started.c_str());
  }

  // Weather forecast for each session
  fetchWeatherForRace(next_race);
}

void sendStatisticData(lv_timer_t *timer) {
  String UUID = getDeviceUUID();
  String current_language = localized_text->language_name_eng;
  String offset = (String)UTCoffset;

  HTTPClient http;
  String url = "https://www.we-race.it/wp-json/f1-halo/v2/sendstats?uuid=" + UUID + "&language=" + current_language + "&offset=" + offset + "&version=" + fw_version + "&flush=" + random(0, millis());
  http.begin(url.c_str());

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[Statistics Data] HTTP error: %d\n", httpCode);
    http.end();
    return;
  }

  String payload = http.getString();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (!error) {
    // check for updates
    updateAvailable = doc["update_available"];
    latestVersionString = doc["latest_version"].as<String>();
    update_link = doc["update_link"].as<String>();

    // populate notifications
    notificationQueue.clear();
    JsonArray notifications = doc["notifications"];
    
    for (JsonObject notification : notifications) {
      NotificationItem newItem;
      newItem.title = notification["title"].as<String>();
      newItem.text = notification["text"].as<String>();
      newItem.qrLink = notification["qr"].as<String>();
      notificationQueue.push_back(newItem);
    }
    Serial.printf("[Statistics Data] Synced: %d notifications available.\n", notificationQueue.size());
  }

  http.end();

  //Serial.printf("[Statistics Data] Stats response: %s\n", payload.c_str()); // debug
  return;
}

//flag for saving data, needed for WiFiManager
bool shouldSaveConfig = false;

// WiFiManager callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("[WiFiManager] Should save config");
  shouldSaveConfig = true;
}

// Called when WiFi Access Point is activated for connection setup (first setup or connection failed)
void configModeCallback (WiFiManager *myWiFiManager) {
  lv_screen_load(screen.wifi);

  // Show WiFi setup instructions
  lv_obj_t * label3 = lv_label_create(screen.wifi);
  lv_label_set_text_fmt(label3, localized_text->wifi_connection_failed, WiFi.softAPIP().toString().c_str());
  lv_obj_align(label3, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_label_set_long_mode(label3, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_width(label3, 250, 0);

  lv_timer_periodic_handler();
}

// WiFi Manager and WiFi Handler, runs in setup once. If connection success sets up a bunch of lvgl timers for API update (clock, F1 baseline, News)
void setupWiFiManager(bool forceConfig) {
  //set config save notify callback
  wm.setSaveConfigCallback(saveConfigCallback);
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(configModeCallback);
  wm.setClass("invert"); // dark theme

  if (forceConfig) {
    if (!wm.startConfigPortal("Halo-F1")) {
      if (clock_timer) lv_timer_del(clock_timer);
      clock_timer = NULL;
      if (f1_api_timer) lv_timer_del(f1_api_timer);
      f1_api_timer = NULL;
      if (news_timer) lv_timer_del(news_timer);
      news_timer = NULL;
      if (statistics_timer) lv_timer_del(statistics_timer);
      statistics_timer = NULL;
      if (notifications_timer) lv_timer_del(notifications_timer);
      notifications_timer = NULL;
      Serial.println("[WiFiManager] failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(5000);
    } else {
      update_internal_clock();
      update_f1_api(nullptr);
      update_ui(nullptr);
      create_or_reload_news_ui(nullptr);
      if (!clock_timer) clock_timer = lv_timer_create(update_ui, 60000, NULL);
      if (!f1_api_timer) f1_api_timer = lv_timer_create(update_f1_api, 3600000, NULL);
      if (!news_timer) news_timer = lv_timer_create(create_or_reload_news_ui, 5*60000, NULL);
      if (!statistics_timer) statistics_timer = lv_timer_create(sendStatisticData, 59*60000, NULL);
      if (!notifications_timer) notifications_timer = lv_timer_create(notification_scheduler_task, NOTIFICATION_INTERVAL_MS, NULL);
      lv_screen_load(screen.home);
    }
  } else {
    if (!wm.autoConnect("Halo-F1")) {
      if (clock_timer) lv_timer_del(clock_timer);
      clock_timer = NULL;
      if (f1_api_timer) lv_timer_del(f1_api_timer);
      f1_api_timer = NULL;
      if (news_timer) lv_timer_del(news_timer);
      news_timer = NULL;
      if (statistics_timer) lv_timer_del(statistics_timer);
      statistics_timer = NULL;
      if (notifications_timer) lv_timer_del(notifications_timer);
      notifications_timer = NULL;
      Serial.println("[WiFiManager] failed to connect and hit timeout");
      delay(3000);
      // if we still have not connected restart and try all over again
      ESP.restart();
      delay(5000);
    } else {
      update_internal_clock();
      update_f1_api(nullptr);
      update_ui(nullptr);
      create_or_reload_news_ui(nullptr);
      if (!clock_timer) clock_timer = lv_timer_create(update_ui, 60000, NULL);
      if (!f1_api_timer) f1_api_timer = lv_timer_create(update_f1_api, 3600000, NULL);
      if (!news_timer) news_timer = lv_timer_create(create_or_reload_news_ui, 5*60000, NULL);
      if (!statistics_timer) statistics_timer = lv_timer_create(sendStatisticData, 59*60000, NULL);
      if (!notifications_timer) notifications_timer = lv_timer_create(notification_scheduler_task, NOTIFICATION_INTERVAL_MS, NULL);
      lv_screen_load(screen.home);
    }
  }
  lv_timer_periodic_handler();


  //save the custom parameters to FS (not used for now)
  if (shouldSaveConfig)
  {
    ESP.restart();
    delay(5000);
  }

}