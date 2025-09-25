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
    static void telemetryPrint(void);
};

#endif /* EDUGRID_TELEMETRY_H_ */
