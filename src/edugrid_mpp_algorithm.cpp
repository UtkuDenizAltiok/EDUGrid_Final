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
uint32_t edugrid_mpp_algorithm::_mppt_update_period_ms = CycleTimes_us.MPPT / 1000UL;
uint32_t edugrid_mpp_algorithm::_last_mppt_update_ms = 0;

/* P&O */
float    edugrid_mpp_algorithm::_lastPin  = 0.0f;
int8_t   edugrid_mpp_algorithm::_dir      = +1;

/* IV sweep */
edugrid_mpp_algorithm::IVPhase edugrid_mpp_algorithm::_iv_phase       = IVPhase::Idle;
uint16_t edugrid_mpp_algorithm::_iv_idx                               = 0;
uint8_t  edugrid_mpp_algorithm::_iv_settle_left                       = 0;
uint8_t  edugrid_mpp_algorithm::_iv_samples_left                      = 0;
float    edugrid_mpp_algorithm::_acc_v                                = 0.0f;
float    edugrid_mpp_algorithm::_acc_i                                = 0.0f; // Corrected the typo here
uint16_t edugrid_mpp_algorithm::_iv_count                             = 0;

/* IV buffers */
float    edugrid_mpp_algorithm::_iv_v[IV_SWEEP_POINTS] = {0};
float    edugrid_mpp_algorithm::_iv_i[IV_SWEEP_POINTS] = {0};

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
    if (mode == AUTO)
    {
        _dir = +1;
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
    if (millis() - _last_mppt_update_ms < _mppt_update_period_ms) {
        return 0;
    }
    _last_mppt_update_ms = millis();

    const float Pin = edugrid_measurement::P_in;
    if (edugrid_measurement::V_in < PV_PRESENT_V) {
        _lastPin = 0.0f;
        return 0;
    }

    const float dP = Pin - _lastPin;

    if (fabsf(dP) > MPP_POWER_DEADBAND_W) {
        if (dP < 0.0f) {
            _dir = -_dir;
        }
    }
    
    int current_pwm = edugrid_pwm_control::getPWM();
    int next_pwm = current_pwm + (_dir * MPP_DUTY_STEP);

    if (next_pwm < edugrid_pwm_control::getPwmLowerLimit()) {
        next_pwm = edugrid_pwm_control::getPwmLowerLimit();
    }
    if (next_pwm > edugrid_pwm_control::getPwmUpperLimit()) {
        next_pwm = edugrid_pwm_control::getPwmUpperLimit();
    }
    
    edugrid_pwm_control::setPWM((uint8_t)next_pwm, /*auto_mode=*/true);
    _lastPin = Pin;
    return 0;
}


/* ========================= IV SWEEP ========================= */

void edugrid_mpp_algorithm::request_iv_sweep()
{
    _iv_count        = 0;
    _iv_idx          = 0;
    _acc_v           = 0.0f;
    _acc_i           = 0.0f;
    _iv_settle_left  = IV_SETTLE_CYCLES;
    _iv_samples_left = IV_SAMPLES_PER_POINT;
    
    const uint8_t startDuty = duty_from_index(0);
    edugrid_pwm_control::setPWM(startDuty, /*auto_mode=*/false);

    _iv_phase = IVPhase::Settle;
    set_mode_state(IV_SWEEP);
}


void edugrid_mpp_algorithm::iv_sweep_step(void)
{
    switch (_iv_phase)
    {
    case IVPhase::Idle:
        return;

    case IVPhase::Settle:
        if (_iv_settle_left > 0) {
            _iv_settle_left--;
            return;
        }
        _acc_v = 0.0f; _acc_i = 0.0f;
        _iv_samples_left = IV_SAMPLES_PER_POINT;
        _iv_phase = IVPhase::Sample;
        return; // prevent fall-through to keep scopes simple

    case IVPhase::Sample:
        _acc_v += edugrid_measurement::V_in;
        _acc_i += edugrid_measurement::I_in;
        if (--_iv_samples_left > 0) {
            return;
        }
        if (_iv_count < IV_SWEEP_POINTS) {
            _iv_v[_iv_count] = _acc_v / (float)IV_SAMPLES_PER_POINT;
            _iv_i[_iv_count] = _acc_i / (float)IV_SAMPLES_PER_POINT;
            _iv_count++;
        }
        _iv_phase = IVPhase::Advance;
        return;

    case IVPhase::Advance: {              // <-- scoped block fixes the error
        _iv_idx++;

        if (_iv_idx >= IV_SWEEP_POINTS) {
            const uint8_t finalDuty = IV_SWEEP_D_MAX_PCT;
            edugrid_pwm_control::setPWM(finalDuty, /*auto_mode=*/false);
            set_mode_state(MANUALLY);
            _iv_phase = IVPhase::Done;
            return;
        }

        const uint8_t nextDuty = duty_from_index(_iv_idx);
        edugrid_pwm_control::setPWM(nextDuty, /*auto_mode=*/false);
        _iv_settle_left  = IV_SETTLE_CYCLES;
        _iv_phase        = IVPhase::Settle;
        return;
    }

    case IVPhase::Done:
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