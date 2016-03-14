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
#include <ctype.h>
#include <stdlib.h>

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

static gboolean disk_commit (PedDisk *disk, gchar *path, GError **error) {
    gint ret = 0;

    ret = ped_disk_commit_to_dev (disk);
    if (ret == 0) {
        set_parted_error (error, BD_PART_ERROR_FAIL);
        g_prefix_error (error, "Failed to commit changes to device '%s'", path);
        return FALSE;
    }

    ret = ped_disk_commit_to_os (disk);
    if (ret == 0) {
        set_parted_error (error, BD_PART_ERROR_FAIL);
        g_prefix_error (error, "Failed to inform OS about changes on the '%s' device", path);
        return FALSE;
    }

    return TRUE;
}

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
    gboolean ret = NULL;

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

    /* commit changes to disk */
    ret = disk_commit (ped_disk, disk, error);

    ped_disk_destroy (ped_disk);
    ped_device_destroy (dev);

    /* just return what we got (error may be set) */
    return ret;
}

/**
 * bd_part_delete_part:
 * @disk: disk to remove the partition from
 * @part: partition to remove
 * @error: (out): place to store error (if any)
 *
 * Returns: whether the @part partition was successfully deleted from @disk
 */
gboolean bd_part_delete_part (gchar *disk, gchar *part, GError **error) {
    PedDevice *dev = NULL;
    PedDisk *ped_disk = NULL;
    PedPartition *ped_part = NULL;
    gchar *part_num_str = NULL;
    gint part_num = 0;
    gint status = 0;
    gboolean ret = FALSE;

    if (!part || (part && (*part == '\0'))) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_INVAL,
                     "Invalid partition path given: '%s'", part);
        return FALSE;
    }

    dev = ped_device_get (disk);
    if (!dev) {
        set_parted_error (error, BD_PART_ERROR_INVAL);
        g_prefix_error (error, "Device '%s' invalid or not existing", disk);
        return FALSE;
    }

    ped_disk = ped_disk_new (dev);
    if (!ped_disk) {
        set_parted_error (error, BD_PART_ERROR_FAIL);
        g_prefix_error (error, "Failed to read partition table on device '%s'", disk);
        ped_disk_destroy (ped_disk);
        ped_device_destroy (dev);
        return FALSE;
    }

    part_num_str = part + (strlen (part) - 1);
    while (isdigit (*part_num_str) || (*part_num_str == '-')) {
        part_num_str--;
    }
    part_num_str++;

    part_num = atoi (part_num_str);
    if (part_num == 0) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_INVAL,
                     "Invalid partition path given: '%s'. Cannot extract partition number", part);
        ped_disk_destroy (ped_disk);
        ped_device_destroy (dev);
        return FALSE;
    }

    ped_part = ped_disk_get_partition (ped_disk, part_num);
    if (!ped_part) {
        set_parted_error (error, BD_PART_ERROR_FAIL);
        g_prefix_error (error, "Failed to get partition '%d' on device '%s'", part_num, disk);
        ped_disk_destroy (ped_disk);
        ped_device_destroy (dev);
        return FALSE;
    }

    status = ped_disk_delete_partition (ped_disk, ped_part);
    if (status == 0) {
        set_parted_error (error, BD_PART_ERROR_FAIL);
        g_prefix_error (error, "Failed to get partition '%d' on device '%s'", part_num, disk);
        ped_disk_destroy (ped_disk);
        ped_device_destroy (dev);
        return FALSE;
    }

    ret = disk_commit (ped_disk, disk, error);

    ped_disk_destroy (ped_disk);
    ped_device_destroy (dev);

    return ret;
}

/**
 * bd_part_set_part_flag:
 * @disk: disk the partition belongs to
 * @part: partition to set the flag on
 * @flag: flag to set
 * @state: state to set for the @flag (%TRUE = enabled)
 * @error: (out): place to store error (if any)
 *
 * Returns: whether the flag @flag was successfully set on the @part partition
 * or not.
 */
gboolean bd_part_set_part_flag (gchar *disk, gchar *part, BDPartPartFlag flag, gboolean state, GError **error) {
    PedDevice *dev = NULL;
    PedDisk *ped_disk = NULL;
    PedPartition *ped_part = NULL;
    gchar *part_num_str = NULL;
    gint part_num = 0;
    gint status = 0;
    gboolean ret = FALSE;

    /* TODO: share this code with the other functions modifying a partition */
    if (!part || (part && (*part == '\0'))) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_INVAL,
                     "Invalid partition path given: '%s'", part);
        return FALSE;
    }

    dev = ped_device_get (disk);
    if (!dev) {
        set_parted_error (error, BD_PART_ERROR_INVAL);
        g_prefix_error (error, "Device '%s' invalid or not existing", disk);
        return FALSE;
    }

    ped_disk = ped_disk_new (dev);
    if (!ped_disk) {
        set_parted_error (error, BD_PART_ERROR_FAIL);
        g_prefix_error (error, "Failed to read partition table on device '%s'", disk);
        ped_disk_destroy (ped_disk);
        ped_device_destroy (dev);
        return FALSE;
    }

    part_num_str = part + (strlen (part) - 1);
    while (isdigit (*part_num_str) || (*part_num_str == '-')) {
        part_num_str--;
    }
    part_num_str++;

    part_num = atoi (part_num_str);
    if (part_num == 0) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_INVAL,
                     "Invalid partition path given: '%s'. Cannot extract partition number", part);
        ped_disk_destroy (ped_disk);
        ped_device_destroy (dev);
        return FALSE;
    }

    ped_part = ped_disk_get_partition (ped_disk, part_num);
    if (!ped_part) {
        set_parted_error (error, BD_PART_ERROR_FAIL);
        g_prefix_error (error, "Failed to get partition '%d' on device '%s'", part_num, disk);
        ped_disk_destroy (ped_disk);
        ped_device_destroy (dev);
        return FALSE;
    }

    status = ped_partition_set_flag (ped_part, (PedPartitionFlag) flag, (int) state);
    if (status == 0) {
        set_parted_error (error, BD_PART_ERROR_FAIL);
        g_prefix_error (error, "Failed to get partition '%d' on device '%s'", part_num, disk);
        ped_disk_destroy (ped_disk);
        ped_device_destroy (dev);
        return FALSE;
    }

    ret = disk_commit (ped_disk, disk, error);

    ped_disk_destroy (ped_disk);
    ped_device_destroy (dev);

    return ret;
}
