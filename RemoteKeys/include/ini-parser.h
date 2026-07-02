// This is the header file for the ini-parser.c
//will be actually reading an INI file

// This will be the header file for handling the reading of the ini File for setting configurations for the software application

#pragma once

#include "globals.h"
#include <stdbool.h>

// Struct for default values used by the INI loader and INI writer
typedef struct IniDefaults {
	int comPort;
	int baudRate;
	bool loggingEnabled;
};

typedef enum iniState {
	INI_STATE_UNKNOWN = 0,
	INI_STATE_SUCCESS =-1, // Transition state to reach State_listening
	INI_STATE_FILE_NOT_FOUND=-2, // Call create_default_config
	INI_STATE_MEMORY_ERROR=-3,// Log "Out of Memory" and close program
	INI_STATE_SYNTAX_ERROR=-4 // Log "invalid INI and exit"
};


/*
*Checks if the config.ini file exists int he local directory and if it is valid[port number in range, triggerid in range, etc]
*Returns TRUE if found, false otherwise
*/
bool config_exists(const char* filename);

/*
*Creates a template INI file with default values
*Returns true if successful, false otherwise
*/
int create_default_config(const char* filename);

/*
* Opens the file
* Counts the key entries
* Malloc memory for the app config keyCommands array
* populates the GlobalCONTEXT with the data read from the INI file
* 
*/
int load_config(struct GlobalContext* GLOBALCONTEXT, const char* filename);

/*
*Deep Cleans the Allocated memory for the INI keyboardCommands
* MUST BE CALLED DURING THE SHUTDOWN STATE
* Check if ctx->config.commands is NULL.
* Call free(ctx->config.commands).
* Set ctx->config.commands = NULL;.
* Set ctx->config.commandCount = 0;.
*/
void free_config(struct GlobalContext* GLOBALCONTEXT);

/*
 * Populate GLOBALCONTEXT with module default values for the configuration.
 * This can be used to apply the current defaults without reading a file.
 */
void set_default_config(struct GlobalContext* GLOBALCONTEXT);

/*
 * Change the defaults used by this module when creating/loading configs.
 * Call ini_set_defaults early in initialization to override the built-in
 * defaults (affects create_default_config and set_default_config behavior).
 * Set module-default values using a struct. Pass a pointer to an IniDefaults
 * describing the desired defaults. If 'defaults' is NULL the call is ignored.
 */
void ini_set_defaults(const IniDefaults* defaults);
