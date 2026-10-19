// this is the header file for all global structs, state tracking enum(idle,comm-active,writing-ini-status-error), extern declarations staettracking struct (bluetooth state, usb state, error count, total commands, uptime))

// Three Threads for coree logic

/* Thread1: Serial listener
*		repsonsiblilty is to continously poll the com port for data comming from the esp32
* 
*		logic it stays 'blocked' until it recieves a command and places that command into a thread safe que	
* 
*/

/* Thread 2 Main logic thread
*		Responsibility: monitors the que and executes the windows API commands for window selection and keyboard execution
		
		logic: when the command appears in the que this thread calls the windowcontroller to find FTR reporter and input emulation to send keys
*/

/* Thread 3 Logging Thread
* 
*		Responsibilty: When enabled by the INI it records data based on device status and execution and periodicly flushed the data to a log file
* 
*		Logic: Prevent Disk I/O from slowing down the hardware communication other threads write to a mem buffer and this thread writes to a log.txt
*/
#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <psapi.h>
#include <stdbool.h>

#define MAX_MACRO_KEYS 8 // Define a max limit for key's to be pressed per action

//AppState is a high level tracking of what the program is doing a the current time
typedef enum AppState {
	STATE_UNKNOWN = 0,		// Unknown state to cause panic
	STATE_INITIALIZING = 1,	// Reading INI File and aquiring com port
	STATE_LISTENTING = 2,	// COM port is open sucessfuilly and waiting for ESP32 commands
	STATE_EXECUTING = 3,	// Processing keyboard commands
	STATE_LOGGING = 4,		// Writing data to the log file
	STATE_SHUTTINGDOWN = 5	// cleaning up open coms and closes log files
};

typedef struct AppConfig {
	int comPortNumber;				//The COM Port to open for connection
	int baudRate;					//The baud rate to use to communicate with the device
	struct KeyCommand* commands;	//Dynamic Array of your key mappings
	int commandCount;				//The total number of mappings loaded from the INI (6 max rn)
};

//represent the mapping of  a single key to trigger a windows action
typedef struct KeyCommand {
	char triggerID[11]; // Serial data string send from the esp32
	char focusApplication[32]; // the name of the application to send commands too
	int virtualKeyCode[MAX_MACRO_KEYS];// the windows hex equvilent for executing the command as an array for multiple key presses per action
	int modifierFlags[MAX_MACRO_KEYS]; // For id shift, Ctrl, Alt should be held at any point. A matching array for key presses to match the virtual keycode array
	int keyCount; // Total keys that will be pressed for a given action
};

//This is the thread safe que the serial thread writes here and the main thread reads here
typedef struct SharedBuffer {
	char pendingCommand[64];// the last command recieved
	bool hasNewData; // a flag to tell the main thread ther is work to preform
	CRITICAL_SECTION lock; // the windows sync object to prevent race conditions
};

//One large context struct for passing the state between threads
typedef struct GlobalContext {
	struct AppConfig config;	// the loaded INI Settings
	struct SharedBuffer buffer; // the communication bridge
	enum AppState currentState;	// The life cycle status
	HANDLE hComPort;			// The system handle for the esp32 connection
	FILE* logFile;				// The handle for the logging file

};

