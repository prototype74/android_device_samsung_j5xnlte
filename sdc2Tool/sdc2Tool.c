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
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/mount.h>

#include "constants.h"
#include "format.h"
#include "wipe.h"
#include "utilities.h"

enum sdc2ToolOptions {
    WIPE_DALVIK = 1,
    WIPE_DATA,
    FORMAT_DATA,
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
    printf("3) Format Data (ext4 or f2fs)\n");
    printf("4) Exit\n");
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

static void user_input(const char *prompt, char *buf, int size) {
    struct termios old, raw;
    int i = 0;
    char c;

    printf("%s", prompt);
    fflush(stdout);

    tcgetattr(STDIN_FILENO, &old);
    raw = old;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    while (1) {
        if (read(STDIN_FILENO, &c, 1) <= 0)
            break;

        if (c == '\n' || c == '\r') {
            break;
        } else if (c == 127 || c == '\b') {
            if (buf && i > 0) {
                i--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c == 27) {
            char seq[2];
            read(STDIN_FILENO, &seq[0], 1);
            read(STDIN_FILENO, &seq[1], 1);
        } else {
            if (buf && i < size - 1) {
                buf[i++] = c;
                printf("%c", c);
                fflush(stdout);
            }
        }
    }

    if (buf)
        buf[i] = '\0';
    printf("\n");
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
}

static int confirm(const char* warning) {
    char input[16];

    printf("[WARNING] %s\n", warning);
    printf("\n");
    user_input("Type 'yes' to confirm: ", input, sizeof(input));

    return strcmp(input, "yes") == 0;
}

static void confirm_enter(void) {
    user_input("\nPress ENTER to continue...", NULL, 0);
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
        user_input("Select option [1-4]: ", input, sizeof(input));

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
                confirm_enter();
                if (result != 0) {
                    reset_screen("Error: Failed to wipe /data_sdc2");
                } else {
                    reset_screen("Wiped /data_sdc2 successfully");
                }
                break;
            case FORMAT_DATA: {
                int fs_option;
                fs_type_t fs_type;

                clear_screen();
                print_header();
                printf("Mount point: %s\n", DATA_MOUNT_POINT);
                printf("Current filesystem: %s\n\n", get_filesystem_type(DATA_PART));
                printf("Select filesystem type:\n");
                printf("1) EXT4\n");
                printf("2) F2FS\n");
                printf("\n");
                user_input("Select option [1, 2], or any other to cancel: ", input, sizeof(input));

                val = strtol(input, &endptr, 10);

                if (endptr == input || (*endptr != '\n' && *endptr != '\0')) {
                    fs_option = -1;
                } else {
                    fs_option = (int)val;
                }

                if (fs_option == 1) {
                    fs_type = FS_EXT4;
                } else if (fs_option == 2) {
                    fs_type = FS_F2FS;
                } else {
                    reset_screen("Format canceled.");
                    break;
                }

                clear_screen();
                print_header();

                if (!confirm("Formatting /data_sdc2 will erase all data, and this action cannot be undone! Continue?")) {
                    reset_screen("Format canceled.");
                    break;
                }

                printf("\n");
                result = format_data_sdc2(fs_type);

                if (result != 0) {
                    confirm_enter();
                    reset_screen("Error: Failed to format /data_sdc2");
                    break;
                }

                /* Mount /data_sdc2 to setup /data_sdc2/media/0 after format */
                if (mount(DATA_PART, DATA_MOUNT_POINT,
                        fs_type == FS_F2FS ? "f2fs" : "ext4", 0, NULL) != 0) {
                    fprintf(stderr, "[ERROR] Failed to mount '%s': %s\n",
                            DATA_MOUNT_POINT, strerror(errno));
                    confirm_enter();
                    reset_screen("Error: Failed to mount /data_sdc2 after format");
                    break;
                }

                if (setup_data_sdc2_media() != 0) {
                    confirm_enter();
                    reset_screen("Error: Failed to setup /data_sdc2/media after format");
                    break;
                }

                confirm_enter();
                reset_screen("Formatted /data_sdc2 successfully");
                break;
            }
            case EXIT_SDC2TOOL:
                running = 0;
                break;
            default:
                reset_screen("Invalid selection. Please choose 1-4.");
                break;
        }
    }

    return 0;
}
