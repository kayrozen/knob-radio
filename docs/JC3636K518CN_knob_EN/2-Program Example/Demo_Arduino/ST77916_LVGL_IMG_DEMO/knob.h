#ifndef _KNOB_H
#define _KNOB_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"
#include "bidi_switch_knob.h"
#include <ESP32_JPEG_Library.h>

extern lv_obj_t *screen1_img;

void knob_gui(void);
void knob_cb(lv_event_t *e);

void knob_change(knob_event_t k,int cont);
void send_file_name(const char *name,size_t file_size);

jpeg_error_t esp_jpeg_decoder_one_image(uint8_t *input_buf, int len, uint8_t *output_buf);
void show_image(const char *filename,size_t file_size);

#ifdef __cplusplus
}
#endif

#endif