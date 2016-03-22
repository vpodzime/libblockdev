#include <glib.h>

#ifndef BD_FS
#define BD_FS

GQuark bd_fs_error_quark (void);
#define BD_FS_ERROR bd_fs_error_quark ()
typedef enum {
    BD_FS_ERROR_INVAL,
    BD_FS_ERROR_FAIL,
} BDFsError;

gboolean bd_fs_wipe (gchar *device, gboolean all, GError **error);
gboolean bd_fs_ext4_mkfs (gchar *device, GError **error);
gboolean bd_fs_ext4_wipe (gchar *device, GError **error);
gboolean bd_fs_ext4_check (gchar *device, GError **error);
gboolean bd_fs_ext4_repair (gchar *device, gboolean unsafe, GError **error);
gboolean bd_fs_ext4_set_label (gchar *device, gchar *label, GError **error);

#endif  /* BD_PART */

