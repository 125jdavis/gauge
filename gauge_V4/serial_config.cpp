/*
 * ========================================
 * SERIAL CONFIGURATION COMMAND HANDLER
 * ========================================
 *
 * Implements GET / SET / SAVE / LOAD / DUMP / RESET / PING / VERSION
 * for the PC configuration tool (CONFIG_TOOL_SPECIFICATION.md §3).
 */

#include "serial_config.h"
#include "globals.h"
#include "config_calibration.h"
#include <EEPROM.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// Firmware version string (sent in response to "version" command)
// ---------------------------------------------------------------------------
static const char FIRMWARE_VERSION[] PROGMEM = "version 4.0";

// ---------------------------------------------------------------------------
// Helper macros
// ---------------------------------------------------------------------------
#define SERIAL_PRINTLN_F(s) Serial.println(F(s))

// ---------------------------------------------------------------------------
// Forward declarations for default-value helpers (used by handleReset)
// ---------------------------------------------------------------------------
static void applyDefaults(void);

// ---------------------------------------------------------------------------
// Parameter printing helpers
// ---------------------------------------------------------------------------
static void printUint8Param(const char *name, uint8_t val) {
    Serial.print(name);
    Serial.print(' ');
    Serial.println(val);
}
static void printUint16Param(const char *name, uint16_t val) {
    Serial.print(name);
    Serial.print(' ');
    Serial.println(val);
}
static void printIntParam(const char *name, int val) {
    Serial.print(name);
    Serial.print(' ');
    Serial.println(val);
}
static void printFloatParam(const char *name, float val, uint8_t decimals = 6) {
    Serial.print(name);
    Serial.print(' ');
    Serial.println(val, decimals);
}

// ---------------------------------------------------------------------------
// handleGet
// ---------------------------------------------------------------------------
void handleGet(const char *param) {
    // Motor sweep steps
    if (strcmp_P(param, PSTR("m1_sweep"))        == 0) { printUint16Param("m1_sweep",        M1_SWEEP);                   return; }
    if (strcmp_P(param, PSTR("m2_sweep"))        == 0) { printUint16Param("m2_sweep",        M2_SWEEP);                   return; }
    if (strcmp_P(param, PSTR("m3_sweep"))        == 0) { printUint16Param("m3_sweep",        M3_SWEEP);                   return; }
    if (strcmp_P(param, PSTR("m4_sweep"))        == 0) { printUint16Param("m4_sweep",        M4_SWEEP);                   return; }
    if (strcmp_P(param, PSTR("ms_sweep"))        == 0) { printUint16Param("ms_sweep",        MS_SWEEP);                   return; }
    if (strcmp_P(param, PSTR("ms_zero_delay"))   == 0) { printUint16Param("ms_zero_delay",   MS_ZERO_STEP_DELAY_US);      return; }
    if (strcmp_P(param, PSTR("ms_zero_factor"))  == 0) { printFloatParam ("ms_zero_factor",  MS_ZERO_SWEEP_FACTOR, 4);   return; }
    if (strcmp_P(param, PSTR("motor_sweep_ms"))  == 0) { printUint16Param("motor_sweep_ms",  MOTOR_SWEEP_TIME_MS);        return; }
    // Analog sensor filters
    if (strcmp_P(param, PSTR("filter_vbatt"))    == 0) { printUint8Param ("filter_vbatt",    FILTER_VBATT);               return; }
    if (strcmp_P(param, PSTR("vbatt_scaler"))    == 0) { printFloatParam ("vbatt_scaler",    VBATT_SCALER, 6);            return; }
    if (strcmp_P(param, PSTR("filter_fuel"))     == 0) { printUint8Param ("filter_fuel",     FILTER_FUEL);                return; }
    if (strcmp_P(param, PSTR("filter_therm"))    == 0) { printUint8Param ("filter_therm",    FILTER_THERM);               return; }
    if (strcmp_P(param, PSTR("filter_av1"))      == 0) { printUint8Param ("filter_av1",      FILTER_AV1);                 return; }
    if (strcmp_P(param, PSTR("filter_av2"))      == 0) { printUint8Param ("filter_av2",      FILTER_AV2);                 return; }
    if (strcmp_P(param, PSTR("filter_av3"))      == 0) { printUint8Param ("filter_av3",      FILTER_AV3);                 return; }
    // Hall speed sensor
    if (strcmp_P(param, PSTR("revs_per_km"))     == 0) { printUint16Param("revs_per_km",     REVS_PER_KM);                return; }
    if (strcmp_P(param, PSTR("teeth_per_rev"))   == 0) { printUint8Param ("teeth_per_rev",   TEETH_PER_REV);              return; }
    if (strcmp_P(param, PSTR("filter_hall"))     == 0) { printUint8Param ("filter_hall",     FILTER_HALL_SPEED);          return; }
    if (strcmp_P(param, PSTR("hall_speed_min"))  == 0) { printUint8Param ("hall_speed_min",  HALL_SPEED_MIN);             return; }
    // RPM sensor
    if (strcmp_P(param, PSTR("cyl_count"))       == 0) { printUint8Param ("cyl_count",       CYL_COUNT);                  return; }
    if (strcmp_P(param, PSTR("filter_rpm"))      == 0) { printUint8Param ("filter_rpm",      FILTER_ENGINE_RPM);          return; }
    if (strcmp_P(param, PSTR("rpm_debounce_us")) == 0) { printUint16Param("rpm_debounce_us", RPM_DEBOUNCE_MICROS);        return; }
    if (strcmp_P(param, PSTR("engine_rpm_min"))  == 0) { printUint8Param ("engine_rpm_min",  ENGINE_RPM_MIN);             return; }
    // Speedometer
    if (strcmp_P(param, PSTR("speedo_max"))      == 0) { printUint16Param("speedo_max",      SPEEDO_MAX);                 return; }
    // LED tachometer
    if (strcmp_P(param, PSTR("num_leds"))        == 0) { printUint8Param ("num_leds",        NUM_LEDS);                   return; }
    if (strcmp_P(param, PSTR("warn_leds"))       == 0) { printUint8Param ("warn_leds",       WARN_LEDS);                  return; }
    if (strcmp_P(param, PSTR("shift_leds"))      == 0) { printUint8Param ("shift_leds",      SHIFT_LEDS);                 return; }
    if (strcmp_P(param, PSTR("tach_max"))        == 0) { printUint16Param("tach_max",        TACH_MAX);                   return; }
    if (strcmp_P(param, PSTR("tach_min"))        == 0) { printUint16Param("tach_min",        TACH_MIN);                   return; }
    // Odometer motor
    if (strcmp_P(param, PSTR("odo_steps"))       == 0) { printUint16Param("odo_steps",       ODO_STEPS);                  return; }
    if (strcmp_P(param, PSTR("odo_motor_teeth")) == 0) { printUint8Param ("odo_motor_teeth", ODO_MOTOR_TEETH);            return; }
    if (strcmp_P(param, PSTR("odo_gear_teeth"))  == 0) { printUint8Param ("odo_gear_teeth",  ODO_GEAR_TEETH);             return; }
    // Signal sources
    if (strcmp_P(param, PSTR("speed_source"))    == 0) { printUint8Param ("speed_source",    SPEED_SOURCE);               return; }
    if (strcmp_P(param, PSTR("rpm_source"))      == 0) { printUint8Param ("rpm_source",      RPM_SOURCE);                 return; }
    if (strcmp_P(param, PSTR("oil_prs_source"))  == 0) { printUint8Param ("oil_prs_source",  OIL_PRS_SOURCE);             return; }
    if (strcmp_P(param, PSTR("fuel_prs_source")) == 0) { printUint8Param ("fuel_prs_source", FUEL_PRS_SOURCE);            return; }
    if (strcmp_P(param, PSTR("coolant_src"))     == 0) { printUint8Param ("coolant_src",     COOLANT_TEMP_SOURCE);        return; }
    if (strcmp_P(param, PSTR("oil_temp_src"))    == 0) { printUint8Param ("oil_temp_src",    OIL_TEMP_SOURCE);            return; }
    if (strcmp_P(param, PSTR("map_source"))      == 0) { printUint8Param ("map_source",      MAP_SOURCE);                 return; }
    if (strcmp_P(param, PSTR("lambda_source"))   == 0) { printUint8Param ("lambda_source",   LAMBDA_SOURCE);              return; }
    if (strcmp_P(param, PSTR("fuel_lvl_src"))    == 0) { printUint8Param ("fuel_lvl_src",    FUEL_LVL_SOURCE);            return; }
    // Fault thresholds
    if (strcmp_P(param, PSTR("oil_warn_kpa"))    == 0) { printFloatParam ("oil_warn_kpa",    OIL_PRS_WARN_THRESHOLD, 2);  return; }
    if (strcmp_P(param, PSTR("coolant_warn_c"))  == 0) { printFloatParam ("coolant_warn_c",  COOLANT_TEMP_WARN_THRESHOLD, 2); return; }
    if (strcmp_P(param, PSTR("batt_warn_v"))     == 0) { printFloatParam ("batt_warn_v",     BATT_VOLT_WARN_THRESHOLD, 2); return; }
    if (strcmp_P(param, PSTR("engine_run_rpm"))  == 0) { printIntParam   ("engine_run_rpm",  ENGINE_RUNNING_RPM_MIN);     return; }
    if (strcmp_P(param, PSTR("fuel_warn_pct"))   == 0) { printUint8Param ("fuel_warn_pct",   FUEL_LVL_WARN_THRESHOLD_PCT); return; }
    // System
    if (strcmp_P(param, PSTR("clock_offset"))    == 0) { printIntParam   ("clock_offset",    (int8_t)clockOffset);        return; }
    if (strcmp_P(param, PSTR("fuel_capacity"))   == 0) { printFloatParam ("fuel_capacity",   fuelCapacity, 2);            return; }
    if (strcmp_P(param, PSTR("can_protocol"))    == 0) { printUint8Param ("can_protocol",    CAN_PROTOCOL);               return; }
    if (strcmp_P(param, PSTR("units"))           == 0) { printUint8Param ("units",           units);                      return; }

    SERIAL_PRINTLN_F("err: unknown");
}

// ---------------------------------------------------------------------------
// handleSet
// ---------------------------------------------------------------------------
void handleSet(const char *param, const char *value) {
    if (!value || value[0] == '\0') {
        SERIAL_PRINTLN_F("err: type");
        return;
    }

    // ---- Motor sweeps ----
    if (strcmp_P(param, PSTR("m1_sweep")) == 0) {
        uint16_t v = (uint16_t)atol(value);
        if (v < 100 || v > 4095) { SERIAL_PRINTLN_F("err: range"); return; }
        M1_SWEEP = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m2_sweep")) == 0) {
        uint16_t v = (uint16_t)atol(value);
        if (v < 100 || v > 4095) { SERIAL_PRINTLN_F("err: range"); return; }
        M2_SWEEP = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m3_sweep")) == 0) {
        uint16_t v = (uint16_t)atol(value);
        if (v < 100 || v > 4095) { SERIAL_PRINTLN_F("err: range"); return; }
        M3_SWEEP = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m4_sweep")) == 0) {
        uint16_t v = (uint16_t)atol(value);
        if (v < 100 || v > 4095) { SERIAL_PRINTLN_F("err: range"); return; }
        M4_SWEEP = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("ms_sweep")) == 0) {
        uint16_t v = (uint16_t)atol(value);
        if (v < 100 || v > 8000) { SERIAL_PRINTLN_F("err: range"); return; }
        MS_SWEEP = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("ms_zero_delay")) == 0) {
        uint16_t v = (uint16_t)atol(value);
        if (v < 10 || v > 5000) { SERIAL_PRINTLN_F("err: range"); return; }
        MS_ZERO_STEP_DELAY_US = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("ms_zero_factor")) == 0) {
        float v = atof(value);
        if (v < 0.1f || v > 1.0f) { SERIAL_PRINTLN_F("err: range"); return; }
        MS_ZERO_SWEEP_FACTOR = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("motor_sweep_ms")) == 0) {
        uint16_t v = (uint16_t)atol(value);
        if (v < 100 || v > 5000) { SERIAL_PRINTLN_F("err: range"); return; }
        MOTOR_SWEEP_TIME_MS = v; SERIAL_PRINTLN_F("ok"); return;
    }
    // ---- Filters ----
    if (strcmp_P(param, PSTR("filter_vbatt")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v < 1 || v > 255) { SERIAL_PRINTLN_F("err: range"); return; }
        FILTER_VBATT = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("vbatt_scaler")) == 0) {
        float v = atof(value);
        if (v < 0.001f || v > 0.1f) { SERIAL_PRINTLN_F("err: range"); return; }
        VBATT_SCALER = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("filter_fuel")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v < 1 || v > 255) { SERIAL_PRINTLN_F("err: range"); return; }
        FILTER_FUEL = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("filter_therm")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v < 1 || v > 255) { SERIAL_PRINTLN_F("err: range"); return; }
        FILTER_THERM = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("filter_av1")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v < 1 || v > 255) { SERIAL_PRINTLN_F("err: range"); return; }
        FILTER_AV1 = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("filter_av2")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v < 1 || v > 255) { SERIAL_PRINTLN_F("err: range"); return; }
        FILTER_AV2 = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("filter_av3")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v < 1 || v > 255) { SERIAL_PRINTLN_F("err: range"); return; }
        FILTER_AV3 = v; SERIAL_PRINTLN_F("ok"); return;
    }
    // ---- Hall speed sensor ----
    if (strcmp_P(param, PSTR("revs_per_km")) == 0) {
        uint16_t v = (uint16_t)atol(value);
        if (v < 100 || v > 10000) { SERIAL_PRINTLN_F("err: range"); return; }
        REVS_PER_KM = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("teeth_per_rev")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v < 1 || v > 64) { SERIAL_PRINTLN_F("err: range"); return; }
        TEETH_PER_REV = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("filter_hall")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v < 1 || v > 255) { SERIAL_PRINTLN_F("err: range"); return; }
        FILTER_HALL_SPEED = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("hall_speed_min")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 100) { SERIAL_PRINTLN_F("err: range"); return; }
        HALL_SPEED_MIN = v; SERIAL_PRINTLN_F("ok"); return;
    }
    // ---- RPM sensor ----
    if (strcmp_P(param, PSTR("cyl_count")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v < 2 || v > 16) { SERIAL_PRINTLN_F("err: range"); return; }
        CYL_COUNT = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("filter_rpm")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v < 1 || v > 255) { SERIAL_PRINTLN_F("err: range"); return; }
        FILTER_ENGINE_RPM = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("rpm_debounce_us")) == 0) {
        uint16_t v = (uint16_t)atol(value);
        if (v < 100 || v > 20000) { SERIAL_PRINTLN_F("err: range"); return; }
        RPM_DEBOUNCE_MICROS = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("engine_rpm_min")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        ENGINE_RPM_MIN = v; SERIAL_PRINTLN_F("ok"); return;
    }
    // ---- Speedometer ----
    if (strcmp_P(param, PSTR("speedo_max")) == 0) {
        uint16_t v = (uint16_t)atol(value);
        if (v < 1000 || v > 30000) { SERIAL_PRINTLN_F("err: range"); return; }
        SPEEDO_MAX = v; SERIAL_PRINTLN_F("ok"); return;
    }
    // ---- LED tachometer ----
    if (strcmp_P(param, PSTR("num_leds")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v < 1 || v > 64) { SERIAL_PRINTLN_F("err: range"); return; }
        NUM_LEDS = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("warn_leds")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 32) { SERIAL_PRINTLN_F("err: range"); return; }
        WARN_LEDS = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("shift_leds")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 16) { SERIAL_PRINTLN_F("err: range"); return; }
        SHIFT_LEDS = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("tach_max")) == 0) {
        uint16_t v = (uint16_t)atol(value);
        if (v < 1000 || v > 15000) { SERIAL_PRINTLN_F("err: range"); return; }
        TACH_MAX = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("tach_min")) == 0) {
        uint16_t v = (uint16_t)atol(value);
        if (v > 5000) { SERIAL_PRINTLN_F("err: range"); return; }
        TACH_MIN = v; SERIAL_PRINTLN_F("ok"); return;
    }
    // ---- Odometer motor ----
    if (strcmp_P(param, PSTR("odo_steps")) == 0) {
        uint16_t v = (uint16_t)atol(value);
        if (v < 512 || v > 8192) { SERIAL_PRINTLN_F("err: range"); return; }
        ODO_STEPS = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("odo_motor_teeth")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v < 1 || v > 64) { SERIAL_PRINTLN_F("err: range"); return; }
        ODO_MOTOR_TEETH = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("odo_gear_teeth")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v < 1 || v > 64) { SERIAL_PRINTLN_F("err: range"); return; }
        ODO_GEAR_TEETH = v; SERIAL_PRINTLN_F("ok"); return;
    }
    // ---- Signal sources ----
    if (strcmp_P(param, PSTR("speed_source")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 6) { SERIAL_PRINTLN_F("err: range"); return; }
        SPEED_SOURCE = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("rpm_source")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 4) { SERIAL_PRINTLN_F("err: range"); return; }
        RPM_SOURCE = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("oil_prs_source")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 5) { SERIAL_PRINTLN_F("err: range"); return; }
        OIL_PRS_SOURCE = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("fuel_prs_source")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 5) { SERIAL_PRINTLN_F("err: range"); return; }
        FUEL_PRS_SOURCE = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("coolant_src")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 3) { SERIAL_PRINTLN_F("err: range"); return; }
        COOLANT_TEMP_SOURCE = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("oil_temp_src")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 2) { SERIAL_PRINTLN_F("err: range"); return; }
        OIL_TEMP_SOURCE = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("map_source")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 5) { SERIAL_PRINTLN_F("err: range"); return; }
        MAP_SOURCE = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("lambda_source")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 4) { SERIAL_PRINTLN_F("err: range"); return; }
        LAMBDA_SOURCE = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("fuel_lvl_src")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 2) { SERIAL_PRINTLN_F("err: range"); return; }
        FUEL_LVL_SOURCE = v; SERIAL_PRINTLN_F("ok"); return;
    }
    // ---- Fault thresholds ----
    if (strcmp_P(param, PSTR("oil_warn_kpa")) == 0) {
        float v = atof(value);
        if (v < 0.0f || v > 1000.0f) { SERIAL_PRINTLN_F("err: range"); return; }
        OIL_PRS_WARN_THRESHOLD = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("coolant_warn_c")) == 0) {
        float v = atof(value);
        if (v < 50.0f || v > 200.0f) { SERIAL_PRINTLN_F("err: range"); return; }
        COOLANT_TEMP_WARN_THRESHOLD = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("batt_warn_v")) == 0) {
        float v = atof(value);
        if (v < 5.0f || v > 16.0f) { SERIAL_PRINTLN_F("err: range"); return; }
        BATT_VOLT_WARN_THRESHOLD = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("engine_run_rpm")) == 0) {
        int v = atoi(value);
        if (v < 0 || v > 2000) { SERIAL_PRINTLN_F("err: range"); return; }
        ENGINE_RUNNING_RPM_MIN = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("fuel_warn_pct")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 50) { SERIAL_PRINTLN_F("err: range"); return; }
        FUEL_LVL_WARN_THRESHOLD_PCT = v; SERIAL_PRINTLN_F("ok"); return;
    }
    // ---- System ----
    if (strcmp_P(param, PSTR("clock_offset")) == 0) {
        int v = atoi(value);
        if (v < -12 || v > 12) { SERIAL_PRINTLN_F("err: range"); return; }
        clockOffset = (byte)(int8_t)v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("fuel_capacity")) == 0) {
        float v = atof(value);
        if (v < 1.0f || v > 200.0f) { SERIAL_PRINTLN_F("err: range"); return; }
        fuelCapacity = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("can_protocol")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 3) { SERIAL_PRINTLN_F("err: range"); return; }
        CAN_PROTOCOL = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("units")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 1) { SERIAL_PRINTLN_F("err: range"); return; }
        units = v; SERIAL_PRINTLN_F("ok"); return;
    }

    SERIAL_PRINTLN_F("err: unknown");
}

// ---------------------------------------------------------------------------
// handleSave
// ---------------------------------------------------------------------------
void handleSave(void) {
    // Display menu state
    for (uint8_t i = 0; i < 4; i++) {
        EEPROM.update(dispArray1Address + i, dispArray1[i]);
    }
    EEPROM.update(dispArray2Address, dispArray2[0]);
    EEPROM.update(unitsAddress, units);
    EEPROM.update(clockOffsetAddress, clockOffset);
    EEPROM.put(odoAddress, odo);
    EEPROM.put(odoTripAddress, odoTrip);
    EEPROM.put(fuelSensorRawAddress, fuelSensorRaw);
    // Calibration parameters (M*_SWEEP, FILTER_*, etc.) live in RAM and are
    // applied immediately on set.  EEPROM persistence for calibration parameters
    // is deferred to a future version; the PC tool re-sends them after each power
    // cycle or the user saves a .txt config file and re-applies it.
    SERIAL_PRINTLN_F("ok");
}

// ---------------------------------------------------------------------------
// handleLoad
// ---------------------------------------------------------------------------
void handleLoad(void) {
    for (uint8_t i = 0; i < 4; i++) {
        dispArray1[i] = EEPROM.read(dispArray1Address + i);
    }
    dispArray2[0] = EEPROM.read(dispArray2Address);
    units         = EEPROM.read(unitsAddress);
    clockOffset   = EEPROM.read(clockOffsetAddress);
    EEPROM.get(odoAddress, odo);
    EEPROM.get(odoTripAddress, odoTrip);
    EEPROM.get(fuelSensorRawAddress, fuelSensorRaw);
    SERIAL_PRINTLN_F("ok");
}

// ---------------------------------------------------------------------------
// handleDump
// ---------------------------------------------------------------------------
void handleDump(void) {
    // Motor sweeps
    printUint16Param("m1_sweep",        M1_SWEEP);
    printUint16Param("m2_sweep",        M2_SWEEP);
    printUint16Param("m3_sweep",        M3_SWEEP);
    printUint16Param("m4_sweep",        M4_SWEEP);
    printUint16Param("ms_sweep",        MS_SWEEP);
    printUint16Param("ms_zero_delay",   MS_ZERO_STEP_DELAY_US);
    printFloatParam ("ms_zero_factor",  MS_ZERO_SWEEP_FACTOR, 4);
    printUint16Param("motor_sweep_ms",  MOTOR_SWEEP_TIME_MS);
    // Filters
    printUint8Param ("filter_vbatt",    FILTER_VBATT);
    printFloatParam ("vbatt_scaler",    VBATT_SCALER, 6);
    printUint8Param ("filter_fuel",     FILTER_FUEL);
    printUint8Param ("filter_therm",    FILTER_THERM);
    printUint8Param ("filter_av1",      FILTER_AV1);
    printUint8Param ("filter_av2",      FILTER_AV2);
    printUint8Param ("filter_av3",      FILTER_AV3);
    // Hall speed sensor
    printUint16Param("revs_per_km",     REVS_PER_KM);
    printUint8Param ("teeth_per_rev",   TEETH_PER_REV);
    printUint8Param ("filter_hall",     FILTER_HALL_SPEED);
    printUint8Param ("hall_speed_min",  HALL_SPEED_MIN);
    // RPM sensor
    printUint8Param ("cyl_count",       CYL_COUNT);
    printUint8Param ("filter_rpm",      FILTER_ENGINE_RPM);
    printUint16Param("rpm_debounce_us", RPM_DEBOUNCE_MICROS);
    printUint8Param ("engine_rpm_min",  ENGINE_RPM_MIN);
    // Speedometer
    printUint16Param("speedo_max",      SPEEDO_MAX);
    // LED tachometer
    printUint8Param ("num_leds",        NUM_LEDS);
    printUint8Param ("warn_leds",       WARN_LEDS);
    printUint8Param ("shift_leds",      SHIFT_LEDS);
    printUint16Param("tach_max",        TACH_MAX);
    printUint16Param("tach_min",        TACH_MIN);
    // Odometer
    printUint16Param("odo_steps",       ODO_STEPS);
    printUint8Param ("odo_motor_teeth", ODO_MOTOR_TEETH);
    printUint8Param ("odo_gear_teeth",  ODO_GEAR_TEETH);
    // Signal sources
    printUint8Param ("speed_source",    SPEED_SOURCE);
    printUint8Param ("rpm_source",      RPM_SOURCE);
    printUint8Param ("oil_prs_source",  OIL_PRS_SOURCE);
    printUint8Param ("fuel_prs_source", FUEL_PRS_SOURCE);
    printUint8Param ("coolant_src",     COOLANT_TEMP_SOURCE);
    printUint8Param ("oil_temp_src",    OIL_TEMP_SOURCE);
    printUint8Param ("map_source",      MAP_SOURCE);
    printUint8Param ("lambda_source",   LAMBDA_SOURCE);
    printUint8Param ("fuel_lvl_src",    FUEL_LVL_SOURCE);
    // Fault thresholds
    printFloatParam ("oil_warn_kpa",    OIL_PRS_WARN_THRESHOLD, 2);
    printFloatParam ("coolant_warn_c",  COOLANT_TEMP_WARN_THRESHOLD, 2);
    printFloatParam ("batt_warn_v",     BATT_VOLT_WARN_THRESHOLD, 2);
    printIntParam   ("engine_run_rpm",  ENGINE_RUNNING_RPM_MIN);
    printUint8Param ("fuel_warn_pct",   FUEL_LVL_WARN_THRESHOLD_PCT);
    // System
    printIntParam   ("clock_offset",    (int8_t)clockOffset);
    printFloatParam ("fuel_capacity",   fuelCapacity, 2);
    printUint8Param ("can_protocol",    CAN_PROTOCOL);
    printUint8Param ("units",           units);
    SERIAL_PRINTLN_F("end");
}

// ---------------------------------------------------------------------------
// handleReset - restore all parameters to compiled defaults
// ---------------------------------------------------------------------------
void handleReset(void) {
    applyDefaults();
    SERIAL_PRINTLN_F("ok");
}

static void applyDefaults(void) {
    M1_SWEEP                    = 58 * 12;
    M2_SWEEP                    = 58 * 12;
    M3_SWEEP                    = 58 * 12;
    M4_SWEEP                    = 58 * 12;
    MS_SWEEP                    = 4032;
    MS_ZERO_STEP_DELAY_US       = 40;
    MS_ZERO_SWEEP_FACTOR        = 0.25f;
    MOTOR_SWEEP_TIME_MS         = 1000;
    FILTER_VBATT                = 8;
    VBATT_SCALER                = 0.040923f;
    FILTER_FUEL                 = 1;
    FILTER_THERM                = 50;
    FILTER_AV1                  = 4;
    FILTER_AV2                  = 12;
    FILTER_AV3                  = 12;
    REVS_PER_KM                 = 1625;
    TEETH_PER_REV               = 8;
    FILTER_HALL_SPEED           = 64;
    HALL_SPEED_MIN              = 50;
    CYL_COUNT                   = 8;
    FILTER_ENGINE_RPM           = 179;
    RPM_DEBOUNCE_MICROS         = 5000;
    ENGINE_RPM_MIN              = 100;
    SPEEDO_MAX                  = 100 * 100;
    NUM_LEDS                    = 27;
    WARN_LEDS                   = 6;
    SHIFT_LEDS                  = 2;
    TACH_MAX                    = 6000;
    TACH_MIN                    = 3000;
    ODO_STEPS                   = 2048;
    ODO_MOTOR_TEETH             = 16;
    ODO_GEAR_TEETH              = 20;
    SPEED_SOURCE                = 2;
    RPM_SOURCE                  = 2;
    OIL_PRS_SOURCE              = 5;
    FUEL_PRS_SOURCE             = 5;
    COOLANT_TEMP_SOURCE         = 3;
    OIL_TEMP_SOURCE             = 2;
    MAP_SOURCE                  = 5;
    LAMBDA_SOURCE               = 1;
    FUEL_LVL_SOURCE             = 2;
    OIL_PRS_WARN_THRESHOLD      = 60.0f;
    COOLANT_TEMP_WARN_THRESHOLD = 110.0f;
    BATT_VOLT_WARN_THRESHOLD    = 11.0f;
    ENGINE_RUNNING_RPM_MIN      = 400;
    FUEL_LVL_WARN_THRESHOLD_PCT = 5;
    clockOffset                 = 0;
    fuelCapacity                = 16.0f;
    CAN_PROTOCOL                = 0;
    units                       = 0;
}

// ---------------------------------------------------------------------------
// dispatchConfigCommand - entry point called from processSerialCommands()
// ---------------------------------------------------------------------------
void dispatchConfigCommand(const char *cmd) {
    // "ping"
    if (strcmp_P(cmd, PSTR("ping")) == 0) {
        SERIAL_PRINTLN_F("pong");
        return;
    }
    // "version"
    if (strcmp_P(cmd, PSTR("version")) == 0) {
        Serial.println((__FlashStringHelper*)FIRMWARE_VERSION);
        return;
    }
    // "save"
    if (strcmp_P(cmd, PSTR("save")) == 0) {
        handleSave();
        return;
    }
    // "load"
    if (strcmp_P(cmd, PSTR("load")) == 0) {
        handleLoad();
        return;
    }
    // "dump"
    if (strcmp_P(cmd, PSTR("dump")) == 0) {
        handleDump();
        return;
    }
    // "reset"
    if (strcmp_P(cmd, PSTR("reset")) == 0) {
        handleReset();
        return;
    }
    // "get <param>"
    if (cmd[0]=='g' && cmd[1]=='e' && cmd[2]=='t' && cmd[3]==' ') {
        handleGet(cmd + 4);
        return;
    }
    // "set <param> <value>"
    if (cmd[0]=='s' && cmd[1]=='e' && cmd[2]=='t' && cmd[3]==' ') {
        // Find the space between param and value
        const char *paramStart = cmd + 4;
        const char *space = strchr(paramStart, ' ');
        if (!space) {
            SERIAL_PRINTLN_F("err: type");
            return;
        }
        // Copy param name into stack buffer (max 20 chars)
        char paramBuf[21];
        uint8_t plen = (uint8_t)(space - paramStart);
        if (plen > 20) plen = 20;
        memcpy(paramBuf, paramStart, plen);
        paramBuf[plen] = '\0';
        handleSet(paramBuf, space + 1);
        return;
    }
    // Unknown command verb
    SERIAL_PRINTLN_F("err: unknown");
}
