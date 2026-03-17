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
#include <string.h>
#include <sys/stat.h>

#include "utilities.h"

#define DEV_BLOCK_MICROSD "/dev/block/mmcblk1"

int is_microsd_available(void) {
    struct stat st;

    if (stat(DEV_BLOCK_MICROSD, &st) != 0) {
        return 1;
    }

    if (!S_ISBLK(st.st_mode)) {
        return 1;
    }

    return 0;
}

int is_mounted(const char* mount_point) {
    FILE* mount_points;
    char line[256], dev[256], mnt[256];
    int ret = 0;

    mount_points = fopen("/proc/mounts", "r");
    if (!mount_points)
        return ret;

    while (fgets(line, sizeof(line), mount_points) != NULL) {
        if (sscanf(line, "%255s %255s", dev, mnt) == 2) {
            if (strcmp(mnt, mount_point) == 0) {
                ret = 1;
                break;
            }
        }
    }

    fclose(mount_points);
    return ret;
}
