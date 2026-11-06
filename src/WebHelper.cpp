/*
 * WebHelper.cpp
 * Copyright (C) 2016-2022 Linar Yusupov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <TimeLib.h>
#include "SoCHelper.h"
#include "WebHelper.h"
#include "WebStyles.h"
#include "TrafficHelper.h"
#include "NMEAHelper.h"
#include "BatteryHelper.h"
#include "GDL90Helper.h"
#include "BuddyHelper.h"
#include "BluetoothHelper.h"
#include <ArduinoJson.h>
#include "esp_private/esp_clk.h"
#include "esp_private/pm_impl.h"

#define NOLOGO

extern buddy_info_t buddies[21];

#if !defined(NOLOGO)
static const char Logo[] PROGMEM = {
#include "Logo.h"
    } ;
#endif

#include "jquery_min_js.h"

int get_cpu_frequency_mhz() {
  rtc_cpu_freq_config_t conf;
  rtc_clk_cpu_freq_get_config(&conf);
  return conf.freq_mhz;
}

byte getVal(char c)
{
   if(c >= '0' && c <= '9')
     return (byte)(c - '0');
   else
     return (byte)(toupper(c)-'A'+10);
}

#if DEBUG
void Hex2Bin(String str, byte *buffer)
{
  char hexdata[2 * PKT_SIZE + 1];
  str.toCharArray(hexdata, sizeof(hexdata));
  for(int j = 0; j < PKT_SIZE * 2 ; j+=2)
  {
    buffer[j>>1] = getVal(hexdata[j+1]) + (getVal(hexdata[j]) << 4);
  }
}
#endif

/* ─── Battery log helpers ──────────────────────────────────────────────────── */

void writeBatteryLog(float startVoltage, float endVoltage,
                     unsigned long durationSeconds, float estimated_mAh) {
  File file = SPIFFS.open("/battery_log.txt", FILE_APPEND);
  if (!file) return;
  file.printf("%.2f,%.2f,%lu,%.1f\n", startVoltage, endVoltage,
              durationSeconds, estimated_mAh);
  file.close();
}

String getLastBatteryLogEntry() {
  File file = SPIFFS.open("/battery_log.txt", FILE_READ);
  if (!file) return "No battery log found.";
  String lastLine = "";
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.length() > 5) lastLine = line;
  }
  file.close();
  return lastLine;
}

String formatBatteryLog(String logLine) {
  int idx = 0;
  String parts[4];
  while (logLine.length() && idx < 4) {
    int comma = logLine.indexOf(',');
    if (comma == -1) { parts[idx++] = logLine; break; }
    parts[idx++] = logLine.substring(0, comma);
    logLine = logLine.substring(comma + 1);
  }
  if (idx < 4) return F("Incomplete log entry");
  String r;
  r.reserve(100);
  r += F("Start: "); r += parts[0]; r += F("V, End: ");
  r += parts[1];     r += F("V, Duration: ");
  r += parts[2];     r += F(" sec, Used: ");
  r += parts[3];     r += F(" mAh");
  return r;
}

/* ─── BLE helpers ──────────────────────────────────────────────────────────── */

std::vector<String> getAllowedBLENameList() {
  std::vector<String> allowedNames;
  if (!SPIFFS.begin(true)) return allowedNames;
  File file = SPIFFS.open("/BLEConnections.txt", "r");
  if (!file || file.isDirectory()) return allowedNames;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) allowedNames.push_back(line);
  }
  file.close();
  return allowedNames;
}

void handleBLEScan() {
  std::vector<String> foundDevices = scanForBLEDevices(3);
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  for (const auto& name : foundDevices) array.add(name);
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleAddBLEDevice() {
  if (!server.hasArg("name")) { server.send(400, "text/plain", "Missing name"); return; }
  String newName = server.arg("name");
  File file = SPIFFS.open("/BLEConnections.txt", FILE_APPEND);
  file.println(newName);
  file.close();
  loadAllowedBLENames();
  server.send(200, "text/plain", "Added");
}

void handleDeleteBLEDevice() {
  if (!server.hasArg("name")) { server.send(400, "text/plain", "Missing name"); return; }
  String toDelete = server.arg("name");
  std::vector<String> updatedList;
  auto allowedBLENames = getAllowedBLENameList();
  for (const auto& name : allowedBLENames)
    if (name != toDelete) updatedList.push_back(name);
  File file = SPIFFS.open("/BLEConnections.txt", FILE_WRITE);
  for (const auto& name : updatedList) file.println(name);
  file.close();
  loadAllowedBLENames();
  server.send(200, "text/plain", "Deleted");
}

const char* charging_status_string(int status_code) {
  switch (status_code) {
    case 0: return "Not Charging";
    case 1: return "Pre-Charge";
    case 2: return "Fast Charging";
    case 3: return "Charge Done";
    default: return "Unknown";
  }
}

/* ─── Shared page wrapper helpers ──────────────────────────────────────────── */

/* Sends the common HTML head + nav bar. active_page: "root","settings","uploads","ble","about" */
static void sendPageHead(const char* title, const char* active_page) {
  server.sendContent_P(PSTR(
    "<!DOCTYPE html><html lang='en'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta name='theme-color' content='#1a1a2e'>"
  ));
  server.sendContent(F("<title>"));
  server.sendContent(title);
  server.sendContent(F("</title>"));
  server.sendContent_P(WEB_CSS);
  server.sendContent_P(PSTR("</head><body>"));

  /* Header bar */
  server.sendContent_P(PSTR(
    "<div class='hdr'>"
    "<div style='display:flex;align-items:center'>"
    "<div class='hdr-logo'>SV</div>"
    "<div><div class='hdr-title'>SkyView</div></div>"
    "</div></div>"
  ));

  /* Nav bar */
  char nav[600];
  snprintf_P(nav, sizeof(nav), PSTR(
    "<div class='tab-bar'>"
    "<a class='tab-btn%s' href='/'>Status</a>"
    "<a class='tab-btn%s' href='/settings'>Settings</a>"
    "<a class='tab-btn%s' href='/uploads'>Files</a>"
    "<a class='tab-btn%s' href='/ble/manage'>BLE</a>"
    "<a class='tab-btn%s' href='/firmware'>Firmware</a>"
    "<a class='tab-btn%s' href='/about'>About</a>"
    "</div>"
    "<div class='page-content'>"
  ),
    strcmp(active_page, "root")     == 0 ? " active" : "",
    strcmp(active_page, "settings") == 0 ? " active" : "",
    strcmp(active_page, "uploads")  == 0 ? " active" : "",
    strcmp(active_page, "ble")      == 0 ? " active" : "",
    strcmp(active_page, "firmware") == 0 ? " active" : "",
    strcmp(active_page, "about")    == 0 ? " active" : ""
  );
  server.sendContent(nav);
}

static void sendPageFoot() {
  server.sendContent_P(PSTR("</div></body></html>"));
  server.sendContent(""); /* end chunked */
}

static void beginPage(const char* title, const char* active_page) {
  server.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
  server.sendHeader(F("Pragma"),        F("no-cache"));
  server.sendHeader(F("Expires"),       F("-1"));
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  sendPageHead(title, active_page);
}

/* ═══════════════════════════════════════════════════════════════════════════
   Status page  (/)
   ═══════════════════════════════════════════════════════════════════════════ */

void handleRoot() {
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr  = min / 60;

  float vdd = Battery_voltage();
  bool low_voltage = (vdd <= Battery_threshold());
  char str_Vcc[8];
  dtostrf(vdd, 4, 2, str_Vcc);

  const char* charge_status_str = charging_status_string(charging_status());
  String chargingCurrent = String(read_PMU_charge_current());
  int cpu_freq = get_cpu_frequency_mhz();
  String lastBatteryLog = formatBatteryLog(getLastBatteryLogEntry());

  const char* conn_str =
    settings->connection == CON_SERIAL        ? "Serial" :
    settings->connection == CON_BLUETOOTH_SPP ? "Bluetooth SPP" :
    settings->connection == CON_BLUETOOTH_LE  ? "Bluetooth LE" :
    settings->connection == CON_WIFI_UDP      ? "WiFi UDP" :
    settings->connection == CON_DEMO_FILE     ? "Demo File" : "None";

  const char* bridge_str =
    settings->bridge == BRIDGE_BT_LE  ? "Bluetooth LE" :
    settings->bridge == BRIDGE_UDP    ? "WiFi UDP" :
    settings->bridge == BRIDGE_SERIAL ? "Serial" : "None";

  SoC->swSer_enableRx(false);
  beginPage("SkyView Status", "root");

  /* Device card */
  char buf[512];
  snprintf_P(buf, sizeof(buf), PSTR(
    "<div class='card'>"
    "<div class='card-title'>Device</div>"
    "<div class='row'><label>Device ID</label><span class='val'>%06X</span></div>"
    "<div class='row'><label>Firmware</label><span class='val'>%s &nbsp; %s</span></div>"
    "<div class='row'><label>Uptime</label><span class='val'>%02d:%02d:%02d</span></div>"
    "<div class='row'><label>CPU Freq</label><span class='val'>%d MHz</span></div>"
    "<div class='row'><label>Free memory</label><span class='val'>%u bytes</span></div>"
    "</div>"),
    SoC->getChipId() & 0xFFFFFF,
    SKYVIEW_FIRMWARE_VERSION,
    SoC == NULL ? "NONE" : SoC->name,
    hr, min % 60, sec % 60,
    cpu_freq, ESP.getFreeHeap());
  server.sendContent(buf);

  /* Power card */
  snprintf_P(buf, sizeof(buf), PSTR(
    "<div class='card'>"
    "<div class='card-title'>Power</div>"
    "<div class='row'><label>Battery</label><span class='val %s'>%sV</span></div>"
    "<div class='row'><label>Charging</label><span class='val'>%s</span></div>"
    "<div class='row'><label>Charge Current</label><span class='val'>%s mA</span></div>"
    "<div class='row'><label>Last Battery Log</label>"
    "  <span class='val' style='font-size:12px'>%s</span></div>"
    "<div class='actions'>"
    "  <a href='/download_log'><button class='btn btn-outline btn-sm'>&#x2B73; Log</button></a>"
    "  <form action='/clear_log' method='post' style='display:inline'"
    "    onsubmit=\"return confirm('Clear battery log?')\">"
    "    <button type='submit' class='btn btn-danger btn-sm'>&#x1F5D1; Clear</button>"
    "  </form>"
    "</div></div>"),
    low_voltage ? "warn" : "ok", str_Vcc,
    charge_status_str,
    chargingCurrent.c_str(),
    lastBatteryLog.c_str());
  server.sendContent(buf);

  /* Connection card */
  snprintf_P(buf, sizeof(buf), PSTR(
    "<div class='card'>"
    "<div class='card-title'>Connection</div>"
    "<div class='row'><label>Display</label><span class='val'>AMOLED 1.75\"</span></div>"
    "<div class='row'><label>Connection</label><span class='val'>%s</span></div>"),
    conn_str);
  server.sendContent(buf);

  if (settings->connection == CON_WIFI_UDP) {
    snprintf_P(buf, sizeof(buf), PSTR(
      "<div class='row'><label>Link partner</label><span class='val'>%s</span></div>"
      "<div class='row'><label>Link status</label><span class='val'>%s</span></div>"
      "<div class='row'><label>IP address</label><span class='val'>%s</span></div>"),
      (settings->server && strlen(settings->server) > 0) ? settings->server : "NOT SET",
      WiFi.status() == WL_CONNECTED ? "Established" : "Not established",
      WiFi.localIP().toString().c_str());
    server.sendContent(buf);
  }

  switch (settings->connection) {
    case CON_WIFI_UDP:
    case CON_SERIAL:
    case CON_BLUETOOTH_SPP:
    case CON_BLUETOOTH_LE: {
      bool connected = false;
      const char *d1 = "", *d2 = "", *d3 = "";
      if (settings->protocol == PROTOCOL_GDL90) {
        connected = GDL90_isConnected();
        d1 = (connected && !GDL90_hasHeartBeat()) ? "UNK" : "";
        d2 = GDL90_hasHeartBeat() ? "GDL90" : "";
      } else {
        connected = NMEA_isConnected();
        d1 = (connected && !(NMEA_hasGNSS() || NMEA_hasFLARM())) ? "UNK" : "";
        d2 = NMEA_hasGNSS()  ? "GNSS"  : "";
        d3 = NMEA_hasFLARM() ? "FLARM" : "";
      }
      snprintf_P(buf, sizeof(buf), PSTR(
        "<div class='row'><label>Status</label>"
        "  <span class='val %s'>%s connected</span></div>"
        "<div class='row'><label>Data type</label>"
        "  <span class='val'>%s %s %s</span></div>"),
        connected ? "ok" : "warn",
        connected ? "" : "not",
        d1, d2, d3);
      server.sendContent(buf);
      break;
    }
    default: break;
  }

  snprintf_P(buf, sizeof(buf), PSTR(
    "<div class='row'><label>Bridge output</label><span class='val'>%s</span></div>"),
    bridge_str);
  server.sendContent(buf);
  server.sendContent_P(PSTR("</div>")); /* close Connection card */

  sendPageFoot();
  SoC->swSer_enableRx(true);
}

/* ═══════════════════════════════════════════════════════════════════════════
   Settings page  (/settings)
   ═══════════════════════════════════════════════════════════════════════════ */

void handleSettings() {
  SoC->swSer_enableRx(false);
  beginPage("SkyView Settings", "settings");

  server.sendContent_P(PSTR("<form action='/input' method='GET'>"));

  /* Card 1: Display & Connection */
  server.sendContent_P(PSTR("<div class='card'><div class='card-title'>Display &amp; Connection</div>"));

  char buf[800];

  /* Adapter */
  if (SoC->id == SOC_ESP32) {
    snprintf_P(buf, sizeof(buf), PSTR(
      "<div class='form-row'><label>Display adapter</label><select name='adapter'>"
      "<option %s value='%d'>T-Display AMOLED 1.75</option>"
      "<option %s value='%d'>e-Paper Waveshare ESP32</option>"
      "</select></div>"),
      (settings->adapter == ADAPTER_TTGO_T5S        ? "selected" : ""), ADAPTER_TTGO_T5S,
      (settings->adapter == ADAPTER_WAVESHARE_ESP32 ? "selected" : ""), ADAPTER_WAVESHARE_ESP32);
  } else {
    snprintf_P(buf, sizeof(buf), PSTR(
      "<div class='form-row'><label>Display adapter</label><select name='adapter'>"
      "<option %s value='%d'>NodeMCU</option>"
      "<option %s value='%d'>e-Paper Waveshare ESP8266</option>"
      "</select></div>"),
      (settings->adapter == ADAPTER_NODEMCU           ? "selected" : ""), ADAPTER_NODEMCU,
      (settings->adapter == ADAPTER_WAVESHARE_ESP8266 ? "selected" : ""), ADAPTER_WAVESHARE_ESP8266);
  }
  server.sendContent(buf);

  /* Connection type */
  snprintf_P(buf, sizeof(buf), PSTR(
    "<div class='form-row'><label>Connection type</label><select name='connection'>"
    "<option %s value='%d'>Serial</option>"
    "<option %s value='%d'>WiFi UDP</option>"
    "<option %s value='%d'>Bluetooth SPP</option>"
    "<option %s value='%d'>Bluetooth LE</option>"
    "<option %s value='%d'>Demo File</option>"
    "</select></div>"),
    (settings->connection == CON_SERIAL        ? "selected" : ""), CON_SERIAL,
    (settings->connection == CON_WIFI_UDP      ? "selected" : ""), CON_WIFI_UDP,
    (settings->connection == CON_BLUETOOTH_SPP ? "selected" : ""), CON_BLUETOOTH_SPP,
    (settings->connection == CON_BLUETOOTH_LE  ? "selected" : ""), CON_BLUETOOTH_LE,
    (settings->connection == CON_DEMO_FILE     ? "selected" : ""), CON_DEMO_FILE);
  server.sendContent(buf);

  /* Protocol */
  snprintf_P(buf, sizeof(buf), PSTR(
    "<div class='form-row'><label>Protocol</label><select name='protocol'>"
    "<option %s value='%d'>NMEA</option>"
    "<option %s value='%d'>GDL90</option>"
    "</select></div>"),
    (settings->protocol == PROTOCOL_NMEA  ? "selected" : ""), PROTOCOL_NMEA,
    (settings->protocol == PROTOCOL_GDL90 ? "selected" : ""), PROTOCOL_GDL90);
  server.sendContent(buf);

  /* Baud rate */
  snprintf_P(buf, sizeof(buf), PSTR(
    "<div class='form-row'><label>Baud rate</label><select name='baudrate'>"
    "<option %s value='%d'>4800</option>"
    "<option %s value='%d'>9600</option>"
    "<option %s value='%d'>19200</option>"
    "<option %s value='%d'>38400</option>"
    "<option %s value='%d'>57600</option>"),
    (settings->baudrate == B4800  ? "selected" : ""), B4800,
    (settings->baudrate == B9600  ? "selected" : ""), B9600,
    (settings->baudrate == B19200 ? "selected" : ""), B19200,
    (settings->baudrate == B38400 ? "selected" : ""), B38400,
    (settings->baudrate == B57600 ? "selected" : ""), B57600);
  server.sendContent(buf);

  if (SoC->id == SOC_ESP32) {
    snprintf_P(buf, sizeof(buf), PSTR(
      "<option %s value='%d'>115200</option>"
      "<option %s value='%d'>2000000</option>"),
      (settings->baudrate == B115200  ? "selected" : ""), B115200,
      (settings->baudrate == B2000000 ? "selected" : ""), B2000000);
    server.sendContent(buf);
  }
  server.sendContent_P(PSTR("</select></div>"));

  /* Source ID & Key */
  snprintf_P(buf, sizeof(buf), PSTR(
    "<div class='form-row'><label>Source ID</label>"
    "<input type='text' name='server' maxlength='21' value='%s'></div>"
    "<div class='form-row'><label>Key</label>"
    "<input type='text' name='key' maxlength='17' value='%s'></div>"),
    settings->server, settings->key);
  server.sendContent(buf);
  server.sendContent_P(PSTR("</div>")); /* end card */

  /* Card 2: Bridge & Display */
  server.sendContent_P(PSTR("<div class='card'><div class='card-title'>Bridge &amp; Display</div>"));

  snprintf_P(buf, sizeof(buf), PSTR(
    "<div class='form-row'><label>Bridge Output</label><select name='bridge'>"
    "<option %s value='%d'>None</option>"
    "<option %s value='%d'>Serial</option>"
    "<option %s value='%d'>WiFi UDP</option>"
    "<option %s value='%d'>Bluetooth SPP</option>"
    "<option %s value='%d'>Bluetooth LE</option>"
    "</select></div>"),
    (settings->bridge == BRIDGE_NONE   ? "selected" : ""), BRIDGE_NONE,
    (settings->bridge == BRIDGE_SERIAL ? "selected" : ""), BRIDGE_SERIAL,
    (settings->bridge == BRIDGE_UDP    ? "selected" : ""), BRIDGE_UDP,
    (settings->bridge == BRIDGE_BT_SPP ? "selected" : ""), BRIDGE_BT_SPP,
    (settings->bridge == BRIDGE_BT_LE  ? "selected" : ""), BRIDGE_BT_LE);
  server.sendContent(buf);

  snprintf_P(buf, sizeof(buf), PSTR(
    "<div class='form-row'><label>Units</label><select name='units'>"
    "<option %s value='%d'>Metric</option>"
    "<option %s value='%d'>Imperial</option>"
    "<option %s value='%d'>Mixed</option>"
    "</select></div>"
    "<div class='form-row'><label>View mode</label><select name='vmode'>"
    "<option %s value='%d'>Radar</option>"
    "<option %s value='%d'>Text</option>"
    "</select></div>"),
    (settings->units == UNITS_METRIC    ? "selected" : ""), UNITS_METRIC,
    (settings->units == UNITS_IMPERIAL  ? "selected" : ""), UNITS_IMPERIAL,
    (settings->units == UNITS_MIXED     ? "selected" : ""), UNITS_MIXED,
    (settings->vmode == VIEW_MODE_RADAR ? "selected" : ""), VIEW_MODE_RADAR,
    (settings->vmode == VIEW_MODE_TEXT  ? "selected" : ""), VIEW_MODE_TEXT);
  server.sendContent(buf);

  snprintf_P(buf, sizeof(buf), PSTR(
    "<div class='form-row'><label>Radar orientation</label><select name='orientation'>"
    "<option %s value='%d'>Track Up</option>"
    "<option %s value='%d'>North Up</option>"
    "</select></div>"
    "<div class='form-row'><label>Zoom level</label><select name='zoom'>"
    "<option %s value='%d'>Lowest</option>"
    "<option %s value='%d'>Low</option>"
    "<option %s value='%d'>Medium</option>"
    "<option %s value='%d'>High</option>"
    "</select></div>"),
    (settings->orientation == DIRECTION_TRACK_UP ? "selected" : ""), DIRECTION_TRACK_UP,
    (settings->orientation == DIRECTION_NORTH_UP ? "selected" : ""), DIRECTION_NORTH_UP,
    (settings->zoom == ZOOM_LOWEST ? "selected" : ""), ZOOM_LOWEST,
    (settings->zoom == ZOOM_LOW    ? "selected" : ""), ZOOM_LOW,
    (settings->zoom == ZOOM_MEDIUM ? "selected" : ""), ZOOM_MEDIUM,
    (settings->zoom == ZOOM_HIGH   ? "selected" : ""), ZOOM_HIGH);
  server.sendContent(buf);
  server.sendContent_P(PSTR("</div>")); /* end card */

  /* Card 3: Traffic & Filters */
  server.sendContent_P(PSTR("<div class='card'><div class='card-title'>Traffic &amp; Filters</div>"));

  snprintf_P(buf, sizeof(buf), PSTR(
    "<div class='form-row'><label>Aircrafts data</label><select name='adb'>"
    "<option %s value='%d'>Off</option>"
    "<option %s value='%d'>FlarmNet</option>"
    "<option %s value='%d'>GliderNet</option>"
    "<option %s value='%d'>ICAO</option>"
    "<option %s value='%d'>PureTrack</option>"
    "</select></div>"),
    (settings->adb == DB_NONE      ? "selected" : ""), DB_NONE,
    (settings->adb == DB_FLN       ? "selected" : ""), DB_FLN,
    (settings->adb == DB_OGN       ? "selected" : ""), DB_OGN,
    (settings->adb == DB_ICAO      ? "selected" : ""), DB_ICAO,
    (settings->adb == DB_PURETRACK ? "selected" : ""), DB_PURETRACK);
  server.sendContent(buf);

  snprintf_P(buf, sizeof(buf), PSTR(
    "<div class='form-row'><label>ID preference</label><select name='idpref'>"
    "<option %s value='%d'>Registration</option>"
    "<option %s value='%d'>Tail/CN</option>"
    "<option %s value='%d'>Make &amp; Model</option>"
    "</select></div>"),
    (settings->idpref == ID_REG    ? "selected" : ""), ID_REG,
    (settings->idpref == ID_TAIL   ? "selected" : ""), ID_TAIL,
    (settings->idpref == ID_MAM    ? "selected" : ""), ID_MAM);
  server.sendContent(buf);

  if (SoC->id == SOC_ESP32) {
    snprintf_P(buf, sizeof(buf), PSTR(
      "<div class='form-row'><label>Voice</label><select name='voice'>"
      "<option %s value='%d'>Off</option>"
      "<option %s value='%d'>On</option>"
      "</select></div>"),
      (settings->voice == VOICE_OFF ? "selected" : ""), VOICE_OFF,
      (settings->voice != VOICE_OFF ? "selected" : ""), VOICE_ON);
    server.sendContent(buf);
  }

  snprintf_P(buf, sizeof(buf), PSTR(
    "<div class='form-row'><label>Compass page</label><select name='compass'>"
    "<option %s value='%d'>On</option>"
    "<option %s value='%d'>Off</option>"
    "</select></div>"
    "<div class='form-row'><label>Traffic filter</label><select name='filter'>"
    "<option %s value='%d'>Unfiltered</option>"
    "<option %s value='%d'>Altitude (&#177;500 m)</option>"
    "<option %s value='%d'>Alarm only</option>"
    "</select></div>"
    "<div class='form-row'><label>Power save</label><select name='power_save'>"
    "<option %s value='%d'>Disabled</option>"
    "<option %s value='%d'>WiFi OFF (5 min.)</option>"
    "</select></div>"
    "<div class='form-row'><label>Team Member ID</label>"
    "<input type='text' name='team' maxlength='6' value='%06X'></div>"),
    (settings->compass    == COMPASS_ON           ? "selected" : ""), COMPASS_ON,
    (settings->compass    == COMPASS_OFF          ? "selected" : ""), COMPASS_OFF,
    (settings->filter     == TRAFFIC_FILTER_OFF   ? "selected" : ""), TRAFFIC_FILTER_OFF,
    (settings->filter     == TRAFFIC_FILTER_500M  ? "selected" : ""), TRAFFIC_FILTER_500M,
    (settings->filter     == TRAFFIC_FILTER_ALARM ? "selected" : ""), TRAFFIC_FILTER_ALARM,
    (settings->power_save == POWER_SAVE_NONE      ? "selected" : ""), POWER_SAVE_NONE,
    (settings->power_save == POWER_SAVE_WIFI      ? "selected" : ""), POWER_SAVE_WIFI,
    settings->team);
  server.sendContent(buf);

  server.sendContent_P(PSTR(
    "</div>" /* end card */
    "<button type='submit' class='btn btn-primary btn-full'>"
    "&#x1F4BE; Save &amp; Restart</button>"
    "</form>"));

  sendPageFoot();
  SoC->swSer_enableRx(true);
}

/* ═══════════════════════════════════════════════════════════════════════════
   handleInput — save settings, show confirmation, restart
   ═══════════════════════════════════════════════════════════════════════════ */

void handleInput() {
  for (uint8_t i = 0; i < server.args(); i++) {
    if      (server.argName(i).equals("adapter"))     settings->adapter     = server.arg(i).toInt();
    else if (server.argName(i).equals("connection")) {
      settings->connection = server.arg(i).toInt();
      settings->demo_mode  = (settings->connection == CON_DEMO_FILE);
    }
    else if (server.argName(i).equals("bridge"))      settings->bridge      = server.arg(i).toInt();
    else if (server.argName(i).equals("protocol"))    settings->protocol    = server.arg(i).toInt();
    else if (server.argName(i).equals("baudrate"))    settings->baudrate    = server.arg(i).toInt();
    else if (server.argName(i).equals("server"))      server.arg(i).toCharArray(settings->server, sizeof(settings->server));
    else if (server.argName(i).equals("key"))         server.arg(i).toCharArray(settings->key,    sizeof(settings->key));
    else if (server.argName(i).equals("units"))       settings->units       = server.arg(i).toInt();
    else if (server.argName(i).equals("vmode"))       settings->vmode       = server.arg(i).toInt();
    else if (server.argName(i).equals("orientation")) settings->orientation = server.arg(i).toInt();
    else if (server.argName(i).equals("zoom"))        settings->zoom        = server.arg(i).toInt();
    else if (server.argName(i).equals("adb"))         settings->adb         = server.arg(i).toInt();
    else if (server.argName(i).equals("idpref"))      settings->idpref      = server.arg(i).toInt();
    else if (server.argName(i).equals("voice"))       settings->voice       = server.arg(i).toInt();
    else if (server.argName(i).equals("compass"))     settings->compass     = server.arg(i).toInt();
    else if (server.argName(i).equals("filter"))      settings->filter      = server.arg(i).toInt();
    else if (server.argName(i).equals("power_save"))  settings->power_save  = server.arg(i).toInt();
    else if (server.argName(i).equals("team")) {
      char buf[7];
      server.arg(i).toCharArray(buf, sizeof(buf));
      settings->team = strtoul(buf, NULL, 16);
    }
  }

  if (settings->connection != CON_SERIAL && settings->bridge != BRIDGE_SERIAL)
    settings->bridge = BRIDGE_NONE;

  /* Minimal styled confirmation page — no malloc for large HTML */
  SoC->swSer_enableRx(false);
  server.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
  server.send_P(200, PSTR("text/html"), PSTR(
    "<!DOCTYPE html><html><head>"
    "<meta http-equiv='refresh' content='15; url=/'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Saving...</title>"
    "<style>"
    "body{font-family:sans-serif;background:#1a1a2e;color:#e5e7eb;"
    "display:flex;align-items:center;justify-content:center;"
    "min-height:100vh;text-align:center}"
    "h1{color:#818cf8;margin-bottom:12px}"
    "p{color:#9ca3af}"
    "</style></head><body>"
    "<div><h1>&#x1F4BE; Settings Saved</h1>"
    "<p>Restarting device&hellip;</p>"
    "<p style='font-size:13px'>Redirecting in 15 seconds.</p>"
    "</div></body></html>"
  ));
  delay(1000);
  EEPROM_store();
  delay(1000);
  ESP.restart();
}

/* ═══════════════════════════════════════════════════════════════════════════
   Uploads page  (/uploads)
   ═══════════════════════════════════════════════════════════════════════════ */

void handleUploads() {
  beginPage("SkyView Files", "uploads");

  server.sendContent_P(PSTR(
    "<div class='card'>"
    "<div class='card-title'>Buddy List</div>"
    "<p class='info-text'>CSV: <code>AABBCC,Name</code> &mdash; filename: <b>buddylist.txt</b></p>"
    "<input type='file' id='fBuddy' accept='.txt,.csv'>"
    "<div class='actions'>"
    "  <button class='btn btn-primary btn-sm' onclick='doUpload(\"fBuddy\",\"/upload-buddy\")'>&#x2B06; Upload</button>"
    "  <a href='/download-buddy'><button class='btn btn-outline btn-sm'>&#x2B73; Download</button></a>"
    "  <a href='/buddy-manager'><button class='btn btn-outline btn-sm'>&#x270F; Edit Online</button></a>"
    "</div></div>"

    "<div class='card'>"
    "<div class='card-title'>PureTrack Database</div>"
    "<p class='info-text'>CDB format &mdash; filename: <b>puretrack.cdb</b></p>"
    "<input type='file' id='fPuretrack' accept='.cdb'>"
    "<div class='actions'>"
    "  <button class='btn btn-primary btn-sm' onclick='doUpload(\"fPuretrack\",\"/upload-puretrack\")'>&#x2B06; Upload</button>"
    "  <a href='/download-puretrack'><button class='btn btn-outline btn-sm'>&#x2B73; Download</button></a>"
    "</div></div>"

    "<div class='card'>"
    "<div class='card-title'>Demo NMEA File</div>"
    "<p class='info-text'>NMEA playback &mdash; filename: <b>demo.nmea</b></p>"
    "<input type='file' id='fDemo' accept='.nmea,.txt'>"
    "<div class='actions'>"
    "  <button class='btn btn-primary btn-sm' onclick='doUpload(\"fDemo\",\"/upload-demo\")'>&#x2B06; Upload</button>"
    "  <a href='/download-demo'><button class='btn btn-outline btn-sm'>&#x2B73; Download</button></a>"
    "</div></div>"

    "<div class='card'>"
    "<div class='card-title'>Voice Audio (internal flash)</div>"
    "<p class='info-text'>Select the <b>Audio</b> folder (containing <b>voice1</b>, <b>voice2</b>, <b>voice3</b>) "
    "or a single voice folder. Files are stored in flash at <code>/Audio/voiceN/</code>, so no SD card is needed.</p>"
    "<div class='row'><label>Playing from</label><span class='val' id='aSrc'>&hellip;</span></div>"
    "<div class='row'><label>Flash used</label><span class='val' id='aUse'>&hellip;</span></div>"
    "<div class='row'><label>Files</label><span class='val' id='aCnt'>&hellip;</span></div>"
    "<input type='file' id='fAudio' webkitdirectory directory multiple>"
    "<div class='actions'>"
    "  <button class='btn btn-primary btn-sm' onclick='doAudioUpload()'>&#x2B06; Upload folder</button>"
    "  <button class='btn btn-outline btn-sm' onclick='doAudioErase()'>&#x1F5D1; Erase flash audio</button>"
    "</div></div>"

    "<div id='status-msg'></div>"
    "<script>"
    "function doUpload(id,ep){"
    "  var f=document.getElementById(id).files[0];"
    "  if(!f){alert('Select a file first');return;}"
    "  var fd=new FormData();fd.append('file',f);"
    "  var m=document.getElementById('status-msg');"
    "  m.className='status-ok';m.innerText='Uploading...';"
    "  fetch(ep,{method:'POST',body:fd})"
    "    .then(function(r){return r.text();})"
    "    .then(function(t){m.innerText=t;})"
    "    .catch(function(e){m.className='status-err';m.innerText='Error: '+e;});"
    "}"
    "function audioStatus(){"
    "  fetch('/api/audio').then(function(r){return r.json();}).then(function(j){"
    "    document.getElementById('aSrc').innerText={flash:'Internal flash',sd:'SD card',none:'None (voice disabled)'}[j.source];"
    "    document.getElementById('aUse').innerText=j.mounted?(Math.round(j.used/1024)+' / '+Math.round(j.total/1024)+' KB'):'No audio partition - flash full firmware via USB';"
    "    document.getElementById('aCnt').innerText='voice1: '+j.voice1+', voice2: '+j.voice2+', voice3: '+j.voice3;"
    "  });"
    "}"
    "function doAudioUpload(){"
    "  var fl=document.getElementById('fAudio').files,q=[],tot=0;"
    "  for(var i=0;i<fl.length;i++){"
    "    var f=fl[i],p=(f.webkitRelativePath||f.name).split('/'),n=p[p.length-1],v=null;"
    "    if(!/\\.wav$/i.test(n))continue;"
    "    for(var k=p.length-2;k>=0;k--){if(/^voice[0-9]+$/i.test(p[k])){v=p[k].toLowerCase();break;}}"
    "    if(!v)continue;"
    "    q.push({f:f,path:'/Audio/'+v+'/'+n});tot+=f.size;"
    "  }"
    "  if(!q.length){alert('No voiceN/*.wav files found. Select the Audio folder or a voice1/voice2/voice3 folder.');return;}"
    "  var m=document.getElementById('status-msg'),i=0,done=0;"
    "  m.className='status-ok';"
    "  function next(){"
    "    if(i>=q.length){m.innerText='Uploaded '+q.length+' files. Restart SkyView to play voice from flash.';audioStatus();return;}"
    "    var it=q[i],fd=new FormData();fd.append('file',it.f);"
    "    m.innerText='Uploading '+(i+1)+'/'+q.length+' ('+Math.round(done/tot*100)+'%) '+it.path;"
    "    fetch('/upload-audio?path='+encodeURIComponent(it.path),{method:'POST',body:fd})"
    "      .then(function(r){return r.text().then(function(t){if(!r.ok)throw t;});})"
    "      .then(function(){done+=it.f.size;i++;next();})"
    "      .catch(function(e){m.className='status-err';m.innerText='Error at '+it.path+': '+e;audioStatus();});"
    "  }"
    "  next();"
    "}"
    "function doAudioErase(){"
    "  if(!confirm('Erase all voice files from internal flash?'))return;"
    "  var m=document.getElementById('status-msg');m.className='status-ok';m.innerText='Erasing...';"
    "  fetch('/audio-erase',{method:'POST'}).then(function(r){return r.text();})"
    "    .then(function(t){m.innerText=t;audioStatus();});"
    "}"
    "audioStatus();"
    "</script>"
  ));

  sendPageFoot();
}

/* ═══════════════════════════════════════════════════════════════════════════
   Buddy Manager page  (/buddy-manager)
   ═══════════════════════════════════════════════════════════════════════════ */

void handleBuddyManager() {
  beginPage("Buddy Manager", "uploads");

  server.sendContent_P(PSTR(
    "<div class='card'>"
    "<div class='card-title'>Buddy List Editor</div>"
    "<p class='info-text'>Hex ID must be 6 characters (e.g. AABBCC).</p>"
    "<div id='buddy-list'>Loading...</div>"
    "<div class='actions' style='margin-top:10px'>"
    "  <button class='btn btn-outline btn-sm' onclick='buddyAdd()'>+ Add</button>"
    "  <button class='btn btn-primary btn-sm' onclick='buddySave()'>&#x1F4BE; Save All</button>"
    "  <a href='/uploads'><button class='btn btn-outline btn-sm'>&larr; Back</button></a>"
    "</div>"
    "<div id='buddy-status' class='status-ok' style='margin-top:8px'></div>"
    "</div>"
    "<script>"
    "var _b=[];"
    "async function bLoad(){"
    "  try{var r=await fetch('/api/buddies');_b=await r.json();bRender();}"
    "  catch(e){document.getElementById('buddy-list').innerHTML='Error: '+e;}"
    "}"
    "function bRender(){"
    "  var h='';"
    "  _b.forEach(function(b,i){"
    "    h+='<div class=\"buddy-row\">'"
    "      +'<input type=\"text\" name=\"hex\" maxlength=\"6\" value=\"'+b.hex+'\" placeholder=\"HEX ID\" oninput=\"_b['+i+'].hex=this.value.toUpperCase()\">'"
    "      +'<input type=\"text\" name=\"nm\" value=\"'+b.name+'\" placeholder=\"Name\" oninput=\"_b['+i+'].name=this.value\">'"
    "      +'<button class=\"btn btn-danger btn-sm\" onclick=\"bDel('+i+')\">&#x2715;</button>'"
    "      +'</div>';"
    "  });"
    "  if(!_b.length)h='<p class=\"info-text\">No buddies yet.</p>';"
    "  document.getElementById('buddy-list').innerHTML=h;"
    "}"
    "function buddyAdd(){_b.push({hex:'',name:''});bRender();}"
    "function bDel(i){_b.splice(i,1);bRender();}"
    "async function buddySave(){"
    "  var v=_b.filter(function(b){return b.hex.length===6&&b.name.trim().length>0;});"
    "  var s=document.getElementById('buddy-status');"
    "  s.innerText='Saving...';s.className='status-ok';"
    "  try{"
    "    var r=await fetch('/api/buddies',{method:'POST',"
    "      headers:{'Content-Type':'application/json'},body:JSON.stringify(v)});"
    "    s.innerText=await r.text();await bLoad();"
    "  }catch(e){s.className='status-err';s.innerText='Error: '+e;}"
    "}"
    "bLoad();"
    "</script>"
  ));

  sendPageFoot();
}

/* ═══════════════════════════════════════════════════════════════════════════
   BLE Manager page  (/ble/manage)
   ═══════════════════════════════════════════════════════════════════════════ */

void handleBLEManage() {
  beginPage("BLE Manager", "ble");

  server.sendContent_P(PSTR(
    "<div class='card'>"
    "<div class='card-title'>BLE Device Manager</div>"
    "<p class='info-text'>Scan for nearby BLE devices and manage the allowed list.</p>"
    "<button class='btn btn-primary btn-sm' onclick='bleScan()'>&#x1F50D; Scan Devices</button>"
    "<div id='ble-scan' style='margin-top:12px'></div>"
    "</div>"
    "<div class='card'>"
    "<div class='card-title'>Allowed Devices</div>"
    "<div id='ble-allowed'>Loading...</div>"
    "</div>"
    "<script>"
    "async function bleScan(){"
    "  var d=document.getElementById('ble-scan');"
    "  d.innerHTML='<em>Scanning...</em>';"
    "  try{"
    "    var r=await fetch('/ble/scan');"
    "    var names=await r.json();"
    "    if(!names.length){d.innerHTML='No devices found.';return;}"
    "    d.innerHTML='';"
    "    names.forEach(function(n){"
    "      var el=document.createElement('div');el.className='buddy-row';"
    "      el.innerHTML='<span style=\"flex:1\">'+n+'</span>'"
    "        +'<button class=\"btn btn-success btn-sm\" onclick=\"bleAdd(\\''+ n +'\\')\">"
    "+ Add</button>';"
    "      d.appendChild(el);"
    "    });"
    "  }catch(e){d.innerHTML='Error: '+e;}"
    "}"
    "async function bleLoad(){"
    "  var d=document.getElementById('ble-allowed');"
    "  try{"
    "    var r=await fetch('/ble/allowed');"
    "    var names=await r.json();"
    "    if(!names.length){d.innerHTML='<em>No devices in allowed list.</em>';return;}"
    "    d.innerHTML='';"
    "    names.forEach(function(n){"
    "      var el=document.createElement('div');el.className='buddy-row';"
    "      el.innerHTML='<span style=\"flex:1\">'+n+'</span>'"
    "        +'<button class=\"btn btn-danger btn-sm\" onclick=\"bleDel(\\''+ n +'\\')\">"
    "&#x1F5D1;</button>';"
    "      d.appendChild(el);"
    "    });"
    "  }catch(e){d.innerHTML='Error: '+e;}"
    "}"
    "async function bleAdd(n){"
    "  await fetch('/ble/add?name='+encodeURIComponent(n),{method:'POST'});"
    "  bleLoad();"
    "}"
    "async function bleDel(n){"
    "  await fetch('/ble/delete?name='+encodeURIComponent(n),{method:'POST'});"
    "  bleLoad();"
    "}"
    "bleLoad();"
    "</script>"
  ));

  sendPageFoot();
}

/* ═══════════════════════════════════════════════════════════════════════════
   Firmware update page  (/firmware)
   ═══════════════════════════════════════════════════════════════════════════ */

void handleFirmware() {
  SoC->swSer_enableRx(false);
  server.sendHeader(F("Connection"), F("close"));
  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));

  beginPage("Firmware Update", "firmware");

  server.sendContent_P(PSTR(
    "<div class='card'>"
    "<div class='card-title'>OTA Firmware Update</div>"
    "<p class='info-text'>Select a <b>.bin</b> firmware file then click Update."
    " The device will restart automatically.</p>"
    "<script src='/jquery.min.js'></script>"
    "<form method='POST' action='#' enctype='multipart/form-data' id='fw-form'>"
    "  <input type='file' name='update' accept='.bin' style='margin:10px 0'>"
    "  <div class='actions'>"
    "    <button type='submit' class='btn btn-primary'>&#x1F504; Update Firmware</button>"
    "  </div>"
    "</form>"
    "<div id='fw-prg' style='margin-top:12px;font-weight:600'></div>"
    "</div>"
    "<script>"
    "$('#fw-form').submit(function(e){"
    "  e.preventDefault();"
    "  var data=new FormData($('#fw-form')[0]);"
    "  var p=$('#fw-prg');"
    "  $.ajax({url:'/update',type:'POST',data:data,"
    "    contentType:false,processData:false,"
    "    xhr:function(){"
    "      var xhr=new window.XMLHttpRequest();"
    "      xhr.upload.addEventListener('progress',function(evt){"
    "        if(evt.lengthComputable)"
    "          p.html('Uploading: '+Math.round(evt.loaded/evt.total*100)+'%');"
    "      },false);return xhr;"
    "    },"
    "    success:function(){p.html('Done! Rebooting...');},"
    "    error:function(){p.html('Update failed.');}"
    "  });"
    "});"
    "</script>"
  ));

  sendPageFoot();
  SoC->swSer_enableRx(true);
}

/* ═══════════════════════════════════════════════════════════════════════════
   About page  (/about)
   ═══════════════════════════════════════════════════════════════════════════ */

void handleAbout() {
  beginPage("About SkyView", "about");

  server.sendContent_P(PSTR(
    "<div class='card'>"
    "<div class='card-title'>About SkyView</div>"
    "<p class='info-text'>Traffic awareness device for aviation &mdash; FLARM/GDL90 on AMOLED.</p>"
    "<div class='row'><label>Project</label>"
    "  <span class='val'><a href='http://github.com/lyusupov/SoftRF'>SoftRF</a></span></div>"
    "<div class='row'><label>Original author</label><span class='val'>Linar Yusupov</span></div>"
    "<div class='row'><label>This version</label>"
    "  <span class='val'><a href='https://github.com/moshe-braner/SoftRF'>Moshe Braner</a></span></div>"
    "<div class='row'><label>AMOLED conversion</label><span class='val'>Vlad Belayev &mdash; 2025</span></div>"
    "</div>"
    "<div class='card'>"
    "<div class='card-title'>Credits</div>"
    "<div class='row'><label>Ivan Grokhotkov</label><span class='val' style='font-size:13px'>Arduino core for ESP8266</span></div>"
    "<div class='row'><label>Paul Stoffregen</label><span class='val' style='font-size:13px'>Arduino Time library</span></div>"
    "<div class='row'><label>Mikal Hart</label><span class='val' style='font-size:13px'>TinyGPS++ library</span></div>"
    "<div class='row'><label>Hristo Gochkov</label><span class='val' style='font-size:13px'>Arduino core for ESP32</span></div>"
    "<div class='row'><label>Bodmer</label><span class='val' style='font-size:13px'>TFT_eSPI graphics library</span></div>"
    "<div class='row'><label>Benoit Blanchon</label><span class='val' style='font-size:13px'>ArduinoJson library</span></div>"
    "<div class='row'><label>Ryan David</label><span class='val' style='font-size:13px'>GDL90 decoder</span></div>"
    "<div class='row'><label>h2zero (Ryan Powell)</label><span class='val' style='font-size:13px'>NimBLE-Arduino</span></div>"
    "<div class='row'><label>LewisHe</label><span class='val' style='font-size:13px'>SensorsLib &amp; XPowersLib</span></div>"
    "<div class='row'><label>Brian Park</label><span class='val' style='font-size:13px'>AceButton library</span></div>"
    "<div class='row'><label>LilyGO / Waveshare</label><span class='val' style='font-size:13px'>AMOLED 1.75\" display</span></div>"
    "</div>"
    "<p style='font-size:12px;color:#6b7280;text-align:center;margin-top:8px'>"
    "Copyright &copy; 2019&ndash;2022 Linar Yusupov</p>"
  ));

  sendPageFoot();
}

void handleNotFound() {
  String message = "File Not Found\n\nURI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  server.send(404, "text/plain", message);
}

/* ═══════════════════════════════════════════════════════════════════════════
   Web_setup — route registration
   ═══════════════════════════════════════════════════════════════════════════ */
#include <vector>
#include <FS.h>
#include <SPIFFS.h>
#include <LittleFS.h>

static int countAudioFiles(const char *dir)
{
  int n = 0;
  File d = LittleFS.open(dir);
  if (d && d.isDirectory()) {
    for (File f = d.openNextFile(); f; f = d.openNextFile()) {
      if (!f.isDirectory()) n++;
    }
  }
  return n;
}

void Web_setup()
{
  if (!SPIFFS.begin(true))
    Serial.println("SPIFFS Mount Failed.");
  Serial.println("SPIFFS mounted successfully");

  server.on("/",             handleRoot);
  server.on("/settings",     handleSettings);
  server.on("/input",        handleInput);
  server.on("/uploads",      handleUploads);
  server.on("/buddy-manager", handleBuddyManager);
  server.on("/ble/manage",   HTTP_GET, handleBLEManage);
  server.on("/firmware",     HTTP_GET, handleFirmware);
  server.on("/about",        handleAbout);

  /* Legacy redirects */
  server.on("/buddylist",  []() { server.sendHeader("Location", "/uploads", true); server.send(302); });
  server.on("/demofile",   []() { server.sendHeader("Location", "/uploads", true); server.send(302); });
  server.on("/puretrackdb",[]() { server.sendHeader("Location", "/uploads", true); server.send(302); });

  /* ── Buddy list API ── */
  server.on("/upload-buddy", HTTP_POST, []() {
    server.send(200, "text/plain", "Buddy list uploaded successfully");
  }, []() {
    HTTPUpload& upload = server.upload();
    static File fsUploadFile;
    if      (upload.status == UPLOAD_FILE_START) fsUploadFile = SPIFFS.open("/buddylist.txt", FILE_WRITE);
    else if (upload.status == UPLOAD_FILE_WRITE && fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
    else if (upload.status == UPLOAD_FILE_END)  { if (fsUploadFile) fsUploadFile.close(); BuddyManager::readBuddyList(); }
  });

  server.on("/download-buddy", HTTP_GET, []() {
    File file = SPIFFS.open("/buddylist.txt", "r");
    if (!file || file.isDirectory()) { server.send(404, "text/plain", "File not found"); return; }
    server.streamFile(file, "text/plain");
    file.close();
  });

  server.on("/api/buddies", HTTP_GET, []() {
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();
    File file = SPIFFS.open("/buddylist.txt", "r");
    if (file && !file.isDirectory()) {
      while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
          int ci = line.indexOf(',');
          if (ci > 0) {
            JsonObject obj = arr.createNestedObject();
            obj["hex"]  = line.substring(0, ci);
            obj["name"] = line.substring(ci + 1);
          }
        }
      }
      file.close();
    }
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  });

  server.on("/api/buddies", HTTP_POST, []() {
    if (!server.hasArg("plain")) { server.send(400, "text/plain", "No data"); return; }
    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, server.arg("plain"))) { server.send(400, "text/plain", "Invalid JSON"); return; }
    File file = SPIFFS.open("/buddylist.txt", FILE_WRITE);
    if (!file) { server.send(500, "text/plain", "File error"); return; }
    int count = 0;
    for (JsonObject obj : doc.as<JsonArray>()) {
      String hex  = obj["hex"].as<String>();  hex.toUpperCase();
      String name = obj["name"].as<String>();
      if (hex.length() == 6 && name.length() > 0) { file.print(hex); file.print(","); file.println(name); count++; }
    }
    file.close();
    BuddyManager::readBuddyList();
    server.send(200, "text/plain", "Saved " + String(count) + " buddies");
  });

  /* ── Demo file ── */
  server.on("/upload-demo", HTTP_POST, []() {
    server.send(200, "text/plain", "Demo file uploaded successfully");
  }, []() {
    HTTPUpload& upload = server.upload();
    static File fsUploadFile;
    if (upload.status == UPLOAD_FILE_START) {
      if (SPIFFS.exists("/demo.nmea")) SPIFFS.remove("/demo.nmea");
      fsUploadFile = SPIFFS.open("/demo.nmea", FILE_WRITE);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      yield();
      if (fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (fsUploadFile) fsUploadFile.close();
    }
  });

  server.on("/download-demo", HTTP_GET, []() {
    File file = SPIFFS.open("/demo.nmea", "r");
    if (!file || file.isDirectory()) { server.send(404, "text/plain", "Demo file not found"); return; }
    server.streamFile(file, "text/plain");
    file.close();
  });

  /* ── PureTrack database ── */
  server.on("/upload-puretrack", HTTP_POST, []() {
    server.send(200, "text/plain", "PureTrack database uploaded. Restart to apply.");
  }, []() {
    HTTPUpload& upload = server.upload();
    static File fsUploadFile;
    if (upload.status == UPLOAD_FILE_START) {
      if (SPIFFS.exists("/puretrack.cdb")) SPIFFS.remove("/puretrack.cdb");
      fsUploadFile = SPIFFS.open("/puretrack.cdb", FILE_WRITE);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      yield();
      if (fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (fsUploadFile) fsUploadFile.close();
    }
  });

  server.on("/download-puretrack", HTTP_GET, []() {
    File file = SPIFFS.open("/puretrack.cdb", "r");
    if (!file || file.isDirectory()) { server.send(404, "text/plain", "PureTrack database not found"); return; }
    server.sendHeader("Content-Disposition", "attachment; filename=\"puretrack.cdb\"");
    server.streamFile(file, "application/octet-stream");
    file.close();
  });

  /* ── Voice audio files on the internal flash "audio" partition ── */
  server.on("/api/audio", HTTP_GET, []() {
    bool mounted = ESP32_Audio_flash_begin();
    DynamicJsonDocument doc(256);
    doc["source"]  = ESP32_Audio_source();
    doc["mounted"] = mounted;
    doc["total"]   = mounted ? LittleFS.totalBytes() : 0;
    doc["used"]    = mounted ? LittleFS.usedBytes()  : 0;
    doc["voice1"]  = mounted ? countAudioFiles("/Audio/voice1") : 0;
    doc["voice2"]  = mounted ? countAudioFiles("/Audio/voice2") : 0;
    doc["voice3"]  = mounted ? countAudioFiles("/Audio/voice3") : 0;
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  });

  // one WAV per request; target path comes from the query string, e.g. ?path=/Audio/voice1/0.wav
  static String audioUploadPath;
  static String audioUploadError;
  server.on("/upload-audio", HTTP_POST, []() {
    if (audioUploadError.length()) server.send(500, "text/plain", audioUploadError);
    else                           server.send(200, "text/plain", "OK");
  }, []() {
    HTTPUpload& upload = server.upload();
    static File audioFile;
    if (upload.status == UPLOAD_FILE_START) {
      audioUploadPath  = server.arg("path");
      audioUploadError = "";
      if (!ESP32_Audio_flash_begin()) {
        audioUploadError = "No audio partition - flash the full firmware via USB first";
      } else if (!audioUploadPath.startsWith(WAV_FILE_PREFIX) || audioUploadPath.indexOf("..") >= 0 ||
                 !audioUploadPath.endsWith(WAV_FILE_SUFFIX) || audioUploadPath.length() >= MAX_FILENAME_LEN) {
        audioUploadError = "Invalid path: " + audioUploadPath;
      } else {
        audioFile = LittleFS.open(audioUploadPath, FILE_WRITE, true);  // creates /Audio/voiceN as needed
        if (!audioFile) audioUploadError = "Cannot create " + audioUploadPath;
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (audioFile && audioFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
        audioFile.close();
        LittleFS.remove(audioUploadPath);
        audioUploadError = "Flash full while writing " + audioUploadPath;
      }
      yield();
    } else if (upload.status == UPLOAD_FILE_END) {
      if (audioFile) audioFile.close();
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      if (audioFile) { audioFile.close(); LittleFS.remove(audioUploadPath); }
      audioUploadError = "Upload aborted";
    }
  });

  server.on("/audio-erase", HTTP_POST, []() {
    if (ESP32_Audio_flash_erase()) server.send(200, "text/plain", "Flash audio erased.");
    else                           server.send(500, "text/plain", "Erase failed (no audio partition?)");
  });

  /* ── BLE manager API ── */
  server.on("/ble/scan",    HTTP_GET,  handleBLEScan);
  server.on("/ble/add",     HTTP_POST, handleAddBLEDevice);
  server.on("/ble/delete",  HTTP_POST, handleDeleteBLEDevice);
  server.on("/ble/allowed", HTTP_GET, []() {
    auto allowed = getAllowedBLENameList();
    DynamicJsonDocument doc(1024);
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& name : allowed) arr.add(name);
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
  });

  /* ── Battery log ── */
  server.on("/download_log", HTTP_GET, []() {
    File f = SPIFFS.open("/battery_log.txt", "r");
    if (!f) { server.send(404, "text/plain", "Log not found"); return; }
    server.sendHeader("Content-Disposition", "attachment; filename=\"battery_log.txt\"");
    server.streamFile(f, "text/plain");
    f.close();
  });

  server.on("/clear_log", HTTP_POST, []() {
    if (SPIFFS.exists("/battery_log.txt")) SPIFFS.remove("/battery_log.txt");
    server.sendHeader("Location", "/", true);
    server.send(302);
  });

  server.on("/inline", []() { server.send(200, "text/plain", "this works as well"); });

  /* ── OTA update ── */
  server.on("/update", HTTP_POST, []() {
    SoC->swSer_enableRx(false);
    server.sendHeader(F("Connection"), F("close"));
    server.sendHeader(F("Access-Control-Allow-Origin"), "*");
    server.send(200, F("text/plain"), (Update.hasError()) ? "FAIL" : "OK");
    delay(1000);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);
      SoC->WiFiUDP_stopAll();
      SoC->WDT_fini();
      Serial.printf("Update: %s\r\n", upload.filename.c_str());
      if (!Update.begin(SoC->maxSketchSpace())) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) Serial.printf("Update Success: %u\r\n", upload.totalSize);
      else Update.printError(Serial);
      Serial.setDebugOutput(false);
    }
    yield();
  });

#if !defined(NOLOGO)
  server.on("/logo.png", []() { server.send_P(200, "image/png", Logo, sizeof(Logo)); });
#endif

  server.on("/jquery.min.js", []() {
    PGM_P content = jquery_min_js_gz;
    size_t bytes_left = jquery_min_js_gz_len;
    size_t chunk_size;
    server.setContentLength(bytes_left);
    server.sendHeader(F("Content-Encoding"), F("gzip"));
    server.send(200, F("application/javascript"), "");
    do {
      chunk_size = bytes_left > JS_MAX_CHUNK_SIZE ? JS_MAX_CHUNK_SIZE : bytes_left;
      server.sendContent_P(content, chunk_size);
      content += chunk_size;
      bytes_left -= chunk_size;
    } while (bytes_left > 0);
  });

  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println(F("HTTP server has started at port: 80"));
  delay(1000);
}

void Web_loop() { server.handleClient(); }
void Web_fini()  { server.stop(); }
