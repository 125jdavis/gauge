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
