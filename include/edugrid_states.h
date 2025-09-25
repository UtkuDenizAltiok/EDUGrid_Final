/*************************************************************************
 * @file    edugrid_states.h
 * @date    2025/09/25
 ************************************************************************/
#ifndef EDUGRID_STATES_H_
#define EDUGRID_STATES_H_

/*************************************************************************
 * Global feature flags
 ************************************************************************/
// #define MISSING_VOLTAGE_DRIVER_WORKAROUND
// #define EDUGRID_TELEMETRY_ON
#define OTA_UPDATES_ENABLE

/*************************************************************************
 * INA228 configuration (shared by AUTO & IV)
 ************************************************************************/
#define INA_PV_ADDR               (0x40)
#define INA_LOAD_ADDR             (0x44)
#define INA_SHUNT_OHMS            (0.01f)
#define INA_MAX_CURRENT_A         (16.0f)   /* set to your HW limit */

#define PV_PRESENT_V              (1.0f)

/* We use the INA’s built-in averaging (no software EMA for control paths). 
   Keep MEAS_ALPHA only for UI smoothing if you still want it on the web charts. */
#define MEAS_ALPHA                (0.15f)    /* UI-only */

#define INA_AVG_SAMPLES           (128UL)    /* AVG = 128 */
#define INA_CONV_US               (1052UL)   /* 1.052 ms per shunt/bus conversion */
#define INA_EXTRA_SETTLE_MS       (120UL)    /* extra dwell after duty change */

/* One shared step period for AUTO (P&O) and IV Sweep (ms). 
   2 conversions (shunt+bus) * AVG + settle */
#define INA_STEP_PERIOD_MS  ((uint32_t)((2UL * INA_CONV_US * INA_AVG_SAMPLES)/1000UL) + INA_EXTRA_SETTLE_MS)

/*************************************************************************
 * PWM / power stage
 ************************************************************************/
#define CONVERTER_FREQUENCY       (39000)    /* Hz */
#define PIN_POWER_CONVERTER_PWM   (33)
#define PIN_SD_ENABLE             (32)

/* Hard PWM bounds enforced by control code; keep consistent with IV sweep. */
#define PWM_MIN_DUTY_PCT          (5)
#define PWM_MAX_DUTY_PCT          (95)       /* << Max duty is 95% as requested */

/*************************************************************************
 * AUTO (P&O MPPT)
 ************************************************************************/
#define MPPT_DUTY_STEP_PCT        (1)        /* ±1% fixed step */
#define MPP_POWER_EPS_W           (0.02f)    /* tiny power delta = ignore flip */

/*************************************************************************
 * IV Sweep settings
 * We sweep integer duty percent values MIN..MAX inclusive in STEP increments.
 ************************************************************************/
#define IV_SWEEP_D_MIN_PCT        (5)        /* [%] */
#define IV_SWEEP_D_MAX_PCT        (95)       /* [%] keep aligned with PWM_MAX */
#define IV_SWEEP_STEP_PCT         (1)        /* [%] */

/* Derived: number of points, e.g., 5..95 step 1 => 91 points */
#define IV_SWEEP_POINTS  (((IV_SWEEP_D_MAX_PCT - IV_SWEEP_D_MIN_PCT) / IV_SWEEP_STEP_PCT) + 1)

/* Old software settle/averaging knobs are now unused because we rely on the INA’s
   built-in averaging + INA_STEP_PERIOD_MS cadence. Keep for compatibility = 0. */
#define IV_SETTLE_CYCLES          (0)
#define IV_SAMPLES_PER_POINT      (1)

/* Compile-time sanity checks */
#if (IV_SWEEP_D_MIN_PCT < PWM_MIN_DUTY_PCT) || (IV_SWEEP_D_MAX_PCT > PWM_MAX_DUTY_PCT)
# error "IV sweep bounds must lie within PWM_MIN_DUTY_PCT..PWM_MAX_DUTY_PCT"
#endif
#if (IV_SWEEP_D_MIN_PCT > IV_SWEEP_D_MAX_PCT)
# error "IV_SWEEP_D_MIN_PCT must be <= IV_SWEEP_D_MAX_PCT"
#endif
#if ((IV_SWEEP_D_MAX_PCT - IV_SWEEP_D_MIN_PCT) % IV_SWEEP_STEP_PCT) != 0
# error "Sweep range must be divisible by IV_SWEEP_STEP_PCT"
#endif

/*************************************************************************
 * Manual mode slew limiting (UI slider)
 ************************************************************************/
#define MANUAL_SLEW_STEP_PCT      (1)        /* 1% per ramp step */
#define MANUAL_SLEW_US_BETWEEN    (40000)    /* 40 ms between steps */

/*************************************************************************
 * Filesystem states (unchanged)
 ************************************************************************/
#define STATE_FILESYSTEM_OK       (0)
#define STATE_FILESYSTEM_ERROR    (-1)

/*************************************************************************
 * Include
 ************************************************************************/
#include <version_generated.h>

#endif /* EDUGRID_STATES_H_ */
