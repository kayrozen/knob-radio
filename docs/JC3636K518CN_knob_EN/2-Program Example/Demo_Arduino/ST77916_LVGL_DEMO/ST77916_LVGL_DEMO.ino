#include "scr_st77916.h"
#include <lvgl.h>
#include <demos/lv_demos.h>
#include "hal/lv_hal.h"
#include "SD_MMC.h"
#include "FS.h"
#include <sdmmc_cmd.h>
#include "knob.h"

void setup()
{
  delay(200);
  Serial.begin(115200);
  scr_lvgl_init();
  // lv_demo_widgets();
  //lv_demo_benchmark();
  // lv_demo_music();
  knob_gui();

}

void loop()
{
  lv_timer_handler();
  vTaskDelay(5);
}
