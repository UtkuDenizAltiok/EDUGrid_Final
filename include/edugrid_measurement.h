/*************************************************************************
 * @file edugrid_measurement.h
 * @date 2023/11/05
 *
 ************************************************************************/

#ifndef EDUGRID_MEASUREMENTS_H_
#define EDUGRID_MEASUREMENTS_H_

/*************************************************************************
 * Include
 ************************************************************************/
#include <Arduino.h>
#include <Wire.h>
#include <edugrid_states.h>
#include <Adafruit_INA228.h>

/*************************************************************************
 * Class
 ************************************************************************/

/** edugrid_measurement
 * Class with static members for all measurement tasks
 */
class edugrid_measurement{
public:
  /**
   * @brief Initialize measurement subsystem (I2C + INA228 calibration)
   * Call once from setup(), after Wire.begin() if you use custom pins.
   */
  static void init(void);

  static void calibrateZeroOffsets(size_t samples = 300);

  /**
   * @brief Update all cached measurements once.
   * Call once per MPPT loop cycle (Task 3).
   * Reads both INA228 devices.
   * Also computes P_in, P_out, and efficiency.
   */
  static void getSensors(void);

  /* ================= Public cached values =================
   * These mirror the original project’s style so other modules
   * (telemetry, logging, MPPT) can read them directly.
   */
  static float V_in;    ///< PV bus voltage [V]
  static float I_in;    ///< PV current [A]
  static float P_in;    ///< PV power [W]

  static float V_out;   ///< Load/output voltage [V]
  static float I_out;   ///< Load/output current [A]
  static float P_out;   ///< Output power [W]

  static float eff;     ///< Efficiency [0..1], computed as P_out / P_in

  /* =============== Convenience getters ==================== */
  static inline float getVoltagePV(void)   { return V_in;  }
  static inline float getCurrentPV(void)   { return I_in;  }
  static inline float getVoltageLoad(void) { return V_out; }
  static inline float getCurrentLoad(void) { return I_out; }

private:
  /* ================= INA228 handles & state =============== */
  static Adafruit_INA228 _ina_pv;    ///< INA228 at INA_PV_ADDR
  static Adafruit_INA228 _ina_load;  ///< INA228 at INA_LOAD_ADDR

  static bool _ok_pv;
  static bool _ok_load;

  /**
   * @brief Low-level read of both INA228s into V_in/I_in and V_out/I_out.
   * Assumes _ok_pv/_ok_load reflect successful begin().
   */
  static void _readINA(void);
};

#endif /* EDUGRID_MEASUREMENTS_H_ */