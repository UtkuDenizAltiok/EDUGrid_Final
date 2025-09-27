/*************************************************************************
 * @file edugrid_pwm_control.h
 * @date 2025/08/18
 * @brief Control of the buck converter PWM (LEDC, percent-based API)
 ************************************************************************/

#ifndef EDUGRID_PWM_CONTROL_H_
#define EDUGRID_PWM_CONTROL_H_

/*************************************************************************
 * Includes
 ************************************************************************/
#include <Arduino.h>
#include <edugrid_states.h>

/*************************************************************************
 * Defines
 ************************************************************************/
#define TIMER_PWM_POWER_CONVERTER (0)
#define PWM_RESOLUTION_STEPS      (255)   // 8-bit LEDC resolution (0..255)

// Absolute borders for MPPT / manual (percent, 0..100)
#define PWM_ABS_MIN_MPPT  (5)    // [%]
#define PWM_ABS_MAX_MPPT  (95)   // [%]
#define PWM_ABS_INIT      (10)   // [%] (start at safe low duty)

/*************************************************************************
 * Class
 ************************************************************************/
class edugrid_pwm_control
{
public:
    /* Percent-based API (0..100 %) */
    static void     setPWM(uint8_t pwm_in);
    static uint8_t  getPWM();                  // 0..100 [%]
    static float    getPWM_normalized();       // 0.0..1.0
    static void     requestManualTarget(uint8_t target);
    static void     serviceManualRamp();

    /* Frequency API */
    static void     setFrequency(float freq_hz);   // reconfigure LEDC timer
    static float    getFrequency();                // [Hz]
    static uint8_t  getFrequency_kHz();            // rounded [kHz]

    /* Pin configuration */
    static void     setPin(int pin);
    static void     initPwmPowerConverter(int freq_hz, int pin);

    /* Adjust duty in steps (signed) */
    static void     pwmIncrementDecrement(int step = 5);

    /* Borders */
    static uint8_t  getPwmLowerLimit();
    static uint8_t  getPwmUpperLimit();
    static void     checkAndSetPwmBorders();

private:
    static void     _applyToHardware(uint8_t pwm_percent);

    /* cached state */
    static uint8_t  pwm_power_converter;      // [%]
    static int      frequency_power_converter; // [Hz]
    static int      power_converter_pin;      // GPIO
    static uint8_t  pwm_abs_min;              // [%]
    static uint8_t  pwm_abs_max;              // [%]
    static uint8_t  manual_target;            // [%]
    static uint32_t manual_last_step_ms;      // [ms]

    /* LEDC config */
    static const int _ledc_channel;           // LEDC channel used
    static const int _ledc_timer;             // LEDC timer used
};

#endif /* EDUGRID_PWM_CONTROL_H_ */
