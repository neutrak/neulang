
#ifndef _NL_DECLARATIONS
#define _NL_DECLARATIONS

//BEGIN NL DECLARATIONS -------------------------------------------------------------------------------------------

void nl_out(FILE *fp, nl_val *exp);
nl_val *nl_read_exp(FILE *fp);
void nl_array_push(nl_val *a, nl_val *v);
nl_val *nl_eval(nl_val *exp, nl_env_frame *env);
int nl_val_cmp(const nl_val *v_a, const nl_val *v_b);
nl_env_frame *nl_env_frame_malloc(nl_env_frame *up_scope);
void nl_env_frame_free(nl_env_frame *env);

//END NL DECLARATIONS ---------------------------------------------------------------------------------------------

#endif

