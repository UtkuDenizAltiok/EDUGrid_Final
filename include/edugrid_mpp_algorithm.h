// File: edugrid_mpp_algorithm.h
/*************************************************************************
 * @file    edugrid_mpp_algorithm.h
 * @date    2025/09/25 (SYNCED WITH .CPP)
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
 * Operating modes
 ************************************************************************/
enum OperatingModes_t
{
    MANUALLY = 0,
    AUTO,
    IV_SWEEP,
    _NUM_VALUES
};

/*************************************************************************
 * Cycle times (microseconds)
 * (Used only for default init; runtime cadence comes from INA_STEP_PERIOD_MS)
 ************************************************************************/
struct CycleTimesUs
{
    unsigned long NORMAL;
    unsigned long MPPT;
};
inline constexpr CycleTimesUs CycleTimes_us{10UL * 1000UL, 500UL * 1000UL};  // legacy default

/*************************************************************************
 * Class
 ************************************************************************/
class edugrid_mpp_algorithm
{
public:
    /* ===== MPPT (Perturb & Observe) ===== */
    static int              find_mpp(void);
    static void             set_step_period_ms(uint32_t ms);

    /* ===== IV Sweep ===== */
    static void             request_iv_sweep();   // arm a new sweep
    static void             iv_sweep_step();      // non-blocking state machine

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

    // Non-blocking cadence shared by AUTO & IV
    static uint32_t         _mppt_update_period_ms;
    static uint32_t         _last_mppt_update_ms;

    /* ---------- IV sweep state machine ---------- */
    // Must match the .cpp usage: Idle -> Arm -> WaitAfterSet -> Sample -> Done
    enum class IVPhase : uint8_t { Idle = 0, Arm, WaitAfterSet, Sample, Done };
    static IVPhase          _iv_phase;
    static uint16_t         _iv_idx;        // current point index
    static uint16_t         _iv_count;      // number of points captured
    static uint32_t         _iv_last_ms;    // timing gate (~ INA_STEP_PERIOD_MS)

    /* ---------- IV sweep buffers (Vin, Iin) ---------- */
    static float            _iv_v[IV_SWEEP_POINTS];
    static float            _iv_i[IV_SWEEP_POINTS];

    /* ---------- Mode ---------- */
    static OperatingModes_t _mode_state;

    /* ---------- Helpers (optional) ---------- */
    static inline uint8_t   duty_from_index(uint16_t idx)
    {
        return (uint8_t)(IV_SWEEP_D_MIN_PCT + (idx * IV_SWEEP_STEP_PCT));
    }
};

#endif /* EDUGRID_MPP_ALGORITHM_H_ */
