/*************************************************************************
 * @file    edugrid_states.h
 * @date    2023/11/15  (refreshed)
 ************************************************************************/

#ifndef EDUGRID_STATES_H_
#define EDUGRID_STATES_H_

/*************************************************************************
 * Defines
 ************************************************************************/

/** Filesystem States */
#define STATE_FILESYSTEM_OK       (0)
#define STATE_FILESYSTEM_ERROR    (-1)

/** Edugrid behavior feature flags */
// #define MISSING_VOLTAGE_DRIVER_WORKAROUND
// #define EDUGRID_TELEMETRY_ON
#define OTA_UPDATES_ENABLE

/* ======================= INA228 Config ======================= */
#define INA_PV_ADDR        (0x40)
#define INA_LOAD_ADDR      (0x44)
#define INA_SHUNT_OHMS     (0.01f)
#define INA_MAX_CURRENT_A  (16.0f)

/* Consider PV “absent” below this voltage (helps logic stay calm) */
#define PV_PRESENT_V       (1.0f)

/* Exponential smoothing factor for UI (0..1). 0.15 = gentle. */
#define MEAS_ALPHA         (0.15f)

/* ======================= IV Sweep Setup =======================
   We sweep integer duty *percent* values from MIN to MAX, inclusive,
   in steps of STEP. This guarantees we hit EVERY duty you want
   (e.g., 5,6,7,...,95 when STEP=1).
*/
#define IV_SWEEP_D_MIN_PCT        (5)    /* [%]  lower sweep bound   */
#define IV_SWEEP_D_MAX_PCT        (95)   /* [%]  upper sweep bound   */
#define IV_SWEEP_STEP_PCT         (1)    /* [%]  sweep increment     */

/* Safety/consistency checks at compile time */
#if (IV_SWEEP_D_MIN_PCT < 0) || (IV_SWEEP_D_MAX_PCT > 100)
# error "IV sweep bounds must be within 0..100 %"
#endif
#if (IV_SWEEP_D_MIN_PCT > IV_SWEEP_D_MAX_PCT)
# error "IV_SWEEP_D_MIN_PCT must be <= IV_SWEEP_D_MAX_PCT"
#endif
#if ((IV_SWEEP_D_MAX_PCT - IV_SWEEP_D_MIN_PCT) % IV_SWEEP_STEP_PCT) != 0
# error "Sweep range must be divisible by step (choose STEP so it fits exactly)"
#endif

/* Number of points implied by the bounds + step (e.g., 5..95 step 1 => 91) */
#define IV_SWEEP_POINTS  (((IV_SWEEP_D_MAX_PCT - IV_SWEEP_D_MIN_PCT) / IV_SWEEP_STEP_PCT) + 1)

/* Per-point timing/averaging for correctness and human-visible progress.
   These are interpreted by your sweep state machine in the .cpp
   (e.g., one step per Task-3 loop; settle for a few loops; average N samples).
*/
#define IV_SETTLE_CYCLES        (4)   /* More wait cycles for visible IV stepping */
#define IV_SAMPLES_PER_POINT    (6)   /* More averaging for smoother plotted curve */

/* ======================= PWM Control ======================== */
#define CONVERTER_FREQUENCY       (39000)   /* Hz */
#define PIN_POWER_CONVERTER_PWM   (33)
#define PIN_SD_ENABLE             (32)

/* Optional: if your PWM layer enforces its own safe bounds, keep these aligned.
   The IV sweep bounds above (5..95%) are already conservative for synchronous bucks.
   If you do add PWM limits there, ensure they match the sweep min/max.
*/

/*************************************************************************
 * Include
 ************************************************************************/
#include <version_generated.h>

#endif /* EDUGRID_STATES_H_ */
