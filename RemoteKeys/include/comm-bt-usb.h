//this is the header file for the comm-bt-usb.c

#pragma once
#include <windows.h>
#include <stdbool.h>

/**
* Attempts to open a connection using CreateFile to the specific COM port
* Works for both USB and Bluetooth Classic
* @param ctx the global context contain the confi (port/baud)
* @return 0 on success, or a windows Error code on failure
*/
int comm_init(struct GlobalContext* ctx);

/**
* The entry point for the serial listener Thread.
* This function will run in a loop, polling the COM port
* Possible error to note is that port could exsits but esp doesnt
* @param lpParam A pointer to the GlobalContext (cast to void*).
* @return Thread exit code
*/
DWORD WINAPI comm_listener_thread(LPVOID lpParam);

/**
* Safely closes the COM port handle and resets state
*/
void comm_close(struct GlobalContext* ctx);

/**
* Helper function to validate if the current handle is still valid.
* useful for bluetooth connection that may drop mid-session
* @param GlobalContext
* @return True if valid, False if not valid
*/
bool com_is_connected(struct GlobalContext* ctx);