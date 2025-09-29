/*************************************************************************
 * @file   edugrid_mpp_algorithm.cpp
 * @date   2025/09/22 (CORRECTED)
 ************************************************************************/

#include <edugrid_mpp_algorithm.h>
#include <edugrid_states.h>
#include <edugrid_pwm_control.h>
#include <edugrid_measurement.h>
#include <math.h>

/* ===== Static storage ===== */
// The MPPT and IV sweep share one cadence that aligns with the INA228 averaging
// window.  `_last_mppt_update_ms` stores the last time we applied a duty change
// so that both AUTO and IV_SWEEP respect the same timing budget.
uint32_t edugrid_mpp_algorithm::_mppt_update_period_ms = kDefaultStepPeriodMs;
uint32_t edugrid_mpp_algorithm::_last_mppt_update_ms    = 0;

void edugrid_mpp_algorithm::set_step_period_ms(uint32_t ms) {
  _mppt_update_period_ms = ms;
}

/* P&O */
float   edugrid_mpp_algorithm::_lastPin = 0.0f;
int8_t  edugrid_mpp_algorithm::_dir     = +1;

/* IV sweep */
edugrid_mpp_algorithm::IVPhase edugrid_mpp_algorithm::_iv_phase = IVPhase::Idle;
uint16_t edugrid_mpp_algorithm::_iv_idx   = 0;
uint16_t edugrid_mpp_algorithm::_iv_count = 0;
uint32_t edugrid_mpp_algorithm::_iv_last_ms = 0;
bool edugrid_mpp_algorithm::_iv_finalize_applied = false;

/* IV buffers */
float edugrid_mpp_algorithm::_iv_v[IV_SWEEP_POINTS] = {0};
float edugrid_mpp_algorithm::_iv_i[IV_SWEEP_POINTS] = {0};

/* Mode */
OperatingModes_t edugrid_mpp_algorithm::_mode_state = MANUALLY;


/* ===== Public API ===== */
OperatingModes_t edugrid_mpp_algorithm::get_mode_state(void)
{
    return _mode_state;
}

void edugrid_mpp_algorithm::set_mode_state(OperatingModes_t mode)
{
    _mode_state = mode;
  // Reset P&O direction and last power when entering AUTO
  if (mode == AUTO) {
    // Jumping into AUTO should not inherit stale slope information from a
    // previous run; reset the internal state so the next iteration starts
    // cleanly.
    _dir = +1;
    _lastPin = edugrid_measurement::P_in;
  }
}

void edugrid_mpp_algorithm::toggle_mode_state(void)
{
    OperatingModes_t next = (OperatingModes_t)((int)_mode_state + 1);
    if (next >= AUTO + 1) next = MANUALLY;
    set_mode_state(next);
}

int edugrid_mpp_algorithm::find_mpp(void)
{
  const uint32_t now = millis();
  if ((now - _last_mppt_update_ms) < _mppt_update_period_ms) {
    // Too early -> wait for the next INA228 averaged sample.
    return 0; // wait until INA average+settle window has passed
  }
  _last_mppt_update_ms = now;

  const float Pin = edugrid_measurement::P_in;
  const float dP  = Pin - _lastPin;
  _lastPin = Pin;

  // Fixed ±1% step, reverse direction when power drops (classic P&O)
  if (fabsf(dP) < MPP_POWER_EPS_W) {
    // tiny change: keep going same way
  } else if (dP < 0.0f) {
    _dir = -_dir; // power decreased ⇒ flip direction
  }

  edugrid_pwm_control::pwmIncrementDecrement((_dir >= 0) ? +MPPT_DUTY_STEP_PCT : -MPPT_DUTY_STEP_PCT);
  return 0;
}



/* ========================= IV SWEEP ========================= */

void edugrid_mpp_algorithm::request_iv_sweep()
{
  // A sweep is initiated from the web UI.  We reset the state machine so that
  // the next call to iv_sweep_step() starts from the minimum duty and builds a
  // brand new curve.
  _iv_finalize_applied = false;
  set_mode_state(IV_SWEEP);
  _iv_phase    = IVPhase::Arm;
  _iv_idx    = 0;
  _iv_count    = 0;
  _iv_last_ms  = 0;
}


void edugrid_mpp_algorithm::iv_sweep_step(void)
{
  const uint32_t now = millis();
  if ((now - _iv_last_ms) < _mppt_update_period_ms) {
    return;  // wait until a fresh INA228 average is ready
  }
  _iv_last_ms = now;

  const auto finalizeSweep = [&]() {
    if (_iv_finalize_applied) return;
    _iv_finalize_applied = true;
    edugrid_pwm_control::setPWM(PWM_MAX_DUTY_PCT);
    edugrid_pwm_control::requestManualTarget(PWM_MAX_DUTY_PCT);
    set_mode_state(MANUALLY);
  };

  switch (_iv_phase)
  {
    case IVPhase::Idle:
      return;

    case IVPhase::Arm:
      // Jump to the sweep start duty and wait one full averaging window
      edugrid_pwm_control::setPWM(IV_SWEEP_D_MIN_PCT);
      _iv_idx   = 0;
      _iv_count = 0;
      _iv_phase = IVPhase::Sample;
      return;

    case IVPhase::Sample:
      if (_iv_idx < IV_SWEEP_POINTS) {
        _iv_v[_iv_idx] = edugrid_measurement::V_in;
        _iv_i[_iv_idx] = edugrid_measurement::I_in;
        _iv_count = _iv_idx + 1;
      }

      if ((_iv_idx + 1 >= IV_SWEEP_POINTS) ||
          (edugrid_pwm_control::getPWM() >= IV_SWEEP_D_MAX_PCT)) {
        _iv_phase = IVPhase::Done;
        finalizeSweep();
        return;
      }

      ++_iv_idx;
      edugrid_pwm_control::pwmIncrementDecrement(+IV_SWEEP_STEP_PCT);
      return;

    case IVPhase::Done:
    default:
      finalizeSweep();
      return;
  }
}


/* ========================= Debug & Accessors ========================= */

void edugrid_mpp_algorithm::serial_debug(void)
{
    Serial.print("[MPPT] mode=");
    switch (_mode_state) {
      case MANUALLY: Serial.print("MANUALLY"); break;
      case AUTO:     Serial.print("AUTO");     break;
      case IV_SWEEP: Serial.print("IV_SWEEP"); break;
      default:       Serial.print("UNK");      break;
    }
    Serial.print(" PWM=");  Serial.print(edugrid_pwm_control::getPWM());
    Serial.print("% Pin="); Serial.print(edugrid_measurement::P_in, 2);
    Serial.print(" dP=");   Serial.print(edugrid_measurement::P_in - _lastPin, 2);
    Serial.print(" Dir=");  Serial.print(_dir);
    Serial.println();
}

bool edugrid_mpp_algorithm::iv_sweep_in_progress() { return _iv_phase > IVPhase::Idle && _iv_phase < IVPhase::Done; }
bool edugrid_mpp_algorithm::iv_sweep_done()        { return _iv_phase == IVPhase::Done; }
uint16_t edugrid_mpp_algorithm::iv_point_count()   { return _iv_count; }

void edugrid_mpp_algorithm::iv_get_point(uint16_t idx, float& v, float& i)
{
    if (idx < _iv_count) { v = _iv_v[idx]; i = _iv_i[idx]; }
    else { v = 0.0f; i = 0.0f; }
}