#ifndef HWCONFIG_H
#define HWCONFIG_H

#include <LittleFS.h>
#include "config.h"

struct __attribute__((packed)) HWConfig 
{
    uint8_t rotation;
    uint8_t dac_pin;
    uint8_t controller_type;
    uint8_t sd_freq;
    bool backlight;
};

inline HWConfig loadConfig() 
{
    HWConfig cfg = 
    {
        .rotation = SCREEN_ROTATION,
        .dac_pin = DAC_PIN,
        .controller_type = CONTROLLER_TYPE,
        .sd_freq = SD_FREQ / 1000000,
        #ifdef TFT_BACKLIGHT_ENABLE
        .backlight = true,
        #else
        .backlight = false,
        #endif
    };

    if (!LittleFS.begin()) 
    { 
        Serial.println("LittleFS mount failed, using defines in config.h"); 
        return cfg; 
    }
    Serial.println("LittleFS mounted"); 
    File f = LittleFS.open("/hwconfig.bin", "r");
    if (!f) 
    {
        Serial.println("hwconfig.bin not found, using defines in config.h");
        return cfg;
    }

    Serial.println("hwconfig.bin opened");
    f.read((uint8_t*)&cfg, sizeof(cfg));
    f.close();
    Serial.println("hwconfig.bin read successfully");

    Serial.printf("hw_config.rotation:   %d\n", cfg.rotation);
    Serial.printf("hw_config.dac_pin:    %d\n", cfg.dac_pin);
    Serial.printf("hw_config.controller: %d\n", cfg.controller_type);
    Serial.printf("hw_config.sd_freq:    %dMHz\n", cfg.sd_freq);
    Serial.printf("hw_config.backlight:  %d\n", cfg.backlight);
    return cfg;
}

#endif