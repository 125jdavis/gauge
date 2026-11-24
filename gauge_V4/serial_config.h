/*
 * ========================================
 * SERIAL CONFIGURATION COMMAND HANDLER
 * ========================================
 *
 * Implements GET / SET / SAVE / LOAD / DUMP / RESET / PING / VERSION
 * commands for the PC configuration tool (CONFIG_TOOL_SPECIFICATION.md).
 *
 * Command format:
 *   get <param>              -> "<param> <value>"
 *   set <param> <value>      -> "ok" or "err: <reason>"
 *   save                     -> "ok"
 *   load                     -> "ok"
 *   dump                     -> "<param> <value>\n" x N, then "end"
 *   reset                    -> "ok"
 *   ping                     -> "pong"
 *   version                  -> "version 4.0"
 *   log start                -> "ok", then a data line every SERIAL_LOG_INTERVAL_MS
 *   log stop                 -> "ok"
 *
 * Data lines (see serialLogUpdate) start with "L," so the PC tool can tell them apart
 * from command replies.
 */

#ifndef SERIAL_CONFIG_H
#define SERIAL_CONFIG_H

#include <Arduino.h>

/**
 * dispatchConfigCommand - Route a parsed command line to the correct handler.
 *
 * @param cmd  Null-terminated command string (e.g. "get m1_sweep")
 */
void dispatchConfigCommand(const char *cmd);

/**
 * serialLogUpdate - While "log start" is active, send one data line every
 * SERIAL_LOG_INTERVAL_MS (10 Hz). Call every loop pass.
 *
 * Line: L,<millis>,<speed km/h*100>,<rpm>,<oil kPa*10>,<fuel kPa*10>,
 *       <lambda*1000>,<coolant C*10>,<MAP kPa abs*10>,<battery V*100>
 */
#define SERIAL_LOG_INTERVAL_MS 100
void serialLogUpdate(void);

/**
 * handleGet - Respond with current value of named parameter.
 *
 * @param param  Parameter name token
 */
void handleGet(const char *param);

/**
 * handleSet - Write named parameter to RAM (not EEPROM until save).
 *
 * @param param  Parameter name token
 * @param value  Value string; parsed as required type
 */
void handleSet(const char *param, const char *value);

// Restore the independently versioned tach record; false means it is invalid.
bool loadTachSettings(void);

/**
 * Calibration block (EEPROM 1536+): every serial-settable parameter not stored in
 * its own record. loadCalibration leaves RAM untouched unless it returns CAL_OK.
 */
enum CalLoadResult : uint8_t { CAL_OK, CAL_EMPTY, CAL_INVALID };
void saveCalibration(void);
CalLoadResult loadCalibration(void);

/**
 * handleSave - Flush all calibration values to EEPROM.
 */
void handleSave(void);

/**
 * handleLoad - Re-read all calibration values from EEPROM.
 */
void handleLoad(void);

/**
 * handleDump - Print all parameters in "<name> <value>" format, then "end".
 */
void handleDump(void);

/**
 * handleReset - Restore all parameters to compiled defaults.
 */
void handleReset(void);

#endif // SERIAL_CONFIG_H
