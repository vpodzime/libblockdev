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

#include <string.h>
#include <parted/parted.h>

#include "part.h"

/**
 * SECTION: part
 * @short_description: plugin for operations with partition tables
 * @title: Part
 * @include: part.h
 *
 * A plugin for operations with partition tables.
 */

/**
 * bd_part_error_quark: (skip)
 */
GQuark bd_part_error_quark (void)
{
    return g_quark_from_static_string ("g-bd-part-error-quark");
}

/* in order to be thread-safe, we need to make sure every thread has this
   variable for its own use */
static __thread gchar *error_msg = NULL;

static PedExceptionOption exc_handler (PedException *ex) {
    error_msg = g_strdup (ex->message);
    return PED_EXCEPTION_UNHANDLED;
}

/**
 * set_parted_error: (skip)
 *
 * Set error from the parted error stored in 'error_msg'. In case there is none,
 * the error is set up with an empty string. Otherwise it is set up with the
 * parted's error message and is a subject to later g_prefix_error() call.
 */
static void set_parted_error (GError **error, BDPartError type) {
    if (error_msg) {
        g_set_error (error, BD_PART_ERROR, type,
                     " (%s)", error_msg);
        g_free (error_msg);
    } else
        g_set_error_literal (error, BD_PART_ERROR, type, "");
}

/**
 * init: (skip)
 */
gboolean init() {
    ped_exception_set_handler ((PedExceptionHandler*) exc_handler);
    return TRUE;
}

static const char *table_type_str[BD_PART_TABLE_UNDEF] = {"msdos", "gpt"};

/**
 * bd_part_create_table:
 * @disk: path of the disk block device to create partition table on
 * @type: type of the partition table to create
 * @ignore_existing: whether to ignore/overwrite the existing table or not
 *                   (reports an error if %FALSE and there's some table on @disk)
 * @error: (out): place to store error (if any)
 *
 * Returns: whether the partition table was successfully created or not
 */
gboolean bd_part_create_table (gchar *disk, BDPartTableType type, gboolean ignore_existing, GError **error) {
    PedDevice *dev = NULL;
    PedDisk *ped_disk = NULL;
    PedDiskType *disk_type = NULL;
    gint ret = 0;

    dev = ped_device_get (disk);
    if (!dev) {
        set_parted_error (error, BD_PART_ERROR_INVAL);
        g_prefix_error (error, "Device '%s' invalid or not existing", disk);
        return FALSE;
    }

    if (!ignore_existing) {
        ped_disk = ped_disk_new (dev);
        if (ped_disk) {
            /* no parted error */
            g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_EXISTS,
                         "Device '%s' already contains a partition table", disk);
            ped_disk_destroy (ped_disk);
            ped_device_destroy (dev);
            return FALSE;
        }
    }

    disk_type = ped_disk_type_get (table_type_str[type]);
    ped_disk = ped_disk_new_fresh (dev, disk_type);
    if (!ped_disk) {
        set_parted_error (error, BD_PART_ERROR_FAIL);
        g_prefix_error (error, "Failed to create a new partition table of type '%s' on device '%s'",
                        table_type_str[type], disk);
        ped_device_destroy (dev);
        return FALSE;
    }


    ret = ped_disk_commit_to_dev (ped_disk);
    if (ret == 0) {
        set_parted_error (error, BD_PART_ERROR_FAIL);
        g_prefix_error (error, "Failed to commit changes to device '%s'", disk);
        ped_disk_destroy (ped_disk);
        ped_device_destroy (dev);
        return FALSE;
    }
    ret = ped_disk_commit_to_os (ped_disk);

    ped_disk_destroy (ped_disk);
    ped_device_destroy (dev);

    if (ret == 0) {
        set_parted_error (error, BD_PART_ERROR_FAIL);
        g_prefix_error (error, "Failed to inform OS about changes on the '%s' device", disk);
        return FALSE;
    }

    return TRUE;
}
