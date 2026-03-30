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
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <selinux/selinux.h>

#include "selinux_context.h"

#define SELINUX_CONTEXT_MAX 256

/*
 * Recursively walks path, reads SELinux context of each entry and writes
 * "<relative_path>\t<context>\n" to fp.
 * rel_prefix is the path relative to root_path (used for the output).
 * skip_name: if not NULL, entries with this name are skipped at every level.
 */
static int walk_and_save(const char *abs_path, const char *rel_prefix,
                         const char *skip_name, FILE *fp) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char abs_entry[PATH_MAX];
    char rel_entry[PATH_MAX];
    char *context = NULL;

    dir = opendir(abs_path);
    if (!dir) {
        fprintf(stderr, "[ERROR] Failed to open directory '%s': %s\n",
                abs_path, strerror(errno));
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        if (skip_name && strcmp(entry->d_name, skip_name) == 0)
            continue;

        snprintf(abs_entry, sizeof(abs_entry), "%s/%s", abs_path, entry->d_name);
        snprintf(rel_entry, sizeof(rel_entry), "%s/%s", rel_prefix, entry->d_name);

        if (lstat(abs_entry, &st) != 0) {
            fprintf(stderr, "[WARN] lstat failed for '%s': %s\n",
                    abs_entry, strerror(errno));
            continue;
        }

        /* Skip sockets and other special files tar cannot handle */
        if (S_ISSOCK(st.st_mode))
            continue;

        context = NULL;
        if (lgetfilecon(abs_entry, &context) < 0) {
            fprintf(stderr, "[WARN] Failed to get SELinux context for '%s': %s\n",
                    abs_entry, strerror(errno));
        } else {
            fprintf(fp, "%s\t%s\n", rel_entry, context);
            freecon(context);
        }

        if (S_ISDIR(st.st_mode)) {
            if (walk_and_save(abs_entry, rel_entry, NULL, fp) != 0) {
                closedir(dir);
                return 1;
            }
        }
    }

    closedir(dir);
    return 0;
}

int save_selinux_contexts(const char *root_path, const char *skip_name,
                          const char *context_out_file) {
    FILE *fp;
    int result;

    fp = fopen(context_out_file, "w");
    if (!fp) {
        fprintf(stderr, "[ERROR] Failed to create context file '%s': %s\n",
                context_out_file, strerror(errno));
        return 1;
    }

    /* Walk from root, relative prefix starts empty ("") so paths look like
     * "/system/packages.xml" instead of "/data_sdc2/system/packages.xml" */
    result = walk_and_save(root_path, "", skip_name, fp);

    fclose(fp);

    if (result != 0) {
        fprintf(stderr, "[ERROR] Failed to save SELinux contexts\n");
        return 1;
    }

    return 0;
}

int restore_selinux_contexts(const char *root_path, const char *context_file) {
    FILE *fp;
    char line[PATH_MAX + SELINUX_CONTEXT_MAX];
    char rel_path[PATH_MAX];
    char context[SELINUX_CONTEXT_MAX];
    char abs_path[PATH_MAX];
    int errors = 0;

    fp = fopen(context_file, "r");
    if (!fp) {
        fprintf(stderr, "[ERROR] Failed to open context file '%s': %s\n",
                context_file, strerror(errno));
        return 1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\n")] = '\0';

        /* Parse "<rel_path>\t<context>" */
        char *tab = strchr(line, '\t');
        if (!tab) {
            fprintf(stderr, "[WARN] Malformed line in context file, skipping\n");
            continue;
        }

        *tab = '\0';
        strncpy(rel_path, line, sizeof(rel_path) - 1);
        rel_path[sizeof(rel_path) - 1] = '\0';
        strncpy(context, tab + 1, sizeof(context) - 1);
        context[sizeof(context) - 1] = '\0';

        snprintf(abs_path, sizeof(abs_path), "%s%s", root_path, rel_path);

        if (lsetfilecon(abs_path, context) != 0) {
            fprintf(stderr, "[WARN] Failed to restore context for '%s': %s\n",
                    abs_path, strerror(errno));
            errors++;
        }
    }

    fclose(fp);

    if (errors > 0)
        fprintf(stderr, "[WARN] %d file(s) failed to restore SELinux context\n", errors);

    return 0;
}