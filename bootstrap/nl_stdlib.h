#ifndef _NL_STDLIB
#define _NL_STDLIB

#include "nl_structures.h"

//gcd-reduce a rational number
void nl_gcd_reduce(nl_val *v);

//compare two neulang values; returns -1 if a<b, 0 if a==b, and 1 if a>b
//this is value comparison, NOT pointer comparison
int nl_val_cmp(const nl_val *v_a, const nl_val *v_b);

//push a value onto the end of an array
void nl_array_push(nl_val *a, nl_val *v);

//pop a value off of the end of an array, resizing if needed
void nl_array_pop(nl_val *a);

//insert a value into an array, resizing if needed
void nl_array_ins(nl_val *a, nl_val *v, nl_val *index);

//remove a value from an array, resizing if needed
void nl_array_rm(nl_val *a, nl_val *index);


#endif

