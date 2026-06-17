#include "knob.h"
#include "pincfg.h"
// #include "JPEGDEC.h"


// #include "scr_st77916.h"

static lv_obj_t *screen1 = NULL;
lv_obj_t *screen1_img = NULL;
static lv_obj_t *screen1_label = NULL;
static lv_obj_t *screen1_label2 = NULL;
static lv_obj_t *screen1_label3 = NULL;

static lv_obj_t *screen2 = NULL;
static lv_obj_t *screen2_list = NULL;

static int knob_cont = 0;
static int knob_conts = 0;
static int image_cnt = 0;

char filename[256][20];
size_t filesize[256];
int file_cnt = 0;

void knob_gui(void)
{
   screen1 = lv_obj_create(lv_scr_act());
   lv_obj_set_size(screen1,360,360);
   lv_obj_clear_flag(screen1, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
   lv_obj_set_flex_flow(screen1,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(screen1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
    lv_obj_set_style_bg_opa(screen1, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(screen1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(screen1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(screen1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(screen1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(screen1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(screen1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);

    screen1_img = lv_img_create(screen1);
    lv_obj_set_size(screen1_img,360,360);

    

    show_image(filename[image_cnt],filesize[image_cnt]);
    // image_cnt++;


  //  screen1_label = lv_label_create(screen1);
  //  lv_obj_align(screen1_label,LV_ALIGN_CENTER,-40,0);
  //  lv_label_set_text(screen1_label,"No Rotation");

  //  screen1_label2 = lv_label_create(lv_scr_act());
  //  lv_obj_align_to(screen1_label2,screen1_label,LV_ALIGN_OUT_RIGHT_MID,20,0);
  //  lv_label_set_text(screen1_label2,"0");

  //  screen1_label3 = lv_label_create(screen1);
  //  lv_obj_align_to(screen1_label3,screen1_label2,LV_ALIGN_OUT_RIGHT_MID,20,0);
  //  lv_label_set_text(screen1_label3,"0");
}

void knob_cb(lv_event_t *e)
{
  
} 

void knob_change(knob_event_t k,int cont)
{
    if(k == KNOB_LEFT)
    {
        // knob_cont = 0;
        // knob_conts--;
        // lv_label_set_text(screen1_label,"Knob Left");

        // lv_label_set_text_fmt(screen1_label2,"%d",cont);
        // lv_label_set_text_fmt(screen1_label3,"%d",knob_conts);

        image_cnt--;
        if(image_cnt < 0)
          image_cnt = file_cnt -1;


    }
    else if(k == KNOB_RIGHT)
    {
        // knob_cont++;
        // knob_conts = 0;
        // lv_label_set_text(screen1_label,"Knob Right");
        // lv_label_set_text_fmt(screen1_label3,"+%d",knob_cont);
        // lv_label_set_text_fmt(screen1_label2,"%d",cont);
        image_cnt++;
        if(image_cnt > file_cnt -1)
          image_cnt = 0;
    }
    show_image(filename[image_cnt],filesize[image_cnt]);
}

void send_file_name(const char *name,size_t file_size)
{
  snprintf(filename[file_cnt], 50, "%s", name);
  filesize[file_cnt] = file_size;
  file_cnt++;
}

// static void show_image(int cnt)
// {
//   static char jpeg_filename[256];
//   snprintf(jpeg_filename,sizeof(jpeg_filename),"/sdcard/%s",filename[cnt]);

//   File loadFile;
//   loadFile = SD_MMC.open(jpeg_filename, FILE_READ);
//   if (!loadFile)
//   {
//     return;
//   }

//   char buff[1024];
//   size_t bytesRead = 0;
//   while (loadFile.available() && bytesRead < MAX_PIC_FILE_SIZE)
//   {
//     size_t len = min((size_t)loadFile.available(), min(sizeof(buff), MAX_PIC_FILE_SIZE - bytesRead));
//     loadFile.read((uint8_t *)buff, len);
//     memcpy(img_read_data + bytesRead, buff, len);
//     bytesRead += len;
//   }
//   loadFile.close();

//   jpeg_error_t ret = JPEG_ERR_OK;

//   ret = esp_jpeg_decoder_one_image(img_read_data, bytesRead, (uint8_t *)pic_img_data);
//   if (ret != JPEG_ERR_OK)
//   {
//       esp_rom_printf("JPEG decode failed - %d\n", (int)ret);
//       return;
//   }
//   pic_img_dsc.data = pic_img_data;
//   lv_img_set_src(screen1_img,&pic_img_dsc);
// }

// jpeg_error_t esp_jpeg_decoder_one_image(uint8_t *input_buf, int len, uint8_t *output_buf)
// {
//   jpeg_error_t ret = JPEG_ERR_OK;
//   int inbuf_consumed = 0;

//   // Generate default configuration
//   jpeg_dec_config_t config = {
//       .output_type = JPEG_RAW_TYPE_RGB565_BE,
//       .rotate = JPEG_ROTATE_0D,
//   };

//   // Empty handle to jpeg_decoder
//   jpeg_dec_handle_t *jpeg_dec = NULL;

//   // Create jpeg_dec
//   jpeg_dec = jpeg_dec_open(&config);

//   // Create io_callback handle
//   jpeg_dec_io_t *jpeg_io = (jpeg_dec_io_t *)calloc(1, sizeof(jpeg_dec_io_t));
//   if (jpeg_io == NULL)
//   {
//     return JPEG_ERR_MEM;
//   }

//   // Create out_info handle
//   jpeg_dec_header_info_t *out_info = (jpeg_dec_header_info_t *)calloc(1, sizeof(jpeg_dec_header_info_t));
//   if (out_info == NULL)
//   {
//     return JPEG_ERR_MEM;
//   }
//   // Set input buffer and buffer len to io_callback
//   jpeg_io->inbuf = input_buf;
//   jpeg_io->inbuf_len = len;

//   // Parse jpeg picture header and get picture for user and decoder
//   ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
//   if (ret < 0)
//   {
//     esp_rom_printf("JPEG decode parse failed\n");
//     goto _exit;
//   }

//   jpeg_io->outbuf = output_buf;
//   inbuf_consumed = jpeg_io->inbuf_len - jpeg_io->inbuf_remain;
//   jpeg_io->inbuf = input_buf + inbuf_consumed;
//   jpeg_io->inbuf_len = jpeg_io->inbuf_remain;

//   // Start decode jpeg raw data
//   ret = jpeg_dec_process(jpeg_dec, jpeg_io);
//   if (ret < 0)
//   {
//     esp_rom_printf("JPEG decode process failed\n");
//     goto _exit;
//   }

// _exit:
//   // Decoder deinitialize
//   jpeg_dec_close(jpeg_dec);
//   free(out_info);
//   free(jpeg_io);
//   return ret;
// }
