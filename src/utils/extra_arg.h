#include <glib.h>
#include <glib-object.h>

#ifndef BD_UTILS_EXTRA_ARG
#define BD_UTILS_EXTRA_ARG

#define BD_UTIL_TYPE_EXTRA_ARG (bd_util_extra_arg_get_type ())
GType bd_util_extra_arg_get_type ();

typedef struct BDUtilExtraArg {
    gchar *opt;
    gchar *val;
} BDUtilExtraArg;

BDUtilExtraArg* bd_util_extra_arg_copy (BDUtilExtraArg *arg);
void bd_util_extra_arg_free (BDUtilExtraArg *arg);
BDUtilExtraArg* bd_util_extra_arg_new (gchar *opt, gchar *val);

#endif  /* BD_UTILS_EXTRA_ARG */
