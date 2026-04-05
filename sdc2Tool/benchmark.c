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
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/time.h>

#include "constants.h"
#include "benchmark.h"
#include "utilities.h"

#define BENCHMARK_BLOCK_EXTSD    "/dev/block/mmcblk1p1"
#define BENCHMARK_MOUNT_EXTSD    "/external_sd"
#define BENCHMARK_FILE           DATA_MEDIA_0 "/.sdc2tool_benchmark_tmp"
#define BENCHMARK_FILE_EXTSD     BENCHMARK_MOUNT_EXTSD "/.sdc2tool_benchmark_tmp"
#define BENCHMARK_SIZE_MB        1024
#define BENCHMARK_BLOCK_MB       1
#define MB                       (1024 * 1024)
#define BENCHMARK_4K_BLOCK       4096
#define BENCHMARK_4K_OPS         4096  // number of random 4K operations

// Pointer to temp file path for signal handler cleanup
static const char *g_benchmark_file = NULL;

static void benchmark_signal_handler(int sig) {
    (void)sig;
    if (g_benchmark_file)
        unlink(g_benchmark_file);
    printf("\n[INFO] Benchmark interrupted, temporary file removed\n");
    exit(1);
}

static double get_time_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static const char *write_rating(double mbps) {
    if (mbps >= 45.0)
        return "Peak Performance";
    if (mbps >= 30.0)
        return "Excellent";
    if (mbps >= 20.0)
        return "Good";
    if (mbps >= 10.0)
        return "Average";
    return "Not Suitable";
}

static const char *read_rating(double mbps) {
    if (mbps >= 75.0)
        return "Peak Performance";
    if (mbps >= 60.0)
        return "Excellent";
    if (mbps >= 40.0)
        return "Good";
    if (mbps >= 20.0)
        return "Average";
    return "Not Suitable";
}

static int run_write_benchmark(const char *path, double *mbps_out) {
    int fd;
    char *buf;
    int blocks = BENCHMARK_SIZE_MB / BENCHMARK_BLOCK_MB;
    ssize_t written;
    double start, elapsed;
    int i;

    buf = malloc(BENCHMARK_BLOCK_MB * MB);
    if (!buf) {
        fprintf(stderr, "[ERROR] Failed to allocate write buffer\n");
        return 1;
    }
    memset(buf, 0xAA, BENCHMARK_BLOCK_MB * MB);

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr, "[ERROR] Failed to open benchmark file for writing: %s\n",
                strerror(errno));
        free(buf);
        return 1;
    }

    start = get_time_seconds();

    for (i = 0; i < blocks; i++) {
        written = write(fd, buf, BENCHMARK_BLOCK_MB * MB);
        if (written != BENCHMARK_BLOCK_MB * MB) {
            fprintf(stderr, "\n[ERROR] Write failed at block %d: %s\n",
                    i, strerror(errno));
            close(fd);
            free(buf);
            return 1;
        }
        if ((i + 1) % 64 == 0 || i == blocks - 1) {
            printf("\rWriting... %d / %d MiB", (i + 1) * BENCHMARK_BLOCK_MB,
                BENCHMARK_SIZE_MB);
            fflush(stdout);
        }
    }

    // Flush kernel buffers to get accurate timing
    fsync(fd);
    elapsed = get_time_seconds() - start;

    close(fd);
    free(buf);

    *mbps_out = (double)BENCHMARK_SIZE_MB / elapsed;
    printf("\rWrite: %.1f MiB/s (in %.2fs)      \n",
           *mbps_out, elapsed);

    return 0;
}

// Drop page cache so the read test measures actual device speed
static int drop_page_cache(void) {
    int fd;

    sync();

    fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
    if (fd < 0)
        return 1;

    if (write(fd, "1\n", 2) != 2) {
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

static int run_read_benchmark(const char *path, double *mbps_out) {
    int fd;
    char *buf;
    int blocks = BENCHMARK_SIZE_MB / BENCHMARK_BLOCK_MB;
    ssize_t bytes_read;
    double start, elapsed;
    int i;

    buf = malloc(BENCHMARK_BLOCK_MB * MB);
    if (!buf) {
        fprintf(stderr, "[ERROR] Failed to allocate read buffer\n");
        return 1;
    }

    if (drop_page_cache() != 0)
        fprintf(stderr, "[WARN] Failed to drop page cache, read results may be inaccurate\n");

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[ERROR] Failed to open benchmark file for reading: %s\n",
                strerror(errno));
        free(buf);
        return 1;
    }

    start = get_time_seconds();

    for (i = 0; i < blocks; i++) {
        bytes_read = read(fd, buf, BENCHMARK_BLOCK_MB * MB);
        if (bytes_read != BENCHMARK_BLOCK_MB * MB) {
            fprintf(stderr, "\n[ERROR] Read failed at block %d: %s\n",
                    i, strerror(errno));
            close(fd);
            free(buf);
            return 1;
        }
        if ((i + 1) % 64 == 0 || i == blocks - 1) {
            printf("\rReading... %d / %d MiB", (i + 1) * BENCHMARK_BLOCK_MB,
                BENCHMARK_SIZE_MB);
            fflush(stdout);
        }
    }

    elapsed = get_time_seconds() - start;

    close(fd);
    free(buf);

    *mbps_out = (double)BENCHMARK_SIZE_MB / elapsed;
    printf("\rRead:  %.1f MiB/s (in %.2fs)      \n",
           *mbps_out, elapsed);

    return 0;
}

static const char *app_class_rating(double iops_read, double iops_write) {
    if (iops_read >= 4000.0 && iops_write >= 2000.0)
        return "A2";
    if (iops_read >= 1500.0 && iops_write >= 500.0)
        return "A1";
    return "Below A1";
}

static int run_4k_write_benchmark(const char *path, off_t file_size, double *iops_out) {
    int fd;
    char buf[BENCHMARK_4K_BLOCK];
    off_t max_blocks;
    double start, elapsed;
    int i;

    memset(buf, 0xBB, sizeof(buf));

    max_blocks = file_size / BENCHMARK_4K_BLOCK;

    fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "[ERROR] Failed to open benchmark file for 4K write: %s\n",
                strerror(errno));
        return 1;
    }

    printf("Running 4K write test (%d ops)...", BENCHMARK_4K_OPS);
    fflush(stdout);

    start = get_time_seconds();

    for (i = 0; i < BENCHMARK_4K_OPS; i++) {
        off_t offset = (off_t)(rand() % max_blocks) * BENCHMARK_4K_BLOCK;
        if (lseek(fd, offset, SEEK_SET) < 0 ||
            write(fd, buf, BENCHMARK_4K_BLOCK) != BENCHMARK_4K_BLOCK) {
            fprintf(stderr, "\n[ERROR] 4K write failed at op %d: %s\n",
                    i, strerror(errno));
            close(fd);
            return 1;
        }

    }

    fsync(fd);
    elapsed = get_time_seconds() - start;
    close(fd);

    *iops_out = (double)BENCHMARK_4K_OPS / elapsed;
    printf("\r4K Write: %.0f IOPS (%.1f MiB/s)      \n",
           *iops_out, (*iops_out * BENCHMARK_4K_BLOCK) / MB);

    return 0;
}

static int run_4k_read_benchmark(const char *path, off_t file_size, double *iops_out) {
    int fd;
    char buf[BENCHMARK_4K_BLOCK];
    off_t max_blocks;
    double start, elapsed;
    int i;

    max_blocks = file_size / BENCHMARK_4K_BLOCK;

    if (drop_page_cache() != 0)
        fprintf(stderr, "[WARN] Failed to drop page cache, 4K read results may be inaccurate\n");

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[ERROR] Failed to open benchmark file for 4K read: %s\n",
                strerror(errno));
        return 1;
    }

    printf("Running 4K read test (%d ops)...", BENCHMARK_4K_OPS);
    fflush(stdout);

    start = get_time_seconds();

    for (i = 0; i < BENCHMARK_4K_OPS; i++) {
        off_t offset = (off_t)(rand() % max_blocks) * BENCHMARK_4K_BLOCK;
        if (lseek(fd, offset, SEEK_SET) < 0 ||
            read(fd, buf, BENCHMARK_4K_BLOCK) != BENCHMARK_4K_BLOCK) {
            fprintf(stderr, "\n[ERROR] 4K read failed at op %d: %s\n",
                    i, strerror(errno));
            close(fd);
            return 1;
        }
    }

    elapsed = get_time_seconds() - start;
    close(fd);

    *iops_out = (double)BENCHMARK_4K_OPS / elapsed;
    printf("\r4K Read:  %.0f IOPS (%.1f MiB/s)      \n",
           *iops_out, (*iops_out * BENCHMARK_4K_BLOCK) / MB);

    return 0;
}

int benchmark_microsd(void) {
    struct stat st;
    struct sigaction sa;
    double write_mbps = 0.0;
    double read_mbps = 0.0;
    double iops_4k_write = 0.0;
    double iops_4k_read = 0.0;
    unsigned long long free_space = 0;
    unsigned long long required = (unsigned long long)BENCHMARK_SIZE_MB * MB;
    off_t bench_file_size = (off_t)BENCHMARK_SIZE_MB * MB;
    char free_str[32];
    char req_str[32];
    const char *bench_file;
    const char *bench_mount;
    int extsd_mounted = 0;
    int result;

    if (is_microsd_available() != 0) {
        fprintf(stderr, "[ERROR] No microSD card detected\n");
        return 1;
    }

    if (stat(DATA_PART, &st) == 0) {
        if (!is_mounted(DATA_MOUNT_POINT)) {
            if (mount(DATA_PART, DATA_MOUNT_POINT, "f2fs", 0, NULL) != 0 &&
                mount(DATA_PART, DATA_MOUNT_POINT, "ext4", 0, NULL) != 0) {
                fprintf(stderr, "[ERROR] Failed to mount '%s': %s\n",
                        DATA_MOUNT_POINT, strerror(errno));
                return 1;
            }
        }
        bench_file  = BENCHMARK_FILE;
        bench_mount = DATA_MOUNT_POINT;
    } else {
        /*
        * Fallback to traditional mount point (/dev/block/mmcblk1p1 on /external_sd)
        * in case the microSD card has no userdata partition.
        */
        if (!is_mounted(BENCHMARK_MOUNT_EXTSD)) {
            // Uses sdfat to mount exFAT or FAT32 formatted microSD cards.
            if (mount(BENCHMARK_BLOCK_EXTSD, BENCHMARK_MOUNT_EXTSD, "sdfat", 0, NULL) != 0) {
                fprintf(stderr, "[ERROR] Failed to mount '%s' on '%s': %s\n",
                        BENCHMARK_BLOCK_EXTSD, BENCHMARK_MOUNT_EXTSD, strerror(errno));
                return 1;
            }
            extsd_mounted = 1;
        }
        bench_file  = BENCHMARK_FILE_EXTSD;
        bench_mount = BENCHMARK_MOUNT_EXTSD;
    }

    // Register signal handler for cleanup on Ctrl+C
    g_benchmark_file = bench_file;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = benchmark_signal_handler;
    sigaction(SIGINT, &sa, NULL);

    // Check if enough free space is available for the benchmark file
    if (get_free_space(bench_mount, &free_space) != 0) {
        fprintf(stderr, "[ERROR] Failed to determine free space on '%s'\n",
                bench_mount);
        g_benchmark_file = NULL;
        return 1;
    }

    if (free_space < required) {
        format_size(free_space, free_str, sizeof(free_str));
        format_size(required, req_str, sizeof(req_str));
        fprintf(stderr, "[ERROR] Not enough free space for benchmark\n"
                        "Required: %s, Available: %s\n",
                        req_str, free_str);
        g_benchmark_file = NULL;
        return 1;
    }

    printf("Benchmark size: %d MiB\n\n", BENCHMARK_SIZE_MB);

    result = run_write_benchmark(bench_file, &write_mbps);
    if (result != 0) {
        unlink(bench_file);
        g_benchmark_file = NULL;
        if (extsd_mounted)
            umount(BENCHMARK_MOUNT_EXTSD);
        return 1;
    }

    result = run_read_benchmark(bench_file, &read_mbps);
    if (result != 0) {
        unlink(bench_file);
        g_benchmark_file = NULL;
        if (extsd_mounted)
            umount(BENCHMARK_MOUNT_EXTSD);
        return 1;
    }

    // Seed random for 4K offset generation
    srand((unsigned int)get_time_seconds());

    result = run_4k_write_benchmark(bench_file, bench_file_size, &iops_4k_write);
    if (result != 0) {
        unlink(bench_file);
        g_benchmark_file = NULL;
        if (extsd_mounted)
            umount(BENCHMARK_MOUNT_EXTSD);
        return 1;
    }

    result = run_4k_read_benchmark(bench_file, bench_file_size, &iops_4k_read);
    unlink(bench_file);
    g_benchmark_file = NULL;

    // Restore default SIGINT handler
    sa.sa_handler = SIG_DFL;
    sigaction(SIGINT, &sa, NULL);

    if (extsd_mounted)
        umount(BENCHMARK_MOUNT_EXTSD);

    if (result != 0)
        return 1;

    printf("\n");
    printf("Sequential:\n");
    printf("  Write: %.1f MiB/s -> %s\n", write_mbps, write_rating(write_mbps));
    printf("  Read:  %.1f MiB/s -> %s\n", read_mbps, read_rating(read_mbps));
    printf("\n");
    printf("Random 4K:\n");
    printf("  Write: %.0f IOPS (%.1f MiB/s)\n",
           iops_4k_write, (iops_4k_write * BENCHMARK_4K_BLOCK) / MB);
    printf("  Read:  %.0f IOPS (%.1f MiB/s)\n",
           iops_4k_read, (iops_4k_read * BENCHMARK_4K_BLOCK) / MB);
    printf("\n");
    printf("  Performance Class (estimated): %s\n",
           app_class_rating(iops_4k_read, iops_4k_write));

    return 0;
}
