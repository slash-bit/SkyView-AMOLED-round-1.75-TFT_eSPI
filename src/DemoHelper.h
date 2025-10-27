/*
 * DemoHelper.h
 * Demo mode playback from SPIFFS file
 */

#ifndef DEMOHELPER_H
#define DEMOHELPER_H

#include <Arduino.h>

#define DEMO_FILE_PATH "/demo.nmea"
#define DEFAULT_DEMO_LINE_DELAY_MS 125  // Default delay between lines (125ms) 8 lines per second
#define DEFAULT_DEMO_GPGGA_INTERVAL_MS 1000  // Target 1 second between $GPGGA lines

void Demo_setup();
void Demo_loop();
void Demo_fini();
bool Demo_isPlaying();
void Demo_setLineDelay(uint16_t delay_ms);
uint16_t Demo_getLineDelay();

#endif /* DEMOHELPER_H */
