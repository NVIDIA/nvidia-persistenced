/*
 * nvidia-persistenced: A daemon for maintaining persistent driver state,
 * specifically for use by the NVIDIA Linux driver.
 *
 * Copyright (C) 2013-2016 NVIDIA Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * options.c
 */

#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "common-utils.h"
#include "msg.h"
#include "nvgetopt.h"
#include "nvidia-persistenced.h"
#include "nvpd_defs.h"
#include "option-table.h"

#define TAB    "  "
#define BIGTAB "      "

extern const char *pNV_ID;

/*
 * print_version() - print basic version information about the utility.
 */
static void print_version(void)
{
    nv_info_msg(NULL, "");
    nv_info_msg(NULL, "%s", pNV_ID);
    nv_info_msg(NULL, "");
    nv_info_msg(TAB, "The NVIDIA Persistence Daemon.");
    nv_info_msg(NULL, "");
    nv_info_msg(TAB, "A tool for maintaining persistent driver state, "
                     "specifically for use by the NVIDIA Linux driver.");
    nv_info_msg(NULL, "");
    nv_info_msg(TAB, "Copyright (C) 2013 NVIDIA Corporation.");
    nv_info_msg(NULL, "");
}

/*
 * print_help_callback() - used by nvgetopt to format help output.
 */
static void print_help_callback(const char *name, const char *description)
{
    nv_info_msg(TAB, "%s", name);
    nv_info_msg(BIGTAB, "%s", description);
    nv_info_msg(NULL, "");
}

/*
 * print_help() - loop through the __options, and print the description of
 * each option.
 */
static void print_help(void)
{
    print_version();

    nv_info_msg(NULL, "");
    nv_info_msg(NULL, NVPD_DAEMON_NAME " [options]");
    nv_info_msg(NULL, "");

    nvgetopt_print_help(__options, 0, print_help_callback);
}

/*
 * setup_option_defaults() - loads sensible defaults for the commandline
 * options.
 */
static void setup_option_defaults(NvPdOptions *options)
{
    options->persistence_mode = NV_PERSISTENCE_MODE_ENABLED;
    options->nvidia_cfg_path = NULL;
    options->verbose = 0;
    options->uid = getuid();
    options->gid = getgid();
}

/*
 * parse_options() - fill in an NvPdOptions structure with any pertinent data
 * from the commandline arguments.
 */
void parse_options(int argc, char *argv[], NvPdOptions *options)
{
    int short_name;
    int boolval;
    char *strval;
    struct passwd *pw_entry;

    setup_option_defaults(options);

    while (1)
    {
        short_name = nvgetopt(argc, argv, __options, &strval, &boolval,
                              NULL,      /* intval    */
                              NULL,      /* doubleval */
                              NULL);     /* disable   */
        if (short_name == -1)
        {
            break;
        }

        switch (short_name)
        {
            case 'v':
                print_version();
                exit(EXIT_SUCCESS);
                break;
            case 'h':
                print_help();
                exit(EXIT_SUCCESS);
                break;
            case 'V':
                options->verbose = 1;
                break;
            case 'u':
                pw_entry = getpwnam(strval);
                if (pw_entry == NULL) {
                    nv_error_msg("Failed to find user ID of user '%s': %s", strval,
                                 strerror(errno));
                    exit(EXIT_FAILURE);
                }
                options->uid = pw_entry->pw_uid;
                options->gid = pw_entry->pw_gid;
                break;
            case PERSISTENCE_MODE_OPTION:
                if (boolval) {
                    options->persistence_mode = NV_PERSISTENCE_MODE_ENABLED;
                } else {
                    options->persistence_mode = NV_PERSISTENCE_MODE_DISABLED;
                }
                break;
            case NVIDIA_CFG_PATH_OPTION:
                options->nvidia_cfg_path = strval;
                break;
            default:
                nv_error_msg("Invalid commandline, please run `%s --help` for "
                             "usage information.", argv[0]);
                exit(EXIT_SUCCESS);
        }
    }
}
