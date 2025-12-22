/*
 * TrafficHelper.h
 * Copyright (C) 2019-2022 Linar Yusupov
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

#ifndef TRAFFICHELPER_H
#define TRAFFICHELPER_H

#define D2R (3.141593f/180.0f)
#define R2D (180.0f/3.141593f)

#include "SoCHelper.h"

extern "C" {
#include <gdl90.h>
}

typedef struct traffic_struct {

    time_t    timestamp;
    uint8_t   packet_type;   // 1=PFLAA, 2=PFLAU

/* -------------------------------+------------------------------ */
/*            FLARM FTD-12        |      GDL90 equivalent         */
/* -------------------------------+------------------------------ */
    int8_t    alarm_level;        // trafficAlertStatus
                                  //
    int8_t    IDType;             // addressType                // PFLAA only
    uint32_t  ID;                 // address
    uint16_t  Track;              // trackOrHeading             // PFLAA only
    int16_t   TurnRate;                                         // PFLAA only
    uint16_t  GroundSpeed;        // horizontalVelocity         // PFLAA only
    float     ClimbRate;          // verticalVelocity           // PFLAA only
    int8_t    AcftType;           // emitterCategory            // PFLAA only
    int8_t    rssi;               // RSSI signal strength       // PFLAA only

    uint8_t   alert_level;        // minimum alarm level to generate voice warning

/*            Legacy              */
    float     RelativeNorth;                                   // PFLAA only
    float     RelativeEast;                                    // PFLAA only
    float     RelativeVertical;
    int       RelativeBearing;                                 // PFLAU only

    float     distance;
    float     adj_dist;

/*            GDL90      */
    float     latitude;
    float     longitude;
    float     altitude;
    uint8_t   callsign [GDL90_TRAFFICREPORT_MSG_CALLSIGN_SIZE];

    uint8_t   alert;              // bitmap of issued voice/tone/ble/... alerts
} traffic_t;

typedef struct traffic_by_dist_struct {
  traffic_t *fop;
  float     distance;
  float    climbrate;
  uint16_t lastSeen;
  uint16_t altitude;
  uint8_t  acftType;
  int8_t   rssi;
  time_t   timestamp;
} traffic_by_dist_t;

#define ALARM_ZONE_NONE         5000  // meters
#define ALARM_ZONE_CLOSE        3000

#define VERTICAL_SLOPE          5  /* slope effect for alerts */

/* alarm levels are defined in NMEAHelper.h */

#define ENTRY_EXPIRATION_TIME   30 /* seconds */
#define ENTRY_STALE_TIME   7 /* seconds */
#define TRAFFIC_VECTOR_UPDATE_INTERVAL 2 /* seconds */
#define TRAFFIC_UPDATE_INTERVAL_MS (TRAFFIC_VECTOR_UPDATE_INTERVAL * 1000)
#define isTimeToUpdateTraffic() (millis() - UpdateTrafficTimeMarker > \
                                  TRAFFIC_UPDATE_INTERVAL_MS)

#define isTimeToVoice()         (millis() - Traffic_Voice_TimeMarker > 2000)
#define VOICE_EXPIRATION_TIME   5 /* seconds */

#define TRAFFIC_ALERT_VOICE     1    // bit within the .alert field

// wordings for voice warnings and notifications:
#define WARNING_WORD1 "traffic"
#define WARNING_WORD3 "danger"
// available: "traffic", "warning", "danger"
#define ADVISORY_WORD "traffic"
// also available: "notice"

void Traffic_setup        (void);
void Traffic_loop         (void);
void Traffic_Add          (void);
void Traffic_Update       (traffic_t *);
void Traffic_ClearExpired (void);
int  Traffic_Count        (void);

int  traffic_cmp_by_distance(const void *, const void *);

extern traffic_t ThisAircraft, Container[MAX_TRACKING_OBJECTS], fo, EmptyFO;
extern traffic_by_dist_t traffic[MAX_TRACKING_OBJECTS];

#endif /* TRAFFICHELPER_H */
