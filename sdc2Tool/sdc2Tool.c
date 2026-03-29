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
#include <limits.h>

#include "constants.h"
#include "backup.h"
#include "format.h"
#include "restore.h"
#include "wipe.h"
#include "utilities.h"

enum sdc2ToolOptions {
    WIPE_DALVIK = 1,
    WIPE_DATA,
    FORMAT_DATA,
    BACKUP_DATA,
    RESTORE_DATA,
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
    printf("4) Backup Data\n");
    printf("5) Restore Data\n");
    printf("6) Exit\n");
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
        user_input("Select option [1-6]: ", input, sizeof(input));

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
            case BACKUP_DATA: {
                int dest_option;
                unsigned long long backup_size = 0;
                unsigned long long free_space = 0;
                char backup_size_str[32];
                char free_str[32];
                const char *base_path;
                const char *backup_dir;

                clear_screen();
                print_header();

                if (get_backup_size(&backup_size) != 0) {
                    confirm_enter();
                    reset_screen("Error: Failed to calculate backup size");
                    break;
                }

                format_size(backup_size, backup_size_str, sizeof(backup_size_str));
                printf("Estimated backup size (uncompressed): %s\n", backup_size_str);
                printf("Actual compressed size will likely be smaller.\n\n");
                printf("Select backup destination:\n");

                /* Calculate destination sizes */
                get_free_space("/sdcard", &free_space);
                format_size(free_space, free_str, sizeof(free_str));
                printf("1) Internal storage (/sdcard)   [Free: %s]\n", free_str);

                get_free_space("/data_sdc2/media/0", &free_space);
                format_size(free_space, free_str, sizeof(free_str));
                printf("2) microSD (/data_sdc2/media/0) [Free: %s]\n", free_str);

                get_free_space("/usb-otg", &free_space);
                format_size(free_space, free_str, sizeof(free_str));
                printf("3) OTG (/usb-otg)               [Free: %s]\n", free_str);

                printf("\n");
                user_input("Select option [1-3], or any other to cancel: ", input, sizeof(input));

                val = strtol(input, &endptr, 10);

                if (endptr == input || (*endptr != '\n' && *endptr != '\0')) {
                    dest_option = -1;
                } else {
                    dest_option = (int)val;
                }

                if (dest_option == 1) {
                    base_path = "/sdcard";
                    backup_dir = "/sdcard/TWRP/sdc2Tool/backup";
                } else if (dest_option == 2) {
                    base_path = "/data_sdc2";
                    backup_dir = "/data_sdc2/media/0/TWRP/sdc2Tool/backup";
                } else if (dest_option == 3) {
                    base_path = "/usb-otg";
                    backup_dir = "/usb-otg/TWRP/sdc2Tool/backup";
                } else {
                    reset_screen("Backup canceled.");
                    break;
                }

                clear_screen();
                print_header();

                if (!confirm("This will create a backup of /data_sdc2. Continue?")) {
                    reset_screen("Backup canceled.");
                    break;
                }

                printf("\n");
                result = backup_data_sdc2(base_path, backup_dir);
                confirm_enter();

                if (result != 0) {
                    reset_screen("Error: Failed to backup /data_sdc2");
                } else {
                    reset_screen("Backup completed successfully");
                }
                break;
            }
            case RESTORE_DATA: {
                int src_option;
                int backup_count;
                int backup_choice;
                char backup_paths[MAX_BACKUPS][PATH_MAX];
                char restore_size_str[32];
                char available_str[32];
                unsigned long long restore_size = 0;
                unsigned long long available_space = 0;
                const char *base_path;
                const char *backup_dir;
                const char *backup_name;
                const char *selected_backup;
                char select_prompt[64];
                int i;

                clear_screen();
                print_header();
                printf("Select restore source:\n");
                printf("1) Internal storage (/sdcard)\n");
                printf("2) microSD (/data_sdc2/media/0)\n");
                printf("3) OTG (/usb-otg)\n");

                printf("\n");
                user_input("Select option [1-3], or any other to cancel: ", input, sizeof(input));

                val = strtol(input, &endptr, 10);
                if (endptr == input || (*endptr != '\n' && *endptr != '\0'))
                    src_option = -1;
                else
                    src_option = (int)val;

                if (src_option == 1) {
                    base_path = "/sdcard";
                    backup_dir = "/sdcard/TWRP/sdc2Tool/backup";
                } else if (src_option == 2) {
                    base_path = "/data_sdc2";
                    backup_dir = "/data_sdc2/media/0/TWRP/sdc2Tool/backup";
                } else if (src_option == 3) {
                    base_path = "/usb-otg";
                    backup_dir = "/usb-otg/TWRP/sdc2Tool/backup";
                } else {
                    reset_screen("Restore canceled.");
                    break;
                }

                clear_screen();
                print_header();

                backup_count = list_backups(base_path, backup_dir, backup_paths);

                if (backup_count == 0) {
                    confirm_enter();
                    reset_screen("Restore canceled.");
                    break;
                }

                printf("Available backups:\n");
                for (i = 0; i < backup_count; i++) {
                    backup_name = strrchr(backup_paths[i], '/');
                    backup_name = backup_name ? backup_name + 1 : backup_paths[i];
                    printf("%d) %s\n", i + 1, backup_name);
                }
                printf("\n");

                if (backup_count == 1) {
                    snprintf(select_prompt, sizeof(select_prompt),
                            "Select option [1], or any other to cancel: ");
                } else {
                    snprintf(select_prompt, sizeof(select_prompt),
                            "Select option [1-%d], or any other to cancel: ", backup_count);
                }
                user_input(select_prompt, input, sizeof(input));

                val = strtol(input, &endptr, 10);
                if (endptr == input || (*endptr != '\n' && *endptr != '\0'))
                    backup_choice = -1;
                else
                    backup_choice = (int)val;

                if (backup_choice < 1 || backup_choice > backup_count) {
                    reset_screen("Restore canceled.");
                    break;
                }

                clear_screen();
                print_header();

                selected_backup = backup_paths[backup_choice - 1];
                backup_name = strrchr(selected_backup, '/');
                printf("Selected backup archive: %s\n", backup_name ? backup_name + 1 : selected_backup);

                // Space check
                if (get_restore_size(selected_backup, &restore_size) != 0) {
                    confirm_enter();
                    reset_screen("Error: Failed to calculate restore size.");
                    break;
                }

                if (get_available_space_after_wipe(&available_space) != 0) {
                    confirm_enter();
                    reset_screen("Error: Failed to calculate available space on /data_sdc2.");
                    break;
                }

                format_size(restore_size, restore_size_str, sizeof(restore_size_str));
                format_size(available_space, available_str, sizeof(available_str));
                printf("Estimated restore size: %s\n", restore_size_str);
                printf("Available space on /data_sdc2: %s\n\n", available_str);

                if (available_space < restore_size)
                    printf("[WARNING] Available space may not be sufficient.\n\n");

                if (!confirm("Wipes /data_sdc2 (keeping /data_sdc2/media) and restores the selected backup. Continue?")) {
                    reset_screen("Restore canceled.");
                    break;
                }

                printf("\n");
                result = restore_data_sdc2(base_path, selected_backup);
                confirm_enter();

                if (result != 0)
                    reset_screen("Error: Failed to restore /data_sdc2.");
                else
                    reset_screen("Restore completed successfully.");
                break;
            }
            case EXIT_SDC2TOOL:
                running = 0;
                break;
            default:
                reset_screen("Invalid selection. Please choose 1-6.");
                break;
        }
    }

    return 0;
}
