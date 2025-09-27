/*************************************************************************
 * @file edugrid_pwm_control.cpp
 * @date 2025/08/18
 ************************************************************************/

#include <Arduino.h>
#include <edugrid_pwm_control.h>
#include <edugrid_mpp_algorithm.h>
#if CONFIG_FREERTOS_UNICORE == 0
  #include "freertos/FreeRTOS.h"
  #include "freertos/portmacro.h"
#endif

/* ===== static storage ===== */
uint8_t edugrid_pwm_control::pwm_power_converter   = PWM_ABS_INIT; // start safe
int     edugrid_pwm_control::frequency_power_converter = CONVERTER_FREQUENCY;
int     edugrid_pwm_control::power_converter_pin   = -1;
uint8_t edugrid_pwm_control::pwm_abs_min           = PWM_ABS_MIN_MPPT;
uint8_t edugrid_pwm_control::pwm_abs_max           = PWM_ABS_MAX_MPPT;
uint8_t edugrid_pwm_control::manual_target         = PWM_ABS_INIT;
uint32_t edugrid_pwm_control::manual_last_step_ms  = 0;

const int edugrid_pwm_control::_ledc_channel = 0; // use channel 0
const int edugrid_pwm_control::_ledc_timer   = 0; // use timer   0

#if CONFIG_FREERTOS_UNICORE == 0
static portMUX_TYPE s_pwmMux = portMUX_INITIALIZER_UNLOCKED;
#endif

/* ===== private helpers ===== */
void edugrid_pwm_control::_applyToHardware(uint8_t pwm_percent)
{
    if (pwm_percent > 100) pwm_percent = 100;
    const uint32_t ticks = (uint32_t)((pwm_percent / 100.0f) * PWM_RESOLUTION_STEPS + 0.5f);
    ledcWrite(_ledc_channel, ticks);
}

/* ===== public API ===== */
void edugrid_pwm_control::initPwmPowerConverter(int freq_hz, int pin)
{
    power_converter_pin = pin;
    frequency_power_converter = freq_hz;
    const uint8_t resolution_bits = 8;
    ledcSetup(_ledc_channel, (double)frequency_power_converter, resolution_bits);
    ledcAttachPin(power_converter_pin, _ledc_channel); // attach ONCE
    pwm_abs_min = PWM_ABS_MIN_MPPT;
    pwm_abs_max = PWM_ABS_MAX_MPPT;
    setPWM(PWM_ABS_INIT);
}

void edugrid_pwm_control::setPin(int pin)
{
    if (pin == power_converter_pin) return;
    power_converter_pin = pin;
    ledcAttachPin(power_converter_pin, _ledc_channel);
    _applyToHardware(pwm_power_converter);
}

void edugrid_pwm_control::setFrequency(float freq_hz)
{
    if (freq_hz <= 0) return;
    frequency_power_converter = (int)freq_hz;

    // Reconfigure LEDC
    const uint8_t resolution_bits = 8;
    ledcSetup(_ledc_channel, (double)frequency_power_converter, resolution_bits);

    // Re-apply current duty
    _applyToHardware(pwm_power_converter);
}

float edugrid_pwm_control::getFrequency()
{
    return (float)frequency_power_converter;
}

uint8_t edugrid_pwm_control::getFrequency_kHz()
{
    return (uint8_t)((frequency_power_converter + 500) / 1000); // rounded
}

void edugrid_pwm_control::setPWM(uint8_t pwm_in)
{
    if (pwm_in < pwm_abs_min) pwm_in = pwm_abs_min;
    if (pwm_in > pwm_abs_max) pwm_in = pwm_abs_max;
#if CONFIG_FREERTOS_UNICORE == 0
    portENTER_CRITICAL(&s_pwmMux);
#endif
    pwm_power_converter = pwm_in;
    _applyToHardware(pwm_power_converter);
#if CONFIG_FREERTOS_UNICORE == 0
    portEXIT_CRITICAL(&s_pwmMux);
#endif
}

uint8_t edugrid_pwm_control::getPWM()
{
    return pwm_power_converter;
}

float edugrid_pwm_control::getPWM_normalized()
{
    return pwm_power_converter / 100.0f;
}

void edugrid_pwm_control::requestManualTarget(uint8_t target)
{
    if (target < pwm_abs_min) target = pwm_abs_min;
    if (target > pwm_abs_max) target = pwm_abs_max;
    manual_target = target;
    manual_last_step_ms = millis() - MANUAL_SLEW_INTERVAL_MS;
}

void edugrid_pwm_control::serviceManualRamp()
{
    const OperatingModes_t mode = edugrid_mpp_algorithm::get_mode_state();
    const uint32_t now = millis();

    if (mode != MANUALLY) {
        manual_target = pwm_power_converter;
        manual_last_step_ms = now;
        return;
    }

    if (manual_target == pwm_power_converter) {
        return;
    }

    if ((now - manual_last_step_ms) < MANUAL_SLEW_INTERVAL_MS) {
        return;
    }

    manual_last_step_ms = now;

    int current = pwm_power_converter;
    const int target = manual_target;
    const int diff = target - current;

    if (diff > 0) {
        current += MANUAL_SLEW_STEP_PCT;
        if (current > target) {
            current = target;
        }
    } else {
        current -= MANUAL_SLEW_STEP_PCT;
        if (current < target) {
            current = target;
        }
    }

    setPWM(static_cast<uint8_t>(current));
}

void edugrid_pwm_control::pwmIncrementDecrement(int step)
{
    int val = (int)pwm_power_converter + step;
    if (val < 0)   val = 0;
    if (val > 100) val = 100;
    setPWM((uint8_t)val);
    // Align manual ramp state with the new duty to avoid fighting external updates
    manual_target = pwm_power_converter;
    manual_last_step_ms = millis();
}

uint8_t edugrid_pwm_control::getPwmLowerLimit()
{
    return pwm_abs_min;
}

uint8_t edugrid_pwm_control::getPwmUpperLimit()
{
    return pwm_abs_max;
}

void edugrid_pwm_control::checkAndSetPwmBorders()
{
    // Clamp cached duty to current borders and re-apply if needed
    uint8_t clamped = pwm_power_converter;
    if (clamped < pwm_abs_min) clamped = pwm_abs_min;
    if (clamped > pwm_abs_max) clamped = pwm_abs_max;

    if (clamped != pwm_power_converter) {
        pwm_power_converter = clamped;
        _applyToHardware(pwm_power_converter);
    }
}

