/*  Overview:
    The logging-system uses GlobalContext->logLock for file access. 
    log_init initilizes the loglock and log_shutdown deletes the lock.

*/

#pragma once

#include "globals.h"
#include <stdio.h>
#include <stdbool.h>

//----LOGLEVEL----

//definition
typedef enum LogLevel {
    LOG_INFO = 0,
    LOG_WARN = 1,
    LOG_ERROR = 2
};

//setter
void log_set_level(struct GlobalContext* GLOBALCONTEXT, enum LogLevel level);
//getter
enum LogLevel log_get_level(struct GlobalContext* GLOBALCONTEXT);



//----INITIALIZATION----


/*  
    Opens or creates the logfile and prepares internal state.
    Returns boolean value, true (success) false (failure)
    reminder: InitializeCriticalSection(&GLOBALCONTEXT->logLock)
*/
bool log_init(struct GlobalContext* GLOBALCONTEXT, const char* logfile_path, enum LogLevel level);
    

//----SHUTDOWN----

/* 
    shut down the logging system and release resources.
    closes the logfile held in global context
    reminder: DeleteCriticalSection(&GLOBALCONTEXT->logLock)
*/
void log_shutdown(struct GlobalContext* GLOBALCONTEXT);


//----LOGGING----

/*  
    Log a message. Given: a context, a log level, a message format and variadic (...fancy word) args.

    Will be called by wrapper functions depending on loglevel. function is thread-safe
**/ 
void log_write(struct GlobalContext* GLOBALCONTEXT, enum LogLevel level, const char* format, ...);

/* 
    Wrappers for each of the def LogLevel.
    These accept additional args as variadic(i said it again) parameters.
    call log_write with LOG_INFO level
*/
void log_info(struct GlobalContext* GLOBALCONTEXT, const char* format, ...);

/*
    Wrappers for each of the def LogLevel.
    These accept additional args as variadic(i said it again) parameters.
    call log_write with LOG_WARN level
*/
void log_warn(struct GlobalContext* GLOBALCONTEXT, const char* format, ...);

/*
    Wrappers for each of the def LogLevel.
    These accept additional args as variadic(i said it again) parameters.
    call log_write with LOG_ERROR level
*/
void log_error(struct GlobalContext* GLOBALCONTEXT, const char* format, ...);