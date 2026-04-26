// Production entry point — Rally Co-Pilot S3R
// This file is compiled by the [env:m5stack-atoms3] environment only.
// For driver tests, use: pio run -e test_imu  or  pio run -e test_gps

#include <M5Unified.h>
#include "config.h"

static const char* TAG = "main";

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);
    ESP_LOGI(TAG, "Rally Co-Pilot S3R — production build placeholder.");
}

void loop()
{
    vTaskDelay(1000);
}
