/*
 * Copyright 2014 LKC Technologies, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fwup_create.h"
#include "cfgfile.h"
#include "util.h"
#include "3rdparty/sha2.h"
#include "fwfile.h"

#include <stdlib.h>
#include <stdio.h>
#include <archive.h>
#include <archive_entry.h>

static int compute_file_metadata(cfg_t *cfg)
{
    cfg_t *sec;
    int i = 0;

    while ((sec = cfg_getnsec(cfg, "file-resource", i++)) != NULL) {
        const char *path = cfg_getstr(sec, "host-path");
        if (!path)
            ERR_RETURN("host-path must be set for file-report");

        FILE *fp = fopen(path, "rb");
        if (!fp)
            ERR_RETURN("can't open file-resource '%s'", path);

        SHA256_CTX ctx256;
        char buffer[1024];
        size_t len = fread(buffer, 1, sizeof(buffer), fp);
        size_t total = 0;
        while (len > 0) {
            SHA256_Update(&ctx256, (unsigned char*) buffer, len);
            total += len;
            len = fread(buffer, 1, sizeof(buffer), fp);
        }
        char digest[SHA256_DIGEST_STRING_LENGTH];
        SHA256_End(&ctx256, digest);

        cfg_setstr(sec, "sha256", digest);
        cfg_setint(sec, "length", total);
    }

    return 0;
}

static int add_file_resources(cfg_t *cfg, struct archive *a)
{
    cfg_t *sec;
    int i = 0;

    while ((sec = cfg_getnsec(cfg, "file-resource", i++)) != NULL) {
        const char *hostpath = cfg_getstr(sec, "host-path");
        if (fwfile_add_local_file(a, cfg_title(sec), hostpath) < 0)
            return -1;
    }

    return 0;
}

static int create_archive(cfg_t *cfg, const char *filename)
{
    struct archive *a = archive_write_new();
    archive_write_set_format_zip(a);
    if (archive_write_open_filename(a, filename) != ARCHIVE_OK)
        ERR_RETURN("error creating archive");

    if (fwfile_add_meta_conf(cfg, a) < 0)
        return -1;

    if (add_file_resources(cfg, a) < 0)
        return -1;

    archive_write_close(a);
    archive_write_free(a);

    return 0;
}

int fwup_create(const char *configfile, const char *output_firmware)
{
    cfg_t *cfg;

    // Set the NOW environment variable for use by the config script.
    set_now_time();

    // Parse configuration
    if (cfgfile_parse_file(configfile, &cfg) < 0)
        return -1;

    // Force the creation date to be set
    cfg_setstr(cfg, "meta-creation-date", get_creation_timestamp());

    // Compute all metadata
    if (compute_file_metadata(cfg))
        return -1;

    // Create the archive
    if (create_archive(cfg, output_firmware) < 0)
        ERR_RETURN("Error creating archive");

    cfgfile_free(cfg);
    return 0;
}
