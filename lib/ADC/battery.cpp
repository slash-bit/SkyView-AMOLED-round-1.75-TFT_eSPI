
#include <esp32-hal-log.h>
#include <soc/adc_channel.h>
#include <Arduino.h>

#include "battery.h"

// Local logging tag
static const char TAG[] = "main";

esp_adc_cal_characteristics_t *adc_characs =
    (esp_adc_cal_characteristics_t *)calloc(
        1, sizeof(esp_adc_cal_characteristics_t));
        
#if defined(ESP32S3)
static adc1_channel_t adc_channel = ADC1_GPIO4_CHANNEL;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
#else
static adc1_channel_t adc_channel = ADC1_GPIO36_CHANNEL;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
#endif
static const adc_unit_t unit = ADC_UNIT_1;

void calibrate_voltage(adc1_channel_t channel) {

  adc_channel = channel;

  // configure ADC
  ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_12));
  ESP_ERROR_CHECK(adc1_config_channel_atten(adc_channel, atten));
  // calibrate ADC
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
      unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_characs);
  // show ADC characterization base
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
    ESP_LOGI(TAG,
             "ADC characterization based on Two Point values stored in eFuse");
  } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    ESP_LOGI(TAG,
             "ADC characterization based on reference voltage stored in eFuse");
  } else {
    ESP_LOGI(TAG, "ADC characterization based on default reference voltage");
  }
}

uint16_t read_voltage() {
  // multisample ADC
  uint32_t adc_reading = 0;
  for (int i = 0; i < NO_OF_SAMPLES; i++) {
    adc_reading += adc1_get_raw(adc_channel);
    yield();
  }
  adc_reading /= NO_OF_SAMPLES;
  // Convert ADC reading to voltage in mV
  uint16_t voltage =
      (uint16_t)esp_adc_cal_raw_to_voltage(adc_reading, adc_characs);
#if defined(AMOLED)
      voltage = voltage * 2;
#endif 
#ifdef BATT_FACTOR
  voltage *= BATT_FACTOR;
#endif
  ESP_LOGD(TAG, "Raw: %d / Voltage: %dmV", adc_reading, voltage);
#if defined(DEBUG_ADC)
  Serial.printf("Battery Raw: %d / Voltage: %dmV\r\n", adc_reading, voltage);
#endif
  return voltage;
}
