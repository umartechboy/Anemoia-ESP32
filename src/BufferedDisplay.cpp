#include "BufferedDisplay.h"
void (*HW_startWrite)() = 0;
void (*HW_setAddressWindow)(int x, int y, int width, int height) = 0;
void (*HW_writePixels)(uint16_t *colors, uint16_t count) = 0;
void (*HW_endWrite)() = 0;

#define _swap_(a, b) \
    (((a) ^= (b)), ((b) ^= (a)), ((a) ^= (b))) ///< No-temp-var swap operation

uint16_t *screenBuffer;


BufferedDisplay::BufferedDisplay(
    Adafruit_GFX &hw,
    void (*startWrite)(),
    void (*setAddressWindow)(int x, int y, int width, int height),
    void (*writePixels)(uint16_t *colors, uint16_t count),
    void (*endWrite)()) : Adafruit_GFX(hw.width(), hw.height())
{
    this->HW = &hw;
    HW_startWrite = startWrite;
    HW_setAddressWindow = setAddressWindow;
    HW_writePixels = writePixels;
    HW_endWrite = endWrite;
    Serial.printf("BufferedDisplay constructor: %d x %d\n", hw.width(), hw.height());
    screenBuffer = new uint16_t[hw.width() * hw.height()];
    for (int i = 0; i < hw.width() * hw.height(); i++)
    {
        screenBuffer[i] = 0xFFFF; // White
    }
}

void BufferedDisplay::drawPixel(int16_t x, int16_t y, Color color){
    if (color.a() == 0x00)
        return;
    else if (color.a() == 0xFF)
        drawPixel(x,y, color.toColor16());
    else 
    {
        Color existing = readPixel(x, y);
        existing.BlendBelow(color);
        drawPixel(x, y, existing.toColor16());
    }
}
void BufferedDisplay::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    x += xOffset;
    y += yOffset;
    if (drawOpacity == 0){        
        if (Debug)
            Serial.printf("skip trans drawPixel(%d x %d) = %d\n", x, y, color);
        return;
    }
    else if (drawOpacity != 100){
        if (Debug)
            Serial.printf("drawPixel with opacity %d at (%d, %d) color %d\n", drawOpacity, x, y, color);
        // Use mixing
        Color c(color);
        c.a(((int)drawOpacity * 255) / 100);
        Color e = readPixel(x - xOffset, y - yOffset); // need to remove the doule offseting
        e.BlendBelow(c);
        //color = 0b11111100000;
        color = e.toColor16();
    }
    // else, Opaque. No need to mix
    if ((x >= 0) && (x < width()) && (y >= 0) && (y < height()))
    {
        // Pixel is in-bounds. Rotate coordinates if needed.
        switch (getRotation())
        {
        case 1:
            _swap_(x, y);
            x = WIDTH - x - 1;
            break;
        case 2:
            x = WIDTH - x - 1;
            y = HEIGHT - y - 1;
            break;
        case 3:
            _swap_(x, y);
            y = HEIGHT - y - 1;
            break;
        }
        if (screenBuffer[x + y * WIDTH] != color)
        {
            if (Debug)
                Serial.printf("drawPixel(%d x %d) = %d\n", x, y, color);
            // Serial.printf("screenBuffer[%d] = %d\n", x + y * WIDTH, color);
            screenBuffer[x + y * WIDTH] = color; // we never give 16 bit colors to this functions
            updateRequired = true;

            if (x < updateX0)
            {
                updateX0 = x;
                // Serial.printf("updateX0 set to %d\n", updateX0);
            }
            if (x > updateX1)
            {
                updateX1 = x;
                // Serial.printf("updateX1 set to %d\n", updateX1);
            }
            if (y < updateY0)
            {
                updateY0 = y;
                // Serial.printf("updateY0 set to %d\n", updateY0);
            }
            if (y > updateY1)
            {
                updateY1 = y;
                // Serial.printf("updateY1 set to %d\n", updateY1);
            }
        }
        else
        {
            if (Debug)
                Serial.printf("skip drawPixel(%d x %d) = %d\n", x, y, color);
        }
    }
}
uint16_t BufferedDisplay::readPixel(int16_t x, int16_t y)
{
    x += xOffset;
    y += yOffset;
    if ((x >= 0) && (x < width()) && (y >= 0) && (y < height()))
    {
        // Pixel is in-bounds. Rotate coordinates if needed.
        switch (getRotation())
        {
        case 1:
            _swap_(x, y);
            x = WIDTH - x - 1;
            break;
        case 2:
            x = WIDTH - x - 1;
            y = HEIGHT - y - 1;
            break;
        case 3:
            _swap_(x, y);
            y = HEIGHT - y - 1;
            break;
        }
        return screenBuffer[x + y * WIDTH];
    }
    return 0;
}
uint32_t totalPushTime = 0;
void BufferedDisplay::update(bool forceFullWidth, bool forceFullHeight)
{
    if (InUpdate) return;  // Prevent concurrent calls
    long st = millis();
    if (!AcceptUpdates)
        return;
    if (Debug)
        Serial.printf("update() called. Bounds: (%d, %d) to (%d, %d)\n", updateX0, updateY0, updateX1, updateY1);
    
    InUpdate = true;
    if (forceFullWidth)
    {
        //Serial.println("Force full width");
        updateX0 = 0;
        updateX1 = HW->width() - 1;
    }
    if (forceFullHeight)
    {
        //Serial.println("Force full Height");
        updateY0 = 0;
        updateY1 = HW->height() - 1;
    }
    if (updateX1 < updateX0 || updateY1 < updateY0)
    {
        InUpdate = false;
        //Serial.println("Nothing to update");
        return;
    }
    int updateX0 = this->updateX0;
    int updateX1 = this->updateX1;
    int updateY0 = this->updateY0;
    int updateY1 = this->updateY1;
    int w = updateX1 - updateX0 + 1;
    int h = updateY1 - updateY0 + 1;
    //Serial.printf("Update (%d, %d, %d, %d)\n", updateX0, updateY0, w, h);
    if (w == HW->width()) {// Dump whole color buffer at once
        //Serial.println("Update full width");
        HW_startWrite();
        HW_setAddressWindow(0, updateY0, w, h);
        HW_writePixels(screenBuffer + w * updateY0, w * h);
        uint16_t cbin = readPixel(0, 0);
        uint8_t byte0 = cbin & 0xFF;
        uint8_t byte1 = (cbin >> 8) & 0xFF;
        Color testColor(cbin);
        //Serial.printf("Test readPixelcolor: 0x%04X,\tbyte0: 0x%02X,\tbyte1: 0x%02X,\tColor R: %d\tG: %d\tB: %d\n", cbin, byte0, byte1, testColor.R(), testColor.G(), testColor.B());

        HW_endWrite();
    }
    else {
        //Serial.println("Update partial width");
        for (int yi = 0; yi < h; yi++)
        {
            //Serial.printf("Update line (%d, %d, %d, %d)\n", updateX0, updateY0 + yi, w, 1);
            HW_startWrite();
            HW_setAddressWindow(updateX0, updateY0 + yi, w, 1);
            HW_writePixels(screenBuffer + HW->width() * (updateY0 + yi) + updateX0, w);
            HW_endWrite();
        }
    }
    this->updateX0 = HW->width();
    this->updateX1 = -1;
    this->updateY0 = HW->height();
    this->updateY1 = -1;
    InUpdate = false;
    totalPushTime += (millis() - st);
    Serial.print("Total Frame took: ");
    totalPushTime = 0;
    Serial.println(millis() - st);
}

void BufferedDisplay::updateAsync()
{
    needsUpdateFlag = true;
}

bool BufferedDisplay::needUpdate()
{
    return needsUpdateFlag;
}

void BufferedDisplay::clearUpdateFlag()
{
    needsUpdateFlag = false;
}

void BufferedDisplay::pushPixels(uint16_t* image, uint32_t len, uint16_t scanline)
{
    if (!AcceptUpdates)
        return;
    long st = millis() ;
    constexpr uint32_t sourceWidth = 256;
    const uint32_t screenW = this->width();
    const uint32_t screenH = this->height();
    const uint32_t srcHeight = (sourceWidth > 0) ? (len / sourceWidth) : 0;

    if (srcHeight == 0 || srcHeight * sourceWidth != len)
    {
        totalPushTime += (millis() - st);
        //Serial.printf("pushPixels: invalid image len %u, expected multiple of %u\n", len, sourceWidth);
        return;
    }

    //Serial.printf("pushPixels: image size %u, NES chunk %u x %u, scanline %u, screen %u x %u\n",
                  //len, sourceWidth, srcHeight, scanline, screenW, screenH);

    for (uint32_t sy = 0; sy < srcHeight; sy++)
    {
        const uint32_t srcY = scanline + sy;
        const uint32_t destY = srcY >> 1;
        if (destY >= screenH)
            continue;

        uint16_t* row = image + sy * sourceWidth;
        for (uint32_t sx = 0; sx < sourceWidth; sx++)
        {
            const uint32_t destX = sx >> 1;
            if (destX >= screenW)
                continue;
            // Swap RGB to BGR for display
            // uint16_t color = row[sx];
            // color = ((color & 0x001F) << 11) | (color & 0x07E0) | ((color & 0xF800) >> 11);
            drawPixel(destX, destY, row[sx]);
        }
    }
    totalPushTime += (millis() - st);
}

// void BufferedDisplay::pushPixels(uint16_t* image, uint32_t len, uint16_t scanline)
// {
//     if (!AcceptUpdates)
//         return;
//     long st = millis() ;
//     constexpr uint32_t sourceWidth = 256;
//     const uint32_t screenW = this->width();
//     const uint32_t screenH = this->height();
//     const uint32_t srcHeight = (sourceWidth > 0) ? (len / sourceWidth) : 0;

//     if (srcHeight == 0 || srcHeight * sourceWidth != len)
//     {
//         totalPushTime += (millis() - st);
//         //Serial.printf("pushPixels: invalid image len %u, expected multiple of %u\n", len, sourceWidth);
//         return;
//     }

//     //Serial.printf("pushPixels: image size %u, NES chunk %u x %u, scanline %u, screen %u x %u\n",
//                   //len, sourceWidth, srcHeight, scanline, screenW, screenH);

//     for (uint32_t sy = 0; sy < srcHeight; sy++)
//     {
//         const uint32_t srcY = scanline + sy;
//         const uint32_t destY = (screenH == 240) ? srcY : (srcY * screenH) / 240;
//         if (destY >= screenH)
//             continue;

//         uint16_t* row = image + sy * sourceWidth;
//         for (uint32_t sx = 0; sx < sourceWidth; sx++)
//         {
//             const uint32_t destX = (screenW == sourceWidth) ? sx : (sx * screenW) / sourceWidth;
//             if (destX >= screenW)
//                 continue;
//             // Swap RGB to BGR for display
//             // uint16_t color = row[sx];
//             // color = ((color & 0x001F) << 11) | (color & 0x07E0) | ((color & 0xF800) >> 11);
//             drawPixel(destX, destY, row[sx]);
//         }
//     }
//     totalPushTime += (millis() - st);
// }

void BufferedDisplay::pushPixelsDMA(uint16_t* image, uint32_t len, uint16_t scanline)
{
    // Adafruit_ST7735 does not support DMA in this wrapper.
    // Fall back to regular pixel transfer.
    pushPixels(image, len, scanline);
}

uint8_t BufferedDisplay::GetOpacity()
{
    return drawOpacity;
}
// 0-100
void BufferedDisplay::SetOpacity(uint8_t opacity)
{
    drawOpacity = opacity;
    if (drawOpacity > 100)
        drawOpacity = 100;
}

void BufferedDisplay::setTextDatum(uint8_t datum)
{
    textdatum = datum;
}

uint8_t BufferedDisplay::getTextDatum(void) const
{
    return textdatum;
}

static void adjustTextDatum(int32_t &x, int32_t &y, uint8_t datum, uint16_t w, uint16_t h)
{
    switch (datum)
    {
        case TC_DATUM: x -= w / 2; break;
        case TR_DATUM: x -= w; break;
        case ML_DATUM: y -= h / 2; break;
        case MC_DATUM: x -= w / 2; y -= h / 2; break;
        case MR_DATUM: x -= w; y -= h / 2; break;
        case BL_DATUM: y -= h; break;
        case BC_DATUM: x -= w / 2; y -= h; break;
        case BR_DATUM: x -= w; y -= h; break;
        case TL_DATUM:
        default: break;
    }
}

void BufferedDisplay::drawString(const String &string, int32_t x, int32_t y, uint8_t font)
{
    drawString(string.c_str(), x, y, font);
}

void BufferedDisplay::drawString(const char *string, int32_t x, int32_t y, uint8_t font)
{
    uint8_t prevSizeX = textsize_x;
    uint8_t prevSizeY = textsize_y;
    const uint8_t prevDatum = textdatum;

    if (font == 0) {
        setTextSize(1);
    } else {
        setTextSize(font);
    }

    int16_t x1, y1;
    uint16_t w, h;
    getTextBounds(string, x, y, &x1, &y1, &w, &h);

    int32_t drawX = x;
    int32_t drawY = y;
    adjustTextDatum(drawX, drawY, textdatum, w, h);

    setCursor(drawX, drawY);
    print(string);

    textsize_x = prevSizeX;
    textsize_y = prevSizeY;
    textdatum = prevDatum;
}

/***************************************************************************************
** Function name:           textWidth
** Description:             Return the width in pixels of a string
***************************************************************************************/
int16_t BufferedDisplay::textWidth(const String& string)
{
    return textWidth(string.c_str());
}

int16_t BufferedDisplay::textWidth(const String& string, uint8_t font)
{
    return textWidth(string.c_str(), font);
}

int16_t BufferedDisplay::textWidth(const char *string)
{
    int16_t x1, y1;
    uint16_t w, h;
    getTextBounds(string, 0, 0, &x1, &y1, &w, &h);
    return w;
}

int16_t BufferedDisplay::textWidth(const char *string, uint8_t font)
{
    uint8_t prevSizeX = textsize_x;
    uint8_t prevSizeY = textsize_y;

    if (font == 0) {
        setTextSize(1);
    } else {
        setTextSize(font);
    }

    int16_t x1, y1;
    uint16_t w, h;
    getTextBounds(string, 0, 0, &x1, &y1, &w, &h);

    textsize_x = prevSizeX;
    textsize_y = prevSizeY;
    return w;
}


