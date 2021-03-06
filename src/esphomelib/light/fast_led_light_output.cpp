#include "esphomelib/defines.h"

#ifdef USE_FAST_LED_LIGHT

#include "esphomelib/light/fast_led_light_output.h"
#include "esphomelib/log.h"

ESPHOMELIB_NAMESPACE_BEGIN

namespace light {

static const char *TAG = "light.fast_led";

LightTraits FastLEDLightOutputComponent::get_traits() {
  return {true, true, false, false};
}
void FastLEDLightOutputComponent::write_state(LightState *state) {
  LightColorValues value = state->get_current_values();
  uint8_t max_brightness = roundf(value.get_brightness() * value.get_state() * 255.0f);
  this->correction_.set_max_brightness(max_brightness);

  if (this->is_effect_active())
    return;

  float red, green, blue;
  state->current_values_as_rgb(&red, &green, &blue);
  CRGB crgb = CRGB(red * 255, green * 255, blue * 255);

  for (int i = 0; i < this->num_leds_; i++)
    this->leds_[i] = crgb;

  this->schedule_show();
}
void FastLEDLightOutputComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up FastLED light...");
  this->controller_->init();
  this->controller_->setLeds(this->leds_, this->num_leds_);
  this->effect_data_ = new uint8_t[this->num_leds_];
  if (!this->max_refresh_rate_.has_value()) {
    this->set_max_refresh_rate(this->controller_->getMaxRefreshRate());
  }
  ESP_LOGCONFIG(TAG, "    Max refresh rate: %u", *this->max_refresh_rate_);
}
void FastLEDLightOutputComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "FastLED light:");
  ESP_LOGCONFIG(TAG, "    Num LEDs: %u", this->num_leds_);
  ESP_LOGCONFIG(TAG, "    Max refresh rate: %u", *this->max_refresh_rate_);

}
void FastLEDLightOutputComponent::loop() {
  if (!this->next_show_ && !this->is_effect_active())
    return;

  uint32_t now = micros();
  // protect from refreshing too often
  if (*this->max_refresh_rate_ != 0 && (now - this->last_refresh_) < *this->max_refresh_rate_) {
    return;
  }
  this->last_refresh_ = now;
  this->next_show_ = false;

  ESP_LOGVV(TAG, "Writing RGB values to bus...");

#ifdef USE_OUTPUT
  if (this->power_supply_ != nullptr) {
    bool is_on = false;
    for (int i = 0; i < this->num_leds_; i++) {
      if (bool(this->leds_[i])) {
        is_on = true;
        break;
      }
    }

    if (is_on && !this->has_requested_high_power_) {
      this->power_supply_->request_high_power();
      this->has_requested_high_power_ = true;
    }
    if (!is_on && this->has_requested_high_power_) {
      this->power_supply_->unrequest_high_power();
      this->has_requested_high_power_ = false;
    }
  }
#endif

  this->controller_->showLeds();
}
void FastLEDLightOutputComponent::schedule_show() {
  this->next_show_ = true;
}
CLEDController &FastLEDLightOutputComponent::add_leds(CLEDController *controller, int num_leds) {
  this->controller_ = controller;
  this->num_leds_ = num_leds;
  this->leds_ = new CRGB[num_leds];

  for (int i = 0; i < this->num_leds_; i++)
    this->leds_[i] = CRGB::Black;

  return *this->controller_;
}
CLEDController *FastLEDLightOutputComponent::get_controller() const {
  return this->controller_;
}
void FastLEDLightOutputComponent::set_max_refresh_rate(uint32_t interval_us) {
  this->max_refresh_rate_ = interval_us;
}
float FastLEDLightOutputComponent::get_setup_priority() const {
  return setup_priority::HARDWARE;
}
#ifdef USE_OUTPUT
void FastLEDLightOutputComponent::set_power_supply(PowerSupplyComponent *power_supply) {
  this->power_supply_ = power_supply;
}
void FastLEDLightOutputComponent::set_correction(float red, float green, float blue) {
  this->correction_.set_max_brightness(ESPColor(
      uint8_t(roundf(red * 255.0f)),
      uint8_t(roundf(green * 255.0f)),
      uint8_t(roundf(blue * 255.0f)),
      0
  ));
}
ESPColorView FastLEDLightOutputComponent::operator[](int32_t index) const {
  return ESPColorView(&this->leds_[index].r, &this->leds_[index].g, &this->leds_[index].b, nullptr,
                      &this->effect_data_[index], &this->correction_);
}
int32_t FastLEDLightOutputComponent::size() const {
  return this->num_leds_;
}
void FastLEDLightOutputComponent::setup_state(LightState *state) {
  this->correction_.calculate_gamma_table(state->get_gamma_correct());
}
void FastLEDLightOutputComponent::clear_effect_data() {
  for (int i = 0; i < this->size(); i++)
    this->effect_data_[i] = 0;
}

#endif

} // namespace light

ESPHOMELIB_NAMESPACE_END

#endif //USE_FAST_LED_LIGHT
