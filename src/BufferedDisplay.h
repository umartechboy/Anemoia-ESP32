#ifndef BUFFERED_DISPLAY_H
#define BUFFERED_DISPLAY_H
#include <Adafruit_GFX.h>
#include "color.h"

#define TL_DATUM 0
#define TC_DATUM 1
#define TR_DATUM 2
#define ML_DATUM 3
#define MC_DATUM 4
#define MR_DATUM 5
#define BL_DATUM 6
#define BC_DATUM 7
#define BR_DATUM 8

#define ST7735_GRAY 0x8410

#define ForceFullWidthUpdate

class BufferedDisplay : public Adafruit_GFX
{
private:
    Adafruit_GFX *HW;
    bool InUpdate = false;
    bool updateRequired = false;
    bool needsUpdateFlag = false;
    int updateX0 = 1000, updateX1 = -1, updateY0 = 1000, updateY1 = -1;
    uint8_t drawOpacity = 100;
    uint8_t textdatum = TL_DATUM;
public:
    BufferedDisplay(
        Adafruit_GFX &hw,
        void (*startWrite)(),
        void (*setAddressWindow)(int x, int y, int width, int height),
        void (*writePixels)(uint16_t *colors, uint16_t count),
        void (*endWrite)());
    bool Debug = false;
    int16_t xOffset = 0, yOffset = 0;
    void SetOpacity(uint8_t opacity);
    uint8_t GetOpacity();
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void drawPixel(int16_t x, int16_t y, Color color);
    uint16_t readPixel(int16_t x, int16_t y);
    void update(bool forceFullWidth = true, bool forceFullHeight = false);
    void updateAsync();
    bool needUpdate();
    void clearUpdateFlag();
    void pushPixels(uint16_t* image, uint32_t len, uint16_t scanline);
    void pushPixelsDMA(uint16_t* image, uint32_t len, uint16_t scanline);
    void setTextDatum(uint8_t datum);
    uint8_t getTextDatum(void) const;
    void drawString(const String &string, int32_t x, int32_t y, uint8_t font);
    void drawString(const char *string, int32_t x, int32_t y, uint8_t font);
    int16_t textWidth(const String& string);
    int16_t textWidth(const String& string, uint8_t font);
    int16_t textWidth(const char *string);
    int16_t textWidth(const char *string, uint8_t font);
    bool AcceptUpdates = true;
};

#endif // BUFFERED_DISPLAY_H