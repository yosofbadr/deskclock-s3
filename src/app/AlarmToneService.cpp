#include "AlarmToneService.h"

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#include <stdint.h>

#include "BoardPowerService.h"
#include "codec_board/codec_board.h"
#include "codec_board/codec_init.h"
#include "esp_codec_dev/include/esp_codec_dev.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

namespace DeskClock {
namespace {

constexpr int kSampleRate = 24000;
constexpr int kChannels = 2;
constexpr int kBitsPerSample = 16;
constexpr int kFramesPerChunk = 96;
constexpr float kPi = 3.14159265358979323846F;
constexpr uint32_t kBeepIntervalMs = 900;
constexpr uint32_t kToneChunkMs = 90;
constexpr const char *kPreferencesNamespace = "deskclock";
constexpr const char *kAudioEnabledKey = "aud_en";
constexpr const char *kAlarmVolumeKey = "alarm_vol";

esp_codec_dev_handle_t playback = nullptr;
bool initialized = false;
bool init_attempted = false;
uint32_t active_started_ms = 0;
uint8_t active_alarm_id = 0;
uint32_t last_beep_ms = 0;
float phase = 0.0F;
AlarmToneSettings current_settings;

uint8_t clamp_volume(uint8_t volume)
{
  if (volume < 5) {
    return 5;
  }
  if (volume > 100) {
    return 100;
  }
  return volume;
}

void apply_volume()
{
  if (playback != nullptr) {
    esp_codec_dev_set_out_vol(playback, static_cast<float>(current_settings.volume));
  }
}

void save_settings()
{
  Preferences preferences;
  if (!preferences.begin(kPreferencesNamespace, false)) {
    Serial.println("AlarmToneService: failed to open preferences for write");
    return;
  }
  preferences.putBool(kAudioEnabledKey, current_settings.enabled);
  preferences.putUChar(kAlarmVolumeKey, current_settings.volume);
  preferences.end();
}

bool enable_audio_expander()
{
  return BoardPowerService::enableAudioPower();
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
    Serial.printf("AlarmToneService: init_codec failed, result=%d\n", init_result);
    return false;
  }

  playback = get_playback_handle();
  if (playback == nullptr) {
    Serial.println("AlarmToneService: missing playback handle");
    return false;
  }

  apply_volume();

  esp_codec_dev_sample_info_t format = {};
  format.bits_per_sample = kBitsPerSample;
  format.channel = kChannels;
  format.channel_mask = 0;
  format.sample_rate = kSampleRate;
  format.mclk_multiple = 0;

  int open_playback = esp_codec_dev_open(playback, &format);
  Serial.printf("AlarmToneService: open playback=%d\n", open_playback);
  return open_playback == 0;
}

void play_tone_chunk(float frequency_hz, uint32_t duration_ms)
{
  if (playback == nullptr) {
    return;
  }

  int16_t buffer[kFramesPerChunk * kChannels];
  const uint32_t total_frames = (static_cast<uint64_t>(kSampleRate) * duration_ms) / 1000ULL;
  const float phase_step = (2.0F * kPi * frequency_hz) / static_cast<float>(kSampleRate);

  uint32_t frames_written = 0;
  while (frames_written < total_frames) {
    const uint32_t frames_this_chunk = min<uint32_t>(kFramesPerChunk, total_frames - frames_written);
    for (uint32_t frame = 0; frame < frames_this_chunk; ++frame) {
      const float envelope = 0.38F;
      const int16_t sample = static_cast<int16_t>(sinf(phase) * 32767.0F * envelope);
      buffer[frame * 2] = sample;
      buffer[(frame * 2) + 1] = sample;
      phase += phase_step;
      if (phase > 2.0F * kPi) {
        phase -= 2.0F * kPi;
      }
    }

    const uint32_t bytes = frames_this_chunk * kChannels * sizeof(int16_t);
    if (esp_codec_dev_write(playback, buffer, bytes) != 0) {
      return;
    }
    frames_written += frames_this_chunk;
  }
}

} // namespace

namespace AlarmToneService {

void begin()
{
  Preferences preferences;
  if (preferences.begin(kPreferencesNamespace, true)) {
    if (preferences.isKey(kAudioEnabledKey)) {
      current_settings.enabled = preferences.getBool(kAudioEnabledKey, current_settings.enabled);
    }
    if (preferences.isKey(kAlarmVolumeKey)) {
      current_settings.volume = clamp_volume(preferences.getUChar(kAlarmVolumeKey, current_settings.volume));
    }
    preferences.end();
  }

  if (init_attempted || !current_settings.enabled) {
    return;
  }
  init_attempted = true;
  initialized = enable_audio_expander() && init_audio_codecs();
  Serial.printf("AlarmToneService: %s\n", initialized ? "ready" : "unavailable");
}

void loop(const ActiveAlarmAlert &alert)
{
  if (!initialized || !current_settings.enabled || !alert.active || !alert.sound_allowed) {
    active_alarm_id = 0;
    active_started_ms = 0;
    return;
  }

  const uint32_t now_ms = millis();
  if (active_alarm_id != alert.alarm.id) {
    active_alarm_id = alert.alarm.id;
    active_started_ms = now_ms;
    last_beep_ms = 0;
  }

  if (now_ms - active_started_ms > static_cast<uint32_t>(kAlarmSoundLimitSeconds) * 1000UL) {
    return;
  }

  if (last_beep_ms == 0 || now_ms - last_beep_ms >= kBeepIntervalMs) {
    last_beep_ms = now_ms;
    play_tone_chunk(880.0F, kToneChunkMs);
    play_tone_chunk(1174.66F, kToneChunkMs);
  }
}

void testTone()
{
  if (!initialized || !current_settings.enabled) {
    return;
  }
  play_tone_chunk(523.25F, 140);
  play_tone_chunk(659.25F, 140);
  play_tone_chunk(783.99F, 180);
}

bool available()
{
  return initialized;
}

AlarmToneSettings settings()
{
  return current_settings;
}

void updateSettings(const AlarmToneSettings &settings)
{
  current_settings = settings;
  current_settings.volume = clamp_volume(current_settings.volume);
  save_settings();
  apply_volume();
  if (current_settings.enabled && !init_attempted) {
    begin();
  }
  Serial.printf("AlarmToneService: audio=%s volume=%u\n", current_settings.enabled ? "on" : "off", current_settings.volume);
}

} // namespace AlarmToneService
} // namespace DeskClock
