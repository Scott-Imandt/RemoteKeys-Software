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
#include "keyboard-emulation.h"   // MAT_MOD_* modifier flags
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include <windows.h>

#define INI_LINE_MAX 256


 // defaults that can be changed at runtime via the method ini_set_defaults().
static int static_default_comPort = 5;
static int static_default_baudRate = CBR_115200;
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
	if (writef_or_fail(f, "; Format: TriggerID,Application,Key1|Key2|...,Mod1|Mod2|...\n") < 0) goto write_error;
	if (writef_or_fail(f, "; Keys: A-Z, 0-9, SPACE, ESCAPE, ENTER, TAB, F1-F24, arrows, MEDIA_PLAY_PAUSE, or 0xNN\n") < 0) goto write_error;
	if (writef_or_fail(f, "; Mods: one entry per key, combine with '+' (CTRL+SHIFT), or NONE. Application GLOBAL = no focus change.\n") < 0) goto write_error;
	if (writef_or_fail(f, "TRIGGER_27,GLOBAL,C,CTRL\n") < 0) goto write_error;
	if (writef_or_fail(f, "TRIGGER_14,Spotify Premium,SPACE,NONE\n") < 0) goto write_error;
	if (writef_or_fail(f, "TRIGGER_25,GLOBAL,V,CTRL\n") < 0) goto write_error;
	if (writef_or_fail(f, "TRIGGER_26,GLOBAL,ESCAPE,CTRL+SHIFT\n") < 0) goto write_error;
	if (writef_or_fail(f, "TRIGGER_33,GLOBAL,E,WIN\n") < 0) goto write_error;

	fclose(f);
	return INI_STATE_SUCCESS;

write_error:
	fclose(f);
	return INI_STATE_FILE_NOT_FOUND;
}

/* ---------- INI parsing helpers ---------- */

typedef enum { SEC_NONE, SEC_SERIAL, SEC_LOGGING, SEC_KEYS } IniSection;

// trims leading/trailing whitespace in place, returns pointer into the same buffer
static char* trim(char* s) {
	while (*s && isspace((unsigned char)*s)) s++;
	if (!*s) return s;
	char* e = s + strlen(s) - 1;
	while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
	return s;
}

static bool parse_int(const char* v, int* out) {
	char* end = NULL;
	long n = strtol(v, &end, 0);
	if (end == v || *end != '\0') return false;
	*out = (int)n;
	return true;
}

typedef struct { const char* name; int vk; } KeyName;
static const KeyName key_names[] = {
	{"SPACE", VK_SPACE}, {"ESC", VK_ESCAPE}, {"ESCAPE", VK_ESCAPE},
	{"ENTER", VK_RETURN}, {"RETURN", VK_RETURN}, {"TAB", VK_TAB},
	{"BACKSPACE", VK_BACK}, {"DELETE", VK_DELETE}, {"INSERT", VK_INSERT},
	{"HOME", VK_HOME}, {"END", VK_END}, {"PAGEUP", VK_PRIOR}, {"PAGEDOWN", VK_NEXT},
	{"UP", VK_UP}, {"DOWN", VK_DOWN}, {"LEFT", VK_LEFT}, {"RIGHT", VK_RIGHT},
	{"MEDIA_PLAY_PAUSE", VK_MEDIA_PLAY_PAUSE}, {"MEDIA_NEXT", VK_MEDIA_NEXT_TRACK},
	{"MEDIA_PREV", VK_MEDIA_PREV_TRACK}, {"VOLUME_UP", VK_VOLUME_UP},
	{"VOLUME_DOWN", VK_VOLUME_DOWN}, {"VOLUME_MUTE", VK_VOLUME_MUTE},
};

// accepts "C", "SPACE", "VK_SPACE", "F5", "0x20"
static bool parse_key_token(const char* tok, int* out) {
	if (!tok || !*tok) return false;
	if (_strnicmp(tok, "VK_", 3) == 0) tok += 3;

	if (tok[1] == '\0' && isalnum((unsigned char)tok[0])) {
		*out = toupper((unsigned char)tok[0]);   // VK codes for A-Z / 0-9 equal their ASCII
		return true;
	}
	if ((tok[0] == 'F' || tok[0] == 'f') && isdigit((unsigned char)tok[1])) {
		int n = atoi(tok + 1);
		if (n >= 1 && n <= 24) { *out = VK_F1 + (n - 1); return true; }
	}
	for (size_t i = 0; i < sizeof(key_names) / sizeof(key_names[0]); i++) {
		if (_stricmp(tok, key_names[i].name) == 0) { *out = key_names[i].vk; return true; }
	}
	if (tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X')) return parse_int(tok, out);
	return false;
}

// accepts "NONE", "CTRL", "CTRL+SHIFT", etc.
static bool parse_modifier_token(char* tok, int* out) {
	int flags = 0;
	char* ctx = NULL;
	for (char* part = strtok_s(tok, "+", &ctx); part; part = strtok_s(NULL, "+", &ctx)) {
		part = trim(part);
		if (!_stricmp(part, "NONE") || !*part) continue;
		else if (!_stricmp(part, "CTRL") || !_stricmp(part, "CONTROL")) flags |= MAT_MOD_CTRL;
		else if (!_stricmp(part, "SHIFT")) flags |= MAT_MOD_SHIFT;
		else if (!_stricmp(part, "ALT")) flags |= MAT_MOD_ALT;
		else if (!_stricmp(part, "WIN")) flags |= MAT_MOD_WIN;
		else return false;
	}
	*out = flags;
	return true;
}

// parses one [Keys] line: TriggerID,Application,Key1|Key2,Mod1|Mod2
static bool parse_key_line(char* line, struct KeyCommand* cmd) {
	char* fields[4];
	int n = 0;
	char* p = line;
	fields[n++] = p;
	while (n < 4 && (p = strchr(p, ',')) != NULL) {
		*p++ = '\0';
		fields[n++] = p;
	}
	if (n < 4) return false;
	for (int i = 0; i < 4; i++) fields[i] = trim(fields[i]);

	memset(cmd, 0, sizeof(*cmd));
	if (!*fields[0] || strlen(fields[0]) >= sizeof(cmd->triggerID)) return false;
	if (!*fields[1] || strlen(fields[1]) >= sizeof(cmd->focusApplication)) return false;
	strcpy_s(cmd->triggerID, sizeof(cmd->triggerID), fields[0]);
	strcpy_s(cmd->focusApplication, sizeof(cmd->focusApplication), fields[1]);

	char* ctx = NULL;
	for (char* k = strtok_s(fields[2], "|", &ctx); k; k = strtok_s(NULL, "|", &ctx)) {
		if (cmd->keyCount >= MAX_MACRO_KEYS) return false;
		if (!parse_key_token(trim(k), &cmd->virtualKeyCode[cmd->keyCount])) return false;
		cmd->keyCount++;
	}
	if (cmd->keyCount == 0) return false;

	int modCount = 0;
	ctx = NULL;
	for (char* m = strtok_s(fields[3], "|", &ctx); m; m = strtok_s(NULL, "|", &ctx)) {
		if (modCount >= cmd->keyCount) return false;   // more modifiers than keys
		if (!parse_modifier_token(trim(m), &cmd->modifierFlags[modCount])) return false;
		modCount++;
	}
	return true;
}

// appends a command to the dynamic array (grows by doubling)
static bool append_command(struct AppConfig* cfg, int* capacity, const struct KeyCommand* cmd) {
	if (cfg->commandCount >= *capacity) {
		int newCap = (*capacity == 0) ? 8 : *capacity * 2;
		struct KeyCommand* grown = (struct KeyCommand*)realloc(cfg->commands, newCap * sizeof(struct KeyCommand));
		if (!grown) return false;
		cfg->commands = grown;
		*capacity = newCap;
	}
	cfg->commands[cfg->commandCount++] = *cmd;
	return true;
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

	FILE* f = fopen(filename, "r");
	if (!f) return INI_STATE_FILE_NOT_FOUND;

	IniSection section = SEC_NONE;
	int capacity = 0;
	int lineNo = 0;
	char raw[INI_LINE_MAX];
	char msg[128];

	while (fgets(raw, sizeof(raw), f)) {
		lineNo++;
		char* line = trim(raw);
		if (!*line || *line == ';' || *line == '#') continue;

		// section header
		if (*line == '[') {
			char* close = strchr(line, ']');
			if (!close) continue;
			*close = '\0';
			const char* name = trim(line + 1);
			if (!_stricmp(name, "Serial")) section = SEC_SERIAL;
			else if (!_stricmp(name, "Logging")) section = SEC_LOGGING;
			else if (!_stricmp(name, "Keys")) section = SEC_KEYS;
			else section = SEC_NONE;
			continue;
		}

		if (section == SEC_KEYS) {
			struct KeyCommand cmd;
			if (!parse_key_line(line, &cmd)) {
				snprintf(msg, sizeof(msg), "INI line %d: invalid key command, skipped\n", lineNo);
				OutputDebugStringA(msg);
				continue;
			}
			if (!append_command(&GLOBALCONTEXT->config, &capacity, &cmd)) {
				fclose(f);
				free_config(GLOBALCONTEXT);
				return INI_STATE_MEMORY_ERROR;
			}
			continue;
		}

		// key=value sections
		char* eq = strchr(line, '=');
		if (!eq) continue;
		*eq = '\0';
		char* key = trim(line);
		char* val = trim(eq + 1);

		if (section == SEC_SERIAL) {
			int n;
			if (!_stricmp(key, "COMPort") && parse_int(val, &n) && n > 0) GLOBALCONTEXT->config.comPortNumber = n;
			else if (!_stricmp(key, "BaudRate") && parse_int(val, &n) && n > 0) GLOBALCONTEXT->config.baudRate = n;
		}
		// TODO: [Logging] Enabled= is written to the default INI but AppConfig has no field for it yet.
	}

	fclose(f);

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