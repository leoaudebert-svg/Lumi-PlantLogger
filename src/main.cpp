// Production entry point for PlantLogger.

#include <M5Unified.h>
#include "button.h"

static const char* TAG = "main";

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("PlantLogger", M5.Display.width() / 2, M5.Display.height() / 2);

    ESP_LOGI(TAG, "PlantLogger started.");
}

void loop()
{
    M5.update();
    button::update();

    vTaskDelay(1);
}
