/************************************************************************
 * @file   edugrid_measurement.cpp
 * @date   2025/08/30
 * @brief  INA228-based measurements
 ***********************************************************************/

#include "edugrid_measurement.h"
#include <math.h>
#include "edugrid_mpp_algorithm.h" 

/* ===== Static storage ===== */
// The INA drivers are held as static objects so the measurement class can be
// used without instantiation.  All modules access the public static members.
Adafruit_INA228 edugrid_measurement::_ina_pv;
Adafruit_INA228 edugrid_measurement::_ina_load;

bool  edugrid_measurement::_ok_pv   = false;
bool  edugrid_measurement::_ok_load = false;

float edugrid_measurement::V_in  = 0.0f;
float edugrid_measurement::I_in  = 0.0f;
float edugrid_measurement::P_in  = 0.0f;
float edugrid_measurement::V_out = 0.0f;
float edugrid_measurement::I_out = 0.0f;
float edugrid_measurement::P_out = 0.0f;
float edugrid_measurement::eff   = 0.0f;

/* ===== Private helpers / state ===== */
// Global offset corrections and last raw PV voltage; only this file needs them
// so they remain at translation-unit scope.
static float _I_in_off  = 0.0f;      // current offset (PV)
static float _I_out_off = 0.0f;      // current offset (LOAD)
static float _vin_raw_last = 0.0f;   // raw PV bus (before smoothing), for presence detect


static void _configureInaDevice(Adafruit_INA228& ina) {
  // The order matches the recommendations from the Adafruit driver: configure
  // the sense resistor value, choose averaging/conversion time, then enable the
  // continuous measurement mode so the chip keeps producing results in the
  // background.
  ina.setShunt(INA_SHUNT_OHMS, INA_MAX_CURRENT_A);
  ina.setAveragingCount(INA228_COUNT_128);          // AVG = 128 samples
  ina.setVoltageConversionTime(INA228_TIME_1052_us);
  ina.setCurrentConversionTime(INA228_TIME_1052_us);
  ina.setMode(INA228_MODE_CONT_BUS_SHUNT);          // voltage + current only
}


static void _calibrateZeroOffsets(size_t samples,
                                  Adafruit_INA228* ina_pv,   bool ok_pv,
                                  Adafruit_INA228* ina_load, bool ok_load) {
  // Allow calibration even if only one sensor is present; we simply skip the
  // missing channel instead of aborting everything.
  if (!ok_pv && !ok_load) return;

  float iin=0.0f, iout=0.0f;
  for (size_t i=0; i<samples; ++i) {
    // Convert mA to A once to keep the rest of the code consistent with the
    // public variables that are all stored in amperes.
    if (ok_pv)   { iin  += ina_pv->getCurrent_mA()  / 1000.0f; }
    if (ok_load) { iout += ina_load->getCurrent_mA() / 1000.0f; }
    delay(2);
  }
  if (ok_pv)   { _I_in_off  = iin  / samples; }
  if (ok_load) { _I_out_off = iout / samples; }

  Serial.printf("[CAL] Current offsets: Iin=%0.4f A, Iout=%0.4f A\n",
                _I_in_off, _I_out_off);
}

/* ===== Public API ===== */
void edugrid_measurement::init(void) {
  // If your PCB uses non-default I2C pins, call Wire.begin(SDA,SCL) earlier.
  Wire.begin();
  Wire.setClock(400000);

  _ok_pv   = _ina_pv.begin(INA_PV_ADDR);
  _ok_load = _ina_load.begin(INA_LOAD_ADDR);

  // Log the detected addresses so a new developer immediately sees which
  // sensors responded during boot.
  Serial.print("[INA] PV   @ 0x"); Serial.println(INA_PV_ADDR,   HEX);
  Serial.print("[INA] LOAD @ 0x"); Serial.println(INA_LOAD_ADDR, HEX);
  if (!_ok_pv || !_ok_load) {
    Serial.println("[INA] WARNING: device(s) not found (check I2C and addresses)");
  }

  if (_ok_pv)   { _configureInaDevice(_ina_pv); }
  if (_ok_load) { _configureInaDevice(_ina_load); }

  /* After both INAs are configured, tell the MPPT code the shared step period */
  // Align the MPPT cadence with the INA averaging window so each iteration uses
  // fresh samples.
  edugrid_mpp_algorithm::set_step_period_ms(INA_STEP_PERIOD_MS);
  Serial.printf("[INA] Step period = %lu ms (AVG %lu, conv %lu us, settle %lu ms)\n",
                (unsigned long)INA_STEP_PERIOD_MS,
                (unsigned long)INA_AVG_SAMPLES,
                (unsigned long)INA_CONV_US,
                (unsigned long)INA_EXTRA_SETTLE_MS);

  // One-time zero-offset capture (do this with PV/LOAD near 0 A for best accuracy)
  _calibrateZeroOffsets(300, &_ina_pv, _ok_pv, &_ina_load, _ok_load);
}

void edugrid_measurement::calibrateZeroOffsets(size_t samples) {
  _calibrateZeroOffsets(samples, &_ina_pv, _ok_pv, &_ina_load, _ok_load);
}

void edugrid_measurement::getSensors(void) {
  // Always read from hardware INA228
  _readINA();

  // No reverse readings in this topology; clamp negatives to zero
  if (V_in  < 0.0f) V_in  = 0.0f;
  if (V_out < 0.0f) V_out = 0.0f;
  if (I_in  < 0.0f) I_in  = 0.0f;
  if (I_out < 0.0f) I_out = 0.0f;

  const bool pv_present = (_vin_raw_last >= PV_PRESENT_V);

  // Compute powers from filtered V/I
  P_in  = V_in  * I_in;
  P_out = V_out * I_out;

  if (!pv_present) {
    // Keep V/I visible; only zero power & efficiency when PV absent
    P_in = P_out = 0.0f;
    eff  = 0.0f;
    return;
  }

  if (P_in > 1e-3f) {
    eff = P_out / P_in;
    if (eff < 0.0f)  eff = 0.0f;
    if (eff > 1.05f) eff = 1.05f;  // small guard above 100% due to sensor noise
  } else {
    eff = 0.0f;
  }
}

void edugrid_measurement::_readINA(void) {
  // RAW readings (no offsets yet)
  const float vin_raw  = _ok_pv   ? _ina_pv.getBusVoltage_V()           : 0.0f;
  const float iin_raw  = _ok_pv   ? _ina_pv.getCurrent_mA() / 1000.0f   : 0.0f;
  const float vout_raw = _ok_load ? _ina_load.getBusVoltage_V()         : 0.0f;
  const float iout_raw = _ok_load ? _ina_load.getCurrent_mA() / 1000.0f : 0.0f;

  // Save raw PV bus for presence detection
  _vin_raw_last = vin_raw;

  // Apply offsets to currents
  float vin  = vin_raw;
  float iin  = iin_raw  - _I_in_off;
  float vout = vout_raw;
  float iout = iout_raw - _I_out_off;

  // Deadband around zero
  if (fabsf(vin)  < ZERO_V_CLAMP)  vin  = 0.0f;
  if (fabsf(vout) < ZERO_V_CLAMP)  vout = 0.0f;
  if (fabsf(iin)  < ZERO_I_CLAMP)  iin  = 0.0f;
  if (fabsf(iout) < ZERO_I_CLAMP)  iout = 0.0f;

  V_in  = vin;
  I_in  = iin;
  V_out = vout;
  I_out = iout;
}
