#include "keyboard-emulation.h"
// This file is responsiable for application (launching if not already, selection, Keyboard emulation, reselecting prev focus window)

HWND window_control_focus(const char* window_title)
{
    // 1. If the profile is set to GLOBAL, we don't change focus at all
    if (strncmp(window_title, "GLOBAL", 6) == 0) {
        return NULL;
    }

    // 2. Find the target application window
    HWND target_window = FindWindowA(NULL, window_title);

    if (target_window != NULL) {
        // --- THE THREAD ATTACH TRICK ---

        // A. Get the Thread IDs we need to manipulate
        HWND current_foreground_window = GetForegroundWindow();
        DWORD foreground_thread = GetWindowThreadProcessId(current_foreground_window, NULL);
        DWORD target_thread = GetWindowThreadProcessId(target_window, NULL);
        DWORD my_thread = GetCurrentThreadId();

        // B. Restore the target window if it is currently minimized
        if (IsIconic(target_window)) {
            ShowWindow(target_window, SW_RESTORE);
        }

        // C. Only execute the trick if we aren't already the foreground thread
        if (foreground_thread != my_thread) {

            // Step 1: Fuse our input processing with the currently active window
            AttachThreadInput(my_thread, foreground_thread, TRUE);

            // Step 2: Now that we have inherited foreground rights, force the switch
            SetForegroundWindow(target_window);
            SetFocus(target_window);

            // Step 3: Instantly detach to prevent UI freezing or crashing the other app
            AttachThreadInput(my_thread, foreground_thread, FALSE);

        }
        else {
            // We already own the foreground, standard calls work fine
            SetForegroundWindow(target_window);
            SetFocus(target_window);
        }

        // 3. Give the OS a tiny fraction of a second to render the window
        // before we start smashing virtual keys into it
        Sleep(50);

    }
    else {
        printf("[WARN] Could not find application window: %s\n", window_title);
    }

    return target_window;
}

int keyboard_emulate_input(WORD virtual_key, int modifier_flags)
{
    // An array to hold the chronological keystroke events
    INPUT inputs[10] = { 0 };
    int eventCount = 0;

    // --- PHASE 1: PRESS MODIFIERS DOWN ---
    if (modifier_flags & MAT_MOD_CTRL) {            //Modifier For CTRL
        inputs[eventCount].type = INPUT_KEYBOARD;
        inputs[eventCount].ki.wVk = VK_CONTROL;
        eventCount++;
    }
    if (modifier_flags & MAT_MOD_SHIFT) {           //Modifier For SHIFT
        inputs[eventCount].type = INPUT_KEYBOARD;
        inputs[eventCount].ki.wVk = VK_SHIFT;
        eventCount++;
    }
    if (modifier_flags & MAT_MOD_ALT) {             //Modifier For ALT
        inputs[eventCount].type = INPUT_KEYBOARD;
        inputs[eventCount].ki.wVk = VK_MENU;
        eventCount++;
    }

    if (modifier_flags & MAT_MOD_WIN) {             //Modifier For Windows Key
        inputs[eventCount].type = INPUT_KEYBOARD;
        inputs[eventCount].ki.wVk = VK_LWIN;
        eventCount++;
    }

    Sleep(5);

    // --- PHASE 2: PRESS TARGET KEY DOWN ---
    inputs[eventCount].type = INPUT_KEYBOARD;
    inputs[eventCount].ki.wVk = virtual_key;
    eventCount++;

    // --- PHASE 3: RELEASE TARGET KEY UP ---
    inputs[eventCount].type = INPUT_KEYBOARD;
    inputs[eventCount].ki.wVk = virtual_key;
    inputs[eventCount].ki.dwFlags = KEYEVENTF_KEYUP; // Flag specifies release
    eventCount++;

    // --- PHASE 4: RELEASE MODIFIERS UP ---
    // It is standard practice to release modifiers in the reverse order they were pressed
    if (modifier_flags & MAT_MOD_WIN) {             //Modifier For Windows Key
        inputs[eventCount].type = INPUT_KEYBOARD;
        inputs[eventCount].ki.wVk = VK_LWIN;
        inputs[eventCount].ki.dwFlags = KEYEVENTF_KEYUP;
        eventCount++;
    }
        
    if (modifier_flags & MAT_MOD_ALT) {             //Modifier For ALT
        inputs[eventCount].type = INPUT_KEYBOARD;
        inputs[eventCount].ki.wVk = VK_MENU;
        inputs[eventCount].ki.dwFlags = KEYEVENTF_KEYUP;
        eventCount++;
    }
    if (modifier_flags & MAT_MOD_SHIFT) {           //Modifier For SHIFT
        inputs[eventCount].type = INPUT_KEYBOARD;
        inputs[eventCount].ki.wVk = VK_SHIFT;
        inputs[eventCount].ki.dwFlags = KEYEVENTF_KEYUP;
        eventCount++;
    }
    if (modifier_flags & MAT_MOD_CTRL) {            //Modifier For CTRL
        inputs[eventCount].type = INPUT_KEYBOARD;   
        inputs[eventCount].ki.wVk = VK_CONTROL;
        inputs[eventCount].ki.dwFlags = KEYEVENTF_KEYUP;
        eventCount++;
    }

    // --- PHASE 5: INJECT INTO THE KERNEL ---
    UINT keys_sent = SendInput(eventCount, inputs, sizeof(INPUT));

    // Return 0 on success, -1 on failure
    return (keys_sent == eventCount) ? 0 : -1;
}

int keyboard_send_sequence(const int* keys, const int* modifiers, int count)
{
    for (int i = 0; i < count; i++) {
        // Execute the individual key + modifier pairing
        if (keyboard_emulate_input((WORD)keys[i], modifiers[i]) != 0) {
            printf("[ERROR] Failed to inject key sequence step %d\n", i);
            return -1;
        }

        // Pacing delay: Give the application time to register the keystroke 
        // before smashing the next one (crucial for older software/games)
        Sleep(25);
    }
    return 0;
}

int keyboard_execute_command(const struct KeyCommand* cmd)
{
    if (cmd == NULL) return -1;

    // 1. Shift focus to the target app (if not GLOBAL)
    window_control_focus(cmd->focusApplication);

    // 2. Fire the sequence!
    return keyboard_send_sequence(cmd->virtualKeyCode, cmd->modifierFlags, cmd->keyCount);
}
