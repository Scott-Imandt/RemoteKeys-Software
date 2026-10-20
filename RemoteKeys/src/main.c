#include "globals.h"
#include "ini-parser.h"
#include "logging-system.h"
#include "comm-bt-usb.h"
#include "keyboard-emulation.h"

#include <windows.h>
#include <stdio.h>
#include <stdbool.h>

// Forward declarations for clean structure
bool start_worker_threads(struct GlobalContext* ctx, HANDLE* threads); // This Creates worker threads for logging and serial listenting
//void handle_runtime_monitoring(struct GlobalContext* ctx); // this handles the keyboard commands for KeyCommand Execution
//void setup_example_commands(struct AppConfig* config);
void run_main_loop(struct GlobalContext* ctx, struct AppConfig* config);


int main() {
    // ==========================================
    // PHASE 1: BOOTSTRAP & STACK ZEROING
    //          Initialize structs and amd set initial states 
    // ==========================================

    struct GlobalContext ctx;
    memset(&ctx, 0, sizeof(struct GlobalContext));

    ctx.currentState = STATE_INITIALIZING;

    //Initialize the Critical Sections before threads are created
    InitializeCriticalSection(&ctx.buffer.lock);

    // ==========================================
    // PHASE 2: CONFIGURATION & SUBSYSTEM INIT
    //          1. Load configuration from INI
    //          2. Initialize Logging
    //          3. Initialize COM PORT
    // ==========================================

    // Create Temporary configuration for testing with Comm-bt-usb

    load_config(&ctx, ""); // Possible permission problems with creating file

    // initialize the COM connection and pass the handle to the global context

    if (comm_init(&ctx) != 0) {
        OutputDebugStringA("\nERROR with comm_init\n\n");
    }
    else {
        OutputDebugStringA("\nComm_Init returned 0\n\n");
    }


    // ==========================================
    // PHASE 3: THREAD CREATION & MONITORING
    //          Create 2 Additional Threads for a total of three threads running
    //              1-Main Thread(Responsibility: monitors the que and executes the windows API commands for window selection and keyboard execution)
    //              2-Serial Listener Thread(Repsonsiblilty is to continously poll the com port for data comming from the esp32)
    //             *3-Logging Thread(Responsibilty: When enabled by the INI it records data based on device status and execution and periodicly flushed the data to a log file)
    // ==========================================

    HANDLE worker_threads[1] = { NULL }; // !!need to update to create the logging thread here!!

    start_worker_threads(&ctx, worker_threads);
    
    // ==========================================
    // PHASE 4: KeyCommand Execcution
    //          Process recieved commands and execute 
    //          
    // ==========================================


    run_main_loop(&ctx, &ctx.config);


    // ==========================================
    // PHASE 5: SHUTDOWN & DEEP CLEAN
    //          Signal Threads to shutdown and wait for closures
    //          Release hardware/file resource (log file, free heap, deletecritivcalsection)
    // ==========================================

    Sleep(20);
    printf("\nTrigger Ending Thread\n");

    // STEP 1: Signal the background threads that it is time to die
    ctx.currentState = STATE_SHUTTINGDOWN;

    // STEP 2: Wait (Join) for the Serial Listener thread to finish its last loop
    // This pauses main() for up to 2000 milliseconds to let the thread exit cleanly.
    DWORD waitResult = WaitForSingleObject(worker_threads[0], 2000);

    if (waitResult == WAIT_OBJECT_0) {
        //log_info(&ctx, "Serial Listener Thread joined cleanly.");
    }
    else if (waitResult == WAIT_TIMEOUT) {
        //log_error(&ctx, "CRITICAL: Serial thread froze and refused to die within 2 seconds!");
        // (Optional) TerminateThread(worker_threads[0], 0); Force kill if absolutely frozen
    }

    // Tells Windows we are done tracking this thread, freeing kernel resources
    CloseHandle(worker_threads[0]);

    //Close com port
    comm_close(&ctx);

    //Free Heap allocated items
    free(ctx.config.commands);

    //Destroy Win32 Sync mechanisms last
    DeleteCriticalSection(&ctx.buffer.lock);
}

bool start_worker_threads(struct GlobalContext* ctx, HANDLE* threads) {

    // Thread 1: Serial Listener
    threads[0] = CreateThread(
        NULL,                   // 1. Default security attributes
        0,                      // 2. Default stack size (0 = default 1MB)
        comm_listener_thread,   // 3. The ThreadProc function pointer from comm-bt-usb.c
        ctx,                    // 4. Pass our master context structure pointer as LPVOID
        0,                      // 5. Creation flags (0 = run immediately)
        NULL                    // 6. Out-pointer for Thread ID (NULL because we use the HANDLE)
    );

    // Error check to ensure the operating system actually spawned the thread
    if (threads[0] == NULL) {
        DWORD error = GetLastError();
        //log_error(ctx, "Failed to create Serial Listener Thread. Win32 Error: %lu", error);
        return false;
    }

    //Thread 3: Asynchronus Logging Flush Thread

    return (threads[0] != NULL);
}

//void setup_example_commands(struct AppConfig* config) {
//    config->commandCount = 5;
//    // calloc takes two separate arguments instead of multiplying them!
//    config->commands = (struct KeyCommand*)calloc(config->commandCount, sizeof(struct KeyCommand));
//
//    if (config->commands == NULL) {
//        printf("Memory allocation failed!\n");
//        return;
//    }
//
//    // ==========================================
//    // EXAMPLE 1: Trigger_27 -> Global "Copy" Command (Ctrl + C)
//    // ==========================================
//    // Notice the size limit passed to strncpy_s is now 12 to match the struct!
//    //strncpy_s(config->commands[0].triggerID, 12, "TRIGGER_27", _TRUNCATE);
//    //strncpy_s(config->commands[0].focusApplication, 32, "GLOBAL", _TRUNCATE);
//    //config->commands[0].keyCount = 1;
//
//    //config->commands[0].virtualKeyCode[0] = 'C';
//    //config->commands[0].modifierFlags[0] = MAT_MOD_CTRL;
//
//}

void run_main_loop(struct GlobalContext* ctx, struct AppConfig* config) {
    printf("[SYSTEM] Main execution loop engaged. Standing by for triggers...\n");

    // A local copy buffer so we can extract data and release the lock immediately
    char local_trigger[12];

    // 1. The Lifetime Condition: Run until signaled to shut down
    while (ctx->currentState != STATE_SHUTTINGDOWN) {

        // Check if the user pressed the standard ESC key to exit the application safely
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            ctx->currentState = STATE_SHUTTINGDOWN;
            break;
        }

        // 2. The Check: Did the serial thread drop off a complete command?
        if (ctx->buffer.hasNewData) {

            // =========================================================
            // CRITICAL SECTION: Quick Extract
            // =========================================================
            EnterCriticalSection(&ctx->buffer.lock);

            // Safely copy the text command out into our local thread memory space
            strncpy_s(local_trigger, sizeof(local_trigger), ctx->buffer.pendingCommand, _TRUNCATE);

            // Flip the flag to false, opening the queue up for the ESP32's next button strike
            ctx->buffer.hasNewData = false;

            LeaveCriticalSection(&ctx->buffer.lock);
            // =========================================================

            // 3. The Parse: Scan through our loaded AppConfig command matrix
            bool match_found = false;

            for (int i = 0; i < config->commandCount; i++) {

                // Compare the incoming trigger (e.g. "Trigger_26") to our macro rules
                if (strncmp(local_trigger, config->commands[i].triggerID, 12) == 0) {

                    printf("[EXEC] Match caught for %s! Routing to: %s\n",
                        local_trigger, config->commands[i].focusApplication);

                    // 4. The Execution: Force target focus and inject keyboard inputs
                    keyboard_execute_command(&config->commands[i]);

                    match_found = true;
                    break; // Exit the lookup loop early since we found our match
                }
            }

            if (!match_found) {
                printf("[WARN] Received unknown token: %s (No macro configured)\n", local_trigger);
            }
        }

        // 5. The Throttle: Crucial step to protect CPU integrity
        // Without this 10ms rest, this loop would pin a CPU core at 100% utilization.
        Sleep(10);
    }

    printf("[SYSTEM] Exiting main loop. Beginning thread termination routine.\n");
}
