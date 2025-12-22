/*
 * SerialDebug.h
 * Conditional Serial output that only works when USB is connected
 *
 * Purpose: Save power by not initializing Serial when running on battery
 *
 * Usage:
 *   SerialPrint("Hello");
 *   SerialPrintln("World");
 *   SerialPrintf("Value: %d", value);
 */

#ifndef SERIAL_DEBUG_H
#define SERIAL_DEBUG_H

#include <Arduino.h>

// Global USB connection status flag
extern bool USB_is_connected;

// Check if USB is connected (ESP32-S3 specific)
inline bool isUSBConnected() {
  #if defined(ESP32S3)
    // For ESP32-S3, check if USB is active
    // The USB Serial will be available if USB cable is connected
    #if ARDUINO_USB_CDC_ON_BOOT
      return true;  // USB CDC always on
    #else
      // Check if USB peripheral is powered and configured
      // On ESP32-S3, we can check if USB VBUS is present
      #if defined(WAVESHARE_AMOLED_1_75)
        extern HWCDC USBSerial;
      #endif
      return USBSerial;  // Returns true if USB is connected
    #endif
  #else
    return true;  // For other platforms, assume always connected
  #endif
}

// ===== Conditional Serial macros - only output if USB is connected =====
#if ARDUINO_USB_CDC_ON_BOOT == 0
  // USB CDC not always on - check connection before output
  #define SerialPrint(...) if(USB_is_connected) Serial.print(__VA_ARGS__)
  #define SerialPrintln(...) if(USB_is_connected) Serial.println(__VA_ARGS__)
  #define SerialPrintf(...) if(USB_is_connected) Serial.printf(__VA_ARGS__)
  #define SerialWrite(...) if(USB_is_connected) Serial.write(__VA_ARGS__)
  #define SerialFlush() if(USB_is_connected) Serial.flush()
#else
  // USB CDC always on - use Serial directly
  #define SerialPrint(...) Serial.print(__VA_ARGS__)
  #define SerialPrintln(...) Serial.println(__VA_ARGS__)
  #define SerialPrintf(...) Serial.printf(__VA_ARGS__)
  #define SerialWrite(...) Serial.write(__VA_ARGS__)
  #define SerialFlush() Serial.flush()
#endif

#endif // SERIAL_DEBUG_H
