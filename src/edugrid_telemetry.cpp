/*************************************************************************
 * @file edugrid_telemetry.cpp
 * @date 2025/08/18
 ************************************************************************/
#include <edugrid_telemetry.h>

static const char* modeToStr(OperatingModes_t m) {
  switch (m) {
    case MANUALLY: return "MANUALLY";
    case AUTO:     return "AUTO";
    case IV_SWEEP: return "IV_SWEEP";
    default:       return "UNKNOWN";
  }
}

void edugrid_telemetry::telemetryPrint(void)
{
  // Human readable dump of the most important runtime values.  Triggered from
  // the MPPT task when EDUGRID_TELEMETRY_ON is defined.
  Serial.println("* ------------------------------------ *");
  Serial.println("* PWM CONTROL");
  Serial.println("* ------------------------------------ *");
  Serial.print("Freq / Hz: ");
  Serial.println(edugrid_pwm_control::getFrequency());
  Serial.print("PWM / %: ");
  Serial.println(edugrid_pwm_control::getPWM());

  Serial.println("* ------------------------------------ *");
  Serial.println("* MEASUREMENTS (INA228)");
  Serial.println("* ------------------------------------ *");
  Serial.print("V_in  [V]: "); Serial.println(edugrid_measurement::V_in, 3);
  Serial.print("I_in  [A]: "); Serial.println(edugrid_measurement::I_in, 3);
  Serial.print("P_in  [W]: "); Serial.println(edugrid_measurement::P_in, 2);
  Serial.print("V_out [V]: "); Serial.println(edugrid_measurement::V_out, 3);
  Serial.print("I_out [A]: "); Serial.println(edugrid_measurement::I_out, 3);
  Serial.print("P_out [W]: "); Serial.println(edugrid_measurement::P_out, 2);
  Serial.print("Eff   [%]: "); Serial.println(edugrid_measurement::eff * 100.0f, 1);

  Serial.println("* ------------------------------------ *");
  Serial.println("* MPPT");
  Serial.println("* ------------------------------------ *");
  Serial.print("Mode: "); Serial.println(modeToStr(edugrid_mpp_algorithm::get_mode_state()));

  Serial.println("* ------------------------------------ *");
  Serial.println("* MISC");
  Serial.println("* ------------------------------------ *");

  Serial.println();
}
