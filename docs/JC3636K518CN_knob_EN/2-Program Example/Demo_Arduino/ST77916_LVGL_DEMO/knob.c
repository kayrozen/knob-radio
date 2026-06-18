#include "knob.h"
// #include "scr_st77916.h"

static lv_obj_t *screen1 = NULL;
static lv_obj_t *screen1_img = NULL;
static lv_obj_t *screen1_label = NULL;
static lv_obj_t *screen1_label2 = NULL;
static lv_obj_t *screen1_label3 = NULL;

static lv_obj_t *screen2 = NULL;
static lv_obj_t *screen2_list = NULL;

static int knob_cont = 0;
static int knob_conts = 0;

void knob_gui(void)
{
   screen1 = lv_obj_create(lv_scr_act());
   lv_obj_set_size(screen1,360,360);
   
   screen1_label = lv_label_create(screen1);
   lv_obj_align(screen1_label,LV_ALIGN_CENTER,-40,0);
   lv_label_set_text(screen1_label,"No Rotation");

   screen1_label2 = lv_label_create(lv_scr_act());
   lv_obj_align_to(screen1_label2,screen1_label,LV_ALIGN_OUT_RIGHT_MID,20,0);
   lv_label_set_text(screen1_label2,"0");

   screen1_label3 = lv_label_create(screen1);
   lv_obj_align_to(screen1_label3,screen1_label2,LV_ALIGN_OUT_RIGHT_MID,20,0);
   lv_label_set_text(screen1_label3,"0");
}

void knob_cb(lv_event_t *e)
{
  
} 

void knob_change(knob_event_t k,int cont)
{
    if(k == KNOB_LEFT)
    {
        knob_cont = 0;
        knob_conts--;
        lv_label_set_text(screen1_label,"Knob Left");

        lv_label_set_text_fmt(screen1_label2,"%d",cont);
        lv_label_set_text_fmt(screen1_label3,"%d",knob_conts);
    }
    else if(k == KNOB_RIGHT)
    {
        knob_cont++;
        knob_conts = 0;
        lv_label_set_text(screen1_label,"Knob Right");
        lv_label_set_text_fmt(screen1_label3,"+%d",knob_cont);
        lv_label_set_text_fmt(screen1_label2,"%d",cont);
    }
}

