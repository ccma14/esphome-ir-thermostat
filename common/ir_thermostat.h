#include <Arduino.h>
#include <math.h>

// --- Follow Me Tuning Parameters ---

struct RoomProfile {
  float tau;      // Gain smoothing time constant (seconds)
  float hide_max; // Max offset ("lie") between reported and actual temp (°F)
  float step_max; // Max slew rate per execution tick (°F)
};

// Profiles indexed by room size: 0 = Small, 1 = Medium, 2 = Large
const RoomProfile ROOM_PROFILES[] = {
    {600.0f, 1.50f, 0.15f},  // Small
    {900.0f, 3.00f, 0.10f},  // Medium
    {1800.0f, 4.50f, 0.05f}, // Large
};
constexpr int ROOM_PROFILE_COUNT =
    sizeof(ROOM_PROFILES) / sizeof(ROOM_PROFILES[0]);

// -----------------------------------

float get_correction(int setting) {
  // Index maps |setting| → multiplier: 0→1.0×, 1→1.5×, 2→2.0×, 3→3.0×
  // Negative settings invert the multiplier (i.e. divide).
  // Note: -3 was softened from 4.0× to 3.0× to avoid over-correction.
  const float lookup[] = {1.0f, 1.5f, 2.0f, 3.0f};
  if (setting < -3 || setting > 3)
    return 1.0f;
  return setting < 0 ? 1.0f / lookup[-setting] : lookup[setting];
}

// Convert Celsius to Fahrenheit
float c_to_f(float c_val) { return 32.0f + c_val * (9.0f / 5.0f); }

// Convert Fahrenheit to Celsius
float f_to_c(float f_val) { return (f_val - 32.0f) * (5.0f / 9.0f); }

// Symmetrically clamp a value to ±limit. Named sym_clamp to avoid
// colliding with std::clamp (available since C++17).
float sym_clamp(float val, float limit) {
  if (val > limit)
    return limit;
  if (val < -limit)
    return -limit;
  return val;
}

const RoomProfile &get_room_profile(int index) {
  if (index < 0)
    index = 0;
  if (index >= ROOM_PROFILE_COUNT)
    index = ROOM_PROFILE_COUNT - 1;
  return ROOM_PROFILES[index];
}

// Encode an adjusted Fahrenheit temperature (from get_compensated_temp) into
// a Midea "Follow Me" IR payload. Replace this function to support other
// mini-split models/protocols.
// ref:
// https://github.com/esphome/feature-requests/issues/1627#issuecomment-1365639966
std::vector<uint8_t> midea_follow_me_code(float adjusted_temp_f) {
  int t = (int)roundf(adjusted_temp_f);
  if (t < 32)
    t = 32;
  if (t > 99)
    t = 99;
  ESP_LOGI("follow.me", "Follow Me: sending %d°F", t);
  return {0xA4, 0xA3, 0x6B, 0x7F, (uint8_t)(t - 31)};
}

float get_compensated_temp(float current_celsius, float target_celsius,
                           int correction_setting, int room_size_index,
                           float *out_k, float *out_adj) {
  static float k_filtered = 1.0f;
  static float adjusted_prev = NAN; // NaN = uninitialized
  static uint32_t last_run = 0;

  // Guard: if sensor values aren't ready yet, skip this tick rather than
  // poisoning the static adjusted_prev with NaN on first initialization.
  if (isnan(current_celsius) || isnan(target_celsius)) {
    return NAN;
  }

  float temp_f = c_to_f(current_celsius);
  float target_f = c_to_f(target_celsius);
  float k_target = get_correction(correction_setting);

  // Dynamically calculate time delta (dt) in seconds so YAML is the single
  // source of truth
  uint32_t now = millis();
  float dt = 10.0f;
  if (last_run != 0) {
    dt = (now - last_run) /
         1000.0f; // subtraction already uint32_t; handles rollover
  }
  last_run = now;

  if (isnan(adjusted_prev)) {
    k_filtered = k_target;
    adjusted_prev = target_f; // Start smoothly
  }

  const RoomProfile &profile = get_room_profile(room_size_index);

  // 1) Smooth the gain (k)
  k_filtered = k_filtered + (k_target - k_filtered) * (dt / profile.tau);

  // 2) Calculate Adjusted Temperature
  float delta_t = (temp_f - target_f) * k_filtered;
  float adjusted_target = target_f + delta_t;

  // 3) Cap the "Hide" amount
  adjusted_target =
      temp_f + sym_clamp(adjusted_target - temp_f, profile.hide_max);

  // 4) Slew-rate limit the output
  adjusted_prev += sym_clamp(adjusted_target - adjusted_prev, profile.step_max);

  ESP_LOGI("follow.me",
           "Target.T: %.2f | Raw.T: %.2f | k_filt: %.2f | Adj.T: %.2f",
           target_f, temp_f, k_filtered, adjusted_prev);

  // Export internal tracking variables
  if (out_k != nullptr)
    *out_k = k_filtered;
  if (out_adj != nullptr)
    *out_adj = adjusted_prev;

  return adjusted_prev;
}
