#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <string>
#include <WiFi.h>
#include <vector>

#include "core/bus.h"
#include "controller.h"
#include "debug.h"
#include "ui.h"
#include "config.h"
#include "hwconfig.h"
#include "driver/i2s.h"
#include "esp_wifi.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include <Adafruit_ST7735.h>
#include "pins_D8500s.h"
#include "BufferedDisplay.h"

#ifndef OPTIMIZATION_FLAGS
#error The optimization flags were not applied! Please refer to *Step 4* of the README how to build and upload section.
#endif

void pollingTask(void* param);
void apuTask(void* param) ;
void setupI2SDAC();
bool initSD() ;
IRAM_ATTR void emulate();
void invalidCartridge();


#include <SPI.h>
class Adafruit_ST7735s: public Adafruit_ST7735 {
  public: 
  Adafruit_ST7735s(SPIClass *spiClass, int8_t cs, int8_t dc, int8_t rst):
    Adafruit_ST7735(spiClass, cs, dc, rst){}
  void ST7735sPatch(){
    _height = ST7735_TFTHEIGHT_128;
    _width = ST7735_TFTWIDTH_128;
    Serial.printf("ST7735sPatch applied. New dimensions: %d x %d, width(): %d, height(): %d\n", _width, _height, width(), height());
  }
};
Adafruit_ST7735s tft = Adafruit_ST7735s(&SPI, TFT_CS, TFT_DC, TFT_RST);

void TFT_startWrite()
{
    tft.startWrite();
}
void TFT_setAddressWindow(int x, int y, int width, int height)
{
    tft.setAddrWindow(x, y, width, height);
}
void TFT_writePixels(uint16_t *colors, uint16_t count)
{
    tft.writePixels(colors, count, false, false);
}
void TFT_endWrite()
{
    tft.endWrite();
}
BufferedDisplay* screen;
HWConfig hw_config;
SPIClass SD_SPI(SD_SPI_PORT);
UI ui;
Cartridge* cart;
void setup() 
{
    // Turn off Wifi and Bluetooth to reduce CPU overhead
    #ifdef DEBUG
        Serial.begin(115200);
        log_pin_config();
    #endif
    
    
  uint32_t currentFreq = getCpuFrequencyMhz();
  Serial.print("Current CPU Frequency: ");
  Serial.print(currentFreq);
  Serial.println(" MHz");

  // 2. Change frequency to 240 MHz
  if (setCpuFrequencyMhz(240)) {
    Serial.println("Successfully changed to 240 MHz");
  } else {
    Serial.println("Failed to change frequency");
  }

  // 3. Verify the change
  Serial.print("New CPU Frequency: ");
  Serial.println(getCpuFrequencyMhz());
  
    WiFi.mode(WIFI_OFF);
    esp_wifi_stop();
    esp_wifi_deinit();
    btStop();

    hw_config = loadConfig();
    setupI2SDAC();

    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_CS, OUTPUT);
    SPI.begin(37, 36, 35);
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(3);
    tft.ST7735sPatch();
    screen = new BufferedDisplay(tft, TFT_startWrite, TFT_setAddressWindow, TFT_writePixels, TFT_endWrite);
    ui.begin(screen);
    screen->fillScreen(Color(255,0,0));

    // Initialize microsd card
    if(!initSD()) while (true);
    Serial.println("SD initialized successfully.");
    
    ui.initializeSettings();
    Serial.println("Settings initialized.");
    delay(1); // serial flush
    // Setup buttons
    initController();
    Serial.println("Controllers initialized.");
    Serial.println("Setup complete.");
    delay(1);
}

void loop() 
{
    Serial.println("Selecting game...");
    delay(1);
    cart = ui.selectGame();
    Serial.println("Game selected.");
    delay(1);
    if (cart && cart->isValid())
    {
        Serial.println("Starting emulation...");
        delay(1);
        emulate();
    }
    Serial.println("Invalid cartridge.");
    delay(1);
    invalidCartridge();
}

void displayUpdateTask(void* param) {
    BufferedDisplay* scr = (BufferedDisplay*)param;
    int frame = 0;
    while (true) {
        if (scr->needUpdate()) {
            //scr->AcceptUpdates = frame % 2 == 0;
            frame++;
            scr->update();
            scr->clearUpdateFlag();
        }
        vTaskDelay(1 / portTICK_PERIOD_MS);  // 1ms delay
    }
}

#ifdef DEBUG
    unsigned long last_frame_time = 0;
    unsigned long current_frame_time = 0;
    unsigned long total_frame_time = 0;
    unsigned long frame_count = 0;
#endif
IRAM_ATTR void emulate()
{
    Bus nes;
    nes.insertCartridge(cart);
    nes.connectScreen(screen);
    nes.reset();
    ui.loadEmulatorSettings(&nes);

    TaskHandle_t apu_task_handle;
    xTaskCreatePinnedToCore(
    apuTask,
    "APU Task",
    1024,
    &nes.cpu.apu,
    1,
    &apu_task_handle,
    0
    );

    TaskHandle_t polling_task_handle;
    xTaskCreatePinnedToCore(
    pollingTask,
    "Polling Task",
    1024,
    &nes,
    1,
    &polling_task_handle,
    0
    );

    TaskHandle_t display_task_handle;
    xTaskCreatePinnedToCore(
    displayUpdateTask,
    "Display Update Task",
    2048,
    screen,
    1,
    &display_task_handle,
    1  // core 1
    );

    #ifdef DEBUG
        last_frame_time = esp_timer_get_time();
    #endif

    // Target frame time: 16639µs (60.098 FPS)
    #define FRAME_TIME 16639
    uint64_t next_frame = esp_timer_get_time();
    // Emulation Loop
    while (true) 
    {
        // Start + Select opens the pause menu
        if ((nes.controller & CONTROLLER::Start) && (nes.controller & CONTROLLER::Select)) 
        {
            if (!ui.paused)
            {
                vTaskSuspend(apu_task_handle);
                ui.pauseMenu(&nes);
                vTaskResume(apu_task_handle);
                next_frame = esp_timer_get_time() + FRAME_TIME;
                nes.controller = 0;
            }
        }

        // Generate one frame
        nes.clock();
        //screen->AcceptUpdates = !screen->AcceptUpdates;
        screen->updateAsync();
        #ifdef DEBUG
            current_frame_time = esp_timer_get_time();
            total_frame_time += (current_frame_time - last_frame_time);
            frame_count++;

            if ((frame_count & 63) == 0)
            {
                float avg_fps = (1000000.0 * frame_count) / total_frame_time;
                LOGF("FPS: %.2f\n", avg_fps);
                total_frame_time = 0;
                frame_count = 0;
            }

            last_frame_time = current_frame_time;
        #endif

        // Frame limiting
        #ifndef DEBUG
            uint64_t now = esp_timer_get_time();
            if (now < next_frame) ets_delay_us(next_frame - now);
        #endif
        next_frame += FRAME_TIME;
    }
    #undef FRAME_TIME
}

bool initSD() 
{
#if SD_TYPE == SD_TYPE_SD
    LOG("Initializing SD...");
    SD_SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN, SD_SPI, hw_config.sd_freq * 1000000)) 
#else
    LOG("Initializing LittleFS...");
    if (!LittleFS.begin())
#endif
    {
        LOG("SD Card Mount Failed");
        LittleFS.format();
        LOG("SD Card formatted");
        #if SD_TYPE != SD_TYPE_SD
        if (!LittleFS.begin()) {
            LOG("LittleFS mount failed after format");
        } else {
            LOG("LittleFS mounted after format");
        }
        #endif
        screen->setTextSize(1);
        screen->setFont();
        screen->setCursor(0, 0);
        screen->setTextColor(Color(255,255,255));
        const char* txt1 = "SD Init failed!";
        const char* txt2 = "Insert SD card or";
        const char* txt3 = "lower SD frequency";
        const char* txt4 = "in config.h";

        screen->println(txt1);
        screen->println(txt2);
        screen->println(txt3);
        screen->println(txt4);
        screen->Debug = true;
        return false;
    }

    LOG("SD Card initialized.");
    return true;
}

void invalidCartridge()
{
    screen->fillScreen(BG_COLOR);
    screen->setTextColor(ST7735_WHITE);
    screen->setTextDatum(MC_DATUM);
    screen->drawString("ROM Mapper not supported!", screen->width() / 2, screen->height() / 2, 2);
    screen->update();
    delay(3000);
    ESP.restart();
}

void setupI2SDAC()
{
#if defined(CONFIG_IDF_TARGET_ESP32)
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = 0,
        .dma_buf_count = 2,
        .dma_buf_len = 128,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    if (hw_config.dac_pin == 1) i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    
    if (hw_config.dac_pin == 0)
        i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN);
    else if (hw_config.dac_pin == 1)
        i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = 0,
        .dma_buf_count = 2,
        .dma_buf_len = 128,
        .use_apll = false,
        .tx_desc_auto_clear = true
    };

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        LOGF("I2S install failed: %d\n", err);
        return;
    }

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRC_PIN,
        .data_out_num = I2S_DOUT_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) {
        LOGF("I2S pin config failed: %d\n", err);
        return;
    }
    LOGF("I2S initialized: BCLK=%d, LRC=%d, DOUT=%d\n", I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DOUT_PIN);
#endif
}

void apuTask(void* param) 
{
    Apu2A03* apu = (Apu2A03*)param;

    while (true)
    {
        apu->clock();
    }
}

void pollingTask(void* param)
{
    Bus* nes = (Bus*)param;
    const TickType_t frameTicks = pdMS_TO_TICKS(1000 / 60);
    TickType_t lastWakeTime = xTaskGetTickCount();

    while (true)
    {
        // Read button input
        nes->controller = controllerRead();

        vTaskDelayUntil(&lastWakeTime, frameTicks);
    }
    
}
