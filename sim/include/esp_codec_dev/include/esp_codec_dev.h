#ifndef DESKCLOCK_SIM_ESP_CODEC_DEV_H
#define DESKCLOCK_SIM_ESP_CODEC_DEV_H

#include <cstdint>

using esp_codec_dev_handle_t = void *;

typedef struct {
  int bits_per_sample;
  int channel;
  int channel_mask;
  int sample_rate;
  int mclk_multiple;
} esp_codec_dev_sample_info_t;

inline int esp_codec_dev_open(esp_codec_dev_handle_t, const esp_codec_dev_sample_info_t *) { return 0; }
inline int esp_codec_dev_write(esp_codec_dev_handle_t, const void *, uint32_t) { return 0; }
inline int esp_codec_dev_read(esp_codec_dev_handle_t, void *, uint32_t) { return 0; }
inline void esp_codec_dev_set_out_vol(esp_codec_dev_handle_t, float) {}
inline void esp_codec_dev_set_in_gain(esp_codec_dev_handle_t, float) {}

#endif /* DESKCLOCK_SIM_ESP_CODEC_DEV_H */
