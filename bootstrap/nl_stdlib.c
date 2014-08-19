#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "nl_structures.h"

//BEGIN C-NL-STDLIB SUBROUTINES  ----------------------------------------------------------------------------------

//emulate getch() behavior on *nix /without/ ncurses
int nix_getch(){
	struct termios old_t;
	struct termios new_t;
	int ch;
	
	tcgetattr(STDIN_FILENO,&old_t);
	memcpy(&new_t,&old_t,sizeof(struct termios));
	new_t.c_lflag &= ~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO,TCSANOW,&new_t);
	
	ch=getchar();
	
	tcsetattr(STDIN_FILENO,TCSANOW,&old_t);
	return ch;
}

//gcd-reduce a rational number
void nl_gcd_reduce(nl_val *v){
	//ignore null and non-rational values
	if((v==NULL) || (v->t!=NUM)){
#ifdef _STRICT
		exit(1);
#endif
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
		fprintf(stderr,"Err [line %u]: comparison between different types is nonsensical, assuming a<b...",line_number);
		fprintf(stderr,"(offending values were a=");
		nl_out(stderr,v_a);
		fprintf(stderr," and b=");
		nl_out(stderr,v_b);
		fprintf(stderr,")\n");
#ifdef _STRICT
		exit(1);
#endif
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
//					int element_cmp=nl_val_cmp(&(v_a->d.array.v[n]),&(v_b->d.array.v[n]));
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

//push a c string onto a neulang string (note that this is side-effect based, it's an array push)
void nl_str_push_cstr(nl_val *nl_str, const char *cstr){
	if((nl_str==NULL) || (nl_str->t!=ARRAY)){
		ERR_EXIT(nl_str,"non-array type given to string push operation",TRUE);
		return;
	}
	
	int n=0;
	while(cstr[n]!='\0'){
		nl_val *char_to_push=nl_val_malloc(BYTE);
		char_to_push->d.byte.v=cstr[n];
		
		nl_array_push(nl_str,char_to_push);
		
		n++;
	}
}

//push a neulang string onto another neulang string (only the first one is changed, the second is not, and data is copied in element by element)
void nl_str_push_nlstr(nl_val *nl_str, const nl_val *str_to_push){
	if((nl_str==NULL) || (nl_str->t!=ARRAY)){
		ERR_EXIT(nl_str,"non-array type given to string push operation",TRUE);
		return;
	}
	
	if((str_to_push==NULL) || (str_to_push->t!=ARRAY)){
		ERR_EXIT(str_to_push,"non-array type given as argument/addition in string push operation",TRUE);
		return;
	}
	
	int n;
	for(n=0;n<(str_to_push->d.array.size);n++){
		nl_val *char_to_push=nl_val_cp(str_to_push->d.array.v[n]);
		
		nl_array_push(nl_str,char_to_push);
	}
}

//return a byte version of the given number constant, if possible
nl_val *nl_num_to_byte(nl_val *num_list){
	nl_val *ret=NULL;
	
	int arg_count=nl_c_list_size(num_list);
	if(arg_count>=1){
		if((num_list->d.pair.f->t==NUM) && (num_list->d.pair.f->d.num.d==1)){
			if(num_list->d.pair.f->d.num.n<256){
				ret=nl_val_malloc(BYTE);
				ret->d.byte.v=num_list->d.pair.f->d.num.n;
			}else{
				ERR_EXIT(num_list,"overflow in int_to_byte (given int can't fit in a byte), returning NULL",TRUE);
			}
		}else{
			ERR_EXIT(num_list,"non-int value given to int_to_byte; type conversion nonsensical, returning NULL",TRUE);
		}
		
		if(arg_count>1){
//			fprintf(stderr,"Warn [line %u]: too many arguments given to int_to_byte, ignoring all but the first...\n",line_number);
			ERR(num_list,"too many arguments given to int_to_byte, ignoring all but the first...",TRUE);
		}
	}else{
		ERR_EXIT(num_list,"no arguments given to int_to_byte, can't convert NULL! (returning NULL)",TRUE);
	}
	
	return ret;
}

//return an integer version of the given byte
nl_val *nl_byte_to_num(nl_val *byte_list){
	nl_val *ret=NULL;
	
	int arg_count=nl_c_list_size(byte_list);
	if(arg_count>=1){
		if(byte_list->d.pair.f->t==BYTE){
			ret=nl_val_malloc(NUM);
			ret->d.num.d=1;
			ret->d.num.n=byte_list->d.pair.f->d.byte.v;
		}else{
			ERR_EXIT(byte_list,"wrong type given to byte_to_num",TRUE);
		}
		
		if(arg_count>1){
//			fprintf(stderr,"Warn [line %u]: too many arguments given to int_to_byte, ignoring all but the first...\n",line_number);
			ERR(byte_list,"too many arguments given to byte_to_num, ignoring all but the first...",TRUE);
		}
	}else{
		ERR_EXIT(byte_list,"no arguments given to byte_to_num, can't convert NULL! (returning NULL)",TRUE);
	}
	
	return ret;
}

//returns the string-encoded version of any given expression
nl_val *nl_val_to_str(nl_val *exp){
	nl_val *ret=nl_val_malloc(ARRAY);
	//output for sprintf operations
	char buffer[BUFFER_SIZE];
	//temporary string for sub-expressions
	nl_val *tmp_str;
	
/*
#ifdef _DEBUG
	printf("nl_val_to_str debug 0, converting ");
	nl_out(stdout,exp);
	printf(" into a string...\n");
#endif
*/
	
	if(exp==NULL){
		nl_str_push_cstr(ret,"NULL"); //should this be a null string instead?
		return ret;
	}
	
	switch(exp->t){
		case BYTE:
			if(exp->d.byte.v<2){
				sprintf(buffer,"%i",exp->d.byte.v);
				nl_str_push_cstr(ret,buffer);
			}else{
				sprintf(buffer,"%c",exp->d.byte.v);
				nl_str_push_cstr(ret,buffer);
			}
			break;
		case NUM:
			sprintf(buffer,"%lli/%llu",exp->d.num.n,exp->d.num.d);
			nl_str_push_cstr(ret,buffer);
			break;
		case PAIR:
			//this is also linked list conversion; it's a little more complex to make it pretty because this is good for debugging and just generally
			nl_str_push_cstr(ret,"(");
			
			tmp_str=nl_val_to_str(exp->d.pair.f);
			nl_str_push_nlstr(ret,tmp_str);
			nl_val_free(tmp_str);
			{
				exp=exp->d.pair.r;
				
				//linked-list conversion
				int len=0;
				while((exp!=NULL) && (exp->t==PAIR)){
					nl_str_push_cstr(ret," ");
					
					tmp_str=nl_val_to_str(exp->d.pair.f);
					nl_str_push_nlstr(ret,tmp_str);
					nl_val_free(tmp_str);
					
					len++;
					exp=exp->d.pair.r;
				}
				
				//normal pair conversion (a cons cell, in lisp terminology)
				if(len==0){
					nl_str_push_cstr(ret," . ");
					
					tmp_str=nl_val_to_str(exp);
					nl_str_push_nlstr(ret,tmp_str);
					nl_val_free(tmp_str);
				}
			}
			nl_str_push_cstr(ret,")");
			break;
		case ARRAY:
			nl_str_push_cstr(ret,"[ ");
			{
				unsigned int n;
				for(n=0;n<(exp->d.array.size);n++){
//					tmp_str=nl_val_to_str(&(exp->d.array.v[n]));
					tmp_str=nl_val_to_str(exp->d.array.v[n]);
					nl_str_push_nlstr(ret,tmp_str);
					nl_val_free(tmp_str);
					
					nl_str_push_cstr(ret," ");
				}
			}
			nl_str_push_cstr(ret,"]");
			break;
		case PRI:
			nl_str_push_cstr(ret,"<primitive procedure>");
			break;
		case SUB:
			nl_str_push_cstr(ret,"<closure/subroutine>");
			break;
		case SYMBOL:
			nl_str_push_cstr(ret,"<symbol ");
			if(exp->d.sym.name!=NULL){
				unsigned int n;
				for(n=0;n<(exp->d.sym.name->d.array.size);n++){
//					tmp_str=nl_val_to_str(&(exp->d.sym.name->d.array.v[n]));
					tmp_str=nl_val_to_str(exp->d.sym.name->d.array.v[n]);
					nl_str_push_nlstr(ret,tmp_str);
					nl_val_free(tmp_str);
				}
			}
			nl_str_push_cstr(ret,">");
			break;
		case EVALUATION:
			nl_str_push_cstr(ret,"<evaluation ");
			if((exp->d.eval.sym!=NULL) && (exp->d.eval.sym->d.sym.name!=NULL)){
				unsigned int n;
				for(n=0;n<(exp->d.eval.sym->d.sym.name->d.array.size);n++){
//						tmp_str=nl_val_to_str(&(exp->d.eval.sym->d.sym.name->d.array.v[n]));
					tmp_str=nl_val_to_str(exp->d.eval.sym->d.sym.name->d.array.v[n]);
					nl_str_push_nlstr(ret,tmp_str);
					nl_val_free(tmp_str);
				}
			}
			nl_str_push_cstr(ret,">");
			break;
		default:
			ERR(NULL,"cannot convert unknown type to string",FALSE);
			
			break;
	}
	
	return ret;
}

//returns the string-encoded version of any given list of expressions
nl_val *nl_val_list_to_str(nl_val *val_list){
	nl_val *ret=nl_val_malloc(ARRAY);
	//temporary string for sub-expressions
	nl_val *tmp_str;
	
	while(val_list!=NULL && val_list->t==PAIR){
		nl_val *exp=val_list->d.pair.f;
		
		tmp_str=nl_val_to_str(exp);
		nl_str_push_nlstr(ret,tmp_str);
		nl_val_free(tmp_str);
		
		val_list=val_list->d.pair.r;
		
		//put a space between successive elements
		if(val_list!=NULL){
			nl_str_push_cstr(ret," ");
		}
	}
	
	//okay, now return the string we just got!
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
//		nl_val *new_array_v=(nl_val*)(malloc((new_stored_size)*(sizeof(nl_val))));
		nl_val **new_array_v=(nl_val**)(malloc((new_stored_size)*(sizeof(nl_val*))));
		
		//copy in the old data
		int n;
		for(n=0;n<(new_size-1);n++){
//			memcpy(&(new_array_v[n]),&(a->d.array.v[n]),sizeof(nl_val));
			new_array_v[n]=(a->d.array.v[n]);
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
//	memcpy(&(a->d.array.v[(new_size-1)]),v,sizeof(nl_val));
//	nl_val_free(v);
	a->d.array.v[(new_size-1)]=v;
}

//returns the entry in the array a (first arg) at index idx (second arg)
nl_val *nl_array_idx(nl_val *args){
	if(nl_c_list_size(args)!=2){
		ERR_EXIT(args,"wrong number of arguments to array idx",TRUE);
		return NULL;
	}
	
	nl_val *a=args->d.pair.f;
	nl_val *idx=args->d.pair.r->d.pair.f;
	
	if((a==NULL) || (idx==NULL)){
		ERR_EXIT(args,"null argument given to array idx",TRUE);
		return NULL;
	}
	
	if((a->t!=ARRAY) || (idx->t!=NUM)){
		ERR_EXIT(args,"wrong type given to array idx, takes array then index (an integer)",TRUE);
		return NULL;
	}
	
	int index=0;
	if((idx!=NULL) && (idx->t==NUM) && (idx->d.num.d==1)){
		index=idx->d.num.n;
	}else{
		ERR_EXIT(args,"given array index is not an integer",TRUE);
		return NULL;
	}
	
	if((index<0) || (index>=(a->d.array.size))){
		ERR_EXIT(args,"out-of-bounds index given to array idx",TRUE);
		return NULL;
	}
//	return nl_val_cp(&(a->d.array.v[index]));
	return nl_val_cp(a->d.array.v[index]);
}

//return the size of the first argument
//NOTE: subsequent arguments are IGNORED
nl_val *nl_array_size(nl_val *array_list){
	nl_val *acc=NULL;
	if((array_list!=NULL) && (array_list->t==PAIR) && (array_list->d.pair.f->t==ARRAY)){
		acc=nl_val_malloc(NUM);
		acc->d.num.d=1;
		acc->d.num.n=array_list->d.pair.f->d.array.size;
		
		if(array_list->d.pair.r!=NULL){
//			fprintf(stderr,"Warn [line %u]: too many arguments given to array size operation, only the first will be used...\n",line_number);
			ERR(array_list,"too many arguments given to array size operation, only the first will be used...",TRUE);
		}
	}else{
		ERR_EXIT(array_list,"wrong syntax or type for array size operation (did you give us a NULL?)",TRUE);
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
//			nl_array_push(acc,nl_val_cp(&(current_array->d.array.v[n])));
			nl_array_push(acc,nl_val_cp(current_array->d.array.v[n]));
		}
		
		array_list=array_list->d.pair.r;
	}
	
	if(array_list!=NULL){
		ERR_EXIT(array_list,"got a non-array value in array concatenation operation",TRUE);
	}
	
	return acc;
}

//returns a new array with the given value substituted for value at given index in the given array
//arguments array, index, new-value
nl_val *nl_array_replace(nl_val *arg_list){
	int argc=nl_c_list_size(arg_list);
	if(argc!=3){
		ERR_EXIT(arg_list,"incorrect number of arguments given to array replace operation",TRUE);
		return NULL;
	}
	nl_val *ar=arg_list->d.pair.f;
	nl_val *idx=arg_list->d.pair.r->d.pair.f;
	nl_val *new_val=arg_list->d.pair.r->d.pair.r->d.pair.f;
	
	if((ar==NULL) || (ar->t!=ARRAY) || (idx==NULL) || (idx->t!=NUM)){
		ERR_EXIT(arg_list,"invalid type given to array replace operation",TRUE);
		return NULL;
	}
	
	if(idx->d.num.d!=NUM){
		ERR_EXIT(idx,"non-integer index given to array replace operation",TRUE);
		return NULL;
	}
	
	if(idx->d.num.n>=(ar->d.array.size)){
		ERR_EXIT(idx,"index given to array replace operation is larger than array size",TRUE);
		return NULL;
	}
	
	//okay now we're past the error handling and we can actually do something
	//make a new array
	nl_val *ret=nl_val_malloc(ARRAY);
	
	//push in copies of all the old values, substituting the new value where appropriate
	int n;
	for(n=0;n<(ar->d.array.size);n++){
		if(n==(idx->d.num.n)){
			new_val->ref++;
			nl_array_push(ret,new_val);
		}else{
			nl_array_push(ret,nl_val_cp(ar->d.array.v[n]));
		}
	}
	
	return ret;
}

//output the given list of strings in sequence
//returns NULL (a void function)
nl_val *nl_outstr(nl_val *array_list){
	while((array_list!=NULL) && (array_list->t==PAIR))
	{
		nl_val *output_str=array_list->d.pair.f;
		
		int n;
		for(n=0;n<output_str->d.array.size;n++){
//			nl_out(stdout,&(output_str->d.array.v[n]));
			nl_out(stdout,output_str->d.array.v[n]);
		}
		
		array_list=array_list->d.pair.r;
	}
	
	return NULL;
}

//outputs the given value to stdout
//returns NULL (a void function)
nl_val *nl_outexp(nl_val *v_list){
	while(v_list!=NULL && v_list->t==PAIR){
		nl_out(stdout,v_list->d.pair.f);
		v_list=v_list->d.pair.r;
	}
	return NULL;
}

//reads input from stdin and returns the resulting expression
nl_val *nl_inexp(nl_val *arg_list){
	//TODO: support other files (by means of arguments)!
	return nl_read_exp(stdin);
}

//TODO: add an argument for line-editing here!
//reads a line from stdin and returns the result as a [neulang] string
nl_val *nl_inline(nl_val *arg_list){
	nl_val *ret=nl_val_malloc(ARRAY);
	
	int c=fgetc(stdin);
	while((c!='\n') && (c!='\r')){
		if(feof(stdin)){
			break;
		}
		
		nl_val *next_char=nl_val_malloc(BYTE);
		next_char->d.byte.v=(unsigned char)(c);
		
		nl_array_push(ret,next_char);
		
		c=fgetc(stdin);
	}
	
	return ret;
}

//reads a single keystroke from stdin and returns the result as a num
nl_val *nl_inchar(nl_val *arg_list){
	int c=nix_getch();
	nl_val *ret=nl_val_malloc(NUM);
	ret->d.num.d=1;
	ret->d.num.n=c;
	return ret;
}


//TODO: array search (substring search)

//END C-NL-STDLIB-ARRAY SUBROUTINES  ------------------------------------------------------------------------------

//BEGIN C-NL-STDLIB-LIST SUBROUTINES  -----------------------------------------------------------------------------

//returns the length (size) of a singly-linked list
//note that cyclic lists are infinite and this will never terminate on them
int nl_c_list_size(nl_val *list){
	int len=0;
	while((list!=NULL) && (list->t==PAIR)){
		len++;
		list=list->d.pair.r;
	}
	return len;
}

//returns the list element at the given index (we use 0-indexing)
nl_val *nl_c_list_idx(nl_val *list, nl_val *idx){
	nl_val *ret=NULL;
	
	if((idx->t!=NUM) || (idx->d.num.d!=1)){
		fprintf(stderr,"Err [line %u]: nl_list_idx only accepts integer list indices, given index was ",line_number);
		nl_out(stderr,idx);
		fprintf(stderr,"\n");
#ifdef _STRICT
		exit(1);
#endif
		return NULL;
	}
	
	int current_idx=0;
	while((list!=NULL) && (list->t==PAIR)){
		if(current_idx==(idx->d.num.n)){
			ret=nl_val_cp(list->d.pair.f);
			break;
		}
		
		current_idx++;
		list=list->d.pair.r;
	}
	return ret;
}

//return the size of the first argument
//NOTE: subsequent arguments are IGNORED
nl_val *nl_list_size(nl_val *list_list){
	nl_val *ret=nl_val_malloc(NUM);
	ret->d.num.d=1;
	ret->d.num.n=0;
	
	if((list_list!=NULL) && (list_list->t==PAIR)){
		if(list_list->d.pair.r!=NULL){
//			fprintf(stderr,"Warn [line %u]: too many arguments given to list size operation, only the first will be used...\n",line_number);
			ERR(list_list,"too many arguments given to list size operation, only the first will be used...",TRUE);
		}
		
		nl_val *list_entry=list_list->d.pair.f;
		if(list_entry->t!=PAIR){
			nl_val_free(ret);
			ret=NULL;
			ERR_EXIT(list_list,"wrong type given to list size operation!",TRUE);
		}else{
			//if we started on a NULL just skip past that (the empty list is technically NULL . NULL)
			if(list_entry->d.pair.f==NULL){
				list_entry=list_entry->d.pair.r;
			}
			while((list_entry!=NULL) && (list_entry->t==PAIR)){
				ret->d.num.n++;
				list_entry=list_entry->d.pair.r;
			}
		}
	}else{
		nl_val_free(ret);
		ret=NULL;
		ERR_EXIT(list_list,"wrong syntax for list size operation (did you give us a NULL?)",TRUE);
	}
	return ret;
}

//returns the list element at the given index (we use 0-indexing)
//this calls out to the c version which cannot be used directly due to argument count
nl_val *nl_list_idx(nl_val *arg_list){
	int args=nl_c_list_size(arg_list);
	if(args<2 || nl_contains_nulls(arg_list)){
		ERR_EXIT(arg_list,"too few arguments (or a NULL) given to list idx operation",TRUE);
		return NULL;
	}
	if(arg_list->d.pair.f->t!=PAIR && arg_list->d.pair.r->d.pair.f->t!=NUM){
		ERR_EXIT(arg_list,"incorrect types given to list idx operation",TRUE);
		return NULL;
	}
	
	return nl_c_list_idx(arg_list->d.pair.f,arg_list->d.pair.r->d.pair.f);
}

//concatenates all the given lists
nl_val *nl_list_cat(nl_val *list_list){
	nl_val *acc=NULL;
	nl_val *current_acc_cell=NULL;
	char first_list=TRUE;
	
	while((list_list!=NULL) && (list_list->d.pair.f->t==PAIR)){
		//we got a list, so add it to the accumulator
		if(first_list){
			acc=nl_val_cp(list_list->d.pair.f);
			current_acc_cell=acc;
			first_list=FALSE;
		}else{
			current_acc_cell->d.pair.r=nl_val_cp(list_list->d.pair.f);
		}
		
		while((current_acc_cell->t==PAIR) && (current_acc_cell->d.pair.r!=NULL)){
			current_acc_cell=current_acc_cell->d.pair.r;
		}
		
		list_list=list_list->d.pair.r;
	}
	
	if(list_list!=NULL){
		ERR_EXIT(list_list,"got a non-list value in list concatenation operation",TRUE);
		nl_val_free(acc);
		acc=NULL;
	}
	
	return acc;
}

//END C-NL-STDLIB-LIST SUBROUTINES  -------------------------------------------------------------------------------

//BEGIN C-NL-STDLIB-MATH SUBROUTINES  -----------------------------------------------------------------------------

//add a list of (rational) numbers
nl_val *nl_add(nl_val *num_list){
	nl_val *acc=NULL;
	if((num_list!=NULL) && (num_list->t==PAIR) && (num_list->d.pair.f!=NULL) && (num_list->d.pair.f->t==NUM)){
		acc=nl_val_cp(num_list->d.pair.f);
		num_list=num_list->d.pair.r;
	}else{
		ERR_EXIT(num_list,"incorrect use of add operation (null list or incorrect type in first operand)",TRUE);
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
			ERR_EXIT(num_list,"non-number given to add operation, returning NULL from add",TRUE);
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
	if((num_list!=NULL) && (num_list->t==PAIR) && (num_list->d.pair.f!=NULL) && (num_list->d.pair.f->t==NUM)){
		acc=nl_val_cp(num_list->d.pair.f);
		num_list=num_list->d.pair.r;
	}else{
		ERR_EXIT(num_list,"incorrect use of sub operation (null list or incorrect type in first operand)",TRUE);
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
			ERR_EXIT(num_list,"non-number given to subtract operation, returning NULL from subtract",TRUE);
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
	if((num_list!=NULL) && (num_list->t==PAIR) && (num_list->d.pair.f!=NULL) && (num_list->d.pair.f->t==NUM)){
		acc=nl_val_cp(num_list->d.pair.f);
		num_list=num_list->d.pair.r;
	}else{
		ERR_EXIT(num_list,"incorrect use of mul operation (null list or incorrect type in first operand)",TRUE);
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
			ERR_EXIT(num_list,"non-number given to mul operation, returning NULL from mul",TRUE);
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
	if((num_list!=NULL) && (num_list->t==PAIR) && (num_list->d.pair.f!=NULL) && (num_list->d.pair.f->t==NUM)){
		acc=nl_val_cp(num_list->d.pair.f);
		num_list=num_list->d.pair.r;
	}else{
		ERR_EXIT(num_list,"incorrect use of div operation (null list or incorrect type in first operand)",TRUE);
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
			ERR_EXIT(num_list,"non-number given to div operation, returning NULL from mul",TRUE);
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

//checks if the values of given type t within val_list are equal
nl_val *nl_generic_eq(nl_val *val_list, nl_type t){
	nl_val *ret=NULL;
	
	//garbage in, garbage out; if you give us NULL we return NULL
	if(nl_contains_nulls(val_list)){
		ERR_EXIT(val_list,"a NULL argument was given to equality operator, returning NULL",TRUE);
		return NULL;
	}
	if(!((nl_c_list_size(val_list)>=2) && (val_list->d.pair.f->t==t) && (val_list->d.pair.r->d.pair.f->t==t))){
		ERR_EXIT(val_list,"incorrect use of eq operator =; arg count < 2 or incorrect type",TRUE);
		return NULL;
	}
	
	ret=nl_val_malloc(BYTE);
	ret->d.byte.v=FALSE;
	
	nl_val *last_value=val_list->d.pair.f;
	val_list=val_list->d.pair.r;
	
	while((val_list!=NULL) && (val_list->t==PAIR)){
		//if we got a==b, then set the return to true and keep going
		if(nl_val_cmp(last_value,val_list->d.pair.f)==0){
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

//checks if the values of a given type t within val_list are in descending order
nl_val *nl_generic_gt(nl_val *val_list, nl_type t){
	nl_val *ret=NULL;
	
	//garbage in, garbage out; if you give us NULL we return NULL
	if(nl_contains_nulls(val_list)){
		ERR_EXIT(val_list,"a NULL argument was given to greater than operator, returning NULL",TRUE);
		return NULL;
	}
	if(!((nl_c_list_size(val_list)>=2) && (val_list->d.pair.f->t==t) && (val_list->d.pair.r->d.pair.f->t==t))){
		ERR_EXIT(val_list,"incorrect use of gt operator >; arg count < 2 or incorrect type",TRUE);
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

//checks if the values of a given type t within val_list are in ascending order
nl_val *nl_generic_lt(nl_val *val_list, nl_type t){
	nl_val *ret=NULL;
	
	//garbage in, garbage out; if you give us NULL we return NULL
	if(nl_contains_nulls(val_list)){
		ERR_EXIT(val_list,"a NULL argument was given to less than operator, returning NULL",TRUE);
		return NULL;
	}
	if(!((nl_c_list_size(val_list)>=2) && (val_list->d.pair.f->t==t) && (val_list->d.pair.r->d.pair.f->t==t))){
		ERR_EXIT(val_list,"incorrect use of lt operator <; arg count < 2 or incorrect type",TRUE);
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

//checks if list is descending or equal, >=
nl_val *nl_generic_ge(nl_val *val_list, nl_type t){
	nl_val *gt_result=nl_generic_gt(val_list,t);
	if(nl_is_true(gt_result)){
		return gt_result;
	}else{
		nl_val_free(gt_result);
		return nl_generic_eq(val_list,t);
	}
}

//checks if list is ascending or equal, <=
nl_val *nl_generic_le(nl_val *val_list, nl_type t){
	nl_val *gt_result=nl_generic_lt(val_list,t);
	if(nl_is_true(gt_result)){
		return gt_result;
	}else{
		nl_val_free(gt_result);
		return nl_generic_eq(val_list,t);
	}
}

//numeric equality operator =
//if more than two arguments are given then this will only return true if a==b==c==... for (= a b c ...)
nl_val *nl_eq(nl_val *val_list){
	return nl_generic_eq(val_list,NUM);
}

//numeric gt operator >
//if more than two arguments are given then this will only return true if a>b>c>... for (> a b c ...)
nl_val *nl_gt(nl_val *val_list){
	return nl_generic_gt(val_list,NUM);
}

//numeric lt operator <
//if more than two arguments are given then this will only return true if a<b<c<... for (< a b c ...)
nl_val *nl_lt(nl_val *val_list){
	return nl_generic_lt(val_list,NUM);
}

//numeric ge operator >=
nl_val *nl_ge(nl_val *val_list){
	return nl_generic_ge(val_list,NUM);
}

//numeric le operator <=
nl_val *nl_le(nl_val *val_list){
	return nl_generic_le(val_list,NUM);
}

//array equality operator ar=
nl_val *nl_ar_eq(nl_val *val_list){
	return nl_generic_eq(val_list,ARRAY);
}

//array greater than operator ar>
nl_val *nl_ar_gt(nl_val *val_list){
	return nl_generic_gt(val_list,ARRAY);
}

//array less than operator ar<
nl_val *nl_ar_lt(nl_val *val_list){
	return nl_generic_lt(val_list,ARRAY);
}

//array greater than or equal to operator ar>=
nl_val *nl_ar_ge(nl_val *val_list){
	return nl_generic_ge(val_list,ARRAY);
}

//array less than or equal to operator ar<=
nl_val *nl_ar_le(nl_val *val_list){
	return nl_generic_le(val_list,ARRAY);
}

//list equality operator list=
nl_val *nl_list_eq(nl_val *val_list){
	return nl_generic_eq(val_list,PAIR);
}

//list greater than operator list>
nl_val *nl_list_gt(nl_val *val_list){
	return nl_generic_gt(val_list,PAIR);
}

//list less than operator list<
nl_val *nl_list_lt(nl_val *val_list){
	return nl_generic_lt(val_list,PAIR);
}

//list greater than or equal to operator list>=
nl_val *nl_list_ge(nl_val *val_list){
	return nl_generic_ge(val_list,PAIR);
}

//list less than or equal to operator list<=
nl_val *nl_list_le(nl_val *val_list){
	return nl_generic_le(val_list,PAIR);
}

//byte equality operator b=
//if more than two arguments are given then this will only return true if a==b==c==... for (b= a b c ...)
nl_val *nl_byte_eq(nl_val *val_list){
	return nl_generic_eq(val_list,BYTE);
}

//byte greater than operator b>
nl_val *nl_byte_gt(nl_val *val_list){
	return nl_generic_gt(val_list,BYTE);
}

//byte less than operator b<
nl_val *nl_byte_lt(nl_val *val_list){
	return nl_generic_lt(val_list,BYTE);
}

//byte greater than or equal to operator b>=
nl_val *nl_byte_ge(nl_val *val_list){
	return nl_generic_ge(val_list,BYTE);
}

//byte less than or equal to operator b<=
nl_val *nl_byte_le(nl_val *val_list){
	return nl_generic_le(val_list,BYTE);
}

//symbol equality operator sym=
nl_val *nl_sym_eq(nl_val *val_list){
	return nl_generic_eq(val_list,SYMBOL);
}

//symbol greater than operator sym>
nl_val *nl_sym_gt(nl_val *val_list){
	return nl_generic_gt(val_list,SYMBOL);
}

//symbol less than operator sym<
nl_val *nl_sym_lt(nl_val *val_list){
	return nl_generic_lt(val_list,SYMBOL);
}

//symbol greater than or equal to operator sym>=
nl_val *nl_sym_ge(nl_val *val_list){
	return nl_generic_ge(val_list,SYMBOL);
}

//symbol less than or equal to operator sym<=
nl_val *nl_sym_le(nl_val *val_list){
	return nl_generic_le(val_list,SYMBOL);
}

//null check null?
//returns TRUE iff all elements given in the list are NULL
nl_val *nl_is_null(nl_val *val_list){
	nl_val *ret=nl_val_malloc(BYTE);
	ret->d.byte.v=TRUE;
	
	nl_val *val_list_start=val_list;

	//first check for straight-up NULL values
	while((val_list!=NULL) && (val_list->t==PAIR)){
		if(val_list->d.pair.f!=NULL){
			ret->d.byte.v=FALSE;
			break;
		}
		
		val_list=val_list->d.pair.r;
	}
	
	//if we found any NULL values then check for empty lists
	if(ret->d.byte.v==FALSE){
		ret->d.byte.v=TRUE;
		
		val_list=val_list_start;
		nl_val *list_entry=NULL;
		while((ret->d.byte.v==TRUE) && (val_list!=NULL) && (val_list->t==PAIR)){
			list_entry=val_list->d.pair.f;
			while(list_entry!=NULL && list_entry->t==PAIR){
				//if we found an entry within the list that wasn't null then return false
				if(list_entry->d.pair.f!=NULL){
					ret->d.byte.v=FALSE;
					break;
				}
				
				list_entry=list_entry->d.pair.r;
			}
			
			//if we found a non-list entry that wasn't null then return false
			if(list_entry!=NULL){
				ret->d.byte.v=FALSE;
				break;
			}
			
			val_list=val_list->d.pair.r;
		}
	}
	
	return ret;
}

//END C-NL-STDLIB-CMP SUBROUTINES  --------------------------------------------------------------------------------

//BEGIN C-NL-STDLIB-BITOP SUBROUTINES  ----------------------------------------------------------------------------

//bitwise OR operation on the byte type
nl_val *nl_byte_or(nl_val *byte_list){
	nl_val *ret=NULL;
	if(nl_c_list_size(byte_list)<2){
		ERR_EXIT(byte_list,"insufficient arguments given to bitwise or operation",TRUE);
		return NULL;
	}
	
	//allocate an accumulator
	ret=nl_val_malloc(BYTE);
	//start with 0 because we'll OR everything else
	ret->d.byte.v=0;
	
	while((byte_list!=NULL) && (byte_list->t==PAIR)){
		if(byte_list->d.pair.f!=NULL && byte_list->d.pair.f->t==BYTE){
			//store a bitwise OR of the accumulator and the list element in the accumulator
			ret->d.byte.v=((ret->d.byte.v)|(byte_list->d.pair.f->d.byte.v));
		}else{
			nl_val_free(ret);
			ERR_EXIT(byte_list,"incorrect type given to bitwise or operation",TRUE);
			return NULL;
		}
		
		byte_list=byte_list->d.pair.r;
	}
	
	return ret;
}

//bitwise AND operation on the byte type
nl_val *nl_byte_and(nl_val *byte_list){
	nl_val *ret=NULL;
	if(nl_c_list_size(byte_list)<2){
		ERR_EXIT(byte_list,"insufficient arguments given to bitwise or operation",TRUE);
		return NULL;
	}
	
	//allocate an accumulator
	ret=nl_val_malloc(BYTE);
	//start with 0xff because we'll AND everything else
	ret->d.byte.v=0xff;
	
	while((byte_list!=NULL) && (byte_list->t==PAIR)){
		if(byte_list->d.pair.f!=NULL && byte_list->d.pair.f->t==BYTE){
			//store a bitwise AND of the accumulator and the list element in the accumulator
			ret->d.byte.v=((ret->d.byte.v)&(byte_list->d.pair.f->d.byte.v));
		}else{
			nl_val_free(ret);
			ERR_EXIT(byte_list,"incorrect type given to bitwise or operation",TRUE);
			return NULL;
		}
		
		byte_list=byte_list->d.pair.r;
	}
	
	return ret;
}

//END C-NL-STDLIB-BITOP SUBROUTINES  ------------------------------------------------------------------------------

//assert that all conditions in the given list are true; if not, exit (if compiled _STRICT) or return false (not strict)
nl_val *nl_assert(nl_val *cond_list){
	nl_val *ret=nl_val_malloc(BYTE);
	ret->d.byte.v=TRUE;
	while(cond_list!=NULL && cond_list->t==PAIR){
		if(!nl_is_true(cond_list->d.pair.f)){
			ret->d.byte.v=FALSE;
#ifdef _STRICT
			nl_val_free(ret);
			ERR_EXIT(cond_list,"ASSERT FAILED",TRUE);
#endif
		}
		
		cond_list=cond_list->d.pair.r;
	}
#ifdef _DEBUG
	if(cond_list!=NULL){
		printf("line %i: assert succeeded\n",cond_list->line);
	}else{
		printf("line %i: assert succeeded\n",line_number);
	}
#endif
	return ret;
}

//sleep a given number of seconds (if multiple arguments are given they are added)
nl_val *nl_sleep(nl_val *time_list){
	if(nl_c_list_size(time_list)==0){
		ERR_EXIT(time_list,"too few arguments given to sleep operation",FALSE);
		return NULL;
	}
	
	while((time_list!=NULL) && (time_list->t==PAIR)){
		if((time_list->d.pair.f==NULL) || (time_list->d.pair.f->t!=NUM)){
			ERR_EXIT(time_list,"non-number argument given to sleep operation",TRUE);
			return NULL;
		}
		//perform the division (from the rational number) then mul by 1000000 for a precision of 1 microsecond
		useconds_t time_to_sleep=(useconds_t)((1000000*(((double)(time_list->d.pair.f->d.num.n))/((double)(time_list->d.pair.f->d.num.d)))));
/*
#ifdef _DEBUG
		printf("nl_sleep debug 0, sleeping for %u microseconds\n",time_to_sleep);
#endif
*/
		usleep(time_to_sleep);
		
		time_list=time_list->d.pair.r;
	}
	
	return NULL;
}


//END C-NL-STDLIB SUBROUTINES  ------------------------------------------------------------------------------------

