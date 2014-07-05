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
	
	//count the references to this value
	unsigned int ref;
	
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
			//type this symbol is bound to (SYMBOL for unbound)
			nl_type t;
			
			//the string which has the real name
			nl_val *name;
		} sym;
		
		//evaluation (variable lookup)
		struct {
			nl_val *sym;
		} eval;
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
nl_val *nl_eval(nl_val *exp, nl_env_frame *env);
int nl_val_cmp(const nl_val *v_a, const nl_val *v_b);
//END FORWARD DECLARATIONS ----------------------------------------------------------------------------------------


//BEGIN GLOBAL DATA -----------------------------------------------------------------------------------------------
char end_program;
//END GLOBAL DATA -------------------------------------------------------------------------------------------------

//BEGIN MEMORY MANAGEMENT SUBROUTINES -----------------------------------------------------------------------------

//TODO: fuck it, just reference count the damn thing; I don't even care anymore

//allocate a value, and initialize it so that we're not doing anything too crazy
nl_val *nl_val_malloc(nl_type t){
	nl_val *ret=(nl_val*)(malloc(sizeof(nl_val)));
	ret->t=t;
	ret->cnst=TRUE;
	ret->ref=1;
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
//			ret->d.array.t=BYTE;
			ret->d.array.v=NULL;
			ret->d.array.size=0;
			ret->d.array.stored_size=0;
			break;
		case PRI:
			ret->d.pri.function=NULL;
			break;
		case SUB:
			ret->d.sub.t=NUM;
			ret->d.sub.body=NULL;
			ret->d.sub.env=NULL;
			break;
		case SYMBOL:
			ret->d.sym.t=SYMBOL;
			ret->d.sym.name=NULL;
			break;
		case EVALUATION:
			ret->d.eval.sym=NULL;
			break;
		default:
			break;
	}
	
	return ret;
}

//NOTE: reference decrementing is handled here as well
//free a value; this recursively frees complex data types
void nl_val_free(nl_val *exp){
	if(exp==NULL){
		return;
	}
	
	//decrease references on this object
	exp->ref--;
	//if there are still references left then don't free it quite yet
	if(exp->ref>0){
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
		//for an evaluation free the symbol
		case EVALUATION:
			nl_val_free(exp->d.eval.sym);
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
			ret->d.sym.t=v->d.sym.t;
			ret->d.sym.name=nl_val_cp(v->d.sym.name);
			break;
		//recurse to copy symbol
		case EVALUATION:
			ret->d.eval.sym=nl_val_cp(v->d.eval.sym);
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

//bind the given symbol to the given value in the given environment frame
//note that we do NOT change anything in the above scopes; this preserves referential transparency
void nl_bind(nl_val *symbol, nl_val *value, nl_env_frame *env){
	//can't bind to a null environment
	if(env==NULL){
		return;
	}
	
	char found=FALSE;
	
	//look through the symbols
	int n;
	for(n=0;n<(env->symbol_array->d.array.size);n++){
		//if we found this symbol
		if(nl_val_cmp(symbol,env->symbol_array->d.array.v[n])==0){
			//if this symbol was already bound and the type we're trying to re-bind has a different type, don't bind!
			if((value!=NULL) && (value->t!=(env->symbol_array->d.array.v[n]->d.sym.t))){
				fprintf(stderr,"Err: re-binding ");
				nl_out(stderr,symbol);
				fprintf(stderr," to value of wrong type (type %i != type %i) (symbol value unchanged)\n",value->t,env->symbol_array->d.array.v[n]->d.sym.t);
				nl_val_free(value);
				
//				found=TRUE;
//				break;
				return;
			}
			
			//update the value (removing a reference to the old value)
			nl_val_free(env->value_array->d.array.v[n]);
			env->value_array->d.array.v[n]=value;
			
			//this is a new reference to that value
			if(value!=NULL){
				value->ref++;
			}
			
			//note that the symbol is already stored
			//it will get free'd later by the calling code, we'll keep the existing version
			
			found=TRUE;
			break;
		}
	}
	
	//if we didn't find this symbol in the environment, then go ahead and make it now
	if(!found){
		//bind symbol
		nl_array_push(env->symbol_array,symbol);
		symbol->ref++;
		
		nl_array_push(env->value_array,value);
		if(value!=NULL){
			//give the symbol the type that the bound value has (effectively inferring typing at runtime)
			//note that NULL is a generic value and doesn't change the type; (a value initially bound to NULL will be forever a symbol)
			value->ref++;
			symbol->d.sym.t=value->t;
		}
	}
}

//look up the symbol in the given environment frame
//note that this WILL go up to higher scopes, if there are any
nl_val *nl_lookup(nl_val *symbol, nl_env_frame *env){
	//a null environment can't contain anything
	if(env==NULL){
		fprintf(stderr,"Err: unbound symbol ");
		nl_out(stderr,symbol);
		fprintf(stderr,"\n");
		return NULL;
	}
	
	//look through the symbols
	int n;
	for(n=0;n<(env->symbol_array->d.array.size);n++){
		//if we found this symbol
		if(nl_val_cmp(symbol,env->symbol_array->d.array.v[n])==0){
			//return the associated value
			return env->value_array->d.array.v[n];
		}
	}
	
	//if we got here and didn't return, then this symbol wasn't bound in the current environment
	//so look up (to a higher scope)
	return nl_lookup(symbol,env->up_scope);
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
		case EVALUATION:
			return nl_val_cmp(v_a->d.eval.sym,v_b->d.eval.sym);
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

//evaluate all the elements in a list, replacing them with their evaluations
void nl_eval_elements(nl_val *list, nl_env_frame *env){
	while((list!=NULL) && (list->t==PAIR)){
		nl_val *ev_result=nl_eval(list->d.pair.f,env);
		nl_val_free(list->d.pair.f);
		list->d.pair.f=ev_result;
		
		list=list->d.pair.r;
	}
}

//apply a given subroutine to its arguments
nl_val *nl_apply(nl_val *sub, nl_val *arguments, nl_env_frame *env){
	nl_eval_elements(arguments,env);
	//TODO: bind arguments to internal sub symbols, substitute the body in, and actually do the apply
	
	//TODO: remove this, it's just for debugging
	return arguments;
}

//evaluate an if statement with the given arguments
nl_val *nl_eval_if(nl_val *arguments, nl_env_frame *env){
	//return value
	nl_val *ret=NULL;
	
	//allocate internal keywords
	nl_val *else_keyword=nl_sym_from_c_str("else");
	
	if((arguments==NULL) || (arguments->t!=PAIR)){
		fprintf(stderr,"Err: invalid condition given to if statement\n");
		nl_val_free(else_keyword);
		return NULL;
	}
	
	//the condition is the first argument
	nl_val *cond=nl_eval(arguments->d.pair.f,env);
	
	//replace the expression with its evaluation moving forward
	arguments->d.pair.f=cond;
	
	//a holder for temporary results
	//(these replace arguments as we move through the expression) (reference counting keeps memory handled)
	nl_val *tmp_result=NULL;
	
	//check if the condition is true
	if((cond!=NULL) && (((cond->t==BYTE) && (cond->d.byte.v!=0)) || ((cond->t==NUM) && (cond->d.num.n!=0)))){
		printf("nl_eval_if debug 0, hitting true case...\n");
		
		//advance past the condition itself
		arguments=arguments->d.pair.r;
		
		//true eval
		while((arguments!=NULL) && (arguments->t==PAIR)){
			//if we hit an else statement then break out (skipping over false case)
			if((arguments->d.pair.f->t==SYMBOL) && (nl_val_cmp(arguments->d.pair.f,else_keyword)==0)){
				printf("nl_eval_if debug 1, stopping at else statement...\n");
				break;
			}
			
			tmp_result=nl_eval(arguments->d.pair.f,env);
			arguments->d.pair.f=tmp_result;
			arguments=arguments->d.pair.r;
		}
		
		if(tmp_result!=NULL){
			tmp_result->ref++;
		}
		
		printf("nl_eval_if debug 2, got a result of ");
		nl_out(stdout,tmp_result);
		printf("\n");
		
		//when we're done with the list then break out
		ret=tmp_result;
	}else if(cond!=NULL){
		printf("nl_eval_if debug 3, hitting false case...\n");
		
		//false eval
		
		//skip over true case
		while((arguments!=NULL) && (arguments->t==PAIR)){
			//if we hit an else statement then break out
			if((arguments->d.pair.f->t==SYMBOL) && (nl_val_cmp(arguments->d.pair.f,else_keyword)==0)){
				break;
			}
			
			//otherwise just skip to the next list element
			arguments=arguments->d.pair.r;
		}
		
		//if we actually hit an else statement just then (rather than the list end)
		if((arguments!=NULL) && (nl_val_cmp(arguments->d.pair.f,else_keyword)==0)){
			//evaluate false case
			while((arguments!=NULL) && (arguments->t==PAIR)){
				tmp_result=nl_eval(arguments->d.pair.f,env);
				arguments->d.pair.f=tmp_result;
				arguments=arguments->d.pair.r;
			}
		}
		
		if(tmp_result!=NULL){
			tmp_result->ref++;
		}
		
		ret=tmp_result;
	}else{
		fprintf(stderr,"Err: if statement condition evaluated to NULL\n");
	}
	
	//free internal keywords and return
	nl_val_free(else_keyword);
	return ret;
}

//TODO: proper evaluation of keywords!
//evaluate a keyword expression (or primitive function, if keyword isn't found)
nl_val *nl_eval_keyword(nl_val *keyword_exp, nl_env_frame *env){
	nl_val *keyword=keyword_exp->d.pair.f;
	nl_val *arguments=keyword_exp->d.pair.r;
	
	nl_val *ret=NULL;
	
	//allocate keywords
	nl_val *if_keyword=nl_sym_from_c_str("if");
	nl_val *exit_keyword=nl_sym_from_c_str("exit");
	nl_val *lit_keyword=nl_sym_from_c_str("lit");
	//TODO: make let require a type argument (maybe the first argument is just its own symbol which represents the type?)
	nl_val *let_keyword=nl_sym_from_c_str("let");
	
	//check for if statements
	if(nl_val_cmp(keyword,if_keyword)==0){
		//handle if statements
		ret=nl_eval_if(arguments,env);
		
	//check for exits
	}else if(nl_val_cmp(keyword,exit_keyword)==0){
		end_program=TRUE;
		ret=NULL;
		
	//check for literals (equivilent to scheme quote)
	}else if(nl_val_cmp(keyword,lit_keyword)==0){
		//if there was only one argument, just return that
		if((arguments!=NULL) && (arguments->t==PAIR) && (arguments->d.pair.r==NULL)){
			arguments->d.pair.f->ref++;
			ret=arguments->d.pair.f;
		//if there was a list of multiple arguments, return all of them
		}else if(arguments!=NULL){
			arguments->ref++;
			ret=arguments;
		}
		
	//check for let statements (assignment operations)
	}else if(nl_val_cmp(keyword,let_keyword)==0){
		//if we got a symbol followed by something else, eval that thing and bind
		if((arguments!=NULL) && (arguments->t==PAIR) && (arguments->d.pair.f!=NULL) && (arguments->d.pair.f->t==SYMBOL) && (arguments->d.pair.r!=NULL) && (arguments->d.pair.r->t==PAIR)){
			nl_val *bound_value=nl_eval(arguments->d.pair.r->d.pair.f,env);
			nl_bind(arguments->d.pair.f,bound_value,env);
			
			//if this bind was unsuccessful (for example a type error) then re-set bound_value to NULL so we don't try to access it
			//(it was already free'd)
			if(nl_lookup(arguments->d.pair.f,env)!=bound_value){
				bound_value=NULL;
			}
			
			//return a copy of the value that was just bound (this is also sort of an internal test to ensure it was bound right)
			ret=nl_val_cp(nl_lookup(arguments->d.pair.f,env));
			
			//since what we just returned was a copy, the original won't be free'd by the calling code
			//so we're one reference too high at the moment
			if(bound_value!=NULL){
				bound_value->ref--;
			}
			
			//null-out the list elements we got rid of
			arguments->d.pair.r->d.pair.f=NULL;
		}else{
			fprintf(stderr,"Err: wrong syntax for let statement\n");
		}
	//TODO: check for all other keywords
	}else{
		//in the default case check for subroutines bound to this symbol
		nl_val *prim_sub=nl_lookup(keyword,env);
		if((prim_sub!=NULL) && (prim_sub->t==PRI)){
			nl_apply(prim_sub,arguments,env);
		}else{
			fprintf(stderr,"Err: unknown keyword ");
			nl_out(stderr,keyword);
			fprintf(stderr,"\n");
		}
	}
	
	
	//free keywords
	nl_val_free(if_keyword);
	nl_val_free(exit_keyword);
	nl_val_free(lit_keyword);
	nl_val_free(let_keyword);
	
	//free the original expression (frees the entire list, keyword, arguments, and all)
	//note that this list changed as we evaluated, and is likely not exactly what we started with
	//	^ that's a good thing, because what we started with was free'd in that case
	nl_val_free(keyword_exp);
	
	return ret;
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
			exp->ref++;
			ret=exp;
			break;
		//symbols are generally self-evaluating
		//in the case of primitive calls the pair evaluation checks for them as keywords, without calling out to eval
		//however some primitive constants evaluate to specific values (TRUE, FALSE, NULL)
		case SYMBOL:
			{
				nl_val *true_keyword=nl_sym_from_c_str("TRUE");
				nl_val *false_keyword=nl_sym_from_c_str("FALSE");
				nl_val *null_keyword=nl_sym_from_c_str("NULL");
				
				//TRUE keyword
				if(nl_val_cmp(exp,true_keyword)==0){
					ret=nl_val_malloc(BYTE);
					ret->d.byte.v=1;
				//FALSE keyword
				}else if(nl_val_cmp(exp,false_keyword)==0){
					ret=nl_val_malloc(BYTE);
					ret->d.byte.v=0;
				//NULL keyword
				}else if(nl_val_cmp(exp,null_keyword)==0){
					ret=NULL;
				}else{
					exp->ref++;
					ret=exp;
				}
				
				//free keyword symbols
				nl_val_free(true_keyword);
				nl_val_free(false_keyword);
				nl_val_free(null_keyword);
			}
			break;
		
		//complex cases
		//pairs eagerly evaluate then (if first argument isn't a keyword) call out to apply
		case PAIR:
			//make this check the first list entry, if it is a symbol then check it against keyword and primitive list
			if((exp->d.pair.f!=NULL) && (exp->d.pair.f->t==SYMBOL)){
				exp->ref++;
				ret=nl_eval_keyword(exp,env);
			//otherwise eagerly evaluate then call out to apply
			}else if(exp->d.pair.f!=NULL){
				//TODO: eager evaluation, call apply
				
			//null lists are self-evaluating (the empty list)
			}else{
				exp->ref++;
				ret=exp;
			}
			break;
		//evaluations look up value in the environment
		case EVALUATION:
			//look up the expression in the environment and return a copy of the result
			//the reason this is a copy is so that pointer-equality won't be true, and changing one var doesn't change another
			ret=nl_val_cp(nl_lookup(exp->d.eval.sym,env));
			break;
		//default self-evaluating
		default:
			exp->ref++;
			ret=exp;
			break;
	}
	
	//free the original expression (if it was self-evaluating it got a reference increment earlier so this is okay)
	nl_val_free(exp);
	
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
	
	//peek a character after that, for two-char tokens
	char next_c=getc(fp);
	ungetc(next_c,fp);
	
	//if it starts with a digit or '.' then it's a number, or if it starts with '-' followed by a digit
	if(isdigit(c) || (c=='.') || ((c=='-') && isdigit(next_c))){
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
		
	//the double-slash denotes a comment, as in gnu89 C
	}else if(c=='/' && next_c=='/'){
		//ignore everything until the next newline, then try to read again
		while(c!='\n'){
			c=getc(fp);
		}
		ret=nl_read_exp(fp);
	//if it starts with a $ read an evaluation (a symbol to be looked up)
	}else if(c=='$'){
		ret=nl_val_malloc(EVALUATION);
		ret->d.eval.sym=nl_read_symbol(fp);
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
		fprintf(fp,"NULL");
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
		case EVALUATION:
			fprintf(fp,"<evaluation ");
			if((exp->d.eval.sym!=NULL) && (exp->d.eval.sym->d.sym.name!=NULL)){
				unsigned int n;
				for(n=0;n<(exp->d.eval.sym->d.sym.name->d.array.size);n++){
					nl_out(fp,(exp->d.eval.sym->d.sym.name->d.array.v[n]));
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

//bind all primitive subroutines in the given environment frame
void nl_bind_stdlib(nl_env_frame *env){
	//TODO: write the standard library, bind it here
/*
	nl_bind(nl_sym_from_c_str("+"),nl_sym_from_c_str("+"),env);
	nl_bind(nl_sym_from_c_str("-"),nl_sym_from_c_str("-"),env);
	nl_bind(nl_sym_from_c_str("*"),nl_sym_from_c_str("*"),env);
	nl_bind(nl_sym_from_c_str("/"),nl_sym_from_c_str("/"),env);
	nl_bind(nl_sym_from_c_str("%"),nl_sym_from_c_str("%"),env);
	nl_bind(nl_sym_from_c_str(","),nl_sym_from_c_str(","),env);
*/
}

//runtime!
int main(int argc, char *argv[]){
	printf("neulang version %s, compiled on %s %s\n",VERSION,__DATE__,__TIME__);
	
	FILE *fp=stdin;
	
	//if we were given a file, open it
	if(argc>1){
		fp=fopen(argv[1],"r");
		if(fp==NULL){
			fprintf(stderr,"Err: Could not open input file \"%s\"\n",argv[1]);
			return 1;
		}
	}
	
	//create the global environment
	nl_env_frame *global_env=nl_env_frame_malloc(NULL);
	
	//bind the standard library functions in the global environment
	nl_bind_stdlib(global_env);
	
	end_program=FALSE;
	while(!end_program){
		printf("nl >> ");
		
		//read an expression in
		nl_val *exp=nl_read_exp(fp);
		
//		printf("\n");
		
		//evalute the expression in the global environment
		nl_val *result=nl_eval(exp,global_env);
		
		//expressions should be free-d in nl_eval, unless they are self-evaluating
		
		//output (print) the result of evaluation
		nl_out(stdout,result);
		
		//free the resulting expression
		nl_val_free(result);
		
		//pretty formatting
		printf("\n");
		
		//loop (completing the REPL)
	}
	
	//de-allocate the global environment
	nl_env_frame_free(global_env);
	
	//if we were reading from a file then close that file
	if((fp!=NULL) && (fp!=stdin)){
		fclose(fp);
	}
	
	//play nicely on *nix
	return 0;
}

