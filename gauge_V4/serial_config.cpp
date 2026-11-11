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
#include "menu.h"
#include "tach_settings.h"
#include "outputs.h"
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

// EEPROM addresses 56-65 are reserved for the tach record.
static const int TACH_EEPROM_ADDRESS = 56;
static const int TACH_COLOR_EEPROM_ADDRESS = 66;
static const int TACH_FLASH_EEPROM_ADDRESS = 77;
bool loadTachSettings(void) {
    TachRecord record;
    TachColorRecord colors;
    TachFlashRecord flash;
    EEPROM.get(TACH_EEPROM_ADDRESS, record);
    EEPROM.get(TACH_COLOR_EEPROM_ADDRESS, colors);
    EEPROM.get(TACH_FLASH_EEPROM_ADDRESS, flash);
    bool hasTach = record.version != 0xFF && record.version != 0;
    bool hasColors = colors.version != 0xFF && colors.version != 0;
    bool hasFlash = flash.version != 0xFF && flash.version != 0;
    if ((hasTach && !tachRecordValid(record, MAX_LEDS)) ||
        (hasColors && !tachColorRecordValid(colors)) ||
        (hasFlash && !tachFlashRecordValid(flash))) return false;
    if (hasTach) {
        NUM_LEDS = record.count;
        WARN_LEDS = record.warning;
        SHIFT_LEDS = record.shift;
        TACH_SWEEP = record.sweep;
        TACH_MIN = record.minimum;
        TACH_MAX = record.maximum;
    }
    TACH_FLASH = tachFlashOrMaximum(flash, TACH_MAX);
    TACH_NORMAL_COLOR = hasColors ? ((uint32_t)colors.colors[0] << 16 | (uint32_t)colors.colors[1] << 8 | colors.colors[2]) : 0x1E0F00;
    TACH_WARNING_COLOR = hasColors ? ((uint32_t)colors.colors[3] << 16 | (uint32_t)colors.colors[4] << 8 | colors.colors[5]) : 0x500A00;
    TACH_SHIFT_COLOR = hasColors ? ((uint32_t)colors.colors[6] << 16 | (uint32_t)colors.colors[7] << 8 | colors.colors[8]) : 0x500000;
    return true;
}

// ---------------------------------------------------------------------------
// Parameter printing helpers
// ---------------------------------------------------------------------------
static void printUint8Param(const char *name, uint8_t val) {
    Serial.print(name);
    Serial.print(' ');
    Serial.println(val);
}
static void printColorParam(const char *name, uint32_t color) {
    char text[7];
    snprintf(text, sizeof(text), "%06lX", (unsigned long)color);
    Serial.print(name);
    Serial.print(' ');
    Serial.println(text);
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

static void printScreenListParam(const char *name, const byte *list, byte count) {
    Serial.print(name);
    Serial.print(' ');
    for (byte i = 0; i < count; i++) {
        if (i) Serial.print(',');
        Serial.print(list[i]);
    }
    Serial.println();
}

// ---------------------------------------------------------------------------
// AV calibration parameters: avN_adc_lo / avN_adc_hi / avN_phys_lo / avN_phys_hi (N = 1-3)
// ---------------------------------------------------------------------------
enum AvField : uint8_t { AV_F_ADC_LO, AV_F_ADC_HI, AV_F_PHYS_LO, AV_F_PHYS_HI, AV_F_NONE };

static AvField parseAvParam(const char *param, uint8_t &idx) {
    if (param[0] != 'a' || param[1] != 'v' || param[2] < '1' || param[2] > '3' || param[3] != '_') return AV_F_NONE;
    idx = param[2] - '1';
    const char *field = param + 4;
    if (strcmp_P(field, PSTR("adc_lo"))  == 0) return AV_F_ADC_LO;
    if (strcmp_P(field, PSTR("adc_hi"))  == 0) return AV_F_ADC_HI;
    if (strcmp_P(field, PSTR("phys_lo")) == 0) return AV_F_PHYS_LO;
    if (strcmp_P(field, PSTR("phys_hi")) == 0) return AV_F_PHYS_HI;
    return AV_F_NONE;
}

static void printAvParam(const char *param, uint8_t idx, AvField f) {
    switch (f) {
        case AV_F_ADC_LO:  printUint16Param(param, AV_ADC_LO[idx]);  break;
        case AV_F_ADC_HI:  printUint16Param(param, AV_ADC_HI[idx]);  break;
        case AV_F_PHYS_LO: printIntParam   (param, AV_PHYS_LO[idx]); break;
        default:           printIntParam   (param, AV_PHYS_HI[idx]); break;
    }
}

static void printAllAvParams(void) {
    static const char fields[4][9] PROGMEM = {"adc_lo", "adc_hi", "phys_lo", "phys_hi"};
    for (uint8_t i = 0; i < 3; i++) {
        for (uint8_t f = 0; f < 4; f++) {
            char name[16];
            snprintf_P(name, sizeof(name), PSTR("av%u_%S"), i + 1, fields[f]);
            printAvParam(name, i, (AvField)f);
        }
    }
}

static void setAvParam(uint8_t idx, AvField f, const char *value) {
    long v = atol(value);
    if (f == AV_F_ADC_LO || f == AV_F_ADC_HI) {
        if (v < 0 || v > 1023) { SERIAL_PRINTLN_F("err: range"); return; }
        uint16_t &target = (f == AV_F_ADC_LO) ? AV_ADC_LO[idx] : AV_ADC_HI[idx];
        target = (uint16_t)v;
    } else {
        if (v < -32768L || v > 32767L) { SERIAL_PRINTLN_F("err: range"); return; }
        int16_t &target = (f == AV_F_PHYS_LO) ? AV_PHYS_LO[idx] : AV_PHYS_HI[idx];
        target = (int16_t)v;
    }
    SERIAL_PRINTLN_F("ok");
}

static void resetAvParams(void) {
    for (uint8_t i = 0; i < 3; i++) {
        AV_ADC_LO[i] = 102;
        AV_ADC_HI[i] = 921;
        AV_PHYS_LO[i] = 1013;
        AV_PHYS_HI[i] = 7908;
    }
}

// ---------------------------------------------------------------------------
// handleGet
// ---------------------------------------------------------------------------
void handleGet(const char *param) {
    {
        uint8_t avIdx;
        AvField avField = parseAvParam(param, avIdx);
        if (avField != AV_F_NONE) { printAvParam(param, avIdx, avField); return; }
    }
    if (strcmp_P(param, PSTR("disp1_order")) == 0) { printScreenListParam("disp1_order", disp1List, disp1Count); return; }
    if (strcmp_P(param, PSTR("disp2_order")) == 0) { printScreenListParam("disp2_order", disp2List, disp2Count); return; }
    // Motor sweep steps
    if (strcmp_P(param, PSTR("m1_sweep"))        == 0) { printUint16Param("m1_sweep",        M1_SWEEP);                   return; }
    if (strcmp_P(param, PSTR("m2_sweep"))        == 0) { printUint16Param("m2_sweep",        M2_SWEEP);                   return; }
    if (strcmp_P(param, PSTR("m3_sweep"))        == 0) { printUint16Param("m3_sweep",        M3_SWEEP);                   return; }
    if (strcmp_P(param, PSTR("m4_sweep"))        == 0) { printUint16Param("m4_sweep",        M4_SWEEP);                   return; }
    if (strcmp_P(param, PSTR("ms_sweep"))        == 0) { printUint16Param("ms_sweep",        MS_SWEEP);                   return; }
    if (strcmp_P(param, PSTR("m1_func"))         == 0) { printUint8Param ("m1_func",         M1_FUNCTION);                return; }
    if (strcmp_P(param, PSTR("m2_func"))         == 0) { printUint8Param ("m2_func",         M2_FUNCTION);                return; }
    if (strcmp_P(param, PSTR("m3_func"))         == 0) { printUint8Param ("m3_func",         M3_FUNCTION);                return; }
    if (strcmp_P(param, PSTR("m4_func"))         == 0) { printUint8Param ("m4_func",         M4_FUNCTION);                return; }
    if (strcmp_P(param, PSTR("m1_min"))          == 0) { printIntParam   ("m1_min",          M1_MIN);                     return; }
    if (strcmp_P(param, PSTR("m1_max"))          == 0) { printIntParam   ("m1_max",          M1_MAX);                     return; }
    if (strcmp_P(param, PSTR("m2_min"))          == 0) { printIntParam   ("m2_min",          M2_MIN);                     return; }
    if (strcmp_P(param, PSTR("m2_max"))          == 0) { printIntParam   ("m2_max",          M2_MAX);                     return; }
    if (strcmp_P(param, PSTR("m3_min"))          == 0) { printIntParam   ("m3_min",          M3_MIN);                     return; }
    if (strcmp_P(param, PSTR("m3_max"))          == 0) { printIntParam   ("m3_max",          M3_MAX);                     return; }
    if (strcmp_P(param, PSTR("m4_min"))          == 0) { printIntParam   ("m4_min",          M4_MIN);                     return; }
    if (strcmp_P(param, PSTR("m4_max"))          == 0) { printIntParam   ("m4_max",          M4_MAX);                     return; }
    if (strcmp_P(param, PSTR("ms_zero_delay"))   == 0) { printUint16Param("ms_zero_delay",   MS_ZERO_STEP_DELAY_US);      return; }
    if (strcmp_P(param, PSTR("ms_zero_factor"))  == 0) { printFloatParam ("ms_zero_factor",  MS_ZERO_SWEEP_FACTOR, 4);   return; }
    if (strcmp_P(param, PSTR("motor_sweep_ms"))  == 0) { printUint16Param("motor_sweep_ms",  MOTOR_SWEEP_TIME_MS);        return; }
    // EMA filters use a 7-bit previous-value weight.
    if (strcmp_P(param, PSTR("filter_format"))   == 0) { printUint8Param ("filter_format",   7);                          return; }
    if (strcmp_P(param, PSTR("filter_vbatt"))    == 0) { printUint8Param ("filter_vbatt",    FILTER_VBATT);               return; }
    if (strcmp_P(param, PSTR("vbatt_scaler"))    == 0) { printFloatParam ("vbatt_scaler",    VBATT_SCALER, 6);            return; }
    if (strcmp_P(param, PSTR("filter_fuel"))     == 0) { printUint8Param ("filter_fuel",     FILTER_FUEL);                return; }
    if (strcmp_P(param, PSTR("filter_therm"))    == 0) { printUint8Param ("filter_therm",    FILTER_THERM);               return; }
    if (strcmp_P(param, PSTR("filter_av1"))      == 0) { printUint8Param ("filter_av1",      FILTER_AV1);                 return; }
    if (strcmp_P(param, PSTR("filter_av2"))      == 0) { printUint8Param ("filter_av2",      FILTER_AV2);                 return; }
    if (strcmp_P(param, PSTR("filter_av3"))      == 0) { printUint8Param ("filter_av3",      FILTER_AV3);                 return; }
    if (strcmp_P(param, PSTR("av1_type"))        == 0) { printUint8Param ("av1_type",        AV1_TYPE);                   return; }
    if (strcmp_P(param, PSTR("av2_type"))        == 0) { printUint8Param ("av2_type",        AV2_TYPE);                   return; }
    if (strcmp_P(param, PSTR("av3_type"))        == 0) { printUint8Param ("av3_type",        AV3_TYPE);                   return; }
    // Hall speed sensor
    if (strcmp_P(param, PSTR("revs_per_km"))     == 0) { printUint16Param("revs_per_km",     REVS_PER_KM);                return; }
    if (strcmp_P(param, PSTR("teeth_per_rev"))   == 0) { printUint8Param ("teeth_per_rev",   TEETH_PER_REV);              return; }
    if (strcmp_P(param, PSTR("filter_hall"))     == 0) { printUint8Param ("filter_hall",     FILTER_HALL_SPEED);          return; }
    if (strcmp_P(param, PSTR("filter_gps"))      == 0) { printUint8Param ("filter_gps",      FILTER_GPS);                 return; }
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
    if (strcmp_P(param, PSTR("tach_sweep"))       == 0) { printUint8Param ("tach_sweep",       TACH_SWEEP);                  return; }
    if (strcmp_P(param, PSTR("tach_max"))        == 0) { printUint16Param("tach_max",        TACH_MAX);                   return; }
    if (strcmp_P(param, PSTR("tach_flash"))      == 0) { printUint16Param("tach_flash",      TACH_FLASH);                 return; }
    if (strcmp_P(param, PSTR("tach_min"))        == 0) { printUint16Param("tach_min",        TACH_MIN);                   return; }
    if (strcmp_P(param, PSTR("tach_color_normal")) == 0) { printColorParam("tach_color_normal", TACH_NORMAL_COLOR); return; }
    if (strcmp_P(param, PSTR("tach_color_warning")) == 0) { printColorParam("tach_color_warning", TACH_WARNING_COLOR); return; }
    if (strcmp_P(param, PSTR("tach_color_shift")) == 0) { printColorParam("tach_color_shift", TACH_SHIFT_COLOR); return; }
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

    {
        uint8_t avIdx;
        AvField avField = parseAvParam(param, avIdx);
        if (avField != AV_F_NONE) { setAvParam(avIdx, avField, value); return; }
    }

    // ---- Screen lists ----
    if (strcmp_P(param, PSTR("disp1_order")) == 0) {
        if (!parseScreenList(value, disp1List, disp1Count)) { SERIAL_PRINTLN_F("err: range"); return; }
        sanitizeScreenSelections(); SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("disp2_order")) == 0) {
        if (!parseScreenList(value, disp2List, disp2Count)) { SERIAL_PRINTLN_F("err: range"); return; }
        sanitizeScreenSelections(); SERIAL_PRINTLN_F("ok"); return;
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
        if (v < 100 || v > 16000) { SERIAL_PRINTLN_F("err: range"); return; }
        MS_SWEEP = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m1_func")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v >= MOTOR_FUNC_COUNT) { SERIAL_PRINTLN_F("err: range"); return; }
        M1_FUNCTION = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m2_func")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v >= MOTOR_FUNC_COUNT) { SERIAL_PRINTLN_F("err: range"); return; }
        M2_FUNCTION = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m3_func")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v >= MOTOR_FUNC_COUNT) { SERIAL_PRINTLN_F("err: range"); return; }
        M3_FUNCTION = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m4_func")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v >= MOTOR_FUNC_COUNT) { SERIAL_PRINTLN_F("err: range"); return; }
        M4_FUNCTION = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m1_min")) == 0) {
        long v = atol(value);
        if (v < -32768L || v > 32767L) { SERIAL_PRINTLN_F("err: range"); return; }
        M1_MIN = (int16_t)v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m1_max")) == 0) {
        long v = atol(value);
        if (v < -32768L || v > 32767L) { SERIAL_PRINTLN_F("err: range"); return; }
        M1_MAX = (int16_t)v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m2_min")) == 0) {
        long v = atol(value);
        if (v < -32768L || v > 32767L) { SERIAL_PRINTLN_F("err: range"); return; }
        M2_MIN = (int16_t)v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m2_max")) == 0) {
        long v = atol(value);
        if (v < -32768L || v > 32767L) { SERIAL_PRINTLN_F("err: range"); return; }
        M2_MAX = (int16_t)v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m3_min")) == 0) {
        long v = atol(value);
        if (v < -32768L || v > 32767L) { SERIAL_PRINTLN_F("err: range"); return; }
        M3_MIN = (int16_t)v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m3_max")) == 0) {
        long v = atol(value);
        if (v < -32768L || v > 32767L) { SERIAL_PRINTLN_F("err: range"); return; }
        M3_MAX = (int16_t)v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m4_min")) == 0) {
        long v = atol(value);
        if (v < -32768L || v > 32767L) { SERIAL_PRINTLN_F("err: range"); return; }
        M4_MIN = (int16_t)v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("m4_max")) == 0) {
        long v = atol(value);
        if (v < -32768L || v > 32767L) { SERIAL_PRINTLN_F("err: range"); return; }
        M4_MAX = (int16_t)v; SERIAL_PRINTLN_F("ok"); return;
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
    uint8_t *filter = NULL;
    if (strcmp_P(param, PSTR("filter_vbatt")) == 0) filter = &FILTER_VBATT;
    else if (strcmp_P(param, PSTR("filter_fuel")) == 0) filter = &FILTER_FUEL;
    else if (strcmp_P(param, PSTR("filter_therm")) == 0) filter = &FILTER_THERM;
    else if (strcmp_P(param, PSTR("filter_av1")) == 0) filter = &FILTER_AV1;
    else if (strcmp_P(param, PSTR("filter_av2")) == 0) filter = &FILTER_AV2;
    else if (strcmp_P(param, PSTR("filter_av3")) == 0) filter = &FILTER_AV3;
    else if (strcmp_P(param, PSTR("filter_hall")) == 0) filter = &FILTER_HALL_SPEED;
    else if (strcmp_P(param, PSTR("filter_gps")) == 0) filter = &FILTER_GPS;
    else if (strcmp_P(param, PSTR("filter_rpm")) == 0) filter = &FILTER_ENGINE_RPM;
    if (filter) {
        char *end;
        long v = strtol(value, &end, 10);
        if (*end || v < 0 || v > 127) { SERIAL_PRINTLN_F("err: range"); return; }
        *filter = (uint8_t)v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("vbatt_scaler")) == 0) {
        float v = atof(value);
        if (v < 0.001f || v > 0.1f) { SERIAL_PRINTLN_F("err: range"); return; }
        VBATT_SCALER = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("av1_type")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > AV_TYPE_LAMBDA) { SERIAL_PRINTLN_F("err: range"); return; }
        AV1_TYPE = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("av2_type")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > AV_TYPE_LAMBDA) { SERIAL_PRINTLN_F("err: range"); return; }
        AV2_TYPE = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("av3_type")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > AV_TYPE_LAMBDA) { SERIAL_PRINTLN_F("err: range"); return; }
        AV3_TYPE = v; SERIAL_PRINTLN_F("ok"); return;
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
        long v = atol(value);
        if (v < 1 || v > 64) { SERIAL_PRINTLN_F("err: range"); return; }
        NUM_LEDS = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("warn_leds")) == 0) {
        long v = atol(value);
        if (v < 0 || v > 63) { SERIAL_PRINTLN_F("err: range"); return; }
        WARN_LEDS = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("shift_leds")) == 0) {
        long v = atol(value);
        if (v < 0 || v > 63) { SERIAL_PRINTLN_F("err: range"); return; }
        SHIFT_LEDS = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("tach_sweep")) == 0) {
        long v = atol(value);
        if (v < 0 || v > TACH_CENTER_TO_EDGE) { SERIAL_PRINTLN_F("err: range"); return; }
        TACH_SWEEP = (uint8_t)v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("tach_max")) == 0) {
        long v = atol(value);
        if (v < 1000 || v > 15000) { SERIAL_PRINTLN_F("err: range"); return; }
        TACH_MAX = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("tach_flash")) == 0) {
        char *end;
        long v = strtol(value, &end, 10);
        if (end == value || *end != '\0' || v < 0 || v > 15000) { SERIAL_PRINTLN_F("err: range"); return; }
        TACH_FLASH = (unsigned int)v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("tach_min")) == 0) {
        long v = atol(value);
        if (v < 0 || v > 5000) { SERIAL_PRINTLN_F("err: range"); return; }
        TACH_MIN = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("tach_color_normal")) == 0 ||
        strcmp_P(param, PSTR("tach_color_warning")) == 0 ||
        strcmp_P(param, PSTR("tach_color_shift")) == 0) {
        uint32_t color;
        if (strlen(value) != 6 || !tachParseHex(value, 6, color)) { SERIAL_PRINTLN_F("err: color"); return; }
        if (strcmp_P(param, PSTR("tach_color_normal")) == 0) TACH_NORMAL_COLOR = color;
        else if (strcmp_P(param, PSTR("tach_color_warning")) == 0) TACH_WARNING_COLOR = color;
        else TACH_SHIFT_COLOR = color;
        SERIAL_PRINTLN_F("ok"); return;
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
        if (v > 6) { SERIAL_PRINTLN_F("err: range"); return; }
        OIL_PRS_SOURCE = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("fuel_prs_source")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 5) { SERIAL_PRINTLN_F("err: range"); return; }
        FUEL_PRS_SOURCE = v; SERIAL_PRINTLN_F("ok"); return;
    }
    if (strcmp_P(param, PSTR("coolant_src")) == 0) {
        uint8_t v = (uint8_t)atoi(value);
        if (v > 4) { SERIAL_PRINTLN_F("err: range"); return; }
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
        if (v > 3) { SERIAL_PRINTLN_F("err: range"); return; }
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
    if (NUM_LEDS < 1 || NUM_LEDS > MAX_LEDS || TACH_SWEEP > TACH_CENTER_TO_EDGE ||
        WARN_LEDS >= tachSteps(NUM_LEDS, TACH_SWEEP) || SHIFT_LEDS > WARN_LEDS ||
        TACH_MIN > TACH_MAX || TACH_FLASH > 15000) {
        SERIAL_PRINTLN_F("err: invalid tach settings");
        return;
    }
    // Display menu state
    for (uint8_t i = 0; i < 4; i++) {
        EEPROM.update(dispArray1Address + i, dispArray1[i]);
    }
    EEPROM.update(dispArray2Address, dispArray2[0]);
    EEPROM.update(unitsAddress, units);
    EEPROM.update(clockOffsetAddress, clockOffset);
    saveScreenLists();
    EEPROM.put(odoAddress, odo);
    EEPROM.put(odoTripAddress, odoTrip);
    EEPROM.put(fuelSensorRawAddress, fuelSensorRaw);
    TachRecord tach = {1, NUM_LEDS, WARN_LEDS, SHIFT_LEDS, TACH_SWEEP,
                       TACH_MIN, TACH_MAX, 0};
    tach.checksum = tachChecksum(tach);
    EEPROM.put(TACH_EEPROM_ADDRESS, tach);
    TachColorRecord colors = {1, {
        (uint8_t)(TACH_NORMAL_COLOR >> 16), (uint8_t)(TACH_NORMAL_COLOR >> 8), (uint8_t)TACH_NORMAL_COLOR,
        (uint8_t)(TACH_WARNING_COLOR >> 16), (uint8_t)(TACH_WARNING_COLOR >> 8), (uint8_t)TACH_WARNING_COLOR,
        (uint8_t)(TACH_SHIFT_COLOR >> 16), (uint8_t)(TACH_SHIFT_COLOR >> 8), (uint8_t)TACH_SHIFT_COLOR
    }, 0};
    colors.checksum = tachChecksum(colors);
    EEPROM.put(TACH_COLOR_EEPROM_ADDRESS, colors);
    TachFlashRecord flash = {1, TACH_FLASH, 0};
    flash.checksum = tachChecksum(flash);
    EEPROM.put(TACH_FLASH_EEPROM_ADDRESS, flash);
    // Other calibration parameters (M*_SWEEP, FILTER_*, etc.) live in RAM and are
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
    loadScreenLists();
    EEPROM.get(odoAddress, odo);
    EEPROM.get(odoTripAddress, odoTrip);
    EEPROM.get(fuelSensorRawAddress, fuelSensorRaw);
    if (!loadTachSettings()) {
        SERIAL_PRINTLN_F("err: invalid tach settings in EEPROM");
        return;
    }
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
    printUint8Param ("m1_func",         M1_FUNCTION);
    printUint8Param ("m2_func",         M2_FUNCTION);
    printUint8Param ("m3_func",         M3_FUNCTION);
    printUint8Param ("m4_func",         M4_FUNCTION);
    printIntParam   ("m1_min",          M1_MIN);
    printIntParam   ("m1_max",          M1_MAX);
    printIntParam   ("m2_min",          M2_MIN);
    printIntParam   ("m2_max",          M2_MAX);
    printIntParam   ("m3_min",          M3_MIN);
    printIntParam   ("m3_max",          M3_MAX);
    printIntParam   ("m4_min",          M4_MIN);
    printIntParam   ("m4_max",          M4_MAX);
    printUint16Param("ms_zero_delay",   MS_ZERO_STEP_DELAY_US);
    printFloatParam ("ms_zero_factor",  MS_ZERO_SWEEP_FACTOR, 4);
    printUint16Param("motor_sweep_ms",  MOTOR_SWEEP_TIME_MS);
    // Filters
    printUint8Param ("filter_format",   7);
    printUint8Param ("filter_vbatt",    FILTER_VBATT);
    printFloatParam ("vbatt_scaler",    VBATT_SCALER, 6);
    printUint8Param ("filter_fuel",     FILTER_FUEL);
    printUint8Param ("filter_therm",    FILTER_THERM);
    printUint8Param ("filter_av1",      FILTER_AV1);
    printUint8Param ("filter_av2",      FILTER_AV2);
    printUint8Param ("filter_av3",      FILTER_AV3);
    printUint8Param ("av1_type",        AV1_TYPE);
    printUint8Param ("av2_type",        AV2_TYPE);
    printUint8Param ("av3_type",        AV3_TYPE);
    printAllAvParams();
    // Hall speed sensor
    printUint16Param("revs_per_km",     REVS_PER_KM);
    printUint8Param ("teeth_per_rev",   TEETH_PER_REV);
    printUint8Param ("filter_hall",     FILTER_HALL_SPEED);
    printUint8Param ("filter_gps",      FILTER_GPS);
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
    printUint8Param ("tach_sweep",      TACH_SWEEP);
    printUint16Param("tach_max",        TACH_MAX);
    printUint16Param("tach_flash",      TACH_FLASH);
    printUint16Param("tach_min",        TACH_MIN);
    printColorParam("tach_color_normal", TACH_NORMAL_COLOR);
    printColorParam("tach_color_warning", TACH_WARNING_COLOR);
    printColorParam("tach_color_shift", TACH_SHIFT_COLOR);
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
    printScreenListParam("disp1_order", disp1List, disp1Count);
    printScreenListParam("disp2_order", disp2List, disp2Count);
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
    M1_FUNCTION                 = MOTOR_FUNC_FUEL_LEVEL;
    M2_FUNCTION                 = MOTOR_FUNC_COOLANT_TEMP;
    M3_FUNCTION                 = MOTOR_FUNC_FUEL_LEVEL;
    M4_FUNCTION                 = MOTOR_FUNC_COOLANT_TEMP;
    M1_MIN = 10;  M1_MAX = 100;
    M2_MIN = 50;  M2_MAX = 115;
    M3_MIN = 10;  M3_MAX = 100;
    M4_MIN = 50;  M4_MAX = 115;
    MS_ZERO_STEP_DELAY_US       = 40;
    MS_ZERO_SWEEP_FACTOR        = 0.25f;
    MOTOR_SWEEP_TIME_MS         = 1000;
    FILTER_VBATT                = 112;
    VBATT_SCALER                = 0.040923f;
    FILTER_FUEL                 = 126;
    FILTER_THERM                = 64;
    FILTER_AV1                  = 96;
    FILTER_AV2                  = 32;
    FILTER_AV3                  = 32;
    AV1_TYPE                    = AV_TYPE_100PSI_G;
    AV2_TYPE                    = AV_TYPE_100PSI_G;
    AV3_TYPE                    = AV_TYPE_100PSI_G;
    resetAvParams();
    REVS_PER_KM                 = 1625;
    TEETH_PER_REV               = 8;
    FILTER_HALL_SPEED           = 96;
    FILTER_GPS                  = 0;
    HALL_SPEED_MIN              = 50;
    CYL_COUNT                   = 8;
    FILTER_ENGINE_RPM           = 39;
    RPM_DEBOUNCE_MICROS         = 5000;
    ENGINE_RPM_MIN              = 100;
    SPEEDO_MAX                  = 100 * 100;
    NUM_LEDS                    = 27;
    WARN_LEDS                   = 6;
    SHIFT_LEDS                  = 2;
    TACH_SWEEP                  = TACH_EDGE_TO_CENTER;
    TACH_NORMAL_COLOR           = 0x1E0F00;
    TACH_WARNING_COLOR          = 0x500A00;
    TACH_SHIFT_COLOR            = 0x500000;
    TACH_MAX                    = 6000;
    TACH_FLASH                  = 6000;
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
    setDefaultScreenLists();
    sanitizeScreenSelections();
}

// ---------------------------------------------------------------------------
// dispatchConfigCommand - entry point called from processSerialCommands()
// ---------------------------------------------------------------------------
void dispatchConfigCommand(const char *cmd) {
    if (strcmp_P(cmd, PSTR("tachtest off")) == 0) {
        stopTachPreview();
        SERIAL_PRINTLN_F("ok");
        return;
    }
    if (strncmp_P(cmd, PSTR("tachtest "), 9) == 0) {
        TachPreview preview;
        if (!tachParsePreview(cmd + 9, preview)) {
            SERIAL_PRINTLN_F("err: invalid tach preview");
            return;
        }
        setTachPreview(preview);
        SERIAL_PRINTLN_F("ok");
        return;
    }
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
