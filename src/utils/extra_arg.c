#include <glib.h>
#include <glib-object.h>

#include "extra_arg.h"

/**
 * bd_util_extra_arg_copy:
 *
 * Creates a new copy of @arg.
 */
BDUtilExtraArg* bd_util_extra_arg_copy (BDUtilExtraArg *arg) {
    BDUtilExtraArg *ret = g_new0 (BDUtilExtraArg, 1);
    ret->opt = g_strdup (arg->opt);
    ret->val = g_strdup (arg->val);

    return ret;
}

/**
 * bd_util_extra_arg_free:
 *
 * Frees @arg.
 */
void bd_util_extra_arg_free (BDUtilExtraArg *arg) {
    g_free (arg->opt);
    g_free (arg->val);
    g_free (arg);
}

GType bd_util_extra_arg_get_type (void) {
    static GType type = 0;

    if (G_UNLIKELY (!type))
        type = g_boxed_type_register_static ("BDUtilExtraArg",
                                             (GBoxedCopyFunc) bd_util_extra_arg_copy,
                                             (GBoxedFreeFunc) bd_util_extra_arg_free);

    return type;
}

/**
 * bd_util_extra_arg_new: (constructor)
 * @opt: extra option
 * @val: value for the extra option @opt
 *
 * Returns: (transfer full): a new extra argument
 */
BDUtilExtraArg* bd_util_extra_arg_new (gchar *opt, gchar *val) {
    BDUtilExtraArg *ret = g_new0 (BDUtilExtraArg, 1);
    ret->opt = g_strdup (opt);
    ret->val = g_strdup (val);

    return ret;
}
