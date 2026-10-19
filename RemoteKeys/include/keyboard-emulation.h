//This is the header file for the keyboard-emulation.c

// This will handle the OS heavy lifting to execution of keyboard commands loaded from the ini file and stored
// within the Globals.h file

//Main Thread calls keyboard_execute_command() -> window_contol_focus() ->keyboard_send_sequence -> [loop of keyboard_emulate_input()]


#pragma once

#include "globals.h"
#include <windows.h>

#define MAT_MOD_CTRL  0x01
#define MAT_MOD_SHIFT 0x02
#define MAT_MOD_ALT   0x04
#define MAT_MOD_WIN   0x08

/**
* Finds and Target application by its title/class name and forces it into focus to receive keyboard commands
* @param window_title The string name of the application window to search for
* @return HWND A handle to the window if found, or NULL on failure
*/
HWND window_control_focus(const char* window_title);

/**
* Executes and SINGLE key press along with any modifiers flags. (a single key within an keyCommand action group)
* @param virtual_key The virtual keycode to press (ex 0x41 for 'A')
* @param modifier_flags The virtual keycode to simulate holding modifier keys (SHIFT, CTRL, etc)
* @return int 0 for success, or error code on failure
*/
int keyboard_emulate_input(WORD virtual_key, int modifier_flags);


/**
* Loops through the entire array of keys found within a single keyCommand to execute them sequentially,
* This will include pacing delays between actions
* @param keys Array of virtual key codes found within the global KeyCommand struct
* @param modifiers Array of modifiers to match the keys array to execute
* @param count A number to state the total number of keys that need to be pressed within an given sequence
* @return int 0 on success, non-zero if a step fails.
*/
int keyboard_send_sequence(const int* keys, const int* modifiers, int count);


/**
* This is a High level function called by the main logic thread.
* This function calls, window control focus, keyboad_send_sequence and keyboard_emulate_input
* @param cmd Pointer to the KeyCommand macro mapping to execute
* @return int 0 on success, non-zero on failure modes.
*/
int keyboard_execute_command(const struct KeyCommand* cmd);