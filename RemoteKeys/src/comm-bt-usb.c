// This File will be used for functions realted to USB Communcation / BT Communication as well as initialization of these components

#pragma once
#include "globals.h"
#include <windows.h>
#include <stdbool.h>

#define dcbSerialParamsByteSize 8
#define SHARED_BUFFER_SIZE 64

int comm_init(struct GlobalContext* ctx) {

	char portName[20];

	// Condition checks to ensure all data is present for completion of the function

	if (ctx->config.comPortNumber == 0) {
		
		// LOG ERROR to Log file either here or in main.c (need to decide how this want to be done)

		return -3;
	}

	if (ctx->config.baudRate == 0) {

		// LOG ERROR to Log file either here or in main.c (need to decide how this want to be done)

		return -2;
	}

	// Get COM port Information and format it for Windows API
	snprintf(portName, sizeof(portName), "\\\\.\\COM%d", ctx->config.comPortNumber);

	// Open the serial port and configure the serial parameters can do GENERIC_READ | GENERIC_WRITE but need to make sure serial.availiable happends on ESP because this can cause windows program to fail if it doesnt write properly
	ctx->hComPort = CreateFileA(portName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);


	if (ctx->hComPort == INVALID_HANDLE_VALUE) {

		DWORD error = GetLastError();

		// LOG ERROR to Log file either here or in main.c (need to decide how this want to be done)

		return -1;
	}

	// Configure Serial Parameters
	DCB dcbSerialParams = { 0 };

	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

	if (!GetCommState(ctx->hComPort, &dcbSerialParams)) {
		//Log error for failing to get serial state
		return -4;
	}

	dcbSerialParams.BaudRate = ctx->config.baudRate;
	dcbSerialParams.ByteSize = dcbSerialParamsByteSize;
	dcbSerialParams.StopBits = ONESTOPBIT;
	dcbSerialParams.Parity = NOPARITY;
	SetCommState(ctx->hComPort, &dcbSerialParams);

	// Set Timeouts
	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 50;
	timeouts.ReadTotalTimeoutConstant = 50;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	SetCommTimeouts(ctx->hComPort, &timeouts);

	
	OutputDebugStringA("\nSUCCESS: Listing on port\n\n");

	return 0;
}

DWORD WINAPI comm_listener_thread(LPVOID lpParam) {

	// 1. Unpack our shared master context
	struct GlobalContext* ctx = (struct GlobalContext*)lpParam;
	if (ctx == NULL || ctx->hComPort == INVALID_HANDLE_VALUE) {
		return (DWORD)-1;
	}
	
	// Accumulation buffer to assemble incoming raw bytes into a full text command string
	char input_accumulator[SHARED_BUFFER_SIZE];// Need to put SHARED_BUFFER_SIZE
	int accumulator_index = 0;
	memset(input_accumulator, 0, sizeof(input_accumulator));

	printf("Entering Core Logic thread");

	//CORE LOGIC EXECUTION
	while (ctx->currentState != STATE_SHUTTINGDOWN) {
		char read_buffer[64];
		DWORD bytes_read = 0;

		// ReadFile will wait up to our timeout limits (e.g., 50ms) for bytes to arrive
		BOOL read_success = ReadFile(
			ctx->hComPort,
			read_buffer,
			sizeof(read_buffer) - 1,
			&bytes_read,
			NULL
		);

		if (!read_success) {
			DWORD error = GetLastError();
			// Optional: Handle fatal hardware disconnect errors (e.g., USB unplugged)
			if (error == ERROR_OPERATION_ABORTED || error == ERROR_INVALID_HANDLE) {
				//log_error(ctx, "Critical: COM Port connection hardware lost.");
				ctx->currentState = STATE_SHUTTINGDOWN;
				break;
			}
			continue; // Standard timeout/non-fatal event, cycle back and retry
		}

		// 3. Process Incoming Bytes (If any arrived before the timeout)
		for (DWORD i = 0; i < bytes_read; i++) {
			char current_char = read_buffer[i];

			// Look for message delimiters (e.g., Newline '\n' or Carriage Return '\r')
			if (current_char == '\n' || current_char == '\r') {
				if (accumulator_index > 0) {
					input_accumulator[accumulator_index] = '\0'; // Null-terminate string

					char debug_msg[128];
					sprintf_s(debug_msg, sizeof(debug_msg), "[SERIAL THREAD DEBUG] Raw Packet Assembled: %s\n", input_accumulator);
					//OutputDebugStringA(debug_msg);
					printf(debug_msg);

					// =========================================================
					// CRITICAL SECTION: Safe transfer to Main Logic Thread
					// =========================================================
					EnterCriticalSection(&ctx->buffer.lock);

					// Only overwrite if the Main Thread has already consumed the last command
					if (!ctx->buffer.hasNewData) {
						strncpy_s(ctx->buffer.pendingCommand, SHARED_BUFFER_SIZE, input_accumulator, _TRUNCATE);
						ctx->buffer.hasNewData = true;
						//log_info(ctx, "Serial Thread received and buffered: %s", input_accumulator);
					}
					else {
						//log_warn(ctx, "Main thread is lagging! Dropped incoming command: %s", input_accumulator);
					}

					LeaveCriticalSection(&ctx->buffer.lock);
					// =========================================================

					// Reset internal accumulator tracker for the next command packet
					accumulator_index = 0;
					memset(input_accumulator, 0, sizeof(input_accumulator));
				}
			}
			else {
				// Safeguard against stream buffer overflows
				if (accumulator_index < (SHARED_BUFFER_SIZE - 1)) {
					input_accumulator[accumulator_index++] = current_char;
				}
				else {
					//log_error(ctx, "Accumulator overflow! Purging garbage stream.");
					accumulator_index = 0;
				}
			}
		}

		// Yield CPU time slice to prevent core starvation if the ESP32 is silent
		Sleep(5);
	}

	//log_info(ctx, "Serial Listener Thread cleaning up and terminating execution.");
	printf("Ending Thread Execution");
	return 0;
}

void comm_close(struct GlobalContext* ctx) {
	// 1. Structural Sanity Check
	if (ctx == NULL) {
		return;
	}

	// 2. Check if the handle is actually open and valid
	if (ctx->hComPort != NULL && ctx->hComPort != INVALID_HANDLE_VALUE) {

		//log_info(ctx, "Closing active COM Port hardware handle...");

		// 3. Inform the OS to purge any remaining data bytes in the hardware buffers
		// This prevents ReadFile or WriteFile from hanging if they are finishing an action.
		PurgeComm(ctx->hComPort, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

		// 4. Release the OS kernel object lock
		if (CloseHandle(ctx->hComPort)) {
			//log_info(ctx, "COM Port handle closed successfully.");
		}
		else {
			// If it fails, capture why using your GetLastError skill!
			DWORD error = GetLastError();
			//log_error(ctx, "Warning: CloseHandle failed. Win32 Error: %lu", error);
		}

		// 5. Defensive Programming: Reset the variable to a known dead state
		// This ensures that if comm_close is accidentally called twice, 
		// the second time will be caught by our safety checks instead of crashing.
		ctx->hComPort = INVALID_HANDLE_VALUE;
	}
	else {
		//log_warn(ctx, "comm_close invoked, but COM Port handle was already closed or invalid.");
	}
}

bool com_is_connected(struct GlobalContext* ctx) {
	return true;
}