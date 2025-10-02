/*
 * BatteryHelper.cpp
 * Copyright (C) 2019-2021 Linar Yusupov
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

#if defined(ARDUINO)
#include <Arduino.h>
#endif /* ARDUINO */

#include "SoCHelper.h"
#include "BatteryHelper.h"
#include "SkyView.h"
#include "TFTHelper.h"
#include "WebHelper.h"
#include "Arduino_DriveBus_Library.h"
#include "Platform_ESP32.h"

static unsigned long Battery_TimeMarker = 0;

static float Battery_voltage_cache      = 0;
static int Battery_cutoff_count         = 0;
static int charging_status_cache        = 0;
float voltage_start = 0;
unsigned long start_time_ms = 0;

float voltage_end = 0;
unsigned long end_time_ms = 0;
bool PMU_initialized = false;

#if defined(LILYGO_AMOLED_1_75)
std::unique_ptr<Arduino_SY6970> SY6970;

#elif defined(WAVESHARE_AMOLED_1_75)
#include "XPowersLib.h"
#include "ESP_IOExpander_Library.h"

uint32_t printTime = 0;
XPowersPMU power;
ESP_IOExpander *expander = NULL;
#endif

void PMU_setup() {
#if defined(LILYGO_AMOLED_1_75)
  SY6970 = std::unique_ptr<Arduino_SY6970>(new Arduino_SY6970(IIC_Bus, SY6970_DEVICE_ADDRESS, DRIVEBUS_DEFAULT_VALUE, DRIVEBUS_DEFAULT_VALUE));
  if (!SY6970->begin()) {
    Serial.println("SY6970 init failed");
    while (1);
  }
  Serial.println("SY6970 init success");
  PMU_initialized = true;
  // Set BATFET mode to ON
  bool ok = SY6970->IIC_Write_Device_State(
      Arduino_IIC_Power::Device::POWER_BATFET_MODE,
      Arduino_IIC_Power::Device_State::POWER_DEVICE_ON
  );
  Serial.println("SY6970 setup Charging parameters");
  SY6970->IIC_Write_Device_State(SY6970->Arduino_IIC_Power::Device::POWER_DEVICE_ADC_MEASURE, SY6970->Arduino_IIC_Power::Device_State::POWER_DEVICE_ON);
  SY6970->IIC_Write_Device_Value(SY6970->Arduino_IIC_Power::Device_Value::POWER_DEVICE_WATCHDOG_TIMER, 0);
  Serial.println("SY6970 Device ON and set Watchdog Timer to 0");
   // Set fast charging current limit
  SY6970->IIC_Write_Device_Value(SY6970->Arduino_IIC_Power::Device_Value::POWER_DEVICE_FAST_CHARGING_CURRENT_LIMIT, 200);
  // Set precharge charging current limit
  SY6970->IIC_Write_Device_Value(SY6970->Arduino_IIC_Power::Device_Value::POWER_DEVICE_PRECHARGE_CHARGING_CURRENT_LIMIT, 50);
  // Set termination charging current limit
  SY6970->IIC_Write_Device_Value(SY6970->Arduino_IIC_Power::Device_Value::POWER_DEVICE_TERMINATION_CHARGING_CURRENT_LIMIT, 50);
#elif defined(WAVESHARE_AMOLED_1_75)
  // AXP2101 setup commands
    // already been here?
  if (expander != NULL)
  {
    return;
  }

  setupWireIfNeeded(IIC_SDA, IIC_SCL, 400000);  // Use 400kHz I2C speed for better performance

#if defined(I2C_SCAN)
  I2C_Scan();
#endif
  // I2C bus already started at this point, so we can use this constuctor
  LOG_DEBUG("Create TCA95xx_8bit IOExpander object...");
  expander = new ESP_IOExpander_TCA95xx_8bit((i2c_port_t)0,ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000);
  expander->begin();
  expander->pinMode(AXP_IRQ, INPUT);
  expander->pinMode(PWR_BUTTON, INPUT);
  expander->printStatus();

  bool result = power.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (result == false) 
  {
    PRINTLN("[ERROR] AXP2101 PMU is not online!");
    return;
  }
  PMU_initialized = true;
  Serial.printf("AXP2101 Chip ID: 0x%x\n", power.getChipID());

  power.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);

  // Clear all interrupt flags
  power.clearIrqStatus();

  // Enable the required interrupt function
  // IRQ not used anymore
//  power.enableIRQ(
//      XPOWERS_AXP2101_BAT_INSERT_IRQ    | XPOWERS_AXP2101_BAT_REMOVE_IRQ      |   //BATTERY
//      XPOWERS_AXP2101_VBUS_INSERT_IRQ   | XPOWERS_AXP2101_VBUS_REMOVE_IRQ     |   //VBUS
//      XPOWERS_AXP2101_PKEY_SHORT_IRQ    | XPOWERS_AXP2101_PKEY_LONG_IRQ       |   //POWER KEY
//      XPOWERS_AXP2101_BAT_CHG_DONE_IRQ  | XPOWERS_AXP2101_BAT_CHG_START_IRQ       //CHARGE
//  );
//  power.printIntRegister(&Serial);

  // Set the minimum common working voltage of the PMU VBUS input,
  // below this value will turn off the power
  power.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);
  // Set the maximum current of the PMU VBUS input,
  // higher than this value will turn off the PMU
  power.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);
  // Set VSY off voltage as 2600mV , Adjustment range 2600mV ~ 3300mV
  power.setSysPowerDownVoltage(2600);
  // Set the precharge charging current
  power.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
  // Set constant current charge current limit
  power.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_200MA);
  // Set stop charging termination current
  power.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);
  // Set charge cut-off voltage
  power.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V1);

  // show power on/off times after boot, do they survice a reboot?
  uint8_t opt = power.getPowerKeyPressOffTime();
  ShowPowerOffTime(opt);
  opt = power.getPowerKeyPressOnTime();
  ShowPowerOnTime(opt);

//  LOG_DEBUG("Read ESP_IOExpander IRQ pin of AXP2101...");
//  AXP2101_ChkIRQ();

  // power OLED
//  power.setBLDO1Voltage(3300);
//  power.enableALDO1();

  // Set the time of pressing the button to turn off
  power.setPowerKeyPressOffTime(XPOWERS_POWEROFF_6S);
  opt = power.getPowerKeyPressOffTime();
  ShowPowerOffTime(opt);

  // Set the button power-on press time
  power.setPowerKeyPressOnTime(XPOWERS_POWERON_2S);
  opt = power.getPowerKeyPressOnTime();
  ShowPowerOnTime(opt);

  xpower_power_on_source_t powerOnSource = power.getPowerOnSource();
  PRINT("AXP2101 Power On Source = ");
  switch(powerOnSource)
  {
    case XPOWER_POWERON_SRC_POWERON_LOW: PRINTLN("POWER_ON_LOW"); break;
    case XPOWER_POWERON_SRC_BAT_CHARGE:  PRINTLN("BAT CHARGE"); break;
    case XPOWER_POWERON_SRC_BAT_INSERT:  PRINTLN("BAT INSERT"); break;
    case XPOWER_POWERON_SRC_IRQ_LOW:     PRINTLN("IRQ LOW"); break;
    case XPOWER_POWERON_SRC_ENMODE:      PRINTLN("ENMODE?"); break;
    case XPOWER_POWERON_SRC_VBUS_INSERT: PRINTLN("VBUS INSERT"); break;
    default:
    case XPOWER_POWERON_SRC_UNKONW:      PRINTLN("UNKNOWN"); break;
  }

  LOG_DEBUG("AXP2101 Power chip initialized");
  adcOn();
#endif
}                                                       

int read_PMU_voltage()
{
#if defined(LILYGO_AMOLED_1_75)
  int voltage = SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_BATTERY_VOLTAGE);
  if (voltage < 0) {
    return 0;
  }
  return voltage;
#elif defined(WAVESHARE_AMOLED_1_75)
  return power.getBatVoltage();
#else
  return 0;
#endif
}

int read_PMU_charge_current() {
#if defined(LILYGO_AMOLED_1_75)
  int current = SY6970->IIC_Read_Device_Value(SY6970->Arduino_IIC_Power::Value_Information::POWER_CHARGING_CURRENT);
  if (current < 0) {
    return 0;
  }
  return current;
#elif defined(WAVESHARE_AMOLED_1_75)
    // need to translate the enum to actual charge values
  switch(power.getChargerConstantCurr())
  {
    case XPOWERS_AXP2101_CHG_CUR_100MA: return 100;
    case XPOWERS_AXP2101_CHG_CUR_125MA: return 125;
    case XPOWERS_AXP2101_CHG_CUR_150MA: return 150;
    case XPOWERS_AXP2101_CHG_CUR_175MA: return 175;
    case XPOWERS_AXP2101_CHG_CUR_200MA: return 200;
    case XPOWERS_AXP2101_CHG_CUR_300MA: return 300;
    case XPOWERS_AXP2101_CHG_CUR_400MA: return 400;
    case XPOWERS_AXP2101_CHG_CUR_500MA: return 500;
    case XPOWERS_AXP2101_CHG_CUR_600MA: return 600;
    case XPOWERS_AXP2101_CHG_CUR_700MA: return 700;
    case XPOWERS_AXP2101_CHG_CUR_800MA: return 800;
    case XPOWERS_AXP2101_CHG_CUR_900MA: return 900;
    case XPOWERS_AXP2101_CHG_CUR_1000MA: return 1000;
    default: return 0;
  }
#else
  return 0;
#endif
  return current;
}

int read_PMU_charging_status()
{
#if defined(LILYGO_AMOLED_1_75)
  String status = SY6970->IIC_Read_Device_State(SY6970->Arduino_IIC_Power::Status_Information::POWER_CHARGING_STATUS);

  if (status == "Not Charging") return 0;
  else if (status == "Pre-Charge") return 1;
  else if (status == "Fast Charging") return 2;
  else if (status == "Charge Termination Done") return 3;
  else return -1; // Unknown

#elif defined(WAVESHARE_AMOLED_1_75)
  uint8_t charge_status = power.getChargerStatus();

  switch (charge_status) {
    case XPOWERS_AXP2101_CHG_TRI_STATE: // ??
      return 0; // Not charging
    case XPOWERS_AXP2101_CHG_PRE_STATE:
      return 1; // Pre-Charge
    case XPOWERS_AXP2101_CHG_CV_STATE:
    case XPOWERS_AXP2101_CHG_CC_STATE:
      return 2; // Fast Charging
    case XPOWERS_AXP2101_CHG_DONE_STATE:
      return 3; // Charge Termination Done
      break;
    case XPOWERS_AXP2101_CHG_STOP_STATE:
      return 0; // Not charging
  }
  return -1; // Unknown

#else
  return -1; // Not supported
#endif
}
#if defined(XPOWERS_CHIP_AXP2101)
static unsigned long longPressStart = 0;

static void adcOn() 
{
  power.enableTemperatureMeasure();
  // Enable internal ADC detection
  power.enableBattDetection();
  power.enableVbusVoltageMeasure();
  power.enableBattVoltageMeasure();
  power.enableSystemVoltageMeasure();
}

void prepare_AXP2101_deep_sleep()
{
  LOG_DEBUG("Prepare AXP2101 PMU for deep sleep wake up...");
  power.clearIrqStatus();
  // Enable the required interrupt function
  power.enableIRQ(
     XPOWERS_AXP2101_PKEY_LONG_IRQ  | //POWER KEY
     XPOWERS_AXP2101_BAT_INSERT_IRQ | // BAT INSERT
     XPOWERS_AXP2101_VBUS_INSERT_IRQ  // or USB insert
  );
  // Enable PMU sleep
  power.enableSleep();
}

void power_off()
{
  // Cut off the power
  power.shutdown();
}

#if defined(I2C_SCAN)
static void I2C_Scan()
{
  byte error, address;
  int nDevices;

  PRINTLN("==============================================================");  
  PRINTLN("Scanning I2C bus...");

  nDevices = 0;
  for(address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      PRINT("I2C device found at address 0x");
      if (address<16)
        PRINT("0");
      PRINT(address,HEX);
      PRINTLN("  !");

      nDevices++;
    }
    else if (error==4)
    {
      PRINT("Unknown error at address 0x");
      if (address<16)
        PRINT("0");
      PRINTLN(address,HEX);
    }
  }
  if (nDevices == 0)
    PRINTLN("No I2C devices found!");
  else
    PRINTLN("done!");  

  PRINTLN("==============================================================");  
}
#endif // I2C_SCAN

void ShowPowerOffTime(uint opt)
{
  PRINT("PowerKeyPressOffTime: ");
  switch (opt) 
  {
    case XPOWERS_POWEROFF_4S: PRINTLN("4 Second");   break;
    case XPOWERS_POWEROFF_6S: PRINTLN("6 Second");   break;
    case XPOWERS_POWEROFF_8S: PRINTLN("8 Second");   break;
    case XPOWERS_POWEROFF_10S: PRINTLN("10 Second"); break;
    default: PRINTLN("UNKNOWN"); break;
  }
}

void ShowPowerOnTime(uint8_t opt)
{
  PRINT("PowerKeyPressOnTime: ");
  switch (opt) 
  {
    case XPOWERS_POWERON_128MS: PRINTLN("128 ms"); break;
    case XPOWERS_POWERON_512MS: PRINTLN("512 ms"); break;
    case XPOWERS_POWERON_1S: PRINTLN("1 Second");  break;
    case XPOWERS_POWERON_2S: PRINTLN("2 Seconds"); break;
    default: PRINTLN("UNKNOWN"); break;
  }
}

#endif //defined(XPOWERS_CHIP_AXP2101)

void power_off() {
#if defined(LILYGO_AMOLED_1_75)
      if (!PMU_initialized || !SY6970) {
        Serial.println("SY6970 not initialized, cannot power off.");
        return;
    }

    bool ok = SY6970->IIC_Write_Device_State(
        Arduino_IIC_Power::Device::POWER_BATFET_MODE,
        Arduino_IIC_Power::Device_State::POWER_DEVICE_OFF
    );

    if (ok) {
        Serial.println("Battery disconnected: Entering shipping mode.");
        // After this, ESP32 will lose power if not powered via USB
    } else {
        Serial.println("Failed to set BATFET off (shipping mode).");
    }
#elif defined(WAVESHARE_AMOLED_1_75)
  power.shutdown();
#endif
}

void Battery_setup()
{
  SoC->Battery_setup();

  Battery_voltage_cache = voltage_start = SoC->Battery_voltage();
  start_time_ms = millis();
}

float Battery_voltage()
{
  return Battery_voltage_cache;
}
int charging_status() {
  return charging_status_cache;
}
/* low battery voltage threshold */
float Battery_threshold()
{
  return BATTERY_THRESHOLD_LIPO;
}

/* Battery is empty */
float Battery_cutoff()
{
  return BATTERY_CUTOFF_LIPO;
}
void battery_fini() {
  voltage_end = SoC->Battery_voltage();
  end_time_ms = millis();

  unsigned long durationSeconds = (end_time_ms - start_time_ms) / 1000;

  float voltage_drop = voltage_start - voltage_end;
  // Simplified estimate: assume linear discharge, estimate mAh used
  float estimated_mAh = (voltage_drop / 0.001) * 0.0015f; // tuning factor as needed
  writeBatteryLog(voltage_start, voltage_end, durationSeconds, estimated_mAh);

}

void Battery_loop()
{
  if (isTimeToBattery()) {
    if (!PMU_initialized) {
      Serial.println("Error: PMU not initialized!");
      return;
  }
    float voltage = SoC->Battery_voltage();
    int charging_status = read_PMU_charging_status();

    if ( hw_info.model    == SOFTRF_MODEL_SKYVIEW &&
        (hw_info.revision == HW_REV_H741_01)) {

      if (voltage > 2.0 && voltage < Battery_cutoff()) {
        if (Battery_cutoff_count > 3) {
          ESP32_TFT_fini("LOW BATTERY");
          shutdown("LOW BATTERY");
        } else {
          Battery_cutoff_count++;
        }
      } else {
        Battery_cutoff_count = 0;
      }
    }

    Battery_voltage_cache = voltage;
    charging_status_cache = charging_status;
    Battery_TimeMarker = millis();
  }
}
