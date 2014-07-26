#include <stdio.h>
#include <stdlib.h>

#include "nl_structures.h"

//BEGIN C-NL-STDLIB SUBROUTINES  ----------------------------------------------------------------------------------

//gcd-reduce a rational number
void nl_gcd_reduce(nl_val *v){
	//ignore null and non-rational values
	if((v==NULL) || (v->t!=NUM)){
		return;
	}
	
	//if both numerator AND denominator are negative, make them positive for ease
	if(((v->d.num.n)<0) && ((v->d.num.d)<0)){
		(v->d.num.n)/=-1;
		(v->d.num.d)/=-1;
	}
	
	//find greatest common divisor of numerator and denominator (negatives are ignored for this)
	long long int a=abs(v->d.num.n);
	long long int b=abs(v->d.num.d);
	while(b!=0){
		long long int temp=b;
		
		//try to divide, if there is a remainder then try some more
		b=a%b;
		
		a=temp;
	}
	
	//a is now the greatest common divisor between numerator and denominator, so divide both by that
	(v->d.num.n)/=a;
	(v->d.num.d)/=a;
	
	//and now it's reduced! isn't that great?
}

//compare two neulang values; returns -1 if a<b, 0 if a==b, and 1 if a>b
//this is value comparison, NOT pointer comparison
int nl_val_cmp(const nl_val *v_a, const nl_val *v_b){
	//first handle nulls; anything is larger than null
	if((v_a==NULL) && (v_b==NULL)){
		return 0;
	}
	if(v_a==NULL){
		return -1;
	}
	if(v_b==NULL){
		return 1;
	}
	
	//check type equality
	if((v_a->t)!=(v_b->t)){
		fprintf(stderr,"Err: comparison between different types is nonsensical, assuming a<b...");
		fprintf(stderr,"(offending values were a=");
		nl_out(stderr,v_a);
		fprintf(stderr," and b=");
		nl_out(stderr,v_b);
		fprintf(stderr,")\n");
		return -1;
	}
	
	//okay, now the fun stuff
	switch(v_a->t){
		//bytes are equal if their values are equal
		case BYTE:
			if((v_a->d.byte.v)<(v_b->d.byte.v)){
				return -1;
			}else if((v_a->d.byte.v)==(v_b->d.byte.v)){
				return 0;
			}else{
				return 1;
			}
			break;
		//rational numbers are equal if they are equal with a common divisor
		case NUM:
			{
				//first get a common denominator
//				long long int common_denominator=(v_a->d.num.d)*(v_b->d.num.d);
				long long int v_a_n=(v_a->d.num.n)*(v_b->d.num.d);
				long long int v_b_n=(v_b->d.num.n)*(v_a->d.num.d);
				
				//now compare numerators with that new divisor
				if(v_a_n<v_b_n){
					return -1;
				}else if(v_a_n==v_b_n){
					return 0;
				}else{
					return 1;
				}
			}
			break;
		//lists are equal if each element is equal and they are of equal length
		//recursively check through the list
		case PAIR:
			{
				//check the first item
				int f_cmp=nl_val_cmp(v_a->d.pair.f,v_b->d.pair.f);
				//if it's not equal stop here, we've found a result
				if(f_cmp!=0){
					return f_cmp;
				//otherwise keep going (remember the end of the list is nulls, which are considered equal (since they're both null))
				//if one list is shorter than the other than the longer list will be considered bigger, providing elements up to that point are the same
				}else{
					return nl_val_cmp(v_a->d.pair.r,v_b->d.pair.r);
				}
			}
			break;
		//arrays are equal if each element is equal and they are of equal length
		//(we recursively check through the array)
		case ARRAY:
			{
				//look through the array
				int n;
				for(n=0;(n<v_a->d.array.size) && (n<v_b->d.array.size);n++){
					int element_cmp=nl_val_cmp(v_a->d.array.v[n],v_b->d.array.v[n]);
					
					//if we find an unequal element stop and return the comparison result for that element
					if(element_cmp!=0){
						return element_cmp;
					}
				}
				
				//if all elements were equal until we hit the end of an array, then base equality on array length
				if((v_a->d.array.size)<(v_b->d.array.size)){
					return -1;
				}else if((v_a->d.array.size)==(v_b->d.array.size)){
					return 0;
				}else{
					return 1;
				}
			}
			break;
		//primitive procedures are only equal if they are pointer-equal
		//unequal here is assumed to be -1; no actual pointer-lower is checked since that varies by compiler
		case PRI:
			if((v_a->d.pri.function)==(v_b->d.pri.function)){
				return 0;
			}else{
				return 1;
			}
			break;
		//subroutines are equal iff they are pointer-equal
		case SUB:
			//remember 0 means equal here, just like C's strcmp
			if(v_a==v_b){
				return 0;
			}else{
				return 1;
			}
			break;
		//symbols are equal if their names (byte arrays) are equal
		case SYMBOL:
			return nl_val_cmp(v_a->d.sym.name,v_b->d.sym.name);
			break;
		case EVALUATION:
			return nl_val_cmp(v_a->d.eval.sym,v_b->d.eval.sym);
			break;
		default:
			break;
	}
	
	//if we got here and didn't return assume equality
	return 0;
}

//return whether or not this list contains null (this does NOT recurse to sub-lists)
char nl_contains_nulls(nl_val *val_list){
	while((val_list!=NULL) && (val_list->t==PAIR)){
		if(val_list->d.pair.f==NULL){
			return TRUE;
		}
		val_list=val_list->d.pair.r;
	}
	return FALSE;
}

//return a byte version of the given number constant, if possible
nl_val *nl_int_to_byte(nl_val *num_list){
	nl_val *ret=NULL;
	
	int arg_count=nl_list_len(num_list);
	if(arg_count>=1){
		if(num_list->d.pair.f->d.num.d==1){
			if(num_list->d.pair.f->d.num.n<256){
				ret=nl_val_malloc(BYTE);
				ret->d.byte.v=num_list->d.pair.f->d.num.n;
			}else{
				fprintf(stderr,"Err: overflow in int_to_byte (given int can't fit in a byte), returning NULL\n");
			}
		}else{
			fprintf(stderr,"Err: non-int value given to int_to_byte; type conversion nonsensical, returning NULL\n");
		}
		
		if(arg_count>1){
			fprintf(stderr,"Warn: too many arguments given to int_to_byte, ignoring all but the first...\n");
		}
	}else{
		fprintf(stderr,"Err: no arguments given to int_to_byte, can't convert NULL! (returning NULL)\n");
	}
	
	return ret;
}

//BEGIN C-NL-STDLIB-ARRAY SUBROUTINES  ----------------------------------------------------------------------------

//TODO: write all array library functions
//TODO: write the whole standard library

//push a value onto the end of an array
void nl_array_push(nl_val *a, nl_val *v){
	//this operation is undefined on null and non-array values
	if((a==NULL) || (a->t!=ARRAY)){
		return;
	}
	
	unsigned int new_stored_size=a->d.array.stored_size;
	unsigned int new_size=(a->d.array.size)+1;
	
	//if we need to allocate more space do that now, and copy the old data into the new location (freeing the old one)
	if(new_size>=new_stored_size){
		//a brand new array starts at size 1
		if(new_stored_size==0){
			new_stored_size=1;
		//when we run out of space, double the available space (don't want to be doing this all the time)
		}else{
			new_stored_size*=2;
		}
		nl_val **new_array_v=(nl_val**)(malloc((new_stored_size)*(sizeof(nl_val*))));
//		nl_val **new_array_v=(nl_val**)(malloc((new_stored_size)*(sizeof(nl_val))));
		
		//copy in the old data
		int n;
		for(n=0;n<(new_size-1);n++){
			new_array_v[n]=(a->d.array.v[n]);
//			memcpy(new_array_v[n],a->d.array.v[n],sizeof(nl_val));
		}
		
		//free the old memory, and make the array reference the new memory
		if(a->d.array.v!=NULL){
			free(a->d.array.v);
		}
		a->d.array.v=new_array_v;
		
		//update size parameters
		a->d.array.stored_size=new_stored_size;
	}
	
	//update size
	a->d.array.size=new_size;
	
	//copy in the new data
	a->d.array.v[(new_size-1)]=v;
}

//pop a value off of the end of an array, resizing if needed
void nl_array_pop(nl_val *a){
	
}

//insert a value into an array, resizing if needed
void nl_array_ins(nl_val *a, nl_val *v, nl_val *index){
	
}

//remove a value from an array, resizing if needed
void nl_array_rm(nl_val *a, nl_val *index){
	
}

//return the size of the first argument
//NOTE: subsequent arguments are IGNORED
nl_val *nl_array_size(nl_val *array_list){
	nl_val *acc=NULL;
	if((array_list!=NULL) && (array_list->t==PAIR)){
		acc=nl_val_malloc(NUM);
		acc->d.num.d=1;
		acc->d.num.n=array_list->d.pair.f->d.array.size;
		
		if(array_list->d.pair.r!=NULL){
			fprintf(stderr,"Warn: too many arguments given to array size operation, only the first will be used...\n");
		}
	}else{
		fprintf(stderr,"Err: wrong syntax for array size operation (did you give us a NULL?)\n");
	}
	return acc;
}

//concatenate all the given arrays (a list) into one new larger array
nl_val *nl_array_cat(nl_val *array_list){
	nl_val *acc=nl_val_malloc(ARRAY);
	
	while((array_list!=NULL) && (array_list->d.pair.f->t==ARRAY)){
		
		//we got an array, so add it to the accumulator
		nl_val *current_array=array_list->d.pair.f;
		
		//push copies of each element into the larger accumulator
		int n;
		for(n=0;n<current_array->d.array.size;n++){
			nl_array_push(acc,nl_val_cp(current_array->d.array.v[n]));
		}
		
		array_list=array_list->d.pair.r;
	}
	
	if(array_list!=NULL){
		fprintf(stderr,"Err: got a non-array value in array concatenation operation; value was ");
		nl_out(stderr,array_list->d.pair.f);
		fprintf(stderr,"\n");
	}
	
	return acc;
}

//output the given list of strings in sequence
//returns NULL (a void function)
nl_val *nl_strout(nl_val *array_list){
	while((array_list!=NULL) && (array_list->t==PAIR))
	{
		nl_val *output_str=array_list->d.pair.f;
		
		int n;
		for(n=0;n<output_str->d.array.size;n++){
			nl_out(stdout,output_str->d.array.v[n]);
		}
		
		array_list=array_list->d.pair.r;
	}
	
	return NULL;
}

//outputs the given value to stdout
//returns NULL (a void function)
nl_val *nl_output(nl_val *v_list){
	while(v_list!=NULL && v_list->t==PAIR){
		nl_out(stdout,v_list->d.pair.f);
		v_list=v_list->d.pair.r;
	}
	return NULL;
}

//END C-NL-STDLIB-ARRAY SUBROUTINES  ------------------------------------------------------------------------------

//BEGIN C-NL-STDLIB-LIST SUBROUTINES  -----------------------------------------------------------------------------

//returns the length of a singly-linked list
//note that cyclic lists are infinite and this will never terminate on them
int nl_list_len(nl_val *list){
	int len=0;
	while((list!=NULL) && (list->t==PAIR)){
		len++;
		list=list->d.pair.r;
	}
	return len;
}

//returns the list element at the given index (we use 0-indexing)
nl_val *nl_list_idx(nl_val *list, nl_val *idx){
	nl_val *ret=NULL;
	
	if((idx->t!=NUM) || (idx->d.num.d!=1)){
		fprintf(stderr,"Err: nl_list_idx only accepts integer list indices, given index was ");
		nl_out(stderr,idx);
		fprintf(stderr,"\n");
		return NULL;
	}
	
	int current_idx=0;
	while((list!=NULL) && (list->t==PAIR)){
		if(current_idx==(idx->d.num.n)){
			ret=list->d.pair.f;
			break;
		}
		
		list=list->d.pair.r;
	}
	return ret;
}

//END C-NL-STDLIB-LIST SUBROUTINES  -------------------------------------------------------------------------------

//BEGIN C-NL-STDLIB-MATH SUBROUTINES  -----------------------------------------------------------------------------

//add a list of (rational) numbers
nl_val *nl_add(nl_val *num_list){
	nl_val *acc=NULL;
	if((num_list!=NULL) && (num_list->t==PAIR) && (num_list->d.pair.f->t==NUM)){
		acc=nl_val_cp(num_list->d.pair.f);
		num_list=num_list->d.pair.r;
	}else{
		fprintf(stderr,"Err: incorrect use of add operation (null list or incorrect type in first operand)\n");
		return NULL;
	}
	
	while((num_list!=NULL) && (num_list->t==PAIR)){
		//TODO: should null elements make the whole result null? (acting as NaN)
		//ignore null elements
		if(num_list->d.pair.f==NULL){
			continue;
		}
		
		//error on non-numbers
		if(num_list->d.pair.f->t!=NUM){
			fprintf(stderr,"Err: non-number given to add operation, returning NULL from add\n");
			nl_val_free(acc);
			return NULL;
		}
		nl_val *current_num=num_list->d.pair.f;
		
		//okay, now add this number to the accumulator
		long long int numerator=((acc->d.num.n)*(current_num->d.num.d))+((current_num->d.num.n)*(acc->d.num.d));
		long long int denominator=(acc->d.num.d)*(current_num->d.num.d);
		
		acc->d.num.n=numerator;
		acc->d.num.d=denominator;
		
		//and reduce to make later operations simpler
		nl_gcd_reduce(acc);
		
		num_list=num_list->d.pair.r;
	}
	//if we got here and didn't return, then we have a success and the accumulator stored the result!
	return acc;
}

//subtract a list of (rational) numbers
nl_val *nl_sub(nl_val *num_list){
	nl_val *acc=NULL;
	if((num_list!=NULL) && (num_list->t==PAIR) && (num_list->d.pair.f->t==NUM)){
		acc=nl_val_cp(num_list->d.pair.f);
		num_list=num_list->d.pair.r;
	}else{
		fprintf(stderr,"Err: incorrect use of sub operation (null list or incorrect type in first operand)\n");
		return NULL;
	}
	
	while((num_list!=NULL) && (num_list->t==PAIR)){
		//TODO: should null elements make the whole result null? (acting as NaN)
		//ignore null elements
		if(num_list->d.pair.f==NULL){
			continue;
		}
		
		//error on non-numbers
		if(num_list->d.pair.f->t!=NUM){
			fprintf(stderr,"Err: non-number given to subtract operation, returning NULL from subtract\n");
			nl_val_free(acc);
			return NULL;
		}
		nl_val *current_num=num_list->d.pair.f;
		
		//okay, now add this number to the accumulator
		long long int numerator=((acc->d.num.n)*(current_num->d.num.d))-((current_num->d.num.n)*(acc->d.num.d));
		long long int denominator=(acc->d.num.d)*(current_num->d.num.d);
		
		acc->d.num.n=numerator;
		acc->d.num.d=denominator;
		
		//and reduce to make later operations simpler
		nl_gcd_reduce(acc);
		
		num_list=num_list->d.pair.r;
	}
	//if we got here and didn't return, then we have a success and the accumulator stored the result!
	return acc;
}

//multiply a list of (rational) numbers
nl_val *nl_mul(nl_val *num_list){
	nl_val *acc=NULL;
	if((num_list!=NULL) && (num_list->t==PAIR) && (num_list->d.pair.f->t==NUM)){
		acc=nl_val_cp(num_list->d.pair.f);
		num_list=num_list->d.pair.r;
	}else{
		fprintf(stderr,"Err: incorrect use of mul operation (null list or incorrect type in first operand)\n");
		return NULL;
	}
	
	while((num_list!=NULL) && (num_list->t==PAIR)){
		//TODO: should null elements make the whole result null? (acting as NaN)
		//ignore null elements
		if(num_list->d.pair.f==NULL){
			continue;
		}
		
		//error on non-numbers
		if(num_list->d.pair.f->t!=NUM){
			fprintf(stderr,"Err: non-number given to mul operation, returning NULL from mul\n");
			nl_val_free(acc);
			return NULL;
		}
		nl_val *current_num=num_list->d.pair.f;
		
		//okay, now add this number to the accumulator
		long long int numerator=((acc->d.num.n)*(current_num->d.num.n));
		long long int denominator=(acc->d.num.d)*(current_num->d.num.d);
		
		acc->d.num.n=numerator;
		acc->d.num.d=denominator;
		
		//and reduce to make later operations simpler
		nl_gcd_reduce(acc);
		
		num_list=num_list->d.pair.r;
	}
	//if we got here and didn't return, then we have a success and the accumulator stored the result!
	return acc;
}

//divide a list of (rational) numbers
nl_val *nl_div(nl_val *num_list){
	nl_val *acc=NULL;
	if((num_list!=NULL) && (num_list->t==PAIR) && (num_list->d.pair.f->t==NUM)){
		acc=nl_val_cp(num_list->d.pair.f);
		num_list=num_list->d.pair.r;
	}else{
		fprintf(stderr,"Err: incorrect use of mul operation (null list or incorrect type in first operand)\n");
		return NULL;
	}
	
	while((num_list!=NULL) && (num_list->t==PAIR)){
		//TODO: should null elements make the whole result null? (acting as NaN)
		//ignore null elements
		if(num_list->d.pair.f==NULL){
			continue;
		}
		
		//error on non-numbers
		if(num_list->d.pair.f->t!=NUM){
			fprintf(stderr,"Err: non-number given to mul operation, returning NULL from mul\n");
			nl_val_free(acc);
			return NULL;
		}
		nl_val *current_num=num_list->d.pair.f;
		
		//okay, now add this number to the accumulator
		long long int numerator=((acc->d.num.n)*(current_num->d.num.d));
		long long int denominator=(acc->d.num.d)*(current_num->d.num.n);
		
		acc->d.num.n=numerator;
		acc->d.num.d=denominator;
		
		//and reduce to make later operations simpler
		nl_gcd_reduce(acc);
		
		num_list=num_list->d.pair.r;
	}
	//if we got here and didn't return, then we have a success and the accumulator stored the result!
	return acc;
}

//TODO: write the rest of the math library

//END C-NL-STDLIB-MATH SUBROUTINES  -------------------------------------------------------------------------------


//BEGIN C-NL-STDLIB-CMP SUBROUTINES  ------------------------------------------------------------------------------

//numeric equality operator =
//TWO ARGUMENTS ONLY!
nl_val *nl_eq(nl_val *val_list){
	nl_val *ret=NULL;
	
	//garbage in, garbage out; if you give us NULL we return NULL
	if(nl_contains_nulls(val_list)){
		fprintf(stderr,"Err: a NULL argument was given to equality operator, returning NULL\n");
		return NULL;
	}
	
	if(!((nl_list_len(val_list)==2) && (val_list->d.pair.f->t==NUM) && (val_list->d.pair.r->d.pair.f->t==NUM))){
		fprintf(stderr,"Err: incorrect use of equality operator =; arg count != 2 or incorrect type (for string equality use ar=)\n");
		return NULL;
	}
	
	ret=nl_val_malloc(BYTE);
	ret->d.byte.v=FALSE;
	
	if(nl_val_cmp(val_list->d.pair.f,val_list->d.pair.r->d.pair.f)==0){
		ret->d.byte.v=TRUE;
	}
	return ret;
}

//numeric gt operator >
//if more than two arguments are given then this will only return true if a>b>c>... for (> a b c ...)
nl_val *nl_gt(nl_val *val_list){
	nl_val *ret=NULL;
	
	//garbage in, garbage out; if you give us NULL we return NULL
	if(nl_contains_nulls(val_list)){
		fprintf(stderr,"Err: a NULL argument was given to equality operator, returning NULL\n");
		return NULL;
	}
	if(!((nl_list_len(val_list)>=2) && (val_list->d.pair.f->t==NUM) && (val_list->d.pair.r->d.pair.f->t==NUM))){
		fprintf(stderr,"Err: incorrect use of gt operator >; arg count < 2 or incorrect type (for string gt use ar>)\n");
		return NULL;
	}
	
	ret=nl_val_malloc(BYTE);
	ret->d.byte.v=FALSE;
	
	nl_val *last_value=val_list->d.pair.f;
	val_list=val_list->d.pair.r;
	
	while((val_list!=NULL) && (val_list->t==PAIR)){
		//if we got a>b, then set the return to true and keep going
		if(nl_val_cmp(last_value,val_list->d.pair.f)>0){
			ret->d.byte.v=TRUE;
		//as soon as we get one case that's false the whole thing is false so just return out
		}else{
			ret->d.byte.v=FALSE;
			break;
		}
		last_value=val_list->d.pair.f;
		val_list=val_list->d.pair.r;
	}
	return ret;
}

//numeric lt operator <
//if more than two arguments are given then this will only return true if a<b<c<... for (< a b c ...)
nl_val *nl_lt(nl_val *val_list){
	nl_val *ret=NULL;
	
	//garbage in, garbage out; if you give us NULL we return NULL
	if(nl_contains_nulls(val_list)){
		fprintf(stderr,"Err: a NULL argument was given to equality operator, returning NULL\n");
		return NULL;
	}
	if(!((nl_list_len(val_list)>=2) && (val_list->d.pair.f->t==NUM) && (val_list->d.pair.r->d.pair.f->t==NUM))){
		fprintf(stderr,"Err: incorrect use of lt operator <; arg count < 2 or incorrect type (for string lt use ar<)\n");
		return NULL;
	}
	
	ret=nl_val_malloc(BYTE);
	ret->d.byte.v=FALSE;
	
	nl_val *last_value=val_list->d.pair.f;
	val_list=val_list->d.pair.r;
	
	while((val_list!=NULL) && (val_list->t==PAIR)){
		//if we got a<b, then set the return to true and keep going
		if(nl_val_cmp(last_value,val_list->d.pair.f)<0){
			ret->d.byte.v=TRUE;
		//as soon as we get one case that's false the whole thing is false so just return out
		}else{
			ret->d.byte.v=FALSE;
			break;
		}
		last_value=val_list->d.pair.f;
		val_list=val_list->d.pair.r;
	}
	return ret;
}

//END C-NL-STDLIB-CMP SUBROUTINES  --------------------------------------------------------------------------------

//END C-NL-STDLIB SUBROUTINES  ------------------------------------------------------------------------------------

