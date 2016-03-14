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
    BD_PART_FLAG_BOOT=1,
    BD_PART_FLAG_ROOT=2,
    BD_PART_FLAG_SWAP=3,
    BD_PART_FLAG_HIDDEN=4,
    BD_PART_FLAG_RAID=5,
    BD_PART_FLAG_LVM=6,
    BD_PART_FLAG_LBA=7,
    BD_PART_FLAG_HPSERVICE=8,
    BD_PART_FLAG_PALO=9,
    BD_PART_FLAG_PREP=10,
    BD_PART_FLAG_MSFT_RESERVED=11,
    BD_PART_FLAG_BIOS_GRUB=12,
    BD_PART_FLAG_APPLE_TV_RECOVERY=13,
    BD_PART_FLAG_DIAG=14,
    BD_PART_FLAG_LEGACY_BOOT=15,
    BD_PART_FLAG_MSFT_DATA=16,
    BD_PART_FLAG_IRST=17,
    BD_PART_FLAG_ESP=18
} BDPartFlag;

typedef enum {
    BD_PART_TYPE_NORMAL    = 0x00,
    BD_PART_TYPE_LOGICAL   = 0x01,
    BD_PART_TYPE_EXTENDED  = 0x02,
    BD_PART_TYPE_FREESPACE = 0x04,
    BD_PART_TYPE_METADATA  = 0x08,
    BD_PART_TYPE_PROTECTED = 0x10
} BDPartType;

typedef enum {
    BD_PART_TYPE_REQ_NORMAL   = 0x00,
    BD_PART_TYPE_REQ_LOGICAL  = 0x01,
    BD_PART_TYPE_REQ_EXTENDED = 0x02,
    BD_PART_TYPE_REQ_NEXT     = 0x04
} BDPartTypeReq;

typedef enum {
    BD_PART_ALIGN_MINIMAL,
    BD_PART_ALIGN_OPTIMAL,
    BD_PART_ALIGN_NONE
} BDPartAlign;

typedef struct BDPartSpec {
    gchar *path;
    BDPartType type;
    guint64 start;
    guint64 size;
} BDPartSpec;

BDPartSpec* bd_part_spec_copy (BDPartSpec *data);
void bd_part_spec_free (BDPartSpec *data);

gboolean bd_part_create_table (gchar *disk, BDPartTableType type, gboolean ignore_existing, GError **error);
BDPartSpec* bd_part_create_part (gchar *disk, BDPartTypeReq type, guint64 start, guint64 size, BDPartAlign align, GError **error);
gboolean bd_part_delete_part (gchar *disk, gchar *part, GError **error);
gboolean bd_part_set_part_flag (gchar *disk, gchar *part, BDPartFlag flag, gboolean state, GError **error);

#endif  /* BD_PART */
