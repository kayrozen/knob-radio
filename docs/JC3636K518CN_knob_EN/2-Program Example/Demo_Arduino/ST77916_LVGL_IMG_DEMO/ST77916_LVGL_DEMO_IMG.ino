#include <Arduino.h>
#include "scr_st77916.h"
#include <lvgl.h>
#include <demos/lv_demos.h>
#include <SD_MMC.h>
#include "FS.h"

#include "hal/lv_hal.h"
#include "knob.h"

static uint8_t *img_read_data = NULL;
static size_t img_max_size = 0;

static uint16_t *pic_img_data = NULL;
static uint32_t img_ticker = 0;
static lv_img_dsc_t pic_img_dsc = {
    .header = {
        .cf = LV_IMG_CF_TRUE_COLOR,
        .always_zero = 0,
        .reserved = 0,
        .w = 360,
        .h = 360,
    },
    .data_size = 360 * 360 * 2,
    .data = NULL,
};

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }
  

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
      if(img_max_size < file.size())
        img_max_size = file.size();
      send_file_name(file.name(),file.size());
    }
    file = root.openNextFile();
  }
  file.close();
}

void setup()
{

  Serial.begin(115200);

  Serial.println("Pin change failed!");
  if(! SD_MMC.setPins(SD_MMC_CLK_PIN,SD_MMC_CMD_PIN, SD_MMC_D0_PIN, SD_MMC_D1_PIN, SD_MMC_D2_PIN, SD_MMC_D3_PIN)){
       Serial.println("Pin change failed!");
       return;
    }
  
  if (!SD_MMC.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  listDir(SD_MMC, "/pic", 0);

  pic_img_data = (uint16_t *)heap_caps_aligned_alloc(16, 360 * 360 * 2, MALLOC_CAP_SPIRAM);
  memset(pic_img_data, 0, 360 * 360 * 2);
  img_read_data = (uint8_t *)heap_caps_malloc(img_max_size, MALLOC_CAP_SPIRAM);
  memset(img_read_data, 0, img_max_size);
  
  delay(200);
  
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

void show_image(const char * filename,size_t file_size)
{
  static char jpeg_filename[256];
  snprintf(jpeg_filename,sizeof(jpeg_filename),"/sdcard/pic/%s",filename);
  Serial.printf("filename = %s",jpeg_filename);
  Serial.println();

  FILE *f = fopen(jpeg_filename,"rb");
  if(f == NULL)
  {
    Serial.println("open file failed");
    return;
  }

  size_t read_bytes = 0;
  read_bytes = fread(img_read_data,1,file_size,f);
  fclose(f);


  // File loadFile;
  // loadFile = fs.open(jpeg_filename);
  // if (!loadFile)
  // {
  //   Serial.println("open file failed");
  //   return;
  // }

  // char buff[1024];
  // size_t bytesRead = 0;
  // while (loadFile.available() && bytesRead < img_max_size)
  // {
  //   size_t len = min((size_t)loadFile.available(), min(sizeof(buff), img_max_size - bytesRead));
  //   loadFile.read((uint8_t *)buff, len);
  //   memcpy(img_read_data + bytesRead, buff, len);
  //   bytesRead += len;
  // }
  // loadFile.close();

  jpeg_error_t ret = JPEG_ERR_OK;

  ret = esp_jpeg_decoder_one_image(img_read_data, read_bytes, (uint8_t *)pic_img_data);
  if (ret != JPEG_ERR_OK)
  {
      esp_rom_printf("JPEG decode failed - %d\n", (int)ret);
      return;
  }
  pic_img_dsc.data = (uint8_t *)pic_img_data;
  lv_img_set_src(screen1_img,&pic_img_dsc);
}


jpeg_error_t esp_jpeg_decoder_one_image(uint8_t *input_buf, int len, uint8_t *output_buf)
{
  jpeg_error_t ret = JPEG_ERR_OK;
  int inbuf_consumed = 0;

  // Generate default configuration
  jpeg_dec_config_t config = {
      .output_type = JPEG_RAW_TYPE_RGB565_BE,
      .rotate = JPEG_ROTATE_0D,
  };

  // Empty handle to jpeg_decoder
  jpeg_dec_handle_t *jpeg_dec = NULL;

  // Create jpeg_dec
  jpeg_dec = jpeg_dec_open(&config);

  // Create io_callback handle
  jpeg_dec_io_t *jpeg_io = (jpeg_dec_io_t *)calloc(1, sizeof(jpeg_dec_io_t));
  if (jpeg_io == NULL)
  {
    return JPEG_ERR_MEM;
  }

  // Create out_info handle
  jpeg_dec_header_info_t *out_info = (jpeg_dec_header_info_t *)calloc(1, sizeof(jpeg_dec_header_info_t));
  if (out_info == NULL)
  {
    return JPEG_ERR_MEM;
  }
  // Set input buffer and buffer len to io_callback
  jpeg_io->inbuf = input_buf;
  jpeg_io->inbuf_len = len;

  // Parse jpeg picture header and get picture for user and decoder
  ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
  if (ret < 0)
  {
    esp_rom_printf("JPEG decode parse failed\n");
    goto _exit;
  }

  jpeg_io->outbuf = output_buf;
  inbuf_consumed = jpeg_io->inbuf_len - jpeg_io->inbuf_remain;
  jpeg_io->inbuf = input_buf + inbuf_consumed;
  jpeg_io->inbuf_len = jpeg_io->inbuf_remain;

  // Start decode jpeg raw data
  ret = jpeg_dec_process(jpeg_dec, jpeg_io);
  if (ret < 0)
  {
    esp_rom_printf("JPEG decode process failed\n");
    goto _exit;
  }

_exit:
  // Decoder deinitialize
  jpeg_dec_close(jpeg_dec);
  free(out_info);
  free(jpeg_io);
  return ret;
}
