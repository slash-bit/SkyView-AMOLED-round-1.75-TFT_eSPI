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
#include "BuddyHelper.h"
#include "Speed.h"
#include "altitude2.h"
#include "aircrafts.h"
#include "settings.h"

extern xSemaphoreHandle spiMutex;
// extern uint16_t read_voltage();
extern TFT_eSPI tft;
extern TFT_eSprite sprite;
extern TFT_eSprite bearingSprite;
TFT_eSprite TextPopSprite = TFT_eSprite(&tft);


uint8_t TFT_current = 1;
uint8_t pages =0;
uint16_t lock_x = 100;
uint16_t lock_y = 80;
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

    buddy_name = BuddyManager::findBuddyName(traffic[TFT_current - 1].fop->ID);
    if (buddy_name) {
      bud_color = TFT_GREEN;
    } else {
      buddy_name = "Unknown";
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
  sprite.setTextColor(bud_color, TFT_BLACK);

  sprite.drawString(traffic[TFT_current - 1].acftType == 7 ? "PG" : traffic[TFT_current - 1].acftType == 6 ? "HG" : traffic[TFT_current - 1].acftType == 1 ? "G" : traffic[TFT_current - 1].acftType == 2 ? "TAG" : traffic[TFT_current - 1].acftType == 3 ? "H" : traffic[TFT_current - 1].acftType == 9 ? "A" : String(traffic[TFT_current - 1].acftType), 87, 120, 4);
  sprite.drawSmoothRoundRect(84, 110, 6, 5, 40, 40, TFT_WHITE);
  sprite.setTextColor(text_color, TFT_BLACK);
  sprite.drawString(id2_text, 140, 58, 4);
  sprite.setFreeFont(&Orbitron_Light_24);
  sprite.setCursor(140, 110);
  sprite.printf(buddy_name);
  sprite.setCursor(140,123,4);
  sprite.printf("Last seen: %ds ago", traffic[TFT_current - 1].lastSeen);

  sprite.drawRoundRect(150, 160, 170, 10, 5, TFT_CYAN);
  sprite.fillRoundRect(150, 160, traffic[TFT_current - 1].lastSeen > 5 ? 1 : 170 - traffic[TFT_current - 1].lastSeen * 30, 10, 5, TFT_CYAN);

  // sprite.drawString("Vertical", 27, 180, 4);
  sprite.drawNumber((int)(traffic[TFT_current - 1].fop->RelativeVertical) * alt_mult, 30, 213, 7);
  
  sprite.drawSmoothArc(233, 233, 230, 225, 0, 360, TFT_DARKGREY, TFT_BLACK, true);
  //add AV when average vario used
  sprite.drawString(show_avg_vario ? "AV" : "", 377, 150, 4);
  sprite.drawString("Climbrate ", 327, 180, 4);
  // sprite.drawString(" o'clock ", 217, 280, 4);
  //add AV when average vario used

  sprite.setTextDatum(TR_DATUM);
  sprite.drawFloat(traffic_vario, 1, 425, 213, 7);
  sprite.drawNumber(disp_alt, 90, 280, 4);
  sprite.setTextDatum(TL_DATUM);
  sprite.drawString("amsl", 100, 280, 2);
  sprite.drawString("m", 430, 215, 4);
  sprite.drawString("s", 435, 240, 4);

  sprite.drawWideLine(430, 240, 450, 241, 3, text_color);

  sprite.setSwapBytes(true);
  sprite.pushImage(40, 160, 32, 32, altitude2);

  sprite.drawFloat((traffic[TFT_current - 1].fop->distance / 1000.0), 1, 170, 285, 7);
  sprite.drawString("km", 250, 285, 4);
    
  // sprite.setSwapBytes(true);
  // sprite.pushImage(300, 300, 32, 24, Speed);
  // sprite.drawFloat(speed, 0, 340, 300, 6);
  // sprite.drawString("km", 400, 300, 4); //speed km
  // sprite.drawString("h", 400, 325, 4);  //speed h
  // sprite.drawWideLine(400, 322, 420, 322, 3, TFT_WHITE);

  if (vertical > 55) {
    sprite.drawSmoothArc(233, 233, 230, 225, 90, constrain(90 + vertical / 10, 90, 150), vertical > 150 ? TFT_CYAN : TFT_RED, TFT_BLACK, true);
    sprite.drawString("+", 15, 226, 7);
    sprite.drawWideLine(15, 236, 25, 236, 6, text_color); //draw plus
    sprite.drawWideLine(20, 231, 20, 241, 6, text_color);
  }
  else if (vertical < -55) {
    sprite.drawSmoothArc(233, 233, 230, 225, constrain(90 - abs(vertical) / 10, 30, 90), 90, vertical < -150 ? TFT_GREEN : TFT_RED, TFT_BLACK, true);
    // sprite.drawWideLine(15, 231, 25, 231, 6, TFT_WHITE); //draw minus
  }
  if (traffic_vario < -0.5) {
    sprite.drawSmoothArc(233, 233, 230, 225, 270, constrain(270 + abs(traffic_vario) * 12, 270, 360), traffic_vario < 2.5 ? TFT_BLUE : traffic_vario < 1 ? TFT_CYAN : TFT_GREEN, TFT_BLACK, true);
    
  }
  else if (traffic_vario > 0.3) { //exclude 0 case so not to draw 360 arch
    sprite.drawSmoothArc(233, 233, 230, 225, constrain(270 - abs(traffic_vario) * 12, 190, 270), 270, traffic_vario > 3.5 ? TFT_RED : traffic_vario > 2 ? TFT_ORANGE : TFT_YELLOW, TFT_BLACK, true);
  }
  // Lock page
  if (focusOn) {
    sprite.fillSmoothRoundRect(lock_x, lock_y, 20, 20, 4, lock_color, TFT_BLACK);
    sprite.drawArc(lock_x + 9, lock_y, 9, 6, 90, 270, lock_color, TFT_BLACK, false);
  }
  // sprite.drawSmoothRoundRect(lock_x, lock_y, 5, 3, 25, 25, lock_color, TFT_BLACK);
  // sprite.drawArc(lock_x + (focusOn ? 12 : 30), lock_y, 9, 7, 90, 270, lock_color, TFT_BLACK);
  //Airctafts
  sprite.setSwapBytes(true);
  sprite.pushImage(190, 370, 32, 32, aircrafts);
  sprite.drawNumber(Traffic_Count(), 240, 365, 6);

  
  bearingSprite.fillSprite(TFT_BLACK);
  sprite.setPivot(233, 233);

  bearingSprite.fillRect(0, 12, 40, 30, text_color == TFT_GREY ? text_color : TFT_CYAN);
  bearingSprite.fillTriangle(40, 0, 40, 53, 78, 26, text_color == TFT_GREY ? text_color : TFT_CYAN);
  bearingSprite.setPivot(39, 27);
  bearingSprite.pushRotated(&sprite, bearing - 90);
  draw_battery();
  draw_extBattery();
  // if (pages > 1) {
  //   for (int i = 1; i <= pages; i++)
  //   { uint16_t wd = (pages -1) * 18; // width of frame 8px per circle + 8px between circles

  //     sprite.fillCircle((233 - wd /2) + i * 18, 425, 5, i == TFT_current ? TFT_WHITE : TFT_DARKGREY);

  //   }
  // }
  //draw settings icon
    sprite.setSwapBytes(true);
    sprite.pushImage(320, 360, 36, 36, settings_icon_small);
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
    // Serial.print("TFT_current: "); Serial.println(TFT_current);
    // Serial.print("pages: "); Serial.println(pages);     
    }

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