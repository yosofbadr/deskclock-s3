#include <Arduino.h>

#include "i2c_bsp.h"
#include "app/BoardPowerService.h"

#ifndef DESKCLOCK_FIRMWARE_VERSION
#define DESKCLOCK_FIRMWARE_VERSION "dev"
#endif

#ifdef AUDIO_TEST_MODE

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "codec_board/codec_board.h"
#include "codec_board/codec_init.h"
#include "esp_codec_dev/include/esp_codec_dev.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "tca9554/esp_io_expander_tca9554.h"

namespace {

constexpr int kSampleRate = 24000;
constexpr int kChannels = 2;
constexpr int kBitsPerSample = 16;
constexpr int kFramesPerChunk = 256;
constexpr float kPi = 3.14159265358979323846F;

esp_codec_dev_handle_t playback = nullptr;
esp_codec_dev_handle_t record = nullptr;
esp_io_expander_handle_t io_expander = nullptr;

bool enable_audio_expander()
{
  i2c_master_bus_handle_t bus = nullptr;
  esp_err_t err = i2c_master_get_bus_handle(0, &bus);
  if (err != ESP_OK || bus == nullptr) {
    Serial.printf("Audio test: failed to get I2C bus 0, err=%d\n", err);
    return false;
  }

  err = esp_io_expander_new_i2c_tca9554(bus, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &io_expander);
  if (err != ESP_OK) {
    Serial.printf("Audio test: failed to create TCA9554 expander, err=%d\n", err);
    return false;
  }

  // Matches Waveshare's audio example. This enables the board's audio power/control line.
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_7, IO_EXPANDER_OUTPUT));
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_7, 1));
  return true;
}

bool init_audio_codecs()
{
  set_codec_board_type("S3_LCD_3_49");

  codec_init_cfg_t codec_cfg = {
      .in_mode = CODEC_I2S_MODE_TDM,
      .out_mode = CODEC_I2S_MODE_TDM,
      .in_use_tdm = false,
      .reuse_dev = false,
  };

  int init_result = init_codec(&codec_cfg);
  if (init_result != 0) {
    Serial.printf("Audio test: init_codec failed, result=%d\n", init_result);
    return false;
  }

  playback = get_playback_handle();
  record = get_record_handle();
  if (playback == nullptr || record == nullptr) {
    Serial.printf("Audio test: missing codec handle playback=%p record=%p\n", playback, record);
    return false;
  }

  esp_codec_dev_set_out_vol(playback, 90.0);
  esp_codec_dev_set_in_gain(record, 35.0);

  esp_codec_dev_sample_info_t format = {};
  format.bits_per_sample = kBitsPerSample;
  format.channel = kChannels;
  format.channel_mask = 0;
  format.sample_rate = kSampleRate;
  format.mclk_multiple = 0;

  int open_playback = esp_codec_dev_open(playback, &format);
  int open_record = esp_codec_dev_open(record, &format);
  Serial.printf("Audio test: open playback=%d record=%d\n", open_playback, open_record);

  return open_playback == 0 && open_record == 0;
}

void play_tone(float frequency_hz, uint32_t duration_ms)
{
  if (playback == nullptr) {
    return;
  }

  int16_t buffer[kFramesPerChunk * kChannels];
  const uint32_t total_frames = (static_cast<uint64_t>(kSampleRate) * duration_ms) / 1000ULL;
  float phase = 0.0F;
  const float phase_step = (2.0F * kPi * frequency_hz) / static_cast<float>(kSampleRate);

  uint32_t frames_written = 0;
  while (frames_written < total_frames) {
    const uint32_t frames_this_chunk = min<uint32_t>(kFramesPerChunk, total_frames - frames_written);
    for (uint32_t frame = 0; frame < frames_this_chunk; ++frame) {
      const float envelope = 0.45F;
      const int16_t sample = static_cast<int16_t>(sinf(phase) * 32767.0F * envelope);
      buffer[frame * 2] = sample;
      buffer[(frame * 2) + 1] = sample;
      phase += phase_step;
      if (phase > 2.0F * kPi) {
        phase -= 2.0F * kPi;
      }
    }

    const uint32_t bytes = frames_this_chunk * kChannels * sizeof(int16_t);
    int written = esp_codec_dev_write(playback, buffer, bytes);
    if (written != 0) {
      Serial.printf("Audio test: playback write returned %d\n", written);
      return;
    }
    frames_written += frames_this_chunk;
  }
}

void record_mic_window(uint32_t duration_ms)
{
  if (record == nullptr) {
    return;
  }

  int16_t buffer[kFramesPerChunk * kChannels];
  const uint32_t total_frames = (static_cast<uint64_t>(kSampleRate) * duration_ms) / 1000ULL;
  uint32_t frames_read = 0;
  uint32_t samples_seen = 0;
  uint32_t peak = 0;
  uint64_t sum_abs = 0;

  while (frames_read < total_frames) {
    const uint32_t frames_this_chunk = min<uint32_t>(kFramesPerChunk, total_frames - frames_read);
    const uint32_t bytes = frames_this_chunk * kChannels * sizeof(int16_t);
    int read_result = esp_codec_dev_read(record, buffer, bytes);
    if (read_result != 0) {
      Serial.printf("Audio test: record read returned %d\n", read_result);
      return;
    }

    const uint32_t sample_count = frames_this_chunk * kChannels;
    for (uint32_t index = 0; index < sample_count; ++index) {
      const int32_t value = buffer[index];
      const uint32_t abs_value = static_cast<uint32_t>(value < 0 ? -value : value);
      if (abs_value > peak) {
        peak = abs_value;
      }
      sum_abs += abs_value;
      samples_seen++;
    }

    frames_read += frames_this_chunk;
  }

  const uint32_t avg_abs = samples_seen == 0 ? 0 : static_cast<uint32_t>(sum_abs / samples_seen);
  Serial.printf("Audio test: mic peak=%u avg_abs=%u\n", peak, avg_abs);
}

void run_audio_cycle()
{
  Serial.println("Audio test: playing three beeps. Listen for speaker output.");
  play_tone(523.25F, 180);
  delay(120);
  play_tone(659.25F, 180);
  delay(120);
  play_tone(783.99F, 300);

  Serial.println("Audio test: recording microphone level for 2 seconds. Talk/clap near the board.");
  record_mic_window(2000);
}

} // namespace

void setup()
{
  delay(50);
  Serial.begin(115200);
  delay(50);

  i2c_master_Init();
  DeskClock::BoardPowerService::begin();
  delay(450);

  Serial.printf("DeskClock S3 audio hardware test starting (%s)\n", DESKCLOCK_FIRMWARE_VERSION);
  Serial.printf("BoardPowerService: battery power hold %s\n", DeskClock::BoardPowerService::batteryPowerHoldEnabled() ? "enabled" : "unavailable");

  const bool expander_ok = enable_audio_expander();
  const bool codecs_ok = init_audio_codecs();
  Serial.printf("Audio test: expander=%s codecs=%s\n", expander_ok ? "ok" : "failed", codecs_ok ? "ok" : "failed");

  if (expander_ok && codecs_ok) {
    run_audio_cycle();
  }
}

void loop()
{
  delay(5000);
  run_audio_cycle();
}

#else

#include "app/AlarmService.h"
#include "app/AlarmToneService.h"
#include "app/BrightnessService.h"
#include "app/NetworkService.h"
#include "app/SettingsService.h"
#include "app/TimeService.h"
#include "app/TimeSetupView.h"
#include "lvgl_port.h"
#include "src/lcd_bl_bsp/lcd_bl_pwm_bsp.h"

namespace {
constexpr int kBootButtonPin = 0;
constexpr uint32_t kBootDebounceMs = 50;
constexpr uint32_t kBootSettingsLongPressMs = 1200;

bool boot_button_was_down = false;
uint32_t last_boot_button_change_ms = 0;
uint32_t boot_button_down_since_ms = 0;
bool boot_long_press_handled = false;

uint32_t boot_press_duration_ms(uint32_t now_ms)
{
  return now_ms >= boot_button_down_since_ms ? now_ms - boot_button_down_since_ms : 0;
}

void handle_boot_button()
{
  const bool down = digitalRead(kBootButtonPin) == LOW;
  const uint32_t now_ms = millis();

  if (down != boot_button_was_down && now_ms - last_boot_button_change_ms > kBootDebounceMs) {
    boot_button_was_down = down;
    last_boot_button_change_ms = now_ms;
    if (down) {
      boot_button_down_since_ms = now_ms;
      boot_long_press_handled = false;
      Serial.println("BOOT: pressed");
      if (DeskClock::AlarmService::activeAlert().active) {
        Serial.println("BOOT: dismissing active alarm");
        DeskClock::AlarmService::dismissActiveAlert();
        boot_long_press_handled = true;
      }
    } else {
      Serial.printf("BOOT: released after %lu ms\n", static_cast<unsigned long>(boot_press_duration_ms(now_ms)));
    }
  }

  if (boot_button_was_down && !boot_long_press_handled && now_ms - boot_button_down_since_ms >= kBootSettingsLongPressMs) {
    Serial.println("BOOT: long press opening setup");
    DeskClock::TimeSetupView::open();
    boot_long_press_handled = true;
  }
}
} // namespace

void setup()
{
  delay(50);
  Serial.begin(115200);
  delay(50);

  pinMode(kBootButtonPin, INPUT_PULLUP);

  i2c_master_Init();
  DeskClock::BoardPowerService::begin();
  delay(450);

  Serial.printf("DeskClock S3: booting RTC + LVGL shell (%s)\n", DESKCLOCK_FIRMWARE_VERSION);
  Serial.printf("BoardPowerService: battery power hold %s\n", DeskClock::BoardPowerService::batteryPowerHoldEnabled() ? "enabled" : "unavailable");

  DeskClock::SettingsService::begin();
  DeskClock::NetworkService::begin();
  DeskClock::TimeService::begin();
  DeskClock::AlarmService::begin(DeskClock::TimeService::snapshot().now);
  DeskClock::AlarmToneService::begin();
  lvgl_port_init();
  lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);
  DeskClock::BrightnessService::begin();

  Serial.println("DeskClock S3: display shell started");
}

void loop()
{
  DeskClock::TimeService::loop();
  DeskClock::TimeSnapshot snapshot = DeskClock::TimeService::snapshot();
  DeskClock::AlarmService::loop(snapshot.now);
  DeskClock::AlarmToneService::loop(DeskClock::AlarmService::activeAlert());
  DeskClock::BrightnessService::loop(snapshot.now);
  DeskClock::NetworkService::loop();
  handle_boot_button();
  delay(50);
}

#endif
