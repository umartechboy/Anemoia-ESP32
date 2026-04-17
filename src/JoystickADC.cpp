#include "JoystickADC.h"
#include "config.h"
static const int joystickMaxADC[6] = { 860, 815, 770, 700, 530, 20 };
static const int joystickMinADC[6] = { 816, 771, 710, 610, 460, 0 };

uint32_t readADCMV(int pin);
int digitalReadADCJoystick(int pin);

#include <esp_adc_cal.h>
#include <map>
std::map<int, uint16_t> adcMap;
#define ADC1_CHANNEL(pin) ADC1_GPIO ## pin ## _CHANNEL
#include "soc/adc_channel.h"
esp_adc_cal_characteristics_t characteristics[ADC_ATTEN_MAX];
adc_atten_t attenuations[ADC1_CHANNEL_MAX] = {};
uint32_t thresholds[ADC_ATTEN_MAX];

uint16_t adc_result;
int kp1Read;

#ifndef ADC_REFERENCE_VOLTAGE
  #define ADC_REFERENCE_VOLTAGE 3.3
#endif



void adc1_set_attenuation(adc1_channel_t chan, adc_atten_t atten) {
  if (attenuations[chan] != atten) {
    adc1_config_channel_atten(chan, atten);
    attenuations[chan] = atten;
  }
}

#define V_REF 1100
bool hasInit = false;
void adc_init() {
  if (hasInit) return;
  hasInit = true;
  // Configure ADC
  // ads.setGain(GAIN_ONE);
  // ads.setDataRate(RATE_ADS1015_128SPS);
  //ads.setDataRate(RATE_ADS1015_3300SPS);  
  //ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, /*continuous=*/true);
  adc1_config_width(ADC_WIDTH_12Bit);
  adc1_set_attenuation(ADC1_CHANNEL(3), ADC_ATTEN_11db);
  // Note that adc2 is shared with the WiFi module, which has higher priority, so the conversion may fail.
  // That's why we're not setting it up here.

  // Calculate ADC characteristics (i.e., gain and offset factors for each attenuation level)
  for (int i = 0; i < ADC_ATTEN_MAX; i++) {
    esp_adc_cal_characterize(ADC_UNIT_1, (adc_atten_t)i, ADC_WIDTH_BIT_12, V_REF, &characteristics[i]);

    // Change attenuation 100mV below the calibrated threshold
    thresholds[i] = esp_adc_cal_raw_to_voltage(4095, &characteristics[i]);
  }
}

typedef double isr_float_t;   // FPU ops are used for single-precision, so use double for ISRs.
uint32_t readADCMV() {
  adc_init();
  const adc1_channel_t chan = ADC1_CHANNEL(3);
  uint32_t mv;
  esp_adc_cal_get_voltage((adc_channel_t)chan, &characteristics[attenuations[chan]], &mv);
  uint32_t mvToReturn = mv;
  uint16_t this_adc_result = mvToReturn * isr_float_t(1023) / isr_float_t(ADC_REFERENCE_VOLTAGE) / isr_float_t(1000);
  
  return this_adc_result;
}

  // Override digitalRead
uint8_t digitalReadJoystick(uint8_t pin) {
    int adc = readADCMV();
    //int adc = analogRead(3);
    int keyIndex = pin - A_BUTTON;
    int ans = (adc >= joystickMinADC[keyIndex] && adc <= joystickMaxADC[keyIndex]) ? LOW : HIGH;
    Serial.printf("ADC: %d, Pin: %d, State: %s\n", adc, pin, ans == LOW ? "Pressed" : "Released");
    return 1;
}