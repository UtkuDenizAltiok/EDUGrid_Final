// File: edugrid_mpp_algorithm.h
/*************************************************************************
 * @file    edugrid_mpp_algorithm.h
 * @date    2025/08/18 (MODIFIED)
 * @ref     http://ww1.microchip.com/downloads/en/AppNotes/00001521A.pdf
 ************************************************************************/

#ifndef EDUGRID_MPP_ALGORITHM_H_
#define EDUGRID_MPP_ALGORITHM_H_

/*************************************************************************
 * Includes
 ************************************************************************/
#include <Arduino.h>
#include <edugrid_states.h>
#include <edugrid_measurement.h>
#include <edugrid_pwm_control.h>

/*************************************************************************
 * Tunables
 ************************************************************************/
/* Fixed-step Perturb & Observe (P&O) */
#define MPP_DUTY_STEP              (1)        /* [% duty per P&O cycle]   */
#define MPP_POWER_DEADBAND_W       (0.08f)    /* [W] ignore tiny changes  */

/* Operating modes */
enum OperatingModes_t
{
    MANUALLY = 0,
    AUTO,
    IV_SWEEP,
    _NUM_VALUES
};

/* Cycle times (microseconds) */
struct
{
    unsigned long NORMAL = 10UL  * 1000UL;
    unsigned long MPPT   = 500UL * 1000UL;   // 500 ms between P&O steps
} CycleTimes_us;

/*************************************************************************
 * Class
 ************************************************************************/
class edugrid_mpp_algorithm
{
public:
    /* ===== MPPT (Perturb & Observe) ===== */
    static int              find_mpp(void);

    /* ===== IV Sweep ===== */
    static void             request_iv_sweep();
    static void             iv_sweep_step(void);

    /* ===== IV Sweep data accessors ===== */
    static bool             iv_sweep_in_progress();
    static bool             iv_sweep_done();
    static uint16_t         iv_point_count();
    static void             iv_get_point(uint16_t idx, float& v, float& i);

    /* ===== Mode control ===== */
    static OperatingModes_t get_mode_state(void);
    static void             set_mode_state(OperatingModes_t mode);
    static void             toggle_mode_state(void);

    /* ===== Debug ===== */
    static void             serial_debug(void);

private:
    /* ---------- P&O state ---------- */
    static float            _lastPin;
    static int8_t           _dir;

    // MODIFIED: Added declarations for the P&O non-blocking timer
    static uint32_t         _mppt_update_period_ms;
    static uint32_t         _last_mppt_update_ms;

    /* ---------- IV sweep state machine ---------- */
    enum class IVPhase : uint8_t { Idle, Settle, Sample, Advance, Done };
    static IVPhase          _iv_phase;
    static uint16_t         _iv_idx;
    static uint8_t          _iv_settle_left;
    static uint8_t          _iv_samples_left;
    static float            _acc_v, _acc_i;
    static uint16_t         _iv_count;

    /* ---------- IV sweep buffers (Vin, Iin) ---------- */
    static float            _iv_v[IV_SWEEP_POINTS];
    static float            _iv_i[IV_SWEEP_POINTS];

    /* ---------- Mode ---------- */
    static OperatingModes_t _mode_state;

    /* ---------- Helpers (defined in .cpp) ---------- */
    static inline uint8_t   duty_from_index(uint16_t idx)
    {
        return (uint8_t)(IV_SWEEP_D_MIN_PCT + (idx * IV_SWEEP_STEP_PCT));
    }
};

#endif /* EDUGRID_MPP_ALGORITHM_H_ */