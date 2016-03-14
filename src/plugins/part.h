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

typedef enum {
    BD_PART_PART_BOOT=1,
    BD_PART_PART_ROOT=2,
    BD_PART_PART_SWAP=3,
    BD_PART_PART_HIDDEN=4,
    BD_PART_PART_RAID=5,
    BD_PART_PART_LVM=6,
    BD_PART_PART_LBA=7,
    BD_PART_PART_HPSERVICE=8,
    BD_PART_PART_PALO=9,
    BD_PART_PART_PREP=10,
    BD_PART_PART_MSFT_RESERVED=11,
    BD_PART_PART_BIOS_GRUB=12,
    BD_PART_PART_APPLE_TV_RECOVERY=13,
    BD_PART_PART_DIAG=14,
    BD_PART_PART_LEGACY_BOOT=15,
    BD_PART_PART_MSFT_DATA=16,
    BD_PART_PART_IRST=17,
    BD_PART_PART_ESP=18
} BDPartPartFlag;

gboolean bd_part_create_table (gchar *disk, BDPartTableType type, gboolean ignore_existing, GError **error);
gboolean bd_part_delete_part (gchar *disk, gchar *part, GError **error);
gboolean bd_part_set_part_flag (gchar *disk, gchar *part, BDPartPartFlag flag, gboolean state, GError **error);

#endif  /* BD_PART */
