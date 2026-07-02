
#
/*
 * ini-parser.c
 *
 * It will check for a configuration INI file
 * If one does not exist it will create it with default set values.
 * It will then load the values from the ini file to the GlobalContext
 *
 */

#include "ini-parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>


// defaults that can be changed at runtime via the method ini_set_defaults().
static int static_default_comPort = 1;
static int static_default_baudRate = 115200;
static bool static_default_loggingEnabled = true;

// Helper used by create_default_config to write formatted data to the output FILE.
static int writef_or_fail(FILE* file, const char* format, ...) {
	va_list args;
	va_start(args, format);
	int written = vfprintf(file, format, args);
	va_end(args);
	return (written < 0) ? -1 : 0;
}

/*
 * ini_set_defaults
 * set the defaults from an IniDefaults struct.
 */
void ini_set_defaults(const IniDefaults* defaults) {
	if (!defaults) return;
	if (defaults->comPort > 0) static_default_comPort = defaults->comPort;
	if (defaults->baudRate > 0) static_default_baudRate = defaults->baudRate;
	static_default_loggingEnabled = defaults->loggingEnabled;
}

void set_default_config(struct GlobalContext* GLOBALCONTEXT) {
	if (!GLOBALCONTEXT) return;
	GLOBALCONTEXT->config.comPortNumber = static_default_comPort;
	GLOBALCONTEXT->config.baudRate = static_default_baudRate;
	GLOBALCONTEXT->config.commandCount = 0;
	GLOBALCONTEXT->config.commands = NULL;
}

/* CONFIG_EXISTS
 * first check if a file exists
 * takes in a filename
 * will return true if the file can be opened or false if it cannot be read correctly.
 */
bool config_exists(const char* filename) {
	if (!filename) return false;
	FILE* file = fopen(filename, "r");
	if (file) {
		fclose(file);
		return true;
	}
	return false;
}

/*
 * CREATE_DEFAULT_CONFIG
 * creates the default ini file
 * takes in a path to where the default config will be saved.
 * returns INI_STATE_SUCCESS on success and either INI_STATE_UNKOWN or INI_STATE_FILE_NOT_FOUND on failure.
 *
 * The existing file will be overwritten if it already exists and this method is called.
 */
int create_default_config(const char* filename) {
	if (!filename) return INI_STATE_UNKNOWN;
	FILE* f = fopen(filename, "w");
	if (!f) return INI_STATE_FILE_NOT_FOUND;


	// call helper to write to file, if it fails return INI_STATE_FILE_NOT_FOUND from write_error
	if (writef_or_fail(f, "[Serial]\n") < 0) goto write_error;
	if (writef_or_fail(f, "COMPort=%d\n", static_default_comPort) < 0) goto write_error;
	if (writef_or_fail(f, "BaudRate=%d\n\n", static_default_baudRate) < 0) goto write_error;
	if (writef_or_fail(f, "[Logging]\n") < 0) goto write_error;
	if (writef_or_fail(f, "Enabled=%s\n\n", static_default_loggingEnabled ? "true" : "false") < 0) goto write_error;
	if (writef_or_fail(f, "[Keys]\n") < 0) goto write_error;
	if (writef_or_fail(f, "; Format: TriggerID,Application,Key1|Key2,...,Modifier1|Modifier2,...\n") < 0) goto write_error;

	fclose(f);
	return INI_STATE_SUCCESS;

	write_error:
		fclose(f);
		return INI_STATE_FILE_NOT_FOUND;
}

/* LOAD_CONFIG
 * load an INI file into GlobalContext.
 * takes in a GLOBALCONTEXT and a filename.
 * returns INI_STATE_SUCCESS on success or INI_STATE_MEMORY_ERROR on failure.
 * the loader ensures a config file exists and sets default values.
 */
int load_config(struct GlobalContext* GLOBALCONTEXT, const char* filename) {
	if (!GLOBALCONTEXT) return INI_STATE_MEMORY_ERROR;

	if (!config_exists(filename)) {
		int ini_state = create_default_config(filename);
		if (ini_state != INI_STATE_SUCCESS) return ini_state;
		// default config created
	}

	/* populate with the current statically assigned defaults. */
	set_default_config(GLOBALCONTEXT);

	/*TODO: Full INI parsing */

	//on success, return INI_STATE_SUCCESS
	return INI_STATE_SUCCESS;
}

/* FREE_CONFIG
 * release the memory allocated for configs.
 * takes in a GLOBALCONTEXT to be freed up (pointer to the context).
 * Empty array and reset the commandcount.
 */
void free_config(struct GlobalContext* GLOBALCONTEXT) {
	if (!GLOBALCONTEXT) return;
	if (GLOBALCONTEXT->config.commands) {
		free(GLOBALCONTEXT->config.commands);
		GLOBALCONTEXT->config.commands = NULL;
	}
	GLOBALCONTEXT->config.commandCount = 0;
}