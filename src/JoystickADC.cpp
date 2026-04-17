#include "JoystickADC.h"
#include "config.h"
static const int joystickMaxADC[6] = { 860, 815, 770, 700, 530, 20 };
static const int joystickMinADC[6] = { 816, 771, 710, 610, 460, 0 };

uint32_t readADCMV(int pin);
int digitalReadADCJoystick(int pin);

uint32_t readADCMV(int pin) {
  if (pin != JOYSTICK_ADC_PIN) {
    return 0;
  }

  analogReadResolution(10);
  analogSetPinAttenuation(JOYSTICK_ADC_PIN, ADC_11db);
  int raw = analogRead(JOYSTICK_ADC_PIN);
  return static_cast<uint32_t>(raw) * 3300u / 1023u;
}

  // Override digitalRead
uint8_t digitalReadJoystick(uint8_t pin) {
     if (pin < A_BUTTON || pin > SELECT_BUTTON)
        return digitalRead(pin);

     return digitalReadADCJoystick(pin);
    if (pin < A_BUTTON || pin > SELECT_BUTTON)
        return 1;


    int adc = readADCMV(JOYSTICK_ADC_PIN);
    int keyIndex = pin - A_BUTTON;

    int ans = (adc >= joystickMinADC[keyIndex] && adc <= joystickMaxADC[keyIndex]) ? LOW : HIGH;
    Serial.printf("digitalRead override: pin=%d, adc=%d, keyIndex=%d, ans=%d\n", pin, adc, keyIndex, ans);
    return ans;
}