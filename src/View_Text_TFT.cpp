#include "TFTHelper.h"
#include "View_Text_TFT.h"

// #include <Fonts/FreeMonoBold12pt7b.h>
// #include <Fonts/FreeMonoBold18pt7b.h>

#include <TimeLib.h>

#include "TrafficHelper.h"
#include "EEPROMHelper.h"
#include "NMEAHelper.h"
#include "GDL90Helper.h"



unsigned long Battery_TimeMarker = 0;

#include "SkyView.h"
#include "SoCHelper.h"
#include "BuddyHelper.h"
#include "Speed.h"
#include "altitude2.h"
#include "aircrafts.h"
#include "settings.h"

// Aircraft type icons
#include "../include/glider_icon.h"      // Type 1: Glider
#include "../include/pg_icon.h"          // Type 7: Paraglider
#include "../include/lightplane_icon.h"  // Type 2, 8: Towplane, Powered aircraft

// Aircraft icons array - maps aircraft type to icon bitmap
// Index corresponds to AIRCRAFT_TYPE enum values
const unsigned short* aircrafts_icon[] = {
  NULL,              // 0: AIRCRAFT_TYPE_UNKNOWN - no icon
  glider_icon,       // 1: AIRCRAFT_TYPE_GLIDER
  lightplane_icon,   // 2: AIRCRAFT_TYPE_TOWPLANE
  NULL,              // 3: AIRCRAFT_TYPE_HELICOPTER - TODO: add helicopter_icon
  NULL,              // 4: AIRCRAFT_TYPE_PARACHUTE - no icon
  NULL,              // 5: AIRCRAFT_TYPE_DROPPLANE - no icon
  NULL,              // 6: AIRCRAFT_TYPE_HANGGLIDER - no icon
  pg_icon,           // 7: AIRCRAFT_TYPE_PARAGLIDER
  lightplane_icon,   // 8: AIRCRAFT_TYPE_POWERED
  NULL,              // 9: AIRCRAFT_TYPE_JET - no icon
  NULL,              // 10: AIRCRAFT_TYPE_UFO - TODO: add bigplane_icon
  NULL,              // 11: AIRCRAFT_TYPE_BALLOON - no icon
  NULL,              // 12: AIRCRAFT_TYPE_ZEPPELIN - no icon
  NULL,              // 13: AIRCRAFT_TYPE_UAV - no icon
  NULL,              // 14: AIRCRAFT_TYPE_RESERVED - no icon
  NULL               // 15: AIRCRAFT_TYPE_STATIC - no icon
};

extern xSemaphoreHandle spiMutex;
// extern uint16_t read_voltage();
extern TFT_eSPI tft;
extern TFT_eSprite sprite;
extern TFT_eSprite bearingSprite;
TFT_eSprite TextPopSprite = TFT_eSprite(&tft);


// ===== UI Element IDs =====
enum ElementID {
  ELEM_ID_STRING = 0,
  ELEM_ACFT_TYPE,
  ELEM_NAME_STRING,
  ELEM_RADIO_SIGNAL,      // Radio signal icon (replaces LastSeen)
  ELEM_BATTERY,
  ELEM_BATTERY_PCT,
  ELEM_VERTICAL,
  ELEM_CLIMBRATE_LABEL,
  ELEM_CLIMBRATE,
  ELEM_ALTITUDE,
  ELEM_DISTANCE,
  ELEM_LOCK,
  ELEM_AIRCRAFT_COUNT,
  ELEM_COUNT  // Total number of elements
};

// ===== Structure to hold element positions and bounding boxes =====
struct UIElement {
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
  const char* name;
};

// ===== Initialize element positions (based on test sketch) =====
UIElement elements[ELEM_COUNT] = {
  {287, 130, 120, 26, "ID"},           // ID string
  {95, 117, 60, 52, "AircraftType"},   // Aircraft type icon
  {110, 100, 200, 30, "Name"},         // Name string
  {210, 116, 40, 60, "RadioSignal"},   // Radio signal icon (replaces LastSeen)
  {186, 33, 60, 60, "Battery"},        // Battery indicator
  {230, 33, 60, 26, "BattPct"},        // Battery percentage
  {30, 213, 100, 50, "Vertical"},      // Vertical speed number
  {327, 180, 120, 26, "ClimbLabel"},   // "Climbrate" label
  {425, 213, 100, 50, "Climbrate"},    // Climbrate value (right-aligned)
  {120, 276, 60, 26, "Altitude"},       // Altitude value
  {205, 300, 150, 50, "Distance"},     // Distance value
  {361, 95, 30, 30, "Lock"},           // Lock icon
  {239, 382, 60, 40, "AircraftCnt"}    // Aircraft count
};

uint8_t TFT_current = 1;
uint8_t pages =0;
bool isLocked = false;
uint16_t lock_color = TFT_LIGHTGREY;
uint_fast16_t text_color = TFT_WHITE;
// Size of the rolling buffer
const int CLIMB_BUFFER_SIZE = 10;

// Array to hold last 10 readings
float climbRateBuffer[CLIMB_BUFFER_SIZE] = {0};

// Position to insert next reading
int climbIndex = 0;

// Count of valid readings so far (for startup period)
int climbCount = 0;

bool show_avg_vario = false;

// extern buddy_info_t buddies[];
const char* buddy_name = " ";
static int32_t focusOn = 0;
static bool dimmed = false;

void addClimbRate(float newRate) {
    // Store new rate at current index
    climbRateBuffer[climbIndex] = newRate;

    // Advance index (wrap around at 10)
    climbIndex = (climbIndex + 1) % CLIMB_BUFFER_SIZE;

    // Increase count until buffer is full
    if (climbCount < CLIMB_BUFFER_SIZE) {
        climbCount++;
    }
}

// Call this to get average climb rate
float getAverageClimbRate() {
    if (climbCount == 0) return 0; // Avoid division by zero

    float sum = 0;
    for (int i = 0; i < climbCount; i++) {
        sum += climbRateBuffer[i];
    }
    return sum / climbCount;
}

void resetClimbRateBuffer() {
    for (int i = 0; i < CLIMB_BUFFER_SIZE; i++) {
        climbRateBuffer[i] = 0;
    }
    climbIndex = 0;
    climbCount = 0;
}

void setFocusOn(bool on, uint32_t id) {
  if (on) {
    if (id !=0) {
      focusOn = id;
    } else {
      focusOn = traffic[TFT_current - 1].fop->ID;
    }
  Serial.printf("Focus on ID: %02X%02X%02X\n", (focusOn >> 16) & 0xFF, (focusOn >> 8) & 0xFF, (focusOn) & 0xFF);      // Extract low byte
  } else {
    focusOn = 0;
  }
}

// Toggle state for RSSI display mode (false = icon, true = numeric)
static bool rssiShowNumeric = false;

// Function to toggle RSSI display mode
void toggleRssiDisplay() {
  rssiShowNumeric = !rssiShowNumeric;
  extern unsigned long TFTTimeMarker;
  TFTTimeMarker = 0; // Force update of the display
}

static void draw_radioSignal(int rssi, int lastseen) {
  // Get position from elements array
  int16_t radio_x = elements[ELEM_RADIO_SIGNAL].x;
  int16_t radio_y = elements[ELEM_RADIO_SIGNAL].y;

  //Determine wave color based on RSSI signal strength
  uint16_t wave_color;
  if (rssi >= -70) {
    wave_color = TFT_GREEN;  // Strong signal
  } else if (rssi >= -85) {
    wave_color = TFT_YELLOW; // Medium signal
  } else if (rssi >= -100) {
    wave_color = TFT_ORANGE; // Weak signal
  } else {
    wave_color = TFT_DARKGREY; // No signal
  }
  //grey out waves if last seen > 4s
  if (lastseen > 4) {
    wave_color = TFT_DARKGREY;
  }

  // Display either numeric value or icon based on toggle state
  if (rssiShowNumeric) {
    // Display numeric RSSI value
    sprite.setTextColor(wave_color, TFT_BLACK);
    sprite.setFreeFont(&Orbitron_Light_24);
    sprite.setTextDatum(TR_DATUM); 
    // Draw RSSI value
    char rssiStr[8];
    snprintf(rssiStr, sizeof(rssiStr), "%d", rssi);
    sprite.drawString(rssiStr, radio_x + 16, radio_y + 10);

    // Draw "dB" label in font 4
    sprite.setTextDatum(TL_DATUM);
    sprite.drawString("dB", radio_x + 22, radio_y + 15, 4);

    // Update element dimensions for touch area
    elements[ELEM_RADIO_SIGNAL].width = 40;
    elements[ELEM_RADIO_SIGNAL].height = 60;
  } else {
    // Draw icon (original code)
    // Draw antenna base (vertical line)
    sprite.fillRect(radio_x + 18, radio_y + 25, 4, 20, TFT_WHITE);
    // Draw antenna base plate
    sprite.fillRect(radio_x + 12, radio_y + 43, 16, 2, TFT_WHITE);

    // Draw symmetrical HORIZONTAL radio waves on BOTH sides
    // RIGHT SIDE waves (arcs from 45° to 135° - horizontal emission to the left)
    // Inner wave (smallest)
    sprite.drawArc(radio_x + 20, radio_y + 23, 10, 7, 45, 135, rssi >= -100 ? wave_color : TFT_DARKGREY, TFT_BLACK, true);
    // Middle wave
    sprite.drawArc(radio_x + 20, radio_y + 23, 20, 17, 45, 135, rssi >= -85 ? wave_color : TFT_DARKGREY, TFT_BLACK, true);
    // Outer wave (largest)
    sprite.drawArc(radio_x + 20, radio_y + 23, 30, 27, 45, 135, rssi >= -60 ? wave_color : TFT_DARKGREY, TFT_BLACK, true);

    // LEFT SIDE waves (arcs from 225° to 315° - horizontal emission to the right)
    // Inner wave (smallest)
    sprite.drawArc(radio_x + 20, radio_y + 23, 10, 7, 225, 315, rssi >= -100 ? wave_color : TFT_DARKGREY, TFT_BLACK, true);
    // Middle wave
    sprite.drawArc(radio_x + 20, radio_y + 23, 20, 17, 225, 315, rssi >= -85 ? wave_color : TFT_DARKGREY, TFT_BLACK, true);
    // Outer wave (largest)
    sprite.drawArc(radio_x + 20, radio_y + 23, 30, 27, 225, 315, rssi >= -60 ? wave_color : TFT_DARKGREY, TFT_BLACK, true);

    // Draw antenna tip (small circle)
    sprite.fillCircle(radio_x + 20, radio_y + 23, 3, TFT_WHITE);
    //cross radio signal icon if no signal for more than 7 seconds
    if (lastseen > 7) {
      sprite.drawLine(radio_x + 5, radio_y + 10, radio_x + 35, radio_y + 50, TFT_RED);
      sprite.drawLine(radio_x + 35, radio_y + 10, radio_x + 5, radio_y + 50, TFT_RED);
    }

    // Update element dimensions for touch area
    elements[ELEM_RADIO_SIGNAL].width = 40;
    elements[ELEM_RADIO_SIGNAL].height = 60;
  }

}


void TFT_draw_text() {
  int j=0;
  int bearing;
  uint16_t bud_color = TFT_WHITE;
  uint_fast16_t text_color = TFT_WHITE;
  // char info_line [TEXT_VIEW_LINE_LENGTH];
  char id2_text  [TEXT_VIEW_LINE_LENGTH];
  lock_color = TFT_LIGHTGREY;
  bool isFocused = false;
  for (int i=0; i < MAX_TRACKING_OBJECTS; i++) {
    if (Container[i].ID && (now() - Container[i].timestamp) <= ENTRY_EXPIRATION_TIME) {

      traffic[j].fop = &Container[i];
      traffic[j].distance = Container[i].distance;
      // traffic[j].altitude = (int)((float)Container[i].altitude * 3.0);
      traffic[j].altitude = traffic[j].fop->altitude;
      traffic[j].climbrate = Container[i].ClimbRate;
      traffic[j].acftType = Container[i].AcftType;
      traffic[j].rssi = Container[i].rssi;
      traffic[j].lastSeen = now() - Container[i].timestamp;
      traffic[j].timestamp = Container[i].timestamp;
      if (focusOn) {
        if (traffic[j].fop->ID == focusOn) {
          lock_color = TFT_GREEN;
          TFT_current = j + 1;
          // Serial.printf("Found focused target, setting page to %d\n", TFT_current);
          isFocused = true;
          dimmed = false;
        }
      }
      j++;
    }
  }
  if (!focusOn) {
    focusOn = traffic[TFT_current - 1].fop->ID;
    resetClimbRateBuffer();
    Serial.printf("Focus on ID: %02X%02X%02X\n", (focusOn >> 16) & 0xFF, (focusOn >> 8) & 0xFF, (focusOn) & 0xFF);      // Extract low byte
  }
  if ((now() - traffic[TFT_current - 1].timestamp) >= TFT_EXPIRATION_TIME) {
    // if expired, greyout and dim display
      resetClimbRateBuffer();
      text_color = TFT_DARKGREY;
      bud_color = TFT_DARKGREY;
      lock_color = TFT_DARKGREY;
      if (!dimmed) {
        dimmed = true;
      }
  }
  // Serial.printf("TFT_current: %d, pages: %d, isFocused: %d\n", TFT_current, j, isFocused);
  // uint_fast16_t lastSeen = now() -  Container[i].timestamp;
  if (j > 0) {
    // uint8_t db;
    // const char *u_dist, *u_alt, *u_spd;
    // char id2_text[20];
    pages = j;
    // Serial.printf("TFT_current: %d, pages: %d\n", TFT_current, pages);
    // snprintf(id2_text, sizeof(id_text), "%02X%02X%02X", (traffic[TFT_current - 1].fop->ID >> 16) & 0xFF, (traffic[TFT_current - 1].fop->ID >> 8) & 0xFF, (traffic[TFT_current - 1].fop->ID) & 0xFF);      // Extract low byte
    snprintf(id2_text, sizeof(id2_text), "ID: %02X%02X%02X", (traffic[TFT_current - 1].fop->ID >> 16) & 0xFF, (traffic[TFT_current - 1].fop->ID >> 8) & 0xFF, (traffic[TFT_current - 1].fop->ID) & 0xFF);      // Extract low byte
   
    if (TFT_current > j) {
      TFT_current = j;
    
    }
    bearing = (int) (traffic[TFT_current - 1].fop->RelativeBearing);  // relative to our track

    if (bearing < 0)    bearing += 360;
    if (bearing > 360)  bearing -= 360;

    // Priority: 1. Buddy name (green), 2. Database label (cyan), 3. Hex ID (white)
    buddy_name = BuddyManager::findBuddyName(traffic[TFT_current - 1].fop->ID);
    if (buddy_name) {
      bud_color = TFT_GREEN;
    } else {
#if defined(DB)
      // Try aircraft database lookup (PureTrack, OGN, etc.)
      static char db_label[32];
      db_label[0] = '\0';
      if (settings->adb != DB_NONE && SoC->DB_query) {
        int result = SoC->DB_query(settings->adb,
                                   traffic[TFT_current - 1].fop->ID,
                                   db_label, sizeof(db_label),
                                   NULL, 0);
        if (result > 0 && strlen(db_label) > 0) {
          buddy_name = db_label;
          bud_color = TFT_CYAN;
        } else {
          buddy_name = id2_text;
        }
      } else {
        buddy_name = id2_text;
      }
#else
      buddy_name = id2_text;
#endif
    }
    
    float alt_mult = 1.0;
    switch (settings->units) {
      case UNITS_METRIC:
          alt_mult = 1.0;
          break;
      case UNITS_IMPERIAL:
      case UNITS_MIXED: 
          alt_mult = 3.28084;
          break;
      default:
        alt_mult = 3.28084;
          break;
  }
    int vertical = (int) traffic[TFT_current - 1].fop->RelativeVertical;
    int disp_alt = (int)((vertical + ThisAircraft.altitude) * alt_mult);  //converting meter to feet
    // float traffic_vario = (traffic[TFT_current - 1].climbrate);
    addClimbRate(traffic[TFT_current - 1].climbrate);
    float traffic_avg_vario = getAverageClimbRate();
    float traffic_vario = 0.0;
    if (show_avg_vario) {
      traffic_vario = (traffic_avg_vario == 0 ? (traffic[TFT_current - 1].climbrate) : traffic_avg_vario);
    } else {
      traffic_vario = traffic[TFT_current - 1].climbrate;
    }
    // Serial.printf("Average climb rate: %.2f\n", traffic_avg_vario);
    // Serial.printf("Current climb rate: %.2f\n", traffic_vario);
    // float speed = (traffic[TFT_current - 1].fop->GroundSpeed);


#if defined(DEBUG_CONTAINER)
    Serial.print(F(" ID="));
    Serial.print((traffic[TFT_current - 1].fop->ID >> 16) & 0xFF, HEX);
    Serial.print((traffic[TFT_current - 1].fop->ID >>  8) & 0xFF, HEX);
    Serial.print((traffic[TFT_current - 1].fop->ID      ) & 0xFF, HEX);

    Serial.println();
    // Serial.print(F(" RelativeNorth=")); Serial.println(traffic[TFT_current - 1].fop->RelativeNorth);
    // Serial.print(F(" RelativeEast="));  Serial.println(traffic[TFT_current - 1].fop->RelativeEast);
    // Serial.print(F(" RelativeBearing="));  Serial.println(traffic[TFT_current - 1].fop->RelativeBearing);
    // Serial.print(F(" RelativeVertical="));  Serial.println(traffic[TFT_current - 1].fop->RelativeVertical);
    Serial.print(F(" Altitude= "));  Serial.println(traffic[TFT_current - 1].altitude);
    Serial.print(F(" ClimbRate= "));  Serial.println(traffic[TFT_current - 1].climbrate);
    Serial.print(F(" Distance= "));  Serial.println(traffic[TFT_current - 1].fop->distance);
    

#endif
  sprite.setTextDatum(TL_DATUM);
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);

  // ===== Draw circular arc background =====
  sprite.drawSmoothArc(233, 233, 230, 225, 0, 360, TFT_DARKGREY, TFT_BLACK, true);

  // ===== Draw bearing arrow in center =====
  bearingSprite.fillSprite(TFT_BLACK);
  sprite.setPivot(233, 233);
  bearingSprite.fillRect(0, 12, 40, 30, text_color == TFT_GREY ? text_color : TFT_CYAN);
  bearingSprite.fillTriangle(40, 0, 40, 53, 78, 26, text_color == TFT_GREY ? text_color : TFT_CYAN);
  bearingSprite.setPivot(39, 27);
  bearingSprite.pushRotated(&sprite, bearing - 90);

  // ===== Draw ID string =====
  sprite.setTextColor(lock_color, TFT_BLACK);
  sprite.drawString(id2_text, elements[ELEM_ID_STRING].x, elements[ELEM_ID_STRING].y, 4);

  // ===== Draw Traffic Type (Icon type) =====
  uint8_t acft_type = traffic[TFT_current - 1].acftType;

  // Draw aircraft icon if available, otherwise fall back to text
  if (acft_type < sizeof(aircrafts_icon)/sizeof(aircrafts_icon[0]) && aircrafts_icon[acft_type] != NULL) {
    // Icon is 60x52, so we'll scale it down to fit the 30x30 space
    // For now, push at full size - you may want to adjust position
    sprite.setSwapBytes(true);
    sprite.pushImage(elements[ELEM_ACFT_TYPE].x, elements[ELEM_ACFT_TYPE].y, 60, 52, aircrafts_icon[acft_type]);
  } else {
    // Fallback: draw text label if no icon available
    sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    String type_label = (acft_type == 7) ? "PG" :
                       (acft_type == 6) ? "HG" :
                       (acft_type == 1) ? "G" :
                       (acft_type == 2) ? "TAG" :
                       (acft_type == 3) ? "H" :
                       (acft_type == 9) ? "A" :
                       String(acft_type);
    sprite.drawString(type_label, elements[ELEM_ACFT_TYPE].x, elements[ELEM_ACFT_TYPE].y, 4);
    sprite.drawSmoothRoundRect(elements[ELEM_ACFT_TYPE].x - 3, elements[ELEM_ACFT_TYPE].y - 10, 6, 5, 40, 40, TFT_WHITE);
  }

  // ===== Draw Buddy Name =====
  sprite.setFreeFont(&Orbitron_Light_32);
  sprite.setTextColor(bud_color, TFT_BLACK);
  sprite.setCursor(elements[ELEM_NAME_STRING].x, elements[ELEM_NAME_STRING].y);
  sprite.printf(buddy_name);
  // ===== Draw Radio Signal Icon (replaces Last Seen and progress bar) =====
  draw_radioSignal(traffic[TFT_current - 1].rssi, traffic[TFT_current - 1].lastSeen);

  // ===== Draw Vertical speed (left side) =====
  sprite.setTextDatum(TL_DATUM);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.drawNumber((int)(traffic[TFT_current - 1].fop->RelativeVertical) * alt_mult, elements[ELEM_VERTICAL].x, elements[ELEM_VERTICAL].y, 7);

  // Draw altitude icon
  sprite.setSwapBytes(true);
  sprite.pushImage(40, 160, 32, 32, altitude2);

  // ===== Draw Climbrate label and value (right side) =====
  sprite.setTextDatum(TL_DATUM);
  sprite.drawString(show_avg_vario ? "AV" : "", 290, 180, 4);  //add AV when average vario used
  sprite.drawString("Climbrate ", elements[ELEM_CLIMBRATE_LABEL].x, elements[ELEM_CLIMBRATE_LABEL].y, 4);

  sprite.setTextDatum(TR_DATUM);
  sprite.drawFloat(traffic_vario, 1, elements[ELEM_CLIMBRATE].x, elements[ELEM_CLIMBRATE].y, 7);

  sprite.setTextDatum(TL_DATUM);
  sprite.drawString("m", 430, 215, 4);
  sprite.drawString("s", 435, 240, 4);
  sprite.drawWideLine(430, 240, 450, 241, 3, text_color);

  // ===== Draw Altitude =====
  sprite.setTextDatum(TR_DATUM);
  sprite.setFreeFont(&Orbitron_Light_24);
  sprite.drawNumber(disp_alt, elements[ELEM_ALTITUDE].x, elements[ELEM_ALTITUDE].y);
  sprite.setTextDatum(TL_DATUM);
  sprite.drawString("amsl", 130, 280, 2);

  // ===== Draw Distance =====
  sprite.drawFloat((traffic[TFT_current - 1].fop->distance / 1000.0), 1, elements[ELEM_DISTANCE].x, elements[ELEM_DISTANCE].y, 7);
  sprite.drawString("km", 285, 300, 4);
    
  // ===== Draw vertical speed arc indicators =====
  if (vertical > 55) {
    sprite.drawSmoothArc(233, 233, 230, 225, 90, constrain(90 + vertical / 10, 90, 150), vertical > 150 ? TFT_CYAN : TFT_RED, TFT_BLACK, true);
    sprite.drawString("+", 15, 226, 7);
    sprite.drawWideLine(15, 236, 25, 236, 6, text_color); //draw plus
    sprite.drawWideLine(20, 231, 20, 241, 6, text_color);
  }
  else if (vertical < -55) {
    sprite.drawSmoothArc(233, 233, 230, 225, constrain(90 - abs(vertical) / 10, 30, 90), 90, vertical < -150 ? TFT_GREEN : TFT_RED, TFT_BLACK, true);
  }

  // ===== Draw climbrate arc indicators =====
  if (traffic_vario < -0.5) {
    sprite.drawSmoothArc(233, 233, 230, 225, 270, constrain(270 + abs(traffic_vario) * 12, 270, 360), traffic_vario < 2.5 ? TFT_BLUE : traffic_vario < 1 ? TFT_CYAN : TFT_GREEN, TFT_BLACK, true);
  }
  else if (traffic_vario > 0.3) { //exclude 0 case so not to draw 360 arch
    sprite.drawSmoothArc(233, 233, 230, 225, constrain(270 - abs(traffic_vario) * 12, 190, 270), 270, traffic_vario > 3.5 ? TFT_RED : traffic_vario > 2 ? TFT_ORANGE : TFT_YELLOW, TFT_BLACK, true);
  }

  // ===== Draw Lock icon =====
  if (focusOn) {
    int16_t lock_x = elements[ELEM_LOCK].x;
    int16_t lock_y = elements[ELEM_LOCK].y;
    sprite.fillSmoothRoundRect(lock_x, lock_y, 20, 20, 4, lock_color, TFT_BLACK);
    sprite.drawArc(lock_x + 9, lock_y, 9, 6, 90, 270, lock_color, TFT_BLACK, false);
  }

  // ===== Draw Aircraft count =====
  sprite.setSwapBytes(true);
  sprite.pushImage(190, 382, 32, 32, aircrafts);
  sprite.drawNumber(Traffic_Count(), elements[ELEM_AIRCRAFT_COUNT].x, elements[ELEM_AIRCRAFT_COUNT].y, 6);

  // ===== Draw battery indicators =====
  draw_battery();
  draw_extBattery();

  // ===== Draw settings icon =====
  sprite.setSwapBytes(true);
  sprite.pushImage(320, 360, 36, 36, settings_icon_small);

  // ===== Push sprite to display =====
  if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
    if (dimmed) {
      lcd_brightness(175);
    } else {
      lcd_brightness(225);
    }
    lcd_PushColors(6, 0, 466, 466, (uint16_t*)sprite.getPointer());
    xSemaphoreGive(spiMutex);
  } else {
    Serial.println("Failed to acquire SPI semaphore!");
  }
  } // Close if (j > 0) block
}

void TFT_text_Draw_Message(const char *msg1, const char *msg2)
{
    // int16_t  tbx, tby;
    // uint16_t tbw, tbh;
    // uint16_t x, y;

  if (msg1 != NULL && strlen(msg1) != 0) {
    // uint16_t radar_x = 0;
    // uint16_t radar_y = 466 / 2;
    // uint16_t radar_w = 466;
    sprite.setTextDatum(MC_DATUM);
    sprite.fillSprite(TFT_BLACK);
    sprite.setTextColor(TFT_CYAN, TFT_BLACK);

    if (msg2 == NULL) {
      sprite.drawString(msg1, LCD_WIDTH / 2, LCD_HEIGHT / 2, 4);
    } else {
      sprite.drawString(msg1, LCD_WIDTH / 2, LCD_HEIGHT / 2 - 26, 4);
      sprite.drawString(msg2, LCD_WIDTH / 2, LCD_HEIGHT / 2 + 26, 4);
    }
    //Battery indicator
    draw_battery();
    draw_extBattery();
    //draw settings icon
    sprite.setSwapBytes(true);
    sprite.pushImage(320, 360, 36, 36, settings_icon_small);
    if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
      lcd_brightness(0);
      lcd_PushColors(display_column_offset, 0, 466, 466, (uint16_t*)sprite.getPointer());
        for (int i = 0; i <= 255; i++)
        {
          lcd_brightness(i);
            delay(2);
        }
        delay(200);
        for (int i = 255; i >= 0; i--)
        {
          lcd_brightness(i);
            delay(2);
        }
      xSemaphoreGive(spiMutex);
  } else {
      Serial.println("Failed to acquire SPI semaphore!");
  }

  }
}

void TFT_text_loop()
{
  if (isTimeToDisplay()) {
    bool hasData = settings->protocol == PROTOCOL_NMEA  ? NMEA_isConnected()  :
                   settings->protocol == PROTOCOL_GDL90 ? GDL90_isConnected() :
                   false;

    if (hasData) {

      bool hasFix = settings->protocol == PROTOCOL_NMEA  ? isValidGNSSFix()   :
                    settings->protocol == PROTOCOL_GDL90 ? GDL90_hasOwnShip() :
                    false;

      if (hasFix) {
        if (Traffic_Count() > 0) {
          TFT_draw_text();
        } else {
          TFT_text_Draw_Message("NO", "TRAFFIC");
        }
      } else {
        TFT_text_Draw_Message(NO_FIX_TEXT, NULL);
      }
    } else {
      TFT_text_Draw_Message(NO_DATA_TEXT, NULL);
    }

    TFTTimeMarker = millis();
  }
}

void TFT_text_next()
{
  if (TFT_current < MAX_TRACKING_OBJECTS) {
    if (TFT_current < pages) {
      setFocusOn(false);
      TFT_current++;
    }
    else {
      setFocusOn(false);
      TFT_current = 1;
    }
    // Serial.print("TFT_current: ");
    // Serial.println(TFT_current);

    TextPopSprite.createSprite(110, 98);
    TextPopSprite.setSwapBytes(true);
    TextPopSprite.fillSprite(TFT_BLACK);
    TextPopSprite.setTextColor(TFT_CYAN, TFT_BLACK);
    TextPopSprite.setFreeFont(&Orbitron_Light_32);
    TextPopSprite.setCursor(0, 30);
    TextPopSprite.printf("PAGE");
    TextPopSprite.setTextDatum(BC_DATUM);
    TextPopSprite.drawNumber(TFT_current, 55, 86);

    
    // TextPopSprite.pushToSprite(&sprite, 50, 260, TFT_BLACK);
    if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
      lcd_PushColors(178, 174, 110, 98, (uint16_t*)TextPopSprite.getPointer());
      delay(100);
      xSemaphoreGive(spiMutex);
    } else {
      Serial.println("Failed to acquire SPI semaphore!");
    }
    
    // delay(500);
    TextPopSprite.deleteSprite();
  }
  resetClimbRateBuffer();
}


void TFT_text_prev()
{
  if (TFT_current > 1) {
    setFocusOn(false);
    TFT_current--;

  } else {
    setFocusOn(false);
    TFT_current = pages;
  }
  TextPopSprite.createSprite(110, 98);
  TextPopSprite.setSwapBytes(true);
  TextPopSprite.fillSprite(TFT_BLACK);
  TextPopSprite.setTextColor(TFT_CYAN, TFT_BLACK);
  TextPopSprite.setFreeFont(&Orbitron_Light_32);
  TextPopSprite.setCursor(0, 30);
  TextPopSprite.printf("PAGE");
  TextPopSprite.setTextDatum(BC_DATUM);
  TextPopSprite.drawNumber(TFT_current, 55, 86);

  if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
    lcd_PushColors(178, 174, 110, 98, (uint16_t*)TextPopSprite.getPointer());
    delay(100);
    xSemaphoreGive(spiMutex);
  } else {
    Serial.println("Failed to acquire SPI semaphore!");
  }
  // delay(500);
  TextPopSprite.deleteSprite();
  resetClimbRateBuffer();
}