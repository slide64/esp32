#include "screen.h"
#include "board/Display_ST77916.h"
#include <lvgl.h>
#include <SD_MMC.h>
static lv_obj_t *gif, *leftEye, *rightEye, *mouth;
static bool customGif;
static bool fsReady(lv_fs_drv_t *) { return SD_MMC.cardType()!=CARD_NONE; }
static void *fsOpen(lv_fs_drv_t *,const char *path,lv_fs_mode_t mode) {
  if (mode!=LV_FS_MODE_RD) return nullptr;
  String name=path[0]=='/'?String(path):String("/")+path;
  auto f=new File(SD_MMC.open(name));
  if (!*f) { delete f; return nullptr; } return f;
}
static lv_fs_res_t fsClose(lv_fs_drv_t *,void *p) { delete static_cast<File*>(p); return LV_FS_RES_OK; }
static lv_fs_res_t fsRead(lv_fs_drv_t *,void *p,void *buf,uint32_t n,uint32_t *got) {
  *got=static_cast<File*>(p)->read(static_cast<uint8_t*>(buf),n); return LV_FS_RES_OK;
}
static lv_fs_res_t fsSeek(lv_fs_drv_t *,void *p,uint32_t pos,lv_fs_whence_t w) {
  auto &f=*static_cast<File*>(p);
  uint64_t target=pos;
  if (w==LV_FS_SEEK_CUR) target+=f.position();
  if (w==LV_FS_SEEK_END) target+=f.size();
  return target<=f.size() && f.seek(target)?LV_FS_RES_OK:LV_FS_RES_FS_ERR;
}
static lv_fs_res_t fsTell(lv_fs_drv_t *,void *p,uint32_t *pos) { *pos=static_cast<File*>(p)->position(); return LV_FS_RES_OK; }
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels) {
  LCD_addWindow(area->x1,area->y1,area->x2,area->y2,reinterpret_cast<uint16_t*>(pixels));
  lv_display_flush_ready(display);
}
bool initScreen() {
  Backlight_Init(); ST77916_Init();
  if (!LCD_IsReady()) return false;
  lv_init();
  lv_tick_set_cb([]()->uint32_t { return millis(); });
  auto buf=heap_caps_malloc(360*40*2,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
  if (!buf) return false;
  auto display=lv_display_create(360,360);
  lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(display,flush);
  lv_display_set_buffers(display,buf,nullptr,360*40*2,LV_DISPLAY_RENDER_MODE_PARTIAL);
  static lv_fs_drv_t fs;
  lv_fs_drv_init(&fs); fs.letter='S'; fs.ready_cb=fsReady;
  fs.open_cb=fsOpen; fs.close_cb=fsClose; fs.read_cb=fsRead; fs.seek_cb=fsSeek; fs.tell_cb=fsTell;
  lv_fs_drv_register(&fs);
  lv_obj_set_style_bg_color(lv_screen_active(),lv_color_black(),0);
  for (auto obj : {&leftEye,&rightEye,&mouth}) {
    *obj=lv_obj_create(lv_screen_active()); lv_obj_remove_style_all(*obj);
    lv_obj_set_style_bg_opa(*obj,LV_OPA_COVER,0);
    lv_obj_set_style_radius(*obj,16,0);
  }
  return true;
}
void showMonster(const char *name) {
  if (gif) { lv_obj_delete(gif); gif=nullptr; }
  String path=String("/monsters/")+name+".gif";
  customGif=SD_MMC.cardType()!=CARD_NONE && SD_MMC.exists(path);
  // Header dimensions are checked before passing a file to the decoder.
  if (customGif) {
    File f=SD_MMC.open(path); uint8_t h[10]{};
    customGif=f.read(h,10)==10 && !memcmp(h,"GIF8",4) &&
      (h[6]|(h[7]<<8))==360 && (h[8]|(h[9]<<8))==360;
  }
  if (customGif) {
    gif=lv_gif_create(lv_screen_active());
    lv_gif_set_src(gif,(String("S:")+path).c_str());
    customGif=lv_gif_is_loaded(gif);
    if (customGif) lv_obj_center(gif);
    else { lv_obj_delete(gif); gif=nullptr; Serial.println("GIF invalide : animation de secours"); }
  }
  for (auto obj : {leftEye,rightEye,mouth}) {
    if (customGif) lv_obj_add_flag(obj,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(obj,LV_OBJ_FLAG_HIDDEN);
  }
  if (!customGif) {
    uint32_t color=!strcmp(name,"pumpkin")?0xff6600:!strcmp(name,"demon")?0xee1010:0x62ff18;
    for (auto obj : {leftEye,rightEye,mouth}) lv_obj_set_style_bg_color(obj,lv_color_hex(color),0);
  }
}
void screenLoop() {
  if (!customGif && leftEye) {
    int pulse=(millis()/80)%20; pulse=pulse<10?pulse:20-pulse;
    int height=(millis()%4000<130)?5:24+pulse;
    lv_obj_set_size(leftEye,100,height); lv_obj_set_pos(leftEye,65,130+pulse/2);
    lv_obj_set_size(rightEye,100,height); lv_obj_set_pos(rightEye,195,130+pulse/2);
    lv_obj_set_size(mouth,140,12+pulse); lv_obj_set_pos(mouth,110,225);
  }
  lv_timer_handler();
}
void screenPower(bool enabled) { Set_Backlight(enabled?50:0); }
