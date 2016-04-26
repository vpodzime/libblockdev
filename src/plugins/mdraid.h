#include <glib.h>

#ifndef BD_MD
#define BD_MD

#define MDADM_MIN_VERSION "3.3.2"

/* taken from blivet */
// these defaults were determined empirically
#define BD_MD_SUPERBLOCK_SIZE (2 MiB)
#define BD_MD_CHUNK_SIZE (512 KiB)

GQuark bd_md_error_quark (void);
#define BD_MD_ERROR bd_md_error_quark ()
typedef enum {
    BD_MD_ERROR_PARSE,
    BD_MD_ERROR_BAD_FORMAT,
    BD_MD_ERROR_NO_MATCH,
} BDMDError;

typedef struct BDMDExamineData {
    gchar *device;
    gchar *level;
    guint64 num_devices;
    gchar *name;
    guint64 size;
    gchar *uuid;
    guint64 update_time;
    gchar *dev_uuid;
    guint64 events;
    gchar *metadata;
    guint64 chunk_size;
} BDMDExamineData;

BDMDExamineData* bd_md_examine_data_copy (BDMDExamineData *data);
void bd_md_examine_data_free (BDMDExamineData *data);

typedef struct BDMDDetailData {
    gchar *device;
    gchar *metadata;
    gchar *creation_time;
    gchar *level;
    gchar *name;
    guint64 array_size;
    guint64 use_dev_size;
    guint64 raid_devices;
    guint64 total_devices;
    guint64 active_devices;
    guint64 working_devices;
    guint64 failed_devices;
    guint64 spare_devices;
    gboolean clean;
    gchar *uuid;
} BDMDDetailData;

void bd_md_detail_data_free (BDMDDetailData *data);
BDMDDetailData* bd_md_detail_data_copy (BDMDDetailData *data);

guint64 bd_md_get_superblock_size (guint64 member_size, const gchar *version, GError **error);
gboolean bd_md_create (const gchar *device_name, const gchar *level, const gchar **disks, guint64 spares, const gchar *version, gboolean bitmap, BDExtraArg **extra, GError **error);
gboolean bd_md_create_with_chunk_size (const gchar *device_name, const gchar *level, const gchar **disks, guint64 spares, const gchar *version, gboolean bitmap, guint64 chunk_size, BDExtraArg **extra, GError **error);
gboolean bd_md_destroy (const gchar *device, GError **error);
gboolean bd_md_deactivate (const gchar *device_name, GError **error);
gboolean bd_md_activate (const gchar *device_name, const gchar **members, const gchar *uuid, BDExtraArg **extra, GError **error);
gboolean bd_md_run (const gchar *raid_name, GError **error);
gboolean bd_md_nominate (const gchar *device, GError **error);
gboolean bd_md_denominate (const gchar *device, GError **error);
gboolean bd_md_add (const gchar *raid_name, const gchar *device, guint64 raid_devs, BDExtraArg **extra, GError **error);
gboolean bd_md_remove (const gchar *raid_name, const gchar *device, gboolean fail, BDExtraArg **extra, GError **error);
BDMDExamineData* bd_md_examine (const gchar *device, GError **error);
BDMDDetailData* bd_md_detail (const gchar *raid_name, GError **error);
gchar* bd_md_canonicalize_uuid (const gchar *uuid, GError **error);
gchar* bd_md_get_md_uuid (const gchar *uuid, GError **error);
gchar* bd_md_node_from_name (const gchar *name, GError **error);
gchar* bd_md_name_from_node (const gchar *node, GError **error);

#endif  /* BD_MD */
