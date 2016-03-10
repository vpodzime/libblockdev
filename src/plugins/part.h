#include <glib.h>

#ifndef BD_PART
#define BD_PART

GQuark bd_part_error_quark (void);
#define BD_PART_ERROR bd_part_error_quark ()
typedef enum {
    BD_PART_ERROR_EXISTS,
    BD_PART_ERROR_INVAL,
    BD_PART_ERROR_FAIL,
} BDPartError;

typedef enum {
    BD_PART_TABLE_MSDOS = 0,
    BD_PART_TABLE_GPT,
    BD_PART_TABLE_UNDEF
} BDPartTableType;

gboolean bd_part_create_table (gchar *disk, BDPartTableType type, gboolean ignore_existing, GError **error);

#endif  /* BD_PART */
