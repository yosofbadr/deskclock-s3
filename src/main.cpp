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

namespace {

constexpr int kSampleRate = 24000;
constexpr int kChannels = 2;
constexpr int kBitsPerSample = 16;
constexpr int kFramesPerChunk = 256;
constexpr float kPi = 3.14159265358979323846F;

esp_codec_dev_handle_t playback = nullptr;
esp_codec_dev_handle_t record = nullptr;

bool enable_audio_expander()
{
  // Matches Waveshare's audio example, but routes through BoardPowerService so
  // enabling audio cannot reset the shared TCA9554 SYS_EN power-hold pin.
  return DeskClock::BoardPowerService::enableAudioPower();
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

#include "app/AlarmManagerView.h"
#include "app/AlarmService.h"
#include "app/AlarmToneService.h"
#include "app/AssetService.h"
#include "app/BrightnessService.h"
#include "app/BrightnessSettingsView.h"
#include "app/NetworkService.h"
#include "app/NetworkSetupView.h"
#include "app/SettingsService.h"
#include "app/SystemMenuView.h"
#include "app/TimeService.h"
#include "app/TimeSetupView.h"
#include "esp_system.h"
#include "lvgl_port.h"
#include "src/lcd_bl_bsp/lcd_bl_pwm_bsp.h"

namespace {
constexpr int kBootButtonPin = 0;
constexpr int kPowerButtonPin = 16;
constexpr uint32_t kButtonDebounceMs = 50;
constexpr uint32_t kBootSettingsLongPressMs = 1200;
constexpr uint32_t kPowerOffLongPressMs = 1600;

struct ButtonState {
  int pin = -1;
  const char *name = "button";
  bool down = false;
  bool long_handled = false;
  uint32_t last_change_ms = 0;
  uint32_t down_since_ms = 0;
};

ButtonState boot_button{kBootButtonPin, "BOOT"};
ButtonState power_button{kPowerButtonPin, "PWR"};

uint32_t button_press_duration_ms(const ButtonState &button, uint32_t now_ms)
{
  return now_ms >= button.down_since_ms ? now_ms - button.down_since_ms : 0;
}

void cycle_reset_brightness()
{
  const uint8_t next = DeskClock::BrightnessService::cyclePreset(1);
  Serial.printf("RESET: brightness preset -> %u (%u levels)\n", next, DeskClock::BrightnessService::presetCount());
}

bool should_cycle_brightness_for_reset(esp_reset_reason_t reason, bool pwr_held_at_boot)
{
  if (reason == ESP_RST_EXT || reason == ESP_RST_USB) {
    return true;
  }
  // On ESP32 boards, the RESET/EN button can be reported as POWERON. Avoid
  // cycling brightness when the board is being intentionally started by PWR.
  return reason == ESP_RST_POWERON && !pwr_held_at_boot;
}

bool activate_active_text_menu()
{
  if (DeskClock::NetworkSetupView::isOpen()) {
    DeskClock::NetworkSetupView::activateSelected();
    return true;
  }
  if (DeskClock::AlarmManagerView::isOpen()) {
    DeskClock::AlarmManagerView::activateSelected();
    return true;
  }
  if (DeskClock::BrightnessSettingsView::isOpen()) {
    DeskClock::BrightnessSettingsView::activateSelected();
    return true;
  }
  if (DeskClock::TimeSetupView::isOpen()) {
    DeskClock::TimeSetupView::activateSelected();
    return true;
  }
  if (DeskClock::SystemMenuView::isOpen()) {
    DeskClock::SystemMenuView::activateSelected();
    return true;
  }
  return false;
}

void handle_boot_short_press()
{
  if (!activate_active_text_menu()) {
    Serial.println("BOOT: opening settings menu");
    DeskClock::SystemMenuView::open();
  }
}

void handle_power_short_press()
{
  Serial.println("PWR: short press ignored; hold to power off");
}

void update_button(ButtonState &button, uint32_t now_ms)
{
  const bool down = digitalRead(button.pin) == LOW;
  if (down != button.down && now_ms - button.last_change_ms > kButtonDebounceMs) {
    button.down = down;
    button.last_change_ms = now_ms;
    if (down) {
      button.down_since_ms = now_ms;
      button.long_handled = false;
      Serial.printf("%s: pressed\n", button.name);
      if (button.pin == kBootButtonPin && DeskClock::AlarmService::activeAlert().active) {
        Serial.println("BOOT: dismissing active alarm");
        DeskClock::AlarmService::dismissActiveAlert();
        button.long_handled = true;
      }
    } else {
      const uint32_t duration = button_press_duration_ms(button, now_ms);
      Serial.printf("%s: released after %lu ms\n", button.name, static_cast<unsigned long>(duration));
      if (!button.long_handled) {
        if (button.pin == kBootButtonPin) {
          handle_boot_short_press();
        } else if (button.pin == kPowerButtonPin) {
          handle_power_short_press();
        }
      }
    }
  }

  if (button.down && !button.long_handled) {
    const uint32_t duration = button_press_duration_ms(button, now_ms);
    if (button.pin == kBootButtonPin && duration >= kBootSettingsLongPressMs) {
      Serial.println("BOOT: long press opening settings menu");
      DeskClock::SystemMenuView::open();
      button.long_handled = true;
    } else if (button.pin == kPowerButtonPin && duration >= kPowerOffLongPressMs) {
      Serial.println("PWR: long press releasing power hold");
      DeskClock::BoardPowerService::releaseBatteryPowerHold();
      button.long_handled = true;
    }
  }
}

void handle_board_buttons()
{
  const uint32_t now_ms = millis();
  update_button(boot_button, now_ms);
  update_button(power_button, now_ms);
}
} // namespace

void setup()
{
  delay(50);
  Serial.begin(115200);
  delay(50);
  const esp_reset_reason_t reset_reason = esp_reset_reason();
  Serial.printf("Reset reason: %d\n", static_cast<int>(reset_reason));

  pinMode(kBootButtonPin, INPUT_PULLUP);
  pinMode(kPowerButtonPin, INPUT_PULLUP);
  const bool pwr_held_at_boot = digitalRead(kPowerButtonPin) == LOW;
  Serial.printf("PWR held at boot: %u\n", pwr_held_at_boot ? 1U : 0U);

  i2c_master_Init();
  DeskClock::BoardPowerService::begin();
  delay(450);

  Serial.printf("DeskClock S3: booting RTC + LVGL shell (%s)\n", DESKCLOCK_FIRMWARE_VERSION);
  Serial.printf("BoardPowerService: battery power hold %s\n", DeskClock::BoardPowerService::batteryPowerHoldEnabled() ? "enabled" : "unavailable");

  DeskClock::SettingsService::begin();
  DeskClock::AssetService::begin();
  DeskClock::NetworkService::begin();
  DeskClock::TimeService::begin();
  DeskClock::AlarmService::begin(DeskClock::TimeService::snapshot().now);
  DeskClock::AlarmToneService::begin();
  lvgl_port_init();
  lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);
  DeskClock::BrightnessService::begin();
  if (should_cycle_brightness_for_reset(reset_reason, pwr_held_at_boot)) {
    cycle_reset_brightness();
  }

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
  handle_board_buttons();
  delay(50);
}

#endif
