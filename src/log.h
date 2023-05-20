/* From BeRTOS */

#ifndef CFG_LOG_H
#define CFG_LOG_H

#include <stdio.h>

// Use a default setting if nobody defined a log level
#ifndef LOG_LEVEL
#define LOG_LEVEL       LOG_LVL_ERR
#endif

// Use a default setting if nobody defined a log format
#ifndef LOG_FORMAT
#define LOG_FORMAT      LOG_FMT_TERSE
#endif

/**
 * \name Logging level definition
 *
 * When you choose a log level messages you choose
 * also which print function are linked.
 * When using a log level, you link all log functions that have a priority
 * higher or equal than the level you chose.
 * The priority level go from error (highest) to info (lowest).
 *
 * $WIZ$ log_level = "LOG_LVL_NONE", "LOG_LVL_ERR", "LOG_LVL_WARN", "LOG_LVL_INFO"
 * \{
 */
#define LOG_LVL_NONE      0
#define LOG_LVL_ERR       1
#define LOG_LVL_WARN      2
#define LOG_LVL_INFO      3
#define LOG_LVL_DEBUG     4
/** \} */

/**
 * \name Logging format
 *
 * There are two logging format: terse and verbose.  The latter prepends
 * function names and line number information to each log entry.
 *
 * $WIZ$ log_format = "LOG_FMT_VERBOSE", "LOG_FMT_TERSE"
 * \{
 */
#define LOG_FMT_VERBOSE   1
#define LOG_FMT_TERSE     0
/** \} */

#if LOG_FORMAT == LOG_FMT_VERBOSE
    #define LOG_PRINT(str_level, str,...)    fprintf(stderr, "%s():%d:%s: " str, __func__, __LINE__, str_level, ## __VA_ARGS__)
#elif LOG_FORMAT == LOG_FMT_TERSE
    #define LOG_PRINT(str_level, str,...)    fprintf(stderr, "%s: " str, str_level, ## __VA_ARGS__)
#else
    #error No LOG_FORMAT defined
#endif

#if LOG_LEVEL >= LOG_LVL_ERR
	/**
	 * Output an error message
	 */
	#define LOG_ERR(str,...)       LOG_PRINT("ERR", str, ## __VA_ARGS__)
	/**
	 * Define a code block that will be compiled only when LOG_LEVEL >= LOG_LVL_ERR
	 */
	#define LOG_ERRB(x)            x
#else
	inline void LOG_ERR(const char * fmt, ...) { /* nop */ }
	#define LOG_ERRB(x)            /* Nothing */
#endif

#if LOG_LEVEL >= LOG_LVL_WARN
	/**
	 * Output a warning message
	 */
	#define LOG_WARN(str,...)       LOG_PRINT("WARN", str, ## __VA_ARGS__)
	/**
	 * Define a code block that will be compiled only when LOG_LEVEL >= LOG_LVL_WARN
	 */
	#define LOG_WARNB(x)            x
#else
	inline void LOG_WARN(const char * fmt, ...) { /* nop */ }
	#define LOG_WARNB(x)            /* Nothing */
#endif

#if LOG_LEVEL >= LOG_LVL_INFO
	/**
	 * Output an informative message
	 */
	#define LOG_INFO(str,...)       LOG_PRINT("INFO", str, ## __VA_ARGS__)
	/**
	 * Define a code block that will be compiled only when LOG_LEVEL >= LOG_LVL_INFO
	 */
	#define LOG_INFOB(x)            x
#else
	inline void LOG_INFO(const char * fmt, ...) { /* nop */ }
	#define LOG_INFOB(x)            /* Nothing */
#endif
/** \} */

#if LOG_LEVEL >= LOG_LVL_DEBUG
    #define LOG_DEBUG(str, ...)     LOG_PRINT("DEBUG", str, ## __VA_ARGS__)
    #define LOG_DEBUGB(x)           x
#else
    inline void LOG_DEBUG(const char* fmt, ...) { /* nop */ }
    #define LOG_DEBUGB(x)           /* Nothing */
#endif

#endif /* CFG_LOG_H */

