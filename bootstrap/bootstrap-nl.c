//a bootstrap interpreter for the new neulang, the first which changes to a less lisp-like syntax

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

//BEGIN GLOBAL CONSTANTS ------------------------------------------------------------------------------------------

#define VERSION "0.1.0"
#define TRUE 1
#define FALSE 0

//END GLOBAL CONSTANTS --------------------------------------------------------------------------------------------

//BEGIN DATA STRUCTURES -------------------------------------------------------------------------------------------

//primitive data types; the user should be able to abstract these with a struct
typedef enum {
	//external (user-visible) types
	BYTE, //single-byte value (characters, booleans)
	NUM, //rational number support, consists of 2 bignum ints; for an integer the denominator portion will be 1
	PAIR, //cons cell
	ARRAY, //contiguous memory section
	PRI, //primitive procedure (C code)
	SUB, //closure (subroutine)
	
	//internal types (might still be user visible, but mostly an implementation detail)
	SYMBOL, //variable names, for the symbol table, internally this is a [byte]array
	EVALUATION, //to-be-evaluated symbol, denoted $symbol
} nl_type;

typedef struct nl_env_frame nl_env_frame;

//a primitive value structure, the basic unit of evalution in neulang
typedef struct nl_val nl_val;
struct nl_val {
	//type
	nl_type t;
	
	//constant or not flag (default TRUE from nl_val_malloc, set false when value is bound to a variable)
	char cnst;
	
	//union to save memory; called d (short for data)
	union {
		//byte value
		struct {
			char v;
		} byte;
		
		//(rational) num value
		struct {
			//numerator
			long long int n;
			//denominator (note that only the numerator is signed)
			long long int d;
		} num;
		
		//pair value
		struct {
			//"first"
			nl_val *f;
			//"rest"
			nl_val *r;
		} pair;
		
		//array value
		//TODO: make this continguous instead of an array of pointers to who-knows-where
		//note that all neulang arrays internally store a size
		struct {
			//sub-type of the array
//			nl_type t;
			
			//the memory itself and the number of elements stored
			nl_val **v;
			unsigned int size;
			
			//how much storage is used internally; this is so dynamic resizing is a little more efficient
			unsigned int stored_size;
		} array;
		
		//primitive procedure value
		struct {
			nl_val *(*function)(nl_val *arglist);
		} pri;
		
		//subroutine value
		struct {
			//return type of the subroutine
			nl_type t;
			
			//environment (since this is a closure)
			nl_env_frame *env;
			//body (linked list of statements to execute)
			nl_val *body;
		} sub;
		
		//symbol (variable name) value
		struct {
			//the string which has the real name
			nl_val *name;
		} sym;
	} d;
};

//environment frame (one global, then one per closure)
struct nl_env_frame {
	//store symbols
	nl_val *symbol_array;
	//store values
	nl_val *value_array;
	
	//the environment above this one (THIS MUST BE FREE'D SEPERATELY)
	nl_env_frame* up_scope;
}; 

//END DATA STRUCTURES ---------------------------------------------------------------------------------------------

//BEGIN FORWARD DECLARATIONS --------------------------------------------------------------------------------------
void nl_out(FILE *fp, nl_val *exp);
nl_val *nl_read_exp(FILE *fp);
void nl_array_push(nl_val *a, nl_val *v);
//END FORWARD DECLARATIONS ----------------------------------------------------------------------------------------


//BEGIN GLOBAL DATA -----------------------------------------------------------------------------------------------
//END GLOBAL DATA -------------------------------------------------------------------------------------------------

//BEGIN MEMORY MANAGEMENT SUBROUTINES -----------------------------------------------------------------------------

//allocate a value, and initialize it so that we're not doing anything too crazy
nl_val *nl_val_malloc(nl_type t){
	nl_val *ret=(nl_val*)(malloc(sizeof(nl_val)));
	ret->t=t;
	ret->cnst=TRUE;
	switch(ret->t){
		case BYTE:
			ret->d.byte.v=0;
			break;
		case NUM:
			ret->d.num.n=0;
			ret->d.num.d=1;
			break;
		case PAIR:
			ret->d.pair.f=NULL;
			ret->d.pair.r=NULL;
			break;
		case ARRAY:
			ret->d.array.v=NULL;
			ret->d.array.size=0;
			ret->d.array.stored_size=0;
			break;
		case PRI:
			ret->d.pri.function=NULL;
			break;
		case SUB:
			ret->d.sub.body=NULL;
			ret->d.sub.env=NULL;
			break;
		case SYMBOL:
			ret->d.sym.name=NULL;
			break;
		default:
			break;
	}
	
	return ret;
}

//free a value; this recursively frees complex data types
void nl_val_free(nl_val *exp){
	if(exp==NULL){
		return;
	}
	
	switch(exp->t){
		//primitive data types don't need anything else free'd
		case BYTE:
		case NUM:
			break;
		//pairs need each portion free'd
		case PAIR:
			nl_val_free(exp->d.pair.f);
			nl_val_free(exp->d.pair.r);
			break;
		//arrays need each element free'd
		case ARRAY:
			if(exp->d.array.v!=NULL){
				unsigned int n;
				for(n=0;n<(exp->d.array.size);n++){
					nl_val_free(exp->d.array.v[n]);
				}
				free(exp->d.array.v);
			}
			break;
		//primitive procedures are never free'd, they are static memory
		case PRI:
			break;
		//subroutines need the body an closure environment free'd
		case SUB:
			nl_val_free(exp->d.sub.body);
			free(exp->d.sub.env);
			break;
		//symbols need a recursive call for the string that's their name
		case SYMBOL:
			nl_val_free(exp->d.sym.name);
			break;
		default:
			break;
	}
	
	free(exp);
}

//copy a value data-wise into new memory, without changing the original
nl_val *nl_val_cp(const nl_val *v){
	//a null value is copied as a null value
	if(v==NULL){
		return NULL;
	}
	
	nl_val *ret=nl_val_malloc(v->t);
	switch(ret->t){
		case BYTE:
			ret->d.byte.v=v->d.byte.v;
			break;
		case NUM:
			ret->d.num.n=v->d.num.n;
			ret->d.num.d=v->d.num.d;
			break;
		//recurse to copy list elements
		case PAIR:
			ret->d.pair.f=nl_val_cp(v->d.pair.f);
			ret->d.pair.r=nl_val_cp(v->d.pair.r);
			break;
		//recurse to copy array elements, pushing each into the new array
		case ARRAY:
			{
				int n;
				for(n=0;(n<(v->d.array.size));n++){
					nl_array_push(ret,nl_val_cp(v->d.array.v[n]));
				}
			}
			break;
		case PRI:
			ret->d.pri.function=v->d.pri.function;
			break;
		//TODO: recurse and copy body and environment (should environment be constant? if so I think we need ref counting)
		case SUB:
			ret->d.sub.body=NULL;
			ret->d.sub.env=NULL;
			break;
		//recurse to copy name elements
		case SYMBOL:
			ret->d.sym.name=nl_val_cp(v->d.sym.name);
			break;
		default:
			break;
	}
	
	return ret;
}

//allocate an environment frame
nl_env_frame *nl_env_frame_malloc(nl_env_frame *up_scope){
	nl_env_frame *ret=(nl_env_frame*)(malloc(sizeof(nl_env_frame)));
	
	ret->symbol_array=nl_val_malloc(ARRAY);
	ret->value_array=nl_val_malloc(ARRAY);
	
	//the environment above this (NULL for global)
	ret->up_scope=up_scope;
	
	return ret;
}

//free an environment frame
void nl_env_frame_free(nl_env_frame *env){
	nl_val_free(env->symbol_array);
	nl_val_free(env->value_array);
	
	//note that we do NOT free the above environment here; if you want to do that do it elsewhere
	
	free(env);
}

//make a neulang string from a c string
nl_val *nl_str_from_c_str(const char *c_str){
	nl_val *ret=nl_val_malloc(ARRAY);
	
	int n=0;
	while(c_str[n]!='\0'){
		nl_val *character=nl_val_malloc(BYTE);
		character->d.byte.v=c_str[n];
		nl_array_push(ret,character);
		
		n++;
	}
	
	return ret;
}

//make a neulang symbol from a c string
nl_val *nl_sym_from_c_str(const char *c_str){
	nl_val *ret=nl_val_malloc(SYMBOL);
	ret->d.sym.name=nl_str_from_c_str(c_str);
	return ret;
}

//END MEMORY MANAGEMENT SUBROUTINES -------------------------------------------------------------------------------

//BEGIN C-NL-STDLIB SUBROUTINES -----------------------------------------------------------------------------------

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
		fprintf(stderr,"Err: comparison between different types is nonsensical, assuming a<b...\n");
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
		//TODO: subroutine equality
//		case SUB:
//			ret->d.sub.body=NULL;
//			ret->d.sub.env=NULL;
//			break;
		//symbols are equal if their names (byte arrays) are equal
		case SYMBOL:
			return nl_val_cmp(v_a->d.sym.name,v_b->d.sym.name);
			break;
		default:
			break;
	}
	
	//if we got here and didn't return assume equality
	return 0;
}

//TODO: write all array library functions

//push a value onto the end of an array
void nl_array_push(nl_val *a, nl_val *v){
	//this operation is undefined on null and non-array values
	if((a==NULL) || (a->t!=ARRAY) || (v==NULL)){
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
		
		//copy in the old data
		int n;
		for(n=0;n<(new_size-1);n++){
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

//END C-NL-STDLIB SUBROUTINES -------------------------------------------------------------------------------------


//BEGIN EVALUTION SUBROUTINES -------------------------------------------------------------------------------------

//TODO: proper evaluation of keywords!
//evaluate a keyword expression (or primitive function, if keyword isn't found)
nl_val *nl_eval_keyword(nl_val *keyword_exp){
	nl_val *keyword=keyword_exp->d.pair.f;
	nl_val *arguments=keyword_exp->d.pair.r;
	
	//allocate keywords
	nl_val *if_keyword=nl_sym_from_c_str("if");
	
	//check for if statements
	if(nl_val_cmp(keyword,if_keyword)==0){
		//TODO: handle if statements
		fprintf(stdout,"\nGot an if statement!\n");
		nl_out(stdout,keyword);
		fprintf(stdout,"\n");
		nl_out(stdout,arguments);
		fprintf(stdout,"\n");
	//TODO: check for all other keywords
	}else{
		//TODO: in the default case check for primitive procedures bound to this symbol
	}
	
	//free keywords
	nl_val_free(if_keyword);
	
	//free the original expression (frees the entire list, keyword, arguments, and all)
	nl_val_free(keyword_exp);
	
	return NULL;
}

//evaluate the given expression in the given environment
nl_val *nl_eval(nl_val *exp, nl_env_frame *env){
	nl_val *ret=NULL;
	
	//null is self-evaluating
	if(exp==NULL){
		return exp;
	}
	
	switch(exp->t){
		//self-evaluating expressions (all constant values)
		case BYTE:
		case NUM:
		case ARRAY:
		case PRI:
		case SUB:
		//symbols are self-evaluating, in the case of primitive calls the pair evaluation checks for them as keywords, without calling out to eval
		case SYMBOL:
			ret=exp;
			break;
		
		//complex cases
		//pairs eagerly evaluate then (if first argument isn't a keyword) call out to apply
		case PAIR:
			//make this check the first list entry, if it is a symbol then check it against keyword and primitive list
			if((exp->d.pair.f!=NULL) && (exp->d.pair.f->t==SYMBOL)){
				ret=nl_eval_keyword(exp);
			//otherwise eagerly evaluate then call out to apply
			}else if(exp->d.pair.f!=NULL){
				//TODO: eager evaluation, call apply
				
			//null lists are self-evaluating (the empty list)
			}else{
				ret=exp;
			}
			break;
		//evaluations look up value in the environment
		case EVALUATION:
			//TODO: make this look up the expression in the environment and return a copy of the result
			ret=exp;
			break;
		//default self-evaluating
		default:
			ret=exp;
			break;
	}
	
	//return the result of evaluation
	return ret;
}

//END EVALUTION SUBROUTINES ---------------------------------------------------------------------------------------

//BEGIN I/O SUBROUTINES -------------------------------------------------------------------------------------------

//check if a givne character counts as whitespace in neulang
char nl_is_whitespace(char c){
	return (c==' ') || (c=='\t') || (c=='\r') || (c=='\n');
}

//skip fp past any leading whitespaces
void nl_skip_whitespace(FILE *fp){
	char c=getc(fp);
	while(nl_is_whitespace(c)){
		c=getc(fp);
	}
	ungetc(c,fp);
}

//read a number
nl_val *nl_read_num(FILE *fp){
	//create a new number to return
	nl_val *ret=nl_val_malloc(NUM);
	//initialize to 0, this should get reset
	ret->d.num.n=0;
	ret->d.num.d=1;
	
	//whether or not this number is negative (starts with -)
	char negative=FALSE;
	
	//whether we've hit a / and are now reading a denominator or (default) not
	char reading_denom=FALSE;
	//whether we've hit a . and are now reading a float as a rational
	char reading_decimal=FALSE;
	
	char c=getc(fp);
	
	//handle negative signs
	if(c=='-'){
		negative=TRUE;
		c=getc(fp);
	}
	
	//handle numerical expressions starting with . (absolute value less than 1)
	if(c=='.'){
		reading_decimal=TRUE;
		c=getc(fp);
	}
	
	//read a number
	while(!nl_is_whitespace(c) && c!=')'){
		//numerator
		if(isdigit(c) && !(reading_denom) && !(reading_decimal)){
			(ret->d.num.n)*=10;
			(ret->d.num.n)+=(c-'0');
		//denominator
		}else if(isdigit(c) && (reading_denom) && !(reading_decimal)){
			(ret->d.num.d)*=10;
			(ret->d.num.d)+=(c-'0');
		//delimeter to start reading the denominator
		}else if(c=='/' && !(reading_denom) && !(reading_decimal)){
			reading_denom=TRUE;
			ret->d.num.d=0;
		//a decimal point causes us to read into the numerator and keep the order of magnitude correct by the denominator
		}else if(c=='.' && !(reading_denom) && !(reading_decimal)){
			reading_decimal=TRUE;
		//when reading in a decimal value just read into the numerator and keep correct order of magnitude in denominator
		}else if(isdigit(c) && reading_decimal){
			(ret->d.num.n)*=10;
			(ret->d.num.n)+=(c-'0');
			
			(ret->d.num.d)*=10;
		}else{
			fprintf(stderr,"Err: invalid character in numeric literal, \'%c\'\n",c);
		}
		
		c=getc(fp);
	}
	
	if(c==')'){
		ungetc(c,fp);
	}
	
	//incorporate negative values if a negative sign preceded the expression
	if(negative){
		(ret->d.num.n)*=(-1);
	}
	
	if(ret->d.num.d==0){
		fprintf(stderr,"Warn: divide-by-0 in rational number; did you forget a denominator?\n");
	}else{
		//gcd reduce the number for faster computation
		nl_gcd_reduce(ret);
	}
	
	return ret;
}

//read a string (byte array)
nl_val *nl_read_string(FILE *fp){
	nl_val *ret=NULL;
	
	char c=getc(fp);
	if(c!='"'){
		fprintf(stderr,"Err: string literal didn't start with \"; WHAT DID YOU DO? (started with \'%c\')\n",c);
		return ret;
	}
	
	//allocate a byte array
	ret=nl_val_malloc(ARRAY);
	
	c=getc(fp);
	
	//read until end quote, THERE IS NO ESCAPE
	while(c!='"'){
		nl_val *ar_entry=nl_val_malloc(BYTE);
		ar_entry->d.byte.v=c;
		nl_array_push(ret,ar_entry);
		
		c=getc(fp);
	}
	return ret;
}

//read a single character (byte)
nl_val *nl_read_char(FILE *fp){
	nl_val *ret=NULL;
	
	char c=getc(fp);
	if(c!='\''){
		fprintf(stderr,"Err: character literal didn't start with \'; WHAT DID YOU DO? (started with \'%c\')\n",c);
		return ret;
	}
	
	//allocate a byte
	ret=nl_val_malloc(BYTE);
	
	c=getc(fp);
	ret->d.byte.v=c;
	
	c=getc(fp);
	if(c!='\''){
		fprintf(stderr,"Warn: single-character literal didn't end with \'; (ended with \'%c\')\n",c);
	}
	
	return ret;
}

//read an expression list
nl_val *nl_read_exp_list(FILE *fp){
	nl_val *ret=NULL;
	
	char c=getc(fp);
	if(c!='('){
		fprintf(stderr,"Err: expression list didn't start with (; WHAT DID YOU DO? (started with \'%c\')\n",c);
		return ret;
	}
	
	//allocate a pair (the first element of a linked list)
	ret=nl_val_malloc(PAIR);
	
	nl_val *list_cell=ret;
	
	//read the first expression
	nl_val *next_exp=nl_read_exp(fp);
	
	//continue reading expressions until we hit a NULL
	while(next_exp!=NULL){
		list_cell->d.pair.f=next_exp;
		list_cell->d.pair.r=NULL;
		
		next_exp=nl_read_exp(fp);
		if(next_exp!=NULL){
			list_cell->d.pair.r=nl_val_malloc(PAIR);
			list_cell=list_cell->d.pair.r;
		}
	}
	
	//return a reference to the first list element
	return ret;
}

//TODO: read an evaluation

//read a symbol (just a string with a wrapper) (with a rapper? drop them beats man)
nl_val *nl_read_symbol(FILE *fp){
	nl_val *ret=NULL;
	
//	char c=getc(fp);
//	if(!isalpha(c)){
//		fprintf(stderr,"Err: symbol didn't start with alpha; WHAT DID YOU DO? (started with \'%c\')\n",c);
//		return ret;
//	}
//	ungetc(c,fp);
	
	char c;
	
	//allocate a symbol for the return
	ret=nl_val_malloc(SYMBOL);
	
	//allocate a byte array for the name
	nl_val *name=nl_val_malloc(ARRAY);
	
	c=getc(fp);
	
	//read until whitespace or list-termination character
	while(!nl_is_whitespace(c) && c!=')'){
		nl_val *ar_entry=nl_val_malloc(BYTE);
		ar_entry->d.byte.v=c;
		nl_array_push(name,ar_entry);
		
		c=getc(fp);
	}
	
	if(c==')'){
		ungetc(c,fp);
	}
	
	//encapsulate the name string in a symbol
	ret->d.sym.name=name;
	return ret;
}

//read an expression from the given input stream
nl_val *nl_read_exp(FILE *fp){
	//initialize the return to null
	nl_val *ret=NULL;
	
	//skip past any whitespace any time we go to read an expression
	nl_skip_whitespace(fp);
	
	//read a character from the given file
	char c=getc(fp);
	
	//if it starts with a digit or '.' then it's a number
	if(isdigit(c) || (c=='.')){
		ungetc(c,fp);
		ret=nl_read_num(fp);
	//if it starts with a quote read a string (byte array)
	}else if(c=='"'){
		ungetc(c,fp);
		ret=nl_read_string(fp);
	//if it starts with a single quote, read a character (byte)
	}else if(c=='\''){
		ungetc(c,fp);
		ret=nl_read_char(fp);
	//if it starts with a ( read a compound expression (a list of expressions)
	}else if(c=='('){
		ungetc(c,fp);
		ret=nl_read_exp_list(fp);
	//an empty list is just a NULL value, so leave ret as NULL
	}else if(c==')'){
		
	//TODO: if it starts with a $ read an evaluation
	}else if(c=='$'){
//		ungetc(c,fp);
//		ret=nl_read_evaluation(fp);
	//if it starts with a letter read a symbol
//	}else if(isalpha(c)){
	//if it starts with anything not already handled read a symbol
	}else{
		ungetc(c,fp);
		ret=nl_read_symbol(fp);
		//TODO: check if this is a keyword (maybe not in this function, maybe in eval)
//	}else{
//		fprintf(stderr,"Err: invalid symbol at start of expression, \'%c\' (will start reading next expression)\n",c);
	}
	
	//ignore everything until the next whitespace
//	while(!nl_is_whitespace(c)){
//		c=getc(fp);
//	}
	
	return ret;
}

void nl_out(FILE *fp, nl_val *exp){
	if(exp==NULL){
		return;
	}
	
	switch(exp->t){
		case BYTE:
			if(exp->d.byte.v<2){
				fprintf(fp,"%i",exp->d.byte.v);
			}else{
				fprintf(fp,"%c",exp->d.byte.v);
			}
			break;
		case NUM:
			fprintf(fp,"%lli/%llu",exp->d.num.n,exp->d.num.d);
			break;
		case PAIR:
			fprintf(fp,"(");
			nl_out(fp,exp->d.pair.f);
			fprintf(fp," . ");
			nl_out(fp,exp->d.pair.r);
			fprintf(fp,")");
			break;
		case ARRAY:
			fprintf(fp,"[ ");
			{
				unsigned int n;
				for(n=0;n<(exp->d.array.size);n++){
					nl_out(fp,(exp->d.array.v[n]));
					fprintf(fp," ");
				}
			}
			fprintf(fp,"]");
			break;
		case PRI:
			fprintf(fp,"<primitive procedure>");
			break;
		case SUB:
			fprintf(fp,"<closure/subroutine>");
			break;
		case SYMBOL:
			fprintf(fp,"<symbol ");
			if(exp->d.sym.name!=NULL){
				unsigned int n;
				for(n=0;n<(exp->d.sym.name->d.array.size);n++){
					nl_out(fp,(exp->d.sym.name->d.array.v[n]));
				}
			}
			fprintf(fp,">");
			break;
		default:
			fprintf(fp,"Err: Unknown type");
			break;
	}
}

//END I/O SUBROUTINES ---------------------------------------------------------------------------------------------

//runtime!
int main(int argc, char *argv[]){
	printf("neulang version %s, compiled on %s %s\n",VERSION,__DATE__,__TIME__);
	
	//create the global environment
	nl_env_frame *global_env=nl_env_frame_malloc(NULL);
	
	char end_program=FALSE;
	while(!end_program){
		printf("nl >> ");
		
		//read an expression in
		nl_val *exp=nl_read_exp(stdin);
		
//		printf("\n");
		
		//evalute the expression in the global environment
		nl_val *result=nl_eval(exp,global_env);
		
		//TODO: should expressions be free-d in nl_eval?
		//this way (freeing separately) could cause double-frees if the result is the same as the exp (self-evaluating expressions)
		
		//free the original expression
//		nl_val_free(exp);
		
		//TODO: remove this, it's just for debugging
		nl_val *copy=nl_val_cp(result);
		nl_out(stdout,copy);
		nl_val_free(copy);
		printf("\n");
		
		//output (print) the result of evaluation
		nl_out(stdout,result);
		
		//free the resulting expression
		nl_val_free(result);
		
		//pretty formatting
		printf("\n");
		
		//TODO: remove this, it's just for debugging
		end_program=TRUE;
		
		//loop (completing the REPL)
	}
	
	//de-allocate the global environment
	nl_env_frame_free(global_env);
	
	//play nicely on *nix
	return 0;
}

