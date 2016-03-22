/*
 * Copyright (C) 2016  Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Vratislav Podzimek <vpodzime@redhat.com>
 */

#include <exec.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <blkid.h>

#include "fs.h"

/**
 * SECTION: fs
 * @short_description: plugin for operations with file systems
 * @title: FS
 * @include: fs.h
 *
 * A plugin for operations with file systems
 */

/**
 * bd_fs_error_quark: (skip)
 */
GQuark bd_fs_error_quark (void)
{
    return g_quark_from_static_string ("g-bd-fs-error-quark");
}

/**
 * check: (skip)
 */
gboolean check() {
    GError *error = NULL;
    gboolean ret = bd_utils_check_util_version ("mkfs.ext4", NULL, "", NULL, &error);

    if (!ret && error) {
        g_warning("Cannot load the FS plugin: %s" , error->message);
        g_clear_error (&error);
    }
    return ret;
}

/**
 * bd_fs_wipe:
 * @device: the device to wipe signatures from
 * @all: whether to wipe all (%TRUE) signatures or just the first (%FALSE) one
 * @error: (out): place to store error (if any)
 *
 * Returns: whether signagures were successfully wiped on @device or not
 */
gboolean bd_fs_wipe (gchar *device, gboolean all, GError **error) {
    blkid_probe probe = NULL;
    gint fd = 0;
    gint status = 0;

    probe = blkid_new_probe ();
    if (!probe) {
        g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_FAIL,
                     "Failed to create a probe for the device '%s'", device);
        return FALSE;
    }

    fd = open (device, O_RDWR|O_CLOEXEC);
    if (fd == -1) {
        g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_FAIL,
                     "Failed to create a probe for the device '%s'", device);
        blkid_free_probe (probe);
        return FALSE;
    }

    status = blkid_probe_set_device (probe, fd, 0, 0);
    if (status != 0) {
        g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_FAIL,
                     "Failed to create a probe for the device '%s'", device);
        blkid_free_probe (probe);
        close (fd);
        return FALSE;
    }

	blkid_probe_enable_partitions(probe, 1);
	blkid_probe_set_partitions_flags(probe, BLKID_PARTS_MAGIC);
	blkid_probe_enable_superblocks(probe, 1);
	blkid_probe_set_superblocks_flags(probe, BLKID_SUBLKS_MAGIC | BLKID_SUBLKS_BADCSUM);

    status = blkid_do_probe (probe);
    if (status != 0) {
        g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_FAIL,
                     "Failed to probe the device '%s'", device);
        blkid_free_probe (probe);
        close (fd);
        return FALSE;
    }

    status = blkid_do_wipe (probe, FALSE);
    if (status != 0) {
        g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_FAIL,
                     "Failed to wipe signatures on the device '%s'", device);
        blkid_free_probe (probe);
        close (fd);
        return FALSE;
    }
    while (all && (blkid_do_probe (probe) == 0)) {
        status = blkid_do_wipe (probe, FALSE);
        if (status != 0) {
            g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_FAIL,
                         "Failed to wipe signatures on the device '%s'", device);
            blkid_free_probe (probe);
            close (fd);
            return FALSE;
        }
    }

    blkid_free_probe (probe);
    close (fd);

    return TRUE;
}

static gboolean wipe_fs (gchar *device, gchar *fs_type, GError **error) {
    blkid_probe probe = NULL;
    gint fd = 0;
    gint status = 0;
    const gchar *value = NULL;
    size_t len = 0;

    probe = blkid_new_probe ();
    if (!probe) {
        g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_FAIL,
                     "Failed to create a probe for the device '%s'", device);
        return FALSE;
    }

    fd = open (device, O_RDWR|O_CLOEXEC);
    if (fd == -1) {
        g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_FAIL,
                     "Failed to create a probe for the device '%s'", device);
        blkid_free_probe (probe);
        return FALSE;
    }

    status = blkid_probe_set_device (probe, fd, 0, 0);
    if (status != 0) {
        g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_FAIL,
                     "Failed to create a probe for the device '%s'", device);
        blkid_free_probe (probe);
        close (fd);
        return FALSE;
    }

	blkid_probe_enable_partitions(probe, 1);
	blkid_probe_set_partitions_flags(probe, BLKID_PARTS_MAGIC);
	blkid_probe_enable_superblocks(probe, 1);
	blkid_probe_set_superblocks_flags(probe, BLKID_SUBLKS_USAGE | BLKID_SUBLKS_TYPE |
                                             BLKID_SUBLKS_MAGIC | BLKID_SUBLKS_BADCSUM);

    status = blkid_do_probe (probe);
    if (status != 0) {
        g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_FAIL,
                     "Failed to probe the device '%s'", device);
        blkid_free_probe (probe);
        close (fd);
        return FALSE;
    }

    status = blkid_probe_lookup_value (probe, "USAGE", &value, NULL);
    if (status != 0) {
        g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_FAIL,
                     "Failed to get signature type for the device '%s'", device);
        blkid_free_probe (probe);
        close (fd);
        return FALSE;
    }

    if (strncmp (value, "filesystem", 10) != 0) {
        g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_INVAL,
                     "The signature on the device '%s' is of type '%s', not 'filesystem'", device, value);
        blkid_free_probe (probe);
        close (fd);
        return FALSE;
    }

    if (fs_type) {
        status = blkid_probe_lookup_value (probe, "TYPE", &value, &len);
        if (status != 0) {
            g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_FAIL,
                         "Failed to get filesystem type for the device '%s'", device);
            blkid_free_probe (probe);
            close (fd);
            return FALSE;
        }

        if (strncmp (value, fs_type, len-1) != 0) {
            g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_INVAL,
                         "The file system type on the device '%s' is '%s', not '%s'", device, value, fs_type);
            blkid_free_probe (probe);
            close (fd);
            return FALSE;
        }
    }

    status = blkid_do_wipe (probe, FALSE);
    if (status != 0) {
        g_set_error (error, BD_FS_ERROR, BD_FS_ERROR_FAIL,
                     "Failed to wipe the filesystem signature on the device '%s'", device);
        blkid_free_probe (probe);
        close (fd);
        return FALSE;
    }

    blkid_free_probe (probe);
    close (fd);
    return TRUE;
}

/**
 * bd_fs_ext4_mkfs:
 * @device: the device to create a new ext4 fs on
 * @error: (out): place to store error (if any)
 *
 * Returns: whether a new ext4 fs was successfully created on @device or not
 */
gboolean bd_fs_ext4_mkfs (gchar *device, GError **error) {
    gchar *args[3] = {"mkfs.ext4", device, NULL};

    return bd_utils_exec_and_report_error (args, error);
}

/**
 * bd_fs_ext4_wipe:
 * @device: the device to wipe an ext4 signature from
 * @error: (out): place to store error (if any)
 *
 * Returns: whether an ext4 signature was successfully wiped from the @device or
 *          not
 */
gboolean bd_fs_ext4_wipe (gchar *device, GError **error) {
    return wipe_fs (device, "ext4", error);
}
