#ifndef DESKCLOCK_SIM_CODEC_INIT_H
#define DESKCLOCK_SIM_CODEC_INIT_H

#include "esp_codec_dev/include/esp_codec_dev.h"

#define CODEC_I2S_MODE_TDM 1

typedef struct {
  int in_mode;
  int out_mode;
  bool in_use_tdm;
  bool reuse_dev;
} codec_init_cfg_t;

inline int init_codec(const codec_init_cfg_t *) { return 0; }
inline esp_codec_dev_handle_t get_playback_handle()
{
  static int playback;
  return &playback;
}
inline esp_codec_dev_handle_t get_record_handle()
{
  static int record;
  return &record;
}

#endif /* DESKCLOCK_SIM_CODEC_INIT_H */
