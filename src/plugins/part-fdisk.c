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

/**
 * bd_part_create_part:
 * @disk: disk to create partition on
 * @type: type of the partition to create (if %BD_PART_TYPE_REQ_NEXT, the
 *        partition type will be determined automatically based on the existing
 *        partitions)
 * @start: where the partition should start (i.e. offset from the disk start)
 * @size: desired size of the partition (if 0, a max-sized partition is created)
 * @align: alignment to use for the partition
 * @error: (out): place to store error (if any)
 *
 * Returns: (transfer full): specification of the created partition or %NULL in case of error
 *
 * NOTE: The resulting partition may start at a different position than given by
 *       @start and can have different size than @size due to alignment.
 */
BDPartSpec* bd_part_create_part (const gchar *disk, BDPartTypeReq type, guint64 start, guint64 size, BDPartAlign align, GError **error) {
    struct fdisk_context *cxt = NULL;
    struct fdisk_partition *npa = NULL;
    gint status = 0;
    BDPartSpec *ret = NULL;
    guint64 progress_id = 0;
    gchar *msg = NULL;
    guint64 sector_size = 0;
    guint64 grain_size = 0;
    struct fdisk_parttype *ptype = NULL;
    struct fdisk_label *lbl = NULL;
    struct fdisk_table *table = NULL;
    struct fdisk_iter *iter = NULL;
    struct fdisk_partition *pa = NULL;
    struct fdisk_partition *epa = NULL;
    struct fdisk_partition *in_pa = NULL;
    struct fdisk_partition *n_epa = NULL;
    guint n_parts = 0;
    gboolean on_gpt = FALSE;

    msg = g_strdup_printf ("Started adding partition to '%s'", disk);
    progress_id = bd_utils_report_started (msg);
    g_free (msg);

    cxt = get_device_context (disk, error);
    if (!cxt) {
        /* error is already populated */
        bd_utils_report_finished (progress_id, (*error)->message);
        return NULL;
    }

    npa = fdisk_new_partition ();
    if (!npa) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                     "Failed to create new partition object");
        close_context (cxt);
        bd_utils_report_finished (progress_id, (*error)->message);
        return NULL;
    }

    sector_size = (guint64) fdisk_get_sector_size (cxt);

    grain_size = (guint64) fdisk_get_grain_size (cxt);
    if (align == BD_PART_ALIGN_NONE)
        grain_size = sector_size;
    else if (align == BD_PART_ALIGN_MINIMAL)
        grain_size = (guint64) fdisk_get_minimal_iosize (cxt);
    /* else OPTIMAL or unknown -> nothing to do */

    status = fdisk_save_user_grain (cxt, grain_size);
    if (status != 0) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                     "Failed to setup alignment");
        close_context (cxt);
        bd_utils_report_finished (progress_id, (*error)->message);
        return NULL;
    }

    /* this is needed so that the saved grain size from above becomes
     * effective */
    status = fdisk_reset_device_properties (cxt);
    if (status != 0) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                     "Failed to setup alignment");
        close_context (cxt);
        bd_utils_report_finished (progress_id, (*error)->message);
        return NULL;
    }

    grain_size = (guint64) fdisk_get_grain_size (cxt);

    /* align start up to sectors, will be aligned based on grain_size by
     * libfdisk */
    start = (start + sector_size - 1) / sector_size;

    if (size == 0)
        /* no size specified, set the end to default (maximum) */
        fdisk_partition_end_follow_default (npa, 1);
    else {
        /* align size down */
        size = (size / grain_size) * grain_size;
        size = size / sector_size;
        if (fdisk_partition_set_size (npa, size) != 0) {
            g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                         "Failed to set partition size");
            fdisk_unref_partition (npa);
            close_context (cxt);
            bd_utils_report_finished (progress_id, (*error)->message);
            return NULL;
        }
    }

    fdisk_partition_partno_follow_default (npa, 1);

    lbl = fdisk_get_label (cxt, NULL);
    on_gpt = g_strcmp0 (fdisk_label_get_name (lbl), "gpt") == 0;

    /* GPT is easy, all partitions are the same (NORMAL) */
    if (type == BD_PART_TYPE_REQ_NEXT && on_gpt)
        type = BD_PART_TYPE_REQ_NORMAL;

    /* on DOS we may have to decide if requested */
    if (type == BD_PART_TYPE_REQ_NEXT) {
        status = fdisk_get_partitions (cxt, &table);
        if (status != 0) {
            g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                         "Failed to get existing partitions on the device: %s", strerror_l (-status, c_locale));
            fdisk_unref_partition (npa);
            close_context (cxt);
            bd_utils_report_finished (progress_id, (*error)->message);
            return NULL;
        }
        iter = fdisk_new_iter (FDISK_ITER_FORWARD);
        while (fdisk_table_next_partition(table, iter, &pa) == 0) {
            if (fdisk_partition_is_freespace (pa))
                continue;

            if (!epa && fdisk_partition_is_container (pa))
                epa = pa;

            if (!in_pa && fdisk_partition_has_start (pa) && fdisk_partition_has_size (pa) &&
                fdisk_partition_get_start (pa) <= start &&
                (start < (fdisk_partition_get_start (pa) + fdisk_partition_get_size (pa))))
                in_pa = pa;
            n_parts++;
        }

        if (in_pa) {
            if (epa == in_pa)
                /* creating a parititon inside an extended partition -> LOGICAL */
                type = BD_PART_TYPE_LOGICAL;
            else {
                /* trying to create a partition inside an existing one, but not
                   an extended one -> error */
                g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_INVAL,
                             "Cannot create a partition inside an existing non-extended one");
                fdisk_unref_partition (npa);
                fdisk_free_iter (iter);
                fdisk_unref_table (table);
                close_context (cxt);
                bd_utils_report_finished (progress_id, (*error)->message);
                return NULL;
            }
        } else if (epa)
            /* there's an extended partition already and we are creating a new
               one outside of it */
            type = BD_PART_TYPE_NORMAL;
        else if (n_parts == 3) {
            /* already 3 primary partitions -> create an extended partition of
               the biggest possible size and a logical partition as requested in
               it */
            n_epa = fdisk_new_partition ();
            if (!n_epa) {
                g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                             "Failed to create new partition object");
                fdisk_unref_partition (npa);
                close_context (cxt);
                bd_utils_report_finished (progress_id, (*error)->message);
                return NULL;
            }
            if (fdisk_partition_set_start (n_epa, start) != 0) {
                g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                             "Failed to set partition start");
                fdisk_unref_partition (n_epa);
                fdisk_unref_partition (npa);
                fdisk_free_iter (iter);
                fdisk_unref_table (table);
                close_context (cxt);
                bd_utils_report_finished (progress_id, (*error)->message);
                return NULL;
            }

            fdisk_partition_partno_follow_default (n_epa, 1);

            /* set the end to default (maximum) */
            fdisk_partition_end_follow_default (n_epa, 1);

            /* "05" for extended partition */
            ptype = fdisk_label_parse_parttype (fdisk_get_label (cxt, NULL), "05");
            if (fdisk_partition_set_type (n_epa, ptype) != 0) {
                g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                             "Failed to set partition type");
                fdisk_unref_partition (n_epa);
                fdisk_unref_partition (npa);
                fdisk_free_iter (iter);
                fdisk_unref_table (table);
                close_context (cxt);
                bd_utils_report_finished (progress_id, (*error)->message);
                return NULL;
            }
            fdisk_unref_parttype (ptype);

            status = fdisk_add_partition (cxt, n_epa, NULL);
            fdisk_unref_partition (n_epa);
            if (status != 0) {
                g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                             "Failed to add new partition to the table: %s", strerror_l (-status, c_locale));
                fdisk_unref_partition (npa);
                fdisk_free_iter (iter);
                fdisk_unref_table (table);
                close_context (cxt);
                bd_utils_report_finished (progress_id, (*error)->message);
                return NULL;
            }
            /* shift the start 2 MiB further as that's where the first logical
               partition inside an extended partition can start */
            start += (2 MiB / sector_size);
            type = BD_PART_TYPE_LOGICAL;
        } else
            /* no extended partition and not 3 primary partitions -> just create
               another primary (NORMAL) partition*/
            type = BD_PART_TYPE_NORMAL;

        fdisk_free_iter (iter);
        fdisk_unref_table (table);
    }

    if (type == BD_PART_TYPE_REQ_EXTENDED) {
        /* "05" for extended partition */
        ptype = fdisk_label_parse_parttype(fdisk_get_label(cxt, NULL), "05");
        if (fdisk_partition_set_type (npa, ptype) != 0) {
            g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                         "Failed to set partition type");
            fdisk_unref_partition (npa);
            close_context (cxt);
            bd_utils_report_finished (progress_id, (*error)->message);
            return NULL;
        }
        fdisk_unref_parttype (ptype);
    }

    if (fdisk_partition_set_start (npa, start) != 0) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                     "Failed to set partition start");
        fdisk_unref_partition (npa);
        close_context (cxt);
        bd_utils_report_finished (progress_id, (*error)->message);
        return NULL;
    }

    status = fdisk_add_partition (cxt, npa, NULL);
    fdisk_unref_partition (npa);
    if (status != 0) {
        g_set_error (error, BD_PART_ERROR, BD_PART_ERROR_FAIL,
                     "Failed to add new partition to the table: %s", strerror_l (-status, c_locale));
        close_context (cxt);
        bd_utils_report_finished (progress_id, (*error)->message);
        return NULL;
    }

    if (!write_label (cxt, disk, error)) {
        bd_utils_report_finished (progress_id, (*error)->message);
        close_context (cxt);
        return NULL;
    }

    close_context (cxt);
    bd_utils_report_finished (progress_id, "Completed");
    return ret;
}
