/*************************************************************************
 * @file edugrid_telemetry.h
 * @date 2025/08/18
 ************************************************************************/
#ifndef EDUGRID_TELEMETRY_H_
#define EDUGRID_TELEMETRY_H_

#include <Arduino.h>
#include <edugrid_states.h>
#include <edugrid_pwm_control.h>
#include <edugrid_mpp_algorithm.h>
#include <edugrid_measurement.h>

class edugrid_telemetry
{
public:
    // Optional helper that prints a detailed status dump to the serial console
    // when EDUGRID_TELEMETRY_ON is defined.  Useful during bring-up.
    static void telemetryPrint(void);
};

#endif /* EDUGRID_TELEMETRY_H_ */
