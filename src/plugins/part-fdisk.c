/*
 * Copyright (C) 2017  Red Hat, Inc.
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

#include <libfdisk.h>
#include <blockdev/utils.h>

#include "part.h"

/**
 * bd_part_error_quark: (skip)
 */
GQuark bd_part_error_quark (void)
{
    return g_quark_from_static_string ("g-bd-part-error-quark");
}

BDPartSpec* bd_part_spec_copy (BDPartSpec *data) {
    BDPartSpec *ret = g_new0 (BDPartSpec, 1);

    ret->path = g_strdup (data->path);
    ret->name = g_strdup (data->name);
    ret->type_guid = g_strdup (data->type_guid);
    ret->type = data->type;
    ret->start = data->start;
    ret->size = data->size;

    return ret;
}

void bd_part_spec_free (BDPartSpec *data) {
    g_free (data->path);
    g_free (data->name);
    g_free (data->type_guid);
    g_free (data);
}

BDPartDiskSpec* bd_part_disk_spec_copy (BDPartDiskSpec *data) {
    BDPartDiskSpec *ret = g_new0 (BDPartDiskSpec, 1);

    ret->path = g_strdup (data->path);
    ret->table_type = data->table_type;
    ret->size = data->size;
    ret->sector_size = data->sector_size;
    ret->flags = data->flags;

    return ret;
}

void bd_part_disk_spec_free (BDPartDiskSpec *data) {
    g_free (data->path);
    g_free (data);
}

/**
 * bd_part_check_deps:
 *
 * Returns: whether the plugin's runtime dependencies are satisfied or not
 *
 * Function checking plugin's runtime dependencies.
 *
 */
gboolean bd_part_check_deps () {
    return TRUE;
}

/* "C" locale to get the locale-agnostic error messages */
static locale_t c_locale = (locale_t) 0;

/**
 * bd_part_init:
 *
 * Initializes the plugin. **This function is called automatically by the
 * library's initialization functions.**
 *
 */
gboolean bd_part_init () {
    c_locale = newlocale (LC_ALL_MASK, "C", c_locale);
    fdisk_init_debug(0);
    return TRUE;
}

/**
 * bd_part_close:
 *
 * Cleans up after the plugin. **This function is called automatically by the
 * library's functions that unload it.**
 *
 */
void bd_part_close () {
    c_locale = (locale_t) 0;
}

static const gchar *table_type_str[BD_PART_TABLE_UNDEF] = {"dos", "gpt"};

static struct fdisk_context* get_device_context (const gchar *disk, GError **error) {
    struct fdisk_context *cxt = fdisk_new_context ();
    gint ret = 0;

    if (!cxt) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                     "Failed to create a new context");
        return NULL;
    }

    ret = fdisk_assign_device (cxt, disk, FALSE);
    if (ret != 0) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                     "Failed to assign the new context to disk '%s': %s", disk, strerror_l (-ret, c_locale));
        fdisk_unref_context (cxt);
        return NULL;
    }

    fdisk_disable_dialogs(cxt, 1);
    return cxt;
}

static void close_context (struct fdisk_context *cxt) {
    gint ret = 0;

    ret = fdisk_deassign_device (cxt, 0); /* context, nosync */
    if (ret != 0)
        /* XXX: should report error here? */
        g_warning ("Failed to close and sync the device: %s", strerror_l (-ret, c_locale));
    fdisk_unref_context (cxt);
}

static gboolean write_label (struct fdisk_context *cxt, const gchar *disk, GError **error) {
    gint ret = 0;
    ret = fdisk_write_disklabel (cxt);
    if (ret != 0) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                     "Failed to write the new disklabel to disk '%s': %s", disk, strerror_l (-ret, c_locale));
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
gboolean bd_part_create_table (const gchar *disk, BDPartTableType type, gboolean ignore_existing, GError **error) {
    struct fdisk_context *cxt = NULL;
    gint ret = 0;
    guint64 progress_id = 0;
    gchar *msg = NULL;

    msg = g_strdup_printf ("Starting creation of a new partition table on '%s'", disk);
    progress_id = bd_utils_report_started (msg);
    g_free (msg);

    cxt = get_device_context (disk, error);
    if (!cxt) {
        /* error is already populated */
        bd_utils_report_finished (progress_id, (*error)->message);
        return FALSE;
    }

    if (!ignore_existing && fdisk_has_label (cxt)) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_EXISTS,
                     "Device '%s' already contains a partition table", disk);
        bd_utils_report_finished (progress_id, (*error)->message);
        close_context (cxt);
        return FALSE;
    }

    ret = fdisk_create_disklabel (cxt, table_type_str[type]);
    if (ret != 0) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                     "Failed to create a new disklabel for disk '%s': %s", disk, strerror_l (-ret, c_locale));
        bd_utils_report_finished (progress_id, (*error)->message);
        close_context (cxt);
        return FALSE;
    }

    if (!write_label (cxt, disk, error)) {
        bd_utils_report_finished (progress_id, (*error)->message);
        close_context (cxt);
        return FALSE;
    }

    close_context (cxt);
    bd_utils_report_finished (progress_id, "Completed");
    return TRUE;
}

/**
 * bd_part_delete_part:
 * @disk: disk to remove the partition from
 * @part: partition to remove
 * @error: (out): place to store error (if any)
 *
 * Returns: whether the @part partition was successfully deleted from @disk
 */
gboolean bd_part_delete_part (const gchar *disk, const gchar *part, GError **error) {
    const gchar *part_num_str = NULL;
    gint part_num = 0;
    struct fdisk_context *cxt = NULL;
    gint ret = 0;
    guint64 progress_id = 0;
    gchar *msg = NULL;

    msg = g_strdup_printf ("Started deleting partition '%s'", part);
    progress_id = bd_utils_report_started (msg);
    g_free (msg);

    if (!part || (part && (*part == '\0'))) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_INVAL,
                     "Invalid partition path given: '%s'", part);
        bd_utils_report_finished (progress_id, (*error)->message);
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
        bd_utils_report_finished (progress_id, (*error)->message);
        return FALSE;
    }

    /* /dev/sda1 is the partition number 0 in libfdisk */
    part_num--;

    cxt = get_device_context (disk, error);
    if (!cxt) {
        /* error is already populated */
        bd_utils_report_finished (progress_id, (*error)->message);
        return FALSE;
    }

    ret = fdisk_delete_partition (cxt, (size_t) part_num);
    if (ret != 0) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                     "Failed to delete partition '%d' on device '%s': %s", part_num+1, disk, strerror_l (-ret, c_locale));
        close_context (cxt);
        bd_utils_report_finished (progress_id, (*error)->message);
        return FALSE;
    }

    if (!write_label (cxt, disk, error)) {
        bd_utils_report_finished (progress_id, (*error)->message);
        close_context (cxt);
        return FALSE;
    }

    close_context (cxt);
    bd_utils_report_finished (progress_id, "Completed");
    return TRUE;
}
