/*
* Copyright (c) 2026 prototype74
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "wipe.h"
#include "utilities.h"

enum sdc2ToolOptions {
    WIPE_DALVIK = 1,
    WIPE_DATA,
    EXIT_SDC2TOOL,
};

static void print_header(void) {
    printf("\n");
    printf("================================\n");
    printf("       SDC2 Tool\n");
    printf("================================\n");
    printf("\n");
}

static void print_menu(void) {
    printf("1) Wipe Dalvik cache\n");
    printf("2) Wipe Data (keeps /data_sdc2/media)\n");
    printf("3) Exit\n");
    printf("\n");
}

static void clear_screen(void) {
    printf("\033[2J\033[H");
}

static void reset_screen(const char *note) {
    clear_screen();
    print_header();
    if (note) {
        printf("%s\n\n", note);
    }
    print_menu();
}

static int confirm(const char* warning) {
    char input[16];

    printf("[WARNING] %s\n", warning);
    printf("\n");
    printf("Type 'yes' to confirm: ");

    if (fgets(input, sizeof(input), stdin) == NULL)
        return 0;

    input[strcspn(input, "\n")] = '\0';
    return strcmp(input, "yes") == 0;
}

static void confirm_enter(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int main(void) {
    char input[8];
    char *endptr;
    long val;
    enum sdc2ToolOptions option;
    int running = 1;
    int result = 0;

    print_header();

    if (is_microsd_available() != 0) {
        fprintf(stderr, "[ERROR] No microSD card detected\n");
        return 1;
    }

    print_menu();

    while (running) {
        printf("Select option [1-3]: ");

        if (fgets(input, sizeof(input), stdin) == NULL) {
            fprintf(stderr, "[ERROR] Failed to read user input\n");
            return 1;
        }

        val = strtol(input, &endptr, 10);

        if (endptr == input || (*endptr != '\n' && *endptr != '\0')) {
            option = -1;
        } else {
            option = (int)val;
        }

        switch (option) {
            case WIPE_DALVIK:
                clear_screen();
                print_header();
                if (!confirm("This will wipe the Dalvik cache. Continue?")) {
                    reset_screen("Wipe canceled.");
                    break;
                }
                printf("\n");
                result = wipe_dalvik_sdc2();
                printf("\nPress ENTER to continue...");
                confirm_enter();
                if (result != 0) {
                    reset_screen("Error: Failed to wipe Dalvik cache");
                } else {
                    reset_screen("Dalvik cache wiped successfully");
                }
                break;
            case WIPE_DATA:
                clear_screen();
                print_header();
                if (!confirm("This will wipe /data_sdc2 but keep /data_sdc2/media. Continue?")) {
                    reset_screen("Wipe canceled.");
                    break;
                }
                printf("\n");
                result = wipe_data_sdc2();
                printf("\nPress ENTER to continue...");
                confirm_enter();
                if (result != 0) {
                    reset_screen("Error: Failed to wipe /data_sdc2");
                } else {
                    reset_screen("Wiped /data_sdc2 successfully");
                }
                break;
            case EXIT_SDC2TOOL:
                running = 0;
                break;
            default:
                reset_screen("Invalid selection. Please choose 1-3.");
                break;
        }
    }

    return 0;
}
