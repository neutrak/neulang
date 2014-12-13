//a bootstrap interpreter for the new neulang, the first which changes to a less lisp-like syntax

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

//BEGIN DATA STRUCTURES -------------------------------------------------------------------------------------------
//NOTE: this also includes forward declarations for all functions (including standard library ones) and global constants
#include "nl_structures.h"
//END DATA STRUCTURES ---------------------------------------------------------------------------------------------

//BEGIN GLOBAL DATA -----------------------------------------------------------------------------------------------

//bookkeeping
//char end_program;
//unsigned int line_number;

//keywords
nl_val *true_keyword;
nl_val *false_keyword;
nl_val *null_keyword;

nl_val *pair_keyword;
nl_val *f_keyword;
nl_val *r_keyword;
nl_val *if_keyword;
nl_val *else_keyword;
nl_val *and_keyword;
nl_val *or_keyword;
nl_val *not_keyword;
nl_val *xor_keyword;
nl_val *exit_keyword;
nl_val *lit_keyword;
nl_val *let_keyword;
nl_val *sub_keyword;
nl_val *begin_keyword;
nl_val *recur_keyword;
nl_val *return_keyword;
nl_val *while_keyword;
nl_val *for_keyword;
nl_val *after_keyword;
nl_val *array_keyword;
nl_val *list_keyword;
nl_val *struct_keyword;


//END GLOBAL DATA -------------------------------------------------------------------------------------------------

//error message function
void nl_err(const nl_val *v, const char *msg, char output){
	if(v!=nl_null){
		if(output){
			fprintf(stderr,"Err [line %u]: %s ",v->line,msg);
			fprintf(stderr,"(relevant value might be ");
			nl_out(stderr,v);
			fprintf(stderr,")\n");
		}else{
			fprintf(stderr,"Err [line %u]: %s\n",v->line,msg);
		}
	}else{
		//this output case will always list NULL as the value
		//but since the error value might be null it's important to let the user know that
		//note the line number is global, rather that the value's line number (since nl_null is only allocated once)
		// ^ that is the key difference and the reason for two cases in this function
		if(output){
			fprintf(stderr,"Err [line %u]: %s ",line_number,msg);
			fprintf(stderr,"(relevant value might be ");
			nl_out(stderr,v);
			fprintf(stderr,")\n");
		}else{
			fprintf(stderr,"Err [line %u]: %s\n",line_number,msg);
		}
	}
}

//returns a C string consisting of the name of the given type
const char *nl_type_name(nl_type t){
	switch(t){
		case BYTE:
			return "BYTE";
			break;
		case NUM:
			return "NUM";
			break;
		case PAIR:
			return "PAIR";
			break;
		case ARRAY:
			return "ARRAY";
			break;
		case PRI:
			return "PRI";
			break;
		case SUB:
			return "SUB";
			break;
		case STRUCT:
			return "STRUCT";
			break;
		case SYMBOL:
			return "SYMBOL";
			break;
		case EVALUATION:
			return "EVALUATION";
			break;
		case NL_NULL:
			return "NULL";
			break;
		default:
			break;
	}
	return "UNKNOWN";
}

//BEGIN MEMORY MANAGEMENT SUBROUTINES -----------------------------------------------------------------------------

//fuck it, just reference count the damn thing; I don't even care anymore

//allocate a value, and initialize it so that we're not doing anything too crazy
nl_val *nl_val_malloc(nl_type t){
	nl_val *ret=(nl_val*)(malloc(sizeof(nl_val)));
	if(ret==NULL){
		ERR_EXIT(nl_null,"could not malloc a value (out of memory?)",FALSE);
		
		//even in non-strict mode a failed malloc is a hard fail and exit
		exit(1);
	}
	
	ret->t=t;
	ret->cnst=TRUE;
	ret->ref=1;
	ret->line=line_number;
	switch(ret->t){
		case BYTE:
			ret->d.byte.v=0;
			break;
		case NUM:
			ret->d.num.n=0;
			ret->d.num.d=1;
			break;
		case PAIR:
			ret->d.pair.f=nl_null;
			ret->d.pair.r=nl_null;
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
//			ret->d.sub.t=NUM;
			ret->d.sub.args=nl_null;
			ret->d.sub.body=nl_null;
			ret->d.sub.env=NULL;
			break;
		case STRUCT:
//			ret->d.nl_struct.env=NULL;
			ret->d.nl_struct.env=nl_env_frame_malloc(NULL);
			break;
		case SYMBOL:
			ret->d.sym.t=SYMBOL;
			ret->d.sym.name=nl_null;
			break;
		case EVALUATION:
			ret->d.eval.sym=nl_null;
			break;
		case NL_NULL:
			//NOTE: there should be only one, global, NULL, named nl_null
			break;
		default:
			break;
	}
	
	return ret;
}

//NOTE: reference decrementing is handled here as well
//free a value; this recursively frees complex data types
//returns TRUE if successful, FALSE if there are still references
char nl_val_free(nl_val *exp){
	if(exp==NULL){
		return TRUE;
	}else if(exp==nl_null){
		return TRUE;
	}
	
	//decrease references on this object
	exp->ref--;
	//if there are still references left then don't free it quite yet
	if(exp->ref>0){
/*
#ifdef _DEBUG
		printf("nl_val_free debug 0, NOT freeing ");
		nl_out(stdout,exp);
		printf(" (ref=%i)\n",exp->ref);
#endif
*/
		return FALSE;
	}
	
/*
#ifdef _DEBUG
	printf("nl_val_free debug 1, actually freeing ");
	nl_out(stdout,exp);
	printf(" (no references left)\n");
#endif
*/
	
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
//					nl_val_free(&(exp->d.array.v[n]));
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
			nl_val_free(exp->d.sub.args);
			nl_val_free(exp->d.sub.body);
			//if this was chained in an application environment then it needs another free (had an extra reference)
//			if(exp->d.sub.env->shared==FALSE){
//				nl_env_frame_free(exp->d.sub.env);
//			}
			nl_env_frame_free(exp->d.sub.env);
			break;
		case STRUCT:
			nl_env_frame_free(exp->d.nl_struct.env);
			break;
		//symbols need a recursive call for the string that's their name
		case SYMBOL:
			nl_val_free(exp->d.sym.name);
			break;
		//for an evaluation free the symbol
		case EVALUATION:
			nl_val_free(exp->d.eval.sym);
			break;
		//nothing to free for a null
		//(note we should never actually get here because there was a check at the top)
		//(NULL is not reference counted and must be manually free()'d)
		case NL_NULL:
			break;
		default:
			break;
	}
	
	free(exp);
	return TRUE;
}

//copy a value data-wise into new memory, without changing the original
nl_val *nl_val_cp(nl_val *v){
	//a null value is copied as a null value
	if(v==nl_null){
		return nl_null;
	}
	
	nl_val *ret=nl_null;
	
	//if we're not doing a data-wise copy don't allocate new memory
	//(primitive subroutines and closures are copied pointer-wise)
	if((v->t!=PRI) && (v->t!=SUB)){
		ret=nl_val_malloc(v->t);
		//copy the line number too; if we're copying it then the user didn't just enter it
		ret->line=v->line;
	}
	
	switch(v->t){
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
//					nl_array_push(ret,nl_val_cp(&(v->d.array.v[n])));
					nl_array_push(ret,nl_val_cp(v->d.array.v[n]));
				}
			}
			break;
		//TODO: should we recurse and copy body and environment for closures? (should environment be constant between copies?)
		//pointer-wise copies; primitive procedures and closure subroutines
		case PRI:
		case SUB:
			//this is a direct pointer copy; we increment the references just to keep track of everything
			v->ref++;
			ret=v;
			break;
		//in a struct copy all the bindings
		case STRUCT:
			//the malloc created a new trie which isn't going to be used (because we're making a copy) so we free it here
			nl_trie_free(ret->d.nl_struct.env->trie);
			ret->d.nl_struct.env->trie=nl_trie_cp(v->d.nl_struct.env->trie);
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
	
	//environments are shared by default
	ret->shared=TRUE;
	
	ret->trie=nl_trie_malloc();
	
	//the environment above this (NULL for global)
	ret->up_scope=up_scope;
	
	return ret;
}

//free an environment frame
void nl_env_frame_free(nl_env_frame *env){
	//null is already free'd; do nothing
	if(env==NULL){
		return;
	}
	
	//free the trie that binds symbols to values
	nl_trie_free(env->trie);
	
	//note that we do NOT free the above environment here; if you want to do that do it elsewhere
	
	free(env);
}

//bind the given symbol to the given value in the given environment frame
//note that we do NOT change anything in the above scopes; this preserves referential transparency
//^ there is one exception to that, which is for new vars in a non-shared env (an application frame, aka call stack entry)
//returns TRUE for success, FALSE for failure
char nl_bind(nl_val *symbol, nl_val *value, nl_env_frame *env){
	//can't bind to a null environment
	if(env==NULL){
		ERR(symbol,"cannot bind to a null environment",TRUE);
		return FALSE;
/*
#ifdef _DEBUG
	}else if(env->shared==FALSE){
		printf("nl_bind debug 0, got a non-shared environment while binding symbol ");
		nl_out(stdout,symbol);
		printf("...\n");
#endif
*/
	}
	
	if(symbol==nl_null){
		ERR_EXIT(symbol,"cannot bind a null symbol",TRUE);
		return FALSE;
	}
	if(symbol->t!=SYMBOL){
		ERR_EXIT(symbol,"cannot bind a non-symbol type",TRUE);
		return FALSE;
	}
	
	char *symbol_c_str=c_str_from_nl_str(symbol->d.sym.name);
	
#ifdef _DEBUG
//	printf("nl_bind debug 1, binding %s...\n",symbol_c_str);
#endif
	
	//if this was an application environment then also bind it in a higher scope, updating the value there
	if(env->shared==FALSE){
#ifdef _DEBUG
//		printf("nl_bind debug 1.25, recursing...\n");
#endif
		
		//refcount the symbol because it will be free'd in the recursive call
		//since for a trie we don't need to keep it around
//		symbol->ref++;
		if(!nl_bind(symbol,value,env->up_scope)){
			//could not bind in up_scope
		}
	}
	
#ifdef _DEBUG
//	printf("nl_bind debug 1.5, calling out to nl_trie_add_node...\n");
//	printf("env->trie=%p, env->trie->t=%i\n",env->trie,env->trie->t);
//	printf("symbol_c_str=%p, strlen(symbol_c_str)=%lu\n",symbol_c_str,strlen(symbol_c_str));
//	printf("symbol_c_str[0]=%c\n",symbol_c_str[0]);
//	printf("value=%p, value->t=%i\n",value,value->t);
//	printf("----------------------------------------------------\n");
#endif
	
	//bind this symbol in the internal trie structure
	//note that the add_node handles reference counting for value
//	char ret=nl_trie_add_node(env->trie,symbol_c_str,0,strlen(symbol_c_str),value);
	char ret=nl_trie_add_node(env->trie,symbol_c_str,0,symbol->d.sym.name->d.array.size,value);
	
#ifdef _DEBUG
//	printf("nl_bind debug 2, returned from nl_trie_add_node with ret %s...\n",ret?"TRUE":"FALSE");
#endif
	
	//free memory that will no longer be used
	free(symbol_c_str);
//	nl_val_free(symbol);
//	nl_val_free(value);
	
	if(value!=nl_null){
		//once bound values are no longer constant
		value->cnst=FALSE;
	}
	
#ifdef _DEBUG
//	printf("nl_bind debug 3, finished manging memory, returning...\n");
#endif
	
	return ret;
}

//TODO: fix this lookup function; NULL should be an allowed value!!! (which means we need another way to indicate success/failure)

//look up the symbol in the given environment frame
//note that this WILL go up to higher scopes, if there are any
nl_val *nl_lookup(nl_val *symbol, nl_env_frame *env){
	//a null environment can't contain anything
	if(env==NULL){
		ERR_EXIT(symbol,"unbound symbol",TRUE);
		return nl_null;
	}
	if((symbol==nl_null) || (symbol->t!=SYMBOL)){
		ERR_EXIT(symbol,"NULL or invalid type given to nl_lookup",TRUE);
		return nl_null;
	}
	
	//look in the trie for this symbol
	char *symbol_c_str=c_str_from_nl_str(symbol->d.sym.name);
	
	char success=FALSE;
//	nl_val *ret=nl_trie_match(env->trie,symbol_c_str,0,strlen(symbol_c_str),&success,TRUE);
	nl_val *ret=nl_trie_match(env->trie,symbol_c_str,0,symbol->d.sym.name->d.array.size,&success,TRUE);
	free(symbol_c_str);
	
	//if we found it, return!
	if(success){
		return ret;
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

//make a c string from a neulang string (you must free this yourself!)
//returns NULL if not given a valid nl string
char *c_str_from_nl_str(nl_val *nl_str){
	if((nl_str==nl_null) || (nl_str->t!=ARRAY)){
		return NULL;
	}
	
	//the +1 stores the C string null termination character
	int buf_size=sizeof(char)*((nl_str->d.array.size)+1);
	char *c_str=malloc(buf_size);
	bzero(c_str,buf_size);
	
	//copy in the neulang string
	int n;
	for(n=0;n<(nl_str->d.array.size);n++){
		if((nl_str->d.array.v[n]!=nl_null) && (nl_str->d.array.v[n]->t==BYTE)){
			c_str[n]=nl_str->d.array.v[n]->d.byte.v;
		}
	}
	//always null-terminate just in case
	c_str[buf_size-1]='\0';
	
#ifdef _DEBUG
//	printf("c_str_from_nl_str debug 0, made c_str %s\n",c_str);
#endif
	
	//return the dynamically allocated memory
	return c_str;
}

//make a neulang value out of a primitve function so we can bind it
nl_val *nl_primitive_wrap(nl_val *(*function)(nl_val *arglist)){
	nl_val *ret=nl_val_malloc(PRI);
	ret->d.pri.function=function;
	return ret;
}

//END MEMORY MANAGEMENT SUBROUTINES -------------------------------------------------------------------------------

//BEGIN EVALUTION SUBROUTINES -------------------------------------------------------------------------------------

//is this neulang value TRUE? (true values are nonzero numbers and nonzero bytes)
char nl_is_true(nl_val *v){
	if((((v->t==BYTE) && (v->d.byte.v!=0)) || ((v->t==NUM) && (v->d.num.n!=0)))){
		return TRUE;
	}
	return FALSE;
}

//replace all instances of old with new in the given list (new CANNOT be a list itself, and old_val cannot be NULL)
//note that this is recursive and will also descend into lists within the list (except if that list starts with sub, while, or for)
//note also that this manages memory, and will remove references when needed
//returns TRUE if replacements were made, else FALSE
char nl_substitute_elements_skipsub(nl_val *list, nl_val *old_val, nl_val *new_val){
	if(old_val==nl_null){
		ERR(list,"cannot substitute NULLs in a list; that would just be silly",TRUE);
		return FALSE;
	}
	
	char ret=FALSE;
	
	while((list!=nl_null) && (list->t==PAIR)){
		//if we hit a nested list
		if((list->t==PAIR) && (list->d.pair.f->t==PAIR)){
			//if this is not a sub, while, or for statement then recurse
			nl_val *list_start=list->d.pair.f->d.pair.f;
			if((list_start->t==SYMBOL) && ((nl_val_cmp(list_start,sub_keyword)==0) || (nl_val_cmp(list_start,while_keyword)==0) || (nl_val_cmp(list_start,for_keyword)==0))){
				//skip sub, while, and for statements
			}else{
				//recurse! recurse!
				if(nl_substitute_elements_skipsub(list->d.pair.f,old_val,new_val)){
					ret=TRUE;
				}
			}
		//if we found the thing to replace
		}else if((list->d.pair.f->t==old_val->t) && (nl_val_cmp(old_val,list->d.pair.f)==0)){
/*
#ifdef _DEBUG
			printf("nl_substitute_elements debug 1, substituting ");
			nl_out(stdout,old_val);
			printf(" for ");
			nl_out(stdout,new_val);
			printf("\n");
#endif
*/
			//tell the caller we made a replacement
			ret=TRUE;
			
			//replace it and clean up memory
			nl_val_free(list->d.pair.f);
			
			//copies are not needed here because when evaluated in a body or begin the elements will get copied there anyway
//			list->d.pair.f=nl_val_cp(new_val);
			list->d.pair.f=new_val;
		}
		
		list=list->d.pair.r;
	}
	return ret;
}

//returns the number of times value occurred in the list (recursively checks sub-lists)
//(value CANNOT be a list itself, and also cannot be NULL)
int nl_list_occur(nl_val *list, nl_val *value){
	int ret=0;
	if(value==nl_null){
		return 0;
	}
	
	//while we're not at list end
	while(list->t==PAIR){
		if(list->d.pair.f!=nl_null){
			//if we found a sub-list recurse and add that result to the count
			if(list->d.pair.f->t==PAIR){
				ret+=nl_list_occur(list->d.pair.f,value);
			//if we found an instance of the value in the list, add it to the total
			}else if((list->d.pair.f->t==value->t) && (nl_val_cmp(value,list->d.pair.f)==0)){
				ret++;
			}
		}
		list=list->d.pair.r;
	}
	return ret;
}

//evaluate all the elements in a list, replacing them with their evaluations
void nl_eval_elements(nl_val *list, nl_env_frame *env){
	while(list->t==PAIR){
//		char early_ret=FALSE;
//		nl_val *ev_result=nl_eval(list->d.pair.f,env,FALSE,&early_ret);
		nl_val *ev_result=nl_eval(list->d.pair.f,env,FALSE,NULL);
		
		//the calling code will handle freeing these, don't worry about it here
		//gc anything that wasn't self-evaluating
//		if(ev_result!=(list->d.pair.f)){
//			nl_val_free(list->d.pair.f);
//		}
		list->d.pair.f=ev_result;
		
		list=list->d.pair.r;
	}
}

//evaluate the list of values in order, returning the evaluation of the last statement only
//early_ret MUST ALWAYS either be NULL or be dynamically allocated; static allocation breaks the compiler's TCO ability (save stack)
nl_val *nl_eval_sequence(nl_val *body, nl_env_frame *env, char *early_ret){
	nl_val *ret=nl_null;
	
	//if given a NULL then just don't pass the early_ret result up, no worries
	if(early_ret!=NULL){
		//this did NOT return early (yet)
		(*early_ret)=FALSE;
	}
	
	//whether or not this is the last statement in the body
	char on_last_exp=FALSE;
	
/*
#ifdef _DEBUG
	printf("nl_eval sequence debug -1, trying to evaluate ");
	nl_out(stdout,body);
	if(body!=nl_null){
		printf(" with %i references",body->ref);
	}
	printf("\n");
#endif
*/
	nl_val *body_start=body;
	
	nl_val *to_eval=nl_null;
	while(body->t==PAIR){
		//if the next statement is the last, set that for the next loop iteration
		if(body->d.pair.r==nl_null){
			on_last_exp=TRUE;
		}

		//this doesn't work because it'll substitute results in as body statements and fail on recursion or later calls
//		to_eval=body->d.pair.f;
//		to_eval->ref++;
		
		// make a copy so as not to ever change the actual subroutine body statements themselves
		to_eval=nl_val_cp(body->d.pair.f);
		//increment the references because this (to_eval) is a new reference and nl_eval will free it before we can if it's self-evaluating
		if(to_eval!=nl_null){
			to_eval->ref++;
		}
		
		//note that early_ret handles nested returns (such as a return within an if statement)
		//if we hit the "return" keyword then go ahead and treat this as the last expression (even if it wasn't properly)
		if((to_eval->t==PAIR) && (to_eval->d.pair.f->t==SYMBOL) && (nl_val_cmp(return_keyword,to_eval->d.pair.f)==0)){
			on_last_exp=TRUE;
			
			//returns evaluate pretty much just like begin statements
/*
#ifdef _DEBUG
			printf("nl_eval_sequence debug 0, got a return statement with %i references\n",to_eval->ref);
#endif
*/
			//this returned early, so signal the calling code and let it know that
			if(early_ret!=NULL){
				(*early_ret)=TRUE;
			}
		}
		
		//if we're not going into a tailcall then keep this expression around and free it when we're done
		if(!on_last_exp){
/*
#ifdef _DEBUG
			printf("nl_eval_sequence debug 0.4, calling nl_eval in non-tailcall, to_eval=");
			nl_out(stdout,to_eval);
			printf("\n");
#endif
*/
			//did this sub-expression return early?
			char *got_early_ret=malloc(sizeof(char));
			(*got_early_ret)=FALSE;
			
			//always set this to ret, so ret ends up with the last-evaluated thing
			ret=nl_eval(to_eval,env,on_last_exp,got_early_ret);
//			ret=nl_eval(to_eval,env,on_last_exp,NULL);
			
			//clean up our original expression (it got an extra reference if it was self-evaluating)
			nl_val_free(to_eval);
			
			//if the inner expression returned early then break the loop and return ret
			if((*got_early_ret)){
				//set the early_ret here to return upwards if we're within another eval_sequence call
				if(early_ret!=NULL){
					(*early_ret)=TRUE;
				}
				
				free(got_early_ret);
//				body=NULL; //this fails hard because we try a body=body->d.pair.r right after this
//				body=nl_null;
				break;
			}else{
				free(got_early_ret);
				//if we're not going to return this then we need to free the result since it will be unused
				nl_val_free(ret);
				ret=nl_null;
			}
/*
#ifdef _DEBUG
			printf("nl_eval_sequence debug 0.5, freed memory...\n");
#endif
*/
		
		//if we ARE going into a tailcall then clean up our extra references and return
		}else{
			nl_val_free(to_eval);
			nl_val_free(body_start);
			
/*
#ifdef _DEBUG
			printf("nl_eval_sequence debug 1, tailcalling into nl_eval with expression ");
			nl_out(stdout,to_eval);
			printf("\n");
#endif
*/
			//NOTE: C's TCO is broken if you pass a local var's reference as the early_ret argument to eval functions!
			
			//NOTE: this depends on C's own TCO behavior and so requires compilation with -O3 or -O2
			//early_ret MUST be passed through here because we get called within other statements and those need to know they returned early
			//(for example, an if within an if where the first statement in the if body is a return)
			return nl_eval(to_eval,env,on_last_exp,early_ret);
		}
		
		//go to the next body statement and eval again!
		body=body->d.pair.r;
	}
/*
#ifdef _DEBUG
	printf("nl_eval_sequence debug 2, finished with sequence, freeing body_start...\n");
#endif
*/
	nl_val_free(body_start);
	return ret;
}

//bind all the symbols to corresponding values in the given environment
//returns TRUE on success, FALSE on failure
char nl_bind_list(nl_val *symbols, nl_val *values, nl_env_frame *env){
	//if there was nothing to bind
	if((symbols->t==PAIR) && (symbols->d.pair.f==nl_null) && (symbols->d.pair.r==nl_null)){
		//we are successful at doing nothing (aren't you so proud?)
		return TRUE;
	}
	
	while((symbols->t==PAIR) && (values->t==PAIR)){
		//bind the argument to its associated symbol
		if((symbols->d.pair.f!=nl_null) && (!nl_bind(symbols->d.pair.f,values->d.pair.f,env))){
			return FALSE;
		}
		
		symbols=symbols->d.pair.r;
		values=values->d.pair.r;
	}
	
	//if there weren't as many of one list as the other then return with error
	if((symbols!=nl_null) || (values!=nl_null)){
		return FALSE;
	}
	return TRUE;
}

//apply a given subroutine to its arguments
//note that returns are handled in eval_sequence
//also note tailcalls never get here (they are handled by eval turning body into begin, and then by eval_sequence tailcalling back into eval)
nl_val *nl_apply(nl_val *sub, nl_val *arguments, char *early_ret){
	nl_val *ret=nl_null;
	
	if(!(sub->t==PRI || sub->t==SUB)){
		if(sub==nl_null){
			ERR_EXIT(sub,"cannot apply NULL!!!",FALSE);
		}else{
//			fprintf(stderr,"Err [line %u]: invalid type given to apply, expected subroutine or primitve procedure, got %i\n",line_number,sub->t);
			ERR_EXIT(sub,"invalid type given to apply, expected subroutine or primitve procedure",TRUE);
		}
#ifdef _STRICT
		exit(1);
#endif
		return nl_null;
	}
	
	//for primitive procedures just call out to the c function
	if(sub->t==PRI){
		ret=(*(sub->d.pri.function))(arguments);
	//bind arguments to internal sub symbols, substitute the body in, and actually do the apply
	}else if(sub->t==SUB){
		//create an apply environment with an up_scope of the closure environment
		nl_env_frame *apply_env;
		
		//note that apply is never called on a tailcall, so we're always building up stack
		apply_env=nl_env_frame_malloc(sub->d.sub.env);
		
		nl_val *arg_syms=sub->d.sub.args;
		nl_val *arg_vals=arguments;
		if(!nl_bind_list(arg_syms,arg_vals,apply_env)){
			ERR_EXIT(arg_vals,"could not bind arguments to application environment (call stack) from apply (this usually means number of arguments declared and given differ)",TRUE);
		}
		//also bind those same arguments to the closure environment
		//(the body of the closure will always look them up in the apply env, but this keeps any references such as from returned closures safe)
		if(!nl_bind_list(arg_syms,arg_vals,sub->d.sub.env)){
			//could not bind to closure scope
		}
		
		//set the apply env not shared so any new vars go into the closure env
		apply_env->shared=FALSE;
		
		//return keywords are handled in nl_eval_sequence (by setting last expression and executing as begin)
		//and recur are handled in nl_eval_sub (by substituting the closure for all instances of recur in the body)
		
		//now evaluate the body in the application env
//		nl_val *body=sub->d.sub.body;
//		ret=nl_eval_sequence(nl_val_cp(body),apply_env,early_ret);

		//now evaluate the body in the application env
		nl_val *body=sub->d.sub.body;
		ret=nl_eval_sequence(nl_val_cp(body),apply_env,early_ret);
//		ret=nl_eval_sequence(nl_val_cp(body),apply_env,NULL);
		
		//now clean up the apply environment (call stack); again, tailcalls are handled in eval, this is never called on a tailcall
		nl_env_frame_free(apply_env);

/*
#ifdef _DEBUG
		printf("nl_apply debug 7, free'd application environment and returning up\n");
		if(ret!=nl_null){
			printf("nl_apply debug 7.1, after env_frame_free ret is ");
			nl_out(stdout,ret);
			if(ret!=nl_null){
				printf(" with %u references\n",ret->ref);
			}else{
				printf("\n");
			}
		}
#endif
*/
	}
	
	return ret;
}

//evaluate an if statement with the given arguments
nl_val *nl_eval_if(nl_val *arguments, nl_env_frame *env, char last_exp, char *early_ret){
/*
#ifdef _DEBUG
	printf("nl_eval_if debug 0, last_exp=%s\n",last_exp?"TRUE":"FALSE");
#endif
*/
	//return value
	nl_val *ret=nl_null;
	
	if((arguments->t!=PAIR)){
		ERR_EXIT(arguments,"invalid condition given to if statement",TRUE);
		return nl_null;
	}
	
	nl_val *argument_start=arguments;
	
	//the condition is the first argument
//	nl_val *cond=nl_eval(arguments->d.pair.f,env,FALSE,early_ret);
	nl_val *cond=nl_eval(arguments->d.pair.f,env,FALSE,NULL);
	
	//replace the expression with its evaluation moving forward
	arguments->d.pair.f=cond;
	
	//a holder for temporary results
	//(these replace arguments as we move through the expression) (reference counting keeps memory handled)
//	nl_val *tmp_result=nl_null;
	
	//check if the condition is true
	if(nl_is_true(cond)){
		
		//advance past the condition itself
		arguments=arguments->d.pair.r;
		
		//true eval
		nl_val *tmp_args=arguments;
		while(tmp_args->t==PAIR){
			//if we hit an else statement then break out (skipping over false case)
			if((tmp_args->d.pair.r==nl_null) || ((tmp_args->d.pair.r->t==PAIR) && (tmp_args->d.pair.r->d.pair.f!=nl_null) && (tmp_args->d.pair.r->d.pair.f->t==SYMBOL) && (nl_val_cmp(else_keyword,tmp_args->d.pair.r->d.pair.f)==0))){
				nl_val_free(tmp_args->d.pair.r);
				tmp_args->d.pair.r=nl_null;
				break;
			}
			
			tmp_args=tmp_args->d.pair.r;
		}
		//this MUST call out to eval_sequence to handle returns properly
		//call into eval_sequence
		tmp_args=nl_val_cp(arguments);
		nl_val_free(argument_start);
		return nl_eval_sequence(tmp_args,env,early_ret);
	}else if(cond!=nl_null){
		//skip over true case
		while(arguments->t==PAIR){
			//if we hit an else statement then break out
			if((arguments->d.pair.f->t==SYMBOL) && (nl_val_cmp(arguments->d.pair.f,else_keyword)==0)){
				break;
			}
			
			//otherwise just skip to the next list element
			arguments=arguments->d.pair.r;
		}
		
		//false eval
		//if we actually hit an else statement just then (rather than the list end)
		if((arguments!=nl_null) && (nl_val_cmp(arguments->d.pair.f,else_keyword)==0)){
			//this MUST call out to eval_sequence to handle returns properly
			//call into eval_sequence
			nl_val *tmp_args=nl_val_cp(arguments->d.pair.r);
			nl_val_free(argument_start);
			return nl_eval_sequence(tmp_args,env,early_ret);
		}
	}else{
		ERR_EXIT(argument_start,"if statement condition evaluated to NULL (use null? if this is what you intended to check for)",TRUE);
	}
	
	//the calling context didn't free the arguments so do it here if we got to here
	//(we should really always end on a tailcall, only an empty body would result in us getting here)
	nl_val_free(argument_start);
	
	return ret;
}

//evaluate a sub statement with the given arguments
nl_val *nl_eval_sub(nl_val *arguments, nl_env_frame *env){
	nl_val *ret=nl_null;
	
	//invalid syntax case
	if(!((arguments->t==PAIR) && (arguments->d.pair.f->t==PAIR))){
		nl_val_free(ret);
		ERR_EXIT(arguments,"first argument to subroutine expression isn't a list (should be the list of arguments); invalid syntax!",TRUE);
		return nl_null;
	}
	
	//allocate some memory for this and start filling it
	ret=nl_val_malloc(SUB);
	ret->d.sub.args=arguments->d.pair.f;
	ret->d.sub.args->ref++;
	
	//create a new environment for this closure which links up to the existing environment (the closest shared one, application envs don't get used!)
	while(env!=NULL && env->shared==FALSE){
		env=env->up_scope;
	}
	ret->d.sub.env=nl_env_frame_malloc(env);
	
	//the rest of the arguments are the body
	ret->d.sub.body=arguments->d.pair.r;
	if(ret->d.sub.body!=nl_null){
		ret->d.sub.body->ref++;
		
		//be sneaky about fixing recursion
		//check the body for "recur" statements; any time we find one, replace it with a reference to this closure
		//they'll never know!
		
		//if replacements were made
		if(nl_substitute_elements_skipsub(ret->d.sub.body,recur_keyword,ret)){
		}
	}
	
	return ret;
}

//proper evaluation of keywords!
//evaluate a keyword expression (or primitive function, if keyword isn't found)
nl_val *nl_eval_keyword(nl_val *keyword_exp, nl_env_frame *env, char last_exp, char *early_ret){
	nl_val *keyword=keyword_exp->d.pair.f;
	nl_val *arguments=keyword_exp->d.pair.r;
	
	nl_val *ret=nl_null;
	
	//TODO: make let require a type argument? (maybe the first argument is just its own symbol which represents the type?)
	// ^ for the moment we just check re-binds against first-bound type; this is closer to the strong inferred typing I initially envisioned anyway
	
	//check for if statements
	if(nl_val_cmp(keyword,if_keyword)==0){
		//handle memory to allow for TCO
		if(arguments!=nl_null){
			arguments->ref++;
		}
		nl_val_free(keyword_exp);
		
		//handle if statements in a tailcall
		//NOTE: this is used for tailcalls and depends on C TCO (-O3 or -O2)
		return nl_eval_if(arguments,env,last_exp,early_ret);
//		return nl_eval_if(arguments,env,last_exp,NULL);
		
		//handle if statements
//		ret=nl_eval_if(arguments,env,last_exp);
	//check for literals (equivilent to scheme quote)
	}else if(nl_val_cmp(keyword,lit_keyword)==0){
		//if there was only one argument, just return that
		if((arguments->t==PAIR) && (arguments->d.pair.r==nl_null)){
			arguments->d.pair.f->ref++;
			ret=arguments->d.pair.f;
		//if there was a list of multiple arguments, return all of them
		}else if(arguments!=nl_null){
			arguments->ref++;
			ret=arguments;
		}
		
	//check for let statements (assignment operations)
	}else if(nl_val_cmp(keyword,let_keyword)==0){
		//if we got a symbol followed by something else, eval that thing and bind
		if((arguments->t==PAIR) && (arguments->d.pair.f->t==SYMBOL) && (arguments->d.pair.r->t==PAIR)){
			//let should never cause an early return to be passed up; (let a (return b)) will NOT return early
//			nl_val *bound_value=nl_eval(arguments->d.pair.r->d.pair.f,env,last_exp,early_ret);
			nl_val *bound_value=nl_eval(arguments->d.pair.r->d.pair.f,env,last_exp,NULL);
			if(!nl_bind(arguments->d.pair.f,bound_value,env)){
				ERR_EXIT(arguments->d.pair.f,"let couldn't bind symbol to value",TRUE);
			}
			
			//if this bind was unsuccessful (for example a type error) then re-set bound_value to NULL so we don't try to access it
			//(it was already free'd)
			if(nl_lookup(arguments->d.pair.f,env)!=bound_value){
				bound_value=nl_null;
			}
			
			//return a copy of the value that was just bound (this is also sort of an internal test to ensure it was bound right)
			ret=nl_val_cp(nl_lookup(arguments->d.pair.f,env));
			
			//since what we just returned was a copy, the original won't be free'd by the calling code
			//so we're one reference too high at the moment
			nl_val_free(bound_value);
			
			//null-out the list elements we got rid of
			arguments->d.pair.r->d.pair.f=nl_null;
		//if we got a list followed by something else, eval that thing and bind list
		}else if((arguments->t==PAIR) && (arguments->d.pair.f->t==PAIR) && (arguments->d.pair.r->t==PAIR)){
			//let should never cause an early return to be passed up; (let a (return b)) will NOT return early
//			nl_val *bound_value=nl_eval(arguments->d.pair.r->d.pair.f,env,last_exp,early_ret);
			nl_val *bound_value=nl_eval(arguments->d.pair.r->d.pair.f,env,last_exp,NULL);
			if(!nl_bind_list(arguments->d.pair.f,bound_value,env)){
				ERR_EXIT(arguments->d.pair.f,"let couldn't bind list of symbols to list of values",TRUE);
			}
			
			//if this bind was unsuccessful (for example a type error) then re-set bound_value to NULL so we don't try to access it
			//(it was already free'd)
//			if(nl_lookup(arguments->d.pair.f,env)!=bound_value){
//				bound_value=NULL;
//			}
			
			//return a copy of the value that was just bound (this is also sort of an internal test to ensure it was bound right)
//			ret=nl_val_cp(nl_lookup(arguments->d.pair.f,env));
			ret=nl_val_cp(bound_value);
			
			//since what we just returned was a copy, the original won't be free'd by the calling code
			//so we're one reference too high at the moment
			nl_val_free(bound_value);
			
			//null-out the list elements we got rid of
			arguments->d.pair.r->d.pair.f=nl_null;

		}else{
			ERR_EXIT(keyword_exp,"wrong syntax for let statement",TRUE);
		}
	//TODO: add support for a let-list, allowing multiple symbols to be set at once; this will hugely aid returns from loops
	//check for subroutine definitions (lambda expressions which are used as closures)
	}else if(nl_val_cmp(keyword,sub_keyword)==0){
		//handle sub statements
		ret=nl_eval_sub(arguments,env);
		
	//check for begin statements (executed in-order, returning only the last)
	}else if(nl_val_cmp(keyword,begin_keyword)==0){
		//handle begin statements
//		ret=nl_eval_sequence(nl_val_cp(arguments),env,early_ret);
		
		//NOTE: this is used for tailcalls and depends on C TCO (-O3 or -O2)
		if(last_exp){
			nl_val_free(keyword_exp);
			return nl_eval_sequence(nl_val_cp(arguments),env,early_ret);
//			return nl_eval_sequence(nl_val_cp(arguments),env,NULL);
		}else{
			ret=nl_eval_sequence(nl_val_cp(arguments),env,early_ret);
//			ret=nl_eval_sequence(nl_val_cp(arguments),env,NULL);
		}
	//check for return statements, they act in a manner similar to a begin tailcall
	}else if(nl_val_cmp(keyword,return_keyword)==0){
/*
#ifdef _DEBUG
		printf("nl_eval_keyword debug -1, found a return statement, arguments has %i references\n",(arguments!=NULL)?arguments->ref:0);
#endif
*/
		//if we hit the return keyword then of course we're in an early return
		if(early_ret!=NULL){
			(*early_ret)=TRUE;
		}
		
		arguments->ref++;
		nl_val_free(keyword_exp);
		//NOTE: eval sequence frees the associated arguments (which is why we ref++'d a couple lines above this)
		//NOTE: this is used for tailcalls and depends on C TCO (-O3 or -O2)
		return nl_eval_sequence(arguments,env,NULL);
	//check for while statements (we'll convert this to tail recursion)
	}else if(nl_val_cmp(keyword,while_keyword)==0){
		if(nl_c_list_size(arguments)<2){
			ERR_EXIT(keyword_exp,"too few arguments given to while statement",TRUE);
		}else{
			nl_val *cond=arguments->d.pair.f;
			nl_val *body=arguments->d.pair.r;
			nl_val *post_loop=nl_null;
			
			arguments=body;
			while(arguments->t==PAIR){
				nl_val *next_arg=arguments->d.pair.r;
				
				//if we found an "after" then shove everything else in the post-loop and break
				if((next_arg->t==PAIR) && (next_arg->d.pair.f->t==SYMBOL) && (nl_val_cmp(next_arg->d.pair.f,after_keyword)==0)){
					post_loop=next_arg->d.pair.r;
					
					//free the after keyword itself (this will not appear in the resulting sub)
					post_loop->ref++;
					nl_val_free(next_arg);
					
					//separate this list from the body list
					arguments->d.pair.r=nl_null;
					break;
				}
				arguments=arguments->d.pair.r;
			}
			
			cond->ref++;
//			body->ref++;
			if(post_loop!=nl_null){
//				post_loop->ref++;
			}
			
			//build a sub expression to evaluate
			nl_val *sub_to_eval=nl_val_malloc(PAIR);
			//keyword
			sub_to_eval->d.pair.f=nl_val_cp(sub_keyword);
			sub_to_eval->d.pair.r=nl_val_malloc(PAIR);
			//no arguments (closures preserve values of new vars between calls)
			sub_to_eval->d.pair.r->d.pair.f=nl_val_malloc(PAIR);
			sub_to_eval->d.pair.r->d.pair.f->d.pair.f=nl_null;
			sub_to_eval->d.pair.r->d.pair.f->d.pair.r=nl_null;
			//add a condition encompassing the body and post_loop
			sub_to_eval->d.pair.r->d.pair.r=nl_val_malloc(PAIR);
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f=nl_val_malloc(PAIR);
			sub_to_eval->d.pair.r->d.pair.r->d.pair.r=nl_null;
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.f=nl_val_cp(if_keyword);
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.r=nl_val_malloc(PAIR);
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.r->d.pair.f=cond;
//			sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.r->d.pair.r=body;
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.r->d.pair.r=nl_val_cp(body);
			body=sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.r->d.pair.r;
			while(body->t==PAIR){
				if(body->d.pair.r==nl_null){
					//add in the recursive call that makes it, you know, loop
					body->d.pair.r=nl_val_malloc(PAIR);
					body->d.pair.r->d.pair.f=nl_val_malloc(PAIR);
					body->d.pair.r->d.pair.f->d.pair.f=nl_val_cp(recur_keyword);
					body->d.pair.r->d.pair.f->d.pair.r=nl_null;
					body->d.pair.r->d.pair.r=nl_null;
					
					//if we had a post-loop clause then put it in there
					if(post_loop!=nl_null){
						body->d.pair.r->d.pair.r=nl_val_malloc(PAIR);
						body->d.pair.r->d.pair.r->d.pair.f=nl_val_cp(else_keyword);
						body->d.pair.r->d.pair.r->d.pair.r=post_loop;
					}
					break;
				}
				body=body->d.pair.r;
			}
			
			
			//free the original expression
			nl_val_free(keyword_exp);
			
			//make sure the sub we just made gets, you know, applied
			nl_val *to_eval=nl_val_malloc(PAIR);
			to_eval->d.pair.f=sub_to_eval;
			to_eval->d.pair.r=nl_null;
			
/*
#ifdef _DEBUG
			printf("nl_eval_keyword, while debug 0; going to eval ");
			nl_out(stdout,to_eval);
			printf("\n");
#endif
*/
			
			//NOTE: this is used for tailcalls and depends on C TCO (-O3 or -O2)
			//evaluate the sub expression, thereby doing the while loop via tail recursion
			return nl_eval(to_eval,env,last_exp,early_ret);
//			return nl_eval(to_eval,env,last_exp,NULL);
		}
	//check for for statements/loops
	}else if(nl_val_cmp(keyword,for_keyword)==0){
		if(nl_c_list_size(arguments)<5){
			ERR_EXIT(keyword_exp,"too few arguments given to for statement",TRUE);
		}else{
			nl_val *counter=arguments->d.pair.f;
			nl_val *init_val=arguments->d.pair.r->d.pair.f;
			nl_val *cond=arguments->d.pair.r->d.pair.r->d.pair.f;
			nl_val *update=arguments->d.pair.r->d.pair.r->d.pair.r->d.pair.f;
			nl_val *body=arguments->d.pair.r->d.pair.r->d.pair.r->d.pair.r;
			nl_val *post_loop=nl_null;
			
			arguments=body;
			while(arguments->t==PAIR){
				nl_val *next_arg=arguments->d.pair.r;
				
				//if we found an "after" then shove everything else in the post-loop and break
				if((next_arg->t==PAIR) && (next_arg->d.pair.f->t==SYMBOL) && (nl_val_cmp(next_arg->d.pair.f,after_keyword)==0)){
					post_loop=next_arg->d.pair.r;
					
					//free the after keyword itself (this will not appear in the resulting sub)
					post_loop->ref++;
					nl_val_free(next_arg);
					
					//separate this list from the body list
					arguments->d.pair.r=nl_null;
					break;
				}
				arguments=arguments->d.pair.r;
			}
			
			counter->ref++;
			init_val->ref++;
			cond->ref++;
			update->ref++;
//			body->ref++;
			if(post_loop!=nl_null){
//				post_loop->ref++;
			}
			
			//build a sub expression to evaluate
			nl_val *sub_to_eval=nl_val_malloc(PAIR);
			//keyword
			sub_to_eval->d.pair.f=nl_val_cp(sub_keyword);
			sub_to_eval->d.pair.r=nl_val_malloc(PAIR);
			//one argument (the counter symbol)
			sub_to_eval->d.pair.r->d.pair.f=nl_val_malloc(PAIR);
			sub_to_eval->d.pair.r->d.pair.f->d.pair.f=counter;
			sub_to_eval->d.pair.r->d.pair.f->d.pair.r=nl_null;
			//add a condition encompassing the body and post_loop
			sub_to_eval->d.pair.r->d.pair.r=nl_val_malloc(PAIR);
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f=nl_val_malloc(PAIR);
			sub_to_eval->d.pair.r->d.pair.r->d.pair.r=nl_null;
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.f=nl_val_cp(if_keyword);
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.r=nl_val_malloc(PAIR);
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.r->d.pair.f=cond;
//			sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.r->d.pair.r=body;
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.r->d.pair.r=nl_val_cp(body);
			body=sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.r->d.pair.r;
			while(body->t==PAIR){
				if(body->d.pair.r==nl_null){
					//add in the recursive call that makes it, you know, loop
					body->d.pair.r=nl_val_malloc(PAIR);
					body->d.pair.r->d.pair.f=nl_val_malloc(PAIR);
					body->d.pair.r->d.pair.f->d.pair.f=nl_val_cp(recur_keyword);
					body->d.pair.r->d.pair.f->d.pair.r=nl_val_malloc(PAIR);
					//pass in the update expression which will be re-evaluated each iteration
					body->d.pair.r->d.pair.f->d.pair.r->d.pair.f=update;
					body->d.pair.r->d.pair.f->d.pair.r->d.pair.r=nl_null;
					body->d.pair.r->d.pair.r=nl_null;
					
					//if we had a post-loop clause then put it in there
					if(post_loop!=nl_null){
						body->d.pair.r->d.pair.r=nl_val_malloc(PAIR);
						body->d.pair.r->d.pair.r->d.pair.f=nl_val_cp(else_keyword);
						body->d.pair.r->d.pair.r->d.pair.r=post_loop;
					}
					break;
				}
				body=body->d.pair.r;
			}
			
			//free the original expression
			nl_val_free(keyword_exp);
			
			//make sure the sub we just made gets, you know, applied
			nl_val *to_eval=nl_val_malloc(PAIR);
			to_eval->d.pair.f=sub_to_eval;
			//and give it init_val as the initial argument value
			to_eval->d.pair.r=nl_val_malloc(PAIR);
			to_eval->d.pair.r->d.pair.f=init_val;
			to_eval->d.pair.r->d.pair.r=nl_null;
			
/*
#ifdef _DEBUG
			printf("nl_eval_keyword, for debug 0; going to eval ");
			nl_out(stdout,to_eval);
			printf("\n");
#endif
*/
			
			//NOTE: this is used for tailcalls and depends on C TCO (-O3 or -O2)
			//evaluate the sub expression, thereby doing the while loop via tail recursion
			return nl_eval(to_eval,env,last_exp,early_ret);
//			return nl_eval(to_eval,env,last_exp,NULL);
		}

	//check for array statements (turns the evaluated argument list into an array then returns that)
	}else if(nl_val_cmp(keyword,array_keyword)==0){
		//first evaluate arguements
		nl_eval_elements(arguments,env);
		
		//throw them in an array to return
		ret=nl_val_malloc(ARRAY);
		while(arguments->t==PAIR){
			nl_array_push(ret,nl_val_cp(arguments->d.pair.f));
			
			arguments=arguments->d.pair.r;
		}
	//check for f statements (car)
	}else if(nl_val_cmp(keyword,f_keyword)==0){
		if(arguments->t==PAIR){
			//evaluate the first argument
//			nl_val *result=nl_eval(arguments->d.pair.f,env,FALSE,early_ret);
			nl_val *result=nl_eval(arguments->d.pair.f,env,FALSE,NULL);
//			nl_val_free(arguments->d.pair.f);
			arguments->d.pair.f=result;
			
			//if it was a pair then return the first entry
			if(arguments->d.pair.f->t==PAIR){
				ret=arguments->d.pair.f->d.pair.f;
				if(ret!=nl_null){
					ret->ref++;
				}
			}else{
				ERR_EXIT(keyword_exp,"argument given to f statement was not a pair",TRUE);
			}
		}else{
			ERR_EXIT(keyword_exp,"incorrect usage of f statement",TRUE);
		}
	//check for r statements (cdr)
	}else if(nl_val_cmp(keyword,r_keyword)==0){
		if(arguments->t==PAIR){
			//evaluate the first argument
//			nl_val *result=nl_eval(arguments->d.pair.f,env,FALSE,early_ret);
			nl_val *result=nl_eval(arguments->d.pair.f,env,FALSE,NULL);
//			nl_val_free(arguments->d.pair.f);
			arguments->d.pair.f=result;
			
			//if it was a pair then return the second entry
			if(arguments->d.pair.f->t==PAIR){
				ret=arguments->d.pair.f->d.pair.r;
				if(ret!=nl_null){
					ret->ref++;
				}
			}else{
				ERR_EXIT(keyword_exp,"argument given to r statement was not a pair",TRUE);
			}
		}else{
			ERR_EXIT(keyword_exp,"incorrect usage of r statement",TRUE);
		}
	//check for list statements (evaluates argument list, returns it)
	}else if(nl_val_cmp(keyword,list_keyword)==0){
		//first evaluate arguements
		nl_eval_elements(arguments,env);
		
		arguments->ref++;
		ret=arguments;
	//check for boolean operator and
	}else if(nl_val_cmp(keyword,and_keyword)==0){
		ret=nl_val_malloc(BYTE);
		//true until we find a false value
		ret->d.byte.v=TRUE;
		while(arguments->t==PAIR){
//			nl_val *tmp_result=nl_eval(arguments->d.pair.f,env,FALSE,early_ret);
			nl_val *tmp_result=nl_eval(arguments->d.pair.f,env,FALSE,NULL);
			arguments->d.pair.f=tmp_result;
			
			//if we hit one false value, then it's game over, return out
			if(!nl_is_true(tmp_result)){
				ret->d.byte.v=FALSE;
				break;
			}
			arguments=arguments->d.pair.r;
		}
	//check for boolean operator or
	}else if(nl_val_cmp(keyword,or_keyword)==0){
		ret=nl_val_malloc(BYTE);
		//false until we find a true value
		ret->d.byte.v=FALSE;
		while(arguments->t==PAIR){
//			nl_val *tmp_result=nl_eval(arguments->d.pair.f,env,FALSE,early_ret);
			nl_val *tmp_result=nl_eval(arguments->d.pair.f,env,FALSE,NULL);
			arguments->d.pair.f=tmp_result;
			
			//if we hit one true value, then it's game over, return out
			if(nl_is_true(tmp_result)){
				ret->d.byte.v=TRUE;
				break;
			}
			arguments=arguments->d.pair.r;
		}
	//check for boolean operator not
	}else if(nl_val_cmp(keyword,not_keyword)==0){
		if(nl_c_list_size(arguments)>1){
			ERR(keyword_exp,"too many arguments given to not, ignoring all but the first...",TRUE);
		}
		ret=nl_val_malloc(BYTE);
		//false by default
		ret->d.byte.v=FALSE;
		while(arguments->t==PAIR){
//			nl_val *tmp_result=nl_eval(arguments->d.pair.f,env,FALSE,early_ret);
			nl_val *tmp_result=nl_eval(arguments->d.pair.f,env,FALSE,NULL);
			arguments->d.pair.f=tmp_result;
			
			//return the opposite of the first argument
			//if there is more than one argument we IGNORE THE REST
			if(nl_is_true(tmp_result)){
				ret->d.byte.v=FALSE;
				break;
			}else{
				ret->d.byte.v=TRUE;
				break;
			}
			arguments=arguments->d.pair.r;
		}
	//check for boolean operator xor
	}else if(nl_val_cmp(keyword,xor_keyword)==0){
		ret=nl_val_malloc(BYTE);
		//false until we find a true value, after which we better not find any more!
		ret->d.byte.v=FALSE;
		while(arguments->t==PAIR){
//			nl_val *tmp_result=nl_eval(arguments->d.pair.f,env,FALSE,early_ret);
			nl_val *tmp_result=nl_eval(arguments->d.pair.f,env,FALSE,NULL);
			arguments->d.pair.f=tmp_result;
			
			//if we hit one true value and the return so far has been false, then set it true and continue
			if((nl_is_true(tmp_result)) && (ret->d.byte.v==FALSE)){
				ret->d.byte.v=TRUE;
			//if we hit a true value but we already hit one then it's not exclusive and return false
			}else if((nl_is_true(tmp_result)) && (ret->d.byte.v==TRUE)){
				ret->d.byte.v=FALSE;
				break;
			}
			arguments=arguments->d.pair.r;
		}
	//check for pair keyword
	}else if(nl_val_cmp(keyword,pair_keyword)==0){
		if(nl_c_list_size(arguments)!=2){
			ERR_EXIT(keyword_exp,"wrong number of arguments given to pair",TRUE);
		}else{
			//eager evaluation
			nl_eval_elements(arguments,env);
			
			//make a pair from the list entries
			ret=nl_val_malloc(PAIR);
			ret->d.pair.f=arguments->d.pair.f;
			ret->d.pair.f->ref++;
			ret->d.pair.r=arguments->d.pair.r->d.pair.f;
			ret->d.pair.r->ref++;
		}
	//check for structs
	}else if(nl_val_cmp(keyword,struct_keyword)==0){
		//a struct is just a named array, so we re-use the environment frame system to make that simple and painless
		ret=nl_val_malloc(STRUCT);
		while(arguments->t==PAIR){
			nl_val *struct_elements=arguments->d.pair.f;
			
			if((struct_elements==nl_null) || (struct_elements->t!=PAIR) || (nl_c_list_size(struct_elements)!=2)){
				ERR_EXIT(struct_elements,"invalid syntax for struct declaration",TRUE);
				nl_val_free(ret);
				ret=NULL;
			}
/*
#ifdef _DEBUG
			printf("calling off to eval_sequence with ");
			nl_out(stdout,struct_elements->d.pair.r);
			printf("\n");
#endif
*/
			//evaluate what to initially bind to (constructor values if you will)
			nl_eval_elements(struct_elements->d.pair.r,env);
/*
#ifdef _DEBUG
			printf("binding ");
			nl_out(stdout,struct_elements->d.pair.f);
			printf(" to value ");
			nl_out(stdout,struct_elements->d.pair.r->d.pair.f);
			printf("\n");
#endif
*/
			//bind this in the struct
			nl_bind(struct_elements->d.pair.f,struct_elements->d.pair.r->d.pair.f,ret->d.nl_struct.env);
			
			arguments=arguments->d.pair.r;
		}
	//check for exits
	}else if(nl_val_cmp(keyword,exit_keyword)==0){
		end_program=TRUE;
		ret=nl_null;
		
		//if an integer numeric argument was given, then pass that through to the system exit
		exit_status=0;
		if((arguments->t==PAIR) && (arguments->d.pair.f->t==NUM)){
			if(arguments->d.pair.f->d.num.d==1){
				exit_status=(int)(arguments->d.pair.f->d.num.n);
			}
		}
		
		//if we're within a sub-expression then halt HARD (if not we'll clean up the environment a little after this)
		if(env->up_scope!=NULL){
			printf("caught an (exit) call; exiting interpreter...\n");
			
			//de-allocate the environment
			nl_env_frame_free(env);
			
			//free (de-allocate) keywords
			nl_keyword_free();
			
			//an explicit exit call is needed so we don't keep evaluating anything after this
			exit(exit_status);
		}
	//TODO: check for all other keywords
	}else{
		//in the default case check for subroutines bound to this symbol
		nl_val *prim_sub=nl_lookup(keyword,env);
//		if((prim_sub!=NULL) && ((prim_sub->t==PRI) || (prim_sub->t==SUB))){ //this allows user-defined subs without $
		if(prim_sub->t==PRI){
			//do eager evaluation on arguments
			nl_eval_elements(arguments,env);
			//call apply
//			ret=nl_apply(prim_sub,arguments,early_ret);
			ret=nl_apply(prim_sub,arguments,NULL);
		}else{
			ERR_EXIT(keyword,"unknown keyword",TRUE);
		}
	}
	
	//free the original expression (frees the entire list, keyword, arguments, and all)
	//note that this list changed as we evaluated, and is likely not exactly what we started with
	//	^ that's a good thing, because what we started with was free'd in that case
	nl_val_free(keyword_exp);
	
	return ret;
}

//evaluate the given expression in the given environment
nl_val *nl_eval(nl_val *exp, nl_env_frame *env, char last_exp, char *early_ret){
	nl_val *ret=nl_null;
	
tailcall:
/*
#ifdef _DEBUG
	printf("nl_eval debug 0, trying to evaluate ");
	nl_out(stdout,exp);
	if(exp!=nl_null){
		printf(" with %i references",exp->ref);
	}
	printf("\n");
#endif
*/

	//null is self-evaluating
	if(exp==nl_null){
		return exp;
	}
	
/*
#ifdef _DEBUG
	if(exp->t==STRUCT){
		printf("nl_eval debug 0.1, evaluating a struct... ");
		nl_out(stdout,exp);
		printf("\n");
	}
#endif
*/
	
	switch(exp->t){
		//self-evaluating expressions (all constant values)
		case BYTE:
		case NUM:
		case ARRAY:
		case PRI:
		case SUB:
		case STRUCT:
			return exp;
			break;
		//symbols are generally self-evaluating
		//in the case of primitive calls the pair evaluation checks for them as keywords, without calling out to eval
		//however some primitive constants evaluate to specific values (TRUE, FALSE, NULL)
		case SYMBOL:
			{
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
					ret=nl_null;
				}else{
					exp->ref++;
					ret=exp;
				}
			}
			nl_val_free(exp);
			return ret;
			break;
		
		//complex cases
		//pairs eagerly evaluate then (if first argument isn't a keyword) call out to apply
		case PAIR:
			//make this check the first list entry, if it is a symbol then check it against keyword and primitive list
			if(exp->d.pair.f->t==SYMBOL){
				return nl_eval_keyword(exp,env,last_exp,early_ret);
			//otherwise eagerly evaluate then call out to apply
			}else if(exp->d.pair.f!=nl_null){
				//evaluate the first element, the thing we're going to apply to the arguments
				exp->d.pair.f->ref++;
				nl_val *sub=nl_eval(exp->d.pair.f,env,last_exp,early_ret);
//				nl_val *sub=nl_eval(exp->d.pair.f,env,last_exp,NULL);
				
				//do eager evaluation on arguments
				nl_eval_elements(exp->d.pair.r,env);
				
				//this is to handle anonymous subroutines, which don't have a reference from the environment frame
				//this also handles anonymous recursion
				//(it seems like it doesn't, but it does, somehow; honestly I moved this code and am amazed it still works)
				if(sub->ref==1){
/*
#ifdef _DEBUG
					printf("nl_eval debug 0.5, got a sub with one reference, expression was ");
					nl_out(stdout,exp);
					printf("\n");
#endif
*/
					sub->ref++;
					
					//call out to apply; this will run through the body (in the case of a closure)
					ret=nl_apply(sub,exp->d.pair.r,early_ret);
//					ret=nl_apply(sub,exp->d.pair.r,NULL);
					
					//free the original sub expression and substitute in the closure so it can be free'd later
					nl_val_free(exp->d.pair.f);
					exp->d.pair.f=sub;
/*
#ifdef _DEBUG
					printf("nl_eval debug 0.6, finished apply call, expression is ");
					nl_out(stdout,exp);
					printf("\n");
#endif
*/
				//if this is the last expression then it doesn't need any environment trickery and we can just execute the body directly
				}else if((last_exp) && (sub->t==SUB)){
					if(!nl_bind_list(sub->d.sub.args,exp->d.pair.r,env)){
						ERR_EXIT(exp->d.pair.r,"could not bind arguments to application environment (call stack) from eval (this usually means number of arguments declared and given differ)",TRUE);
					}
					//also bind those same arguments to the closure environment
					//(the body of the closure will always look them up in the apply env, but this keeps any references such as from returned closures safe)
					if(!nl_bind_list(sub->d.sub.args,exp->d.pair.r,sub->d.sub.env)){
						//could not bind to closure scope
					}
					
					nl_val_free(exp);
					nl_val_free(sub);
					
					exp=nl_val_malloc(PAIR);
					exp->d.pair.f=nl_val_cp(begin_keyword);
//					exp->d.pair.r=nl_val_cp(sub->d.sub.body);
					exp->d.pair.r=sub->d.sub.body;
					exp->d.pair.r->ref++;
					
/*
#ifdef _DEBUG
					printf("nl_eval debug 1, executing tailcall via goto with ");
					nl_env_frame *frame_iterator=env;
					int n=0;
					while(frame_iterator!=NULL){
						frame_iterator=frame_iterator->up_scope;
						n++;
					}
					printf("%i chained env frames\n",n);
#endif
*/
					
					goto tailcall;
				}else{
					//call out to apply; this will run through the body (in the case of a closure)
					ret=nl_apply(sub,exp->d.pair.r,early_ret);
//					ret=nl_apply(sub,exp->d.pair.r,NULL);
				}
/*
#ifdef _DEBUG
				printf("nl_eval debug 2, returned from apply with value ");
				nl_out(stdout,ret);
				printf("\n");
#endif
*/
				//clean up the evaluation of the first element; if we use it again we'll re-make it with another eval call
				nl_val_free(sub);
/*
#ifdef _DEBUG
				printf("nl_eval debug 2.5, free'd sub we just called...\n");
#endif
*/
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
			
			//this data-wise copy is constant
			if(!((ret->t==PRI) || (ret->t==SUB))){
				ret->cnst=TRUE;
			}
			break;
		//default self-evaluating
		default:
			exp->ref++;
			ret=exp;
			break;
	}
	//free the original expression (if it was self-evaluating it got a reference increment earlier so this is okay)
	nl_val_free(exp);

/*
#ifdef _DEBUG
	printf("nl_eval debug 3, returning from eval with ret=");
	nl_out(stdout,ret);
	printf("...\n");
#endif
*/
	
	//return the result of evaluation
	return ret;
}

//END EVALUTION SUBROUTINES ---------------------------------------------------------------------------------------

//BEGIN I/O SUBROUTINES -------------------------------------------------------------------------------------------

//check if a givne character counts as whitespace in neulang
char nl_is_whitespace(char c){
	return (c==' ') || (c=='\t') || (c=='\r') || (c=='\n');
}

//TODO: add another argument to read_exp for interactive mode, and in interactive mode use getch to handle arrow keys, etc.
//TODO: (cont) this interactive mode should also be user-accessable via an argument to inexp
//read an expression from the given input stream
nl_val *nl_read_exp(FILE *fp){
//	unsigned int old_line_number=line_number;
	
	//read in a string until non-whitespace followed by whitespace is found
	//also if we're in a list, keep reading until the end of it (and care about comments insomuch as nest level doesn't change within comments)
	nl_val *input_string=nl_val_malloc(ARRAY);
	int nest_level=0;
	char found_exp=FALSE;
	char in_multiline_comment=FALSE;
	char in_singleline_comment=FALSE;
	char in_string=FALSE;
	
	//read an input string from a file handle
	//stopping once a complete expression is found or end of file is hit
	while(!feof(fp)){
		char c=getc(fp);
		char next_c='\0';
		
		if((c!='\r') && (c!='\n')){
			next_c=getc(fp);
			ungetc(next_c,fp);
		}
/*
#ifdef _DEBUG
		printf("read a '%c'; next_c='%c'\n",c,next_c);
#endif
*/
		
		//if we hit end of file just give up man
		if(feof(fp)){
/*
#ifdef _DEBUG
			printf("nl_read_exp debug -2, hit EOF\n");
#endif
*/
			end_program=TRUE;
			break;
		}
		
		nl_val *string_char=nl_val_malloc(BYTE);
		string_char->d.byte.v=c;
		nl_array_push(input_string,string_char);
#ifdef _DEBUG
/*
		printf("nl_read_exp debug -1, input_string=");
		nl_out(stdout,input_string);
		printf("\n");
*/
#endif
		
		//newlines end single-line comments (since these are whitespace this check must go prior to the nl_is_whitespace() check)
		if(((c=='\r') || (c=='\n')) && (!in_string) && (!in_multiline_comment)){
/*
#ifdef _DEBUG
			printf("nl_read_exp debug 1, ending single-line comment, input_string=");
			nl_out(stdout,input_string);
			printf("\n");
#endif
*/
			in_singleline_comment=FALSE;
			if(nest_level<=0){
/*
#ifdef _DEBUG
				printf("nl_read_exp debug 1.5, returning due to newline termination and not being in parens, string,  or multiline comment\n");
#endif
*/
				break;
			}
		//if we hit whitespace after finding an expression and not being in a list, then stop for now
		}else if(nl_is_whitespace(c) && (!in_string) && (!in_multiline_comment) && (!in_singleline_comment)){
			if((found_exp) && (nest_level<=0)){
				break;
			}
		//comments are // for single-line or /*...*/ for multi-line
		//note the extra getc so that /*/ is considered an unterminated multi-line comment
		}else if((c=='/') && !(in_string)){
			if(next_c=='*'){
				//make sure next_c makes it into the end string, since we're skipping it here
				string_char=nl_val_malloc(BYTE);
				string_char->d.byte.v=next_c;
				nl_array_push(input_string,string_char);
				
				//and don't skip a beat, but do skip the *
				c=getc(fp);
				in_multiline_comment=TRUE;
			}else if(next_c=='/'){
				in_singleline_comment=TRUE;
/*
#ifdef _DEBUG
				printf("nl_read_exp debug 0, starting single-line comment, input_string=");
				nl_out(stdout,input_string);
				printf("\n");
#endif
*/
			}
		//look for end of /*...*/ multi-line comment
		}else if((c=='*') && (next_c=='/') && !(in_string)){
			in_multiline_comment=FALSE;
		//lists start with (, providing we're not within a comment or string
		}else if((c=='(') && !(in_singleline_comment) && !(in_multiline_comment) && !(in_string)){
			nest_level++;
		//lists end with ), providing we're not within a comment or string
		}else if((c==')') && !(in_singleline_comment) && !(in_multiline_comment) && !(in_string)){
			nest_level--;
/*
			if(nest_level<=0){
				break;
			}
*/
		//strings are double-quote delimited; there is NO ESCAPE
		}else if((c=='"') && (!in_singleline_comment) && (!in_multiline_comment)){
			in_string=(!in_string);
			//strings count as expressions, even empty ones
			if(!in_string){
				found_exp=TRUE;
/*
				if(nest_level<=0){
					break;
				}
*/
			}
		//normal character, just treat it as-is and remember we found something
		//this was already added to the input string at the top of this loop so no need to here
//		}else if((!in_multiline_comment) && (!in_singleline_comment)){
		}else{
			found_exp=TRUE;
		}
		
		//if the user hit enter and we've read in a parseable expression at this point then go ahead and return up
		if(((next_c=='\r') || (next_c=='\n')) && (found_exp) && (nest_level<=0) && (!in_string) && (!in_multiline_comment) && (!in_singleline_comment)){
			//make sure next_c makes it into the end string, since we're skipping it here
			string_char=nl_val_malloc(BYTE);
			string_char->d.byte.v=next_c;
			nl_array_push(input_string,string_char);
			
			//decrement the line number so this newline isn't counted twice
			if(next_c=='\n'){
				line_number--;
			}
			break;
		}
	}
	unsigned int pos=0;
	nl_val *exp=nl_str_read_exp(input_string,&pos);
	
#ifdef _DEBUG
/*
	printf("nl_read_exp debug 2, stopped reading (lines %u-%u) at input_string=",old_line_number,line_number);
	nl_out(stdout,input_string);
	printf("\n");
*/
/*
	if(pos<input_string->d.array.size){
		printf("nl_read_exp debug 3, didn't use whole string (pos=%u, string length=%u)\n",pos,input_string->d.array.size);
	}
*/
#endif
	
	nl_val_free(input_string);
	return exp;
}

//this now outputs a [neulang] string; NOT a c string, so we don't need a length (from a user perspective there's no change, just done in a different function)
//output a neulang value
void nl_out(FILE *fp, const nl_val *exp){
	nl_val *nl_str=nl_val_to_memstr(exp);
	int n;
	for(n=0;n<(nl_str->d.array.size);n++){
		fprintf(fp,"%c",nl_str->d.array.v[n]->d.byte.v);
	}
	nl_val_free(nl_str);
}

//END I/O SUBROUTINES ---------------------------------------------------------------------------------------------

//create global symbol data so it's not constantly being re-allocated (which is slow and unnecessary)
void nl_keyword_malloc(){
	true_keyword=nl_sym_from_c_str("TRUE");
	false_keyword=nl_sym_from_c_str("FALSE");
	null_keyword=nl_sym_from_c_str("NULL");
	
	pair_keyword=nl_sym_from_c_str("pair");
	f_keyword=nl_sym_from_c_str("f");
	r_keyword=nl_sym_from_c_str("r");
	if_keyword=nl_sym_from_c_str("if");
	else_keyword=nl_sym_from_c_str("else");
	and_keyword=nl_sym_from_c_str("and");
	or_keyword=nl_sym_from_c_str("or");
	not_keyword=nl_sym_from_c_str("not");
	xor_keyword=nl_sym_from_c_str("xor");
	exit_keyword=nl_sym_from_c_str("exit");
	lit_keyword=nl_sym_from_c_str("lit");
	let_keyword=nl_sym_from_c_str("let");
	sub_keyword=nl_sym_from_c_str("sub");
	begin_keyword=nl_sym_from_c_str("begin");
	recur_keyword=nl_sym_from_c_str("recur");
	return_keyword=nl_sym_from_c_str("return");
	while_keyword=nl_sym_from_c_str("while");
	for_keyword=nl_sym_from_c_str("for");
	after_keyword=nl_sym_from_c_str("after");
	array_keyword=nl_sym_from_c_str("array");
	list_keyword=nl_sym_from_c_str("list");
	struct_keyword=nl_sym_from_c_str("struct");
}

//free global symbol data for clean exit
void nl_keyword_free(){
	nl_val_free(true_keyword);
	nl_val_free(false_keyword);
	nl_val_free(null_keyword);
	
	nl_val_free(pair_keyword);
	nl_val_free(f_keyword);
	nl_val_free(r_keyword);
	nl_val_free(if_keyword);
	nl_val_free(else_keyword);
	nl_val_free(and_keyword);
	nl_val_free(or_keyword);
	nl_val_free(not_keyword);
	nl_val_free(xor_keyword);
	nl_val_free(exit_keyword);
	nl_val_free(lit_keyword);
	nl_val_free(let_keyword);
	nl_val_free(sub_keyword);
	nl_val_free(begin_keyword);
	nl_val_free(recur_keyword);
	nl_val_free(return_keyword);
	nl_val_free(while_keyword);
	nl_val_free(for_keyword);
	nl_val_free(after_keyword);
	nl_val_free(array_keyword);
	nl_val_free(list_keyword);
	nl_val_free(struct_keyword);
}

//bind a newly alloc'd value (just removes an reference after bind to keep us memory-safe)
void nl_bind_new(nl_val *symbol, nl_val *value, nl_env_frame *env){
	nl_bind(symbol,value,env);
	nl_val_free(symbol);
	nl_val_free(value);
}

//bind all primitive subroutines in the given environment frame
void nl_bind_stdlib(nl_env_frame *env){
	//bind the standard library here
	
	nl_bind_new(nl_sym_from_c_str("+"),nl_primitive_wrap(nl_add),env);
	nl_bind_new(nl_sym_from_c_str("-"),nl_primitive_wrap(nl_sub),env);
	nl_bind_new(nl_sym_from_c_str("*"),nl_primitive_wrap(nl_mul),env);
	nl_bind_new(nl_sym_from_c_str("/"),nl_primitive_wrap(nl_div),env);
/*
	nl_bind_new(nl_sym_from_c_str("%"),nl_primitive_wrap(nl_mod),env);
*/
	nl_bind_new(nl_sym_from_c_str("floor"),nl_primitive_wrap(nl_floor),env);
	nl_bind_new(nl_sym_from_c_str("ceil"),nl_primitive_wrap(nl_ceil),env);
	nl_bind_new(nl_sym_from_c_str("abs"),nl_primitive_wrap(nl_abs),env);
	
	//ALL the comparison operators (for each type)
	nl_bind_new(nl_sym_from_c_str("="),nl_primitive_wrap(nl_generic_eq),env);
	nl_bind_new(nl_sym_from_c_str("!="),nl_primitive_wrap(nl_generic_neq),env);
	nl_bind_new(nl_sym_from_c_str(">"),nl_primitive_wrap(nl_generic_gt),env);
	nl_bind_new(nl_sym_from_c_str("<"),nl_primitive_wrap(nl_generic_lt),env);
	nl_bind_new(nl_sym_from_c_str(">="),nl_primitive_wrap(nl_generic_ge),env);
	nl_bind_new(nl_sym_from_c_str("<="),nl_primitive_wrap(nl_generic_le),env);
	nl_bind_new(nl_sym_from_c_str("null?"),nl_primitive_wrap(nl_is_null),env);
	
	//array concatenation!
	nl_bind_new(nl_sym_from_c_str(","),nl_primitive_wrap(nl_array_cat),env);
	nl_bind_new(nl_sym_from_c_str("ar-cat"),nl_primitive_wrap(nl_array_cat),env);
	
	//size and length are bound to the same primitive function, just to make life easier (only ar-sz is "official")
	nl_bind_new(nl_sym_from_c_str("ar-sz"),nl_primitive_wrap(nl_array_size),env);
	nl_bind_new(nl_sym_from_c_str("ar-len"),nl_primitive_wrap(nl_array_size),env);
	nl_bind_new(nl_sym_from_c_str("ar-idx"),nl_primitive_wrap(nl_array_idx),env);
	nl_bind_new(nl_sym_from_c_str("ar-replace"),nl_primitive_wrap(nl_array_replace),env);
	nl_bind_new(nl_sym_from_c_str("ar-extend"),nl_primitive_wrap(nl_array_extend),env);
	nl_bind_new(nl_sym_from_c_str("ar-omit"),nl_primitive_wrap(nl_array_omit),env);
	
//	nl_bind_new(nl_sym_from_c_str("ar-find"),nl_primitive_wrap(nl_array_find),env);
	nl_bind_new(nl_sym_from_c_str("ar-ins"),nl_primitive_wrap(nl_array_insert),env);
	nl_bind_new(nl_sym_from_c_str("ar-chop"),nl_primitive_wrap(nl_array_chop),env);
	nl_bind_new(nl_sym_from_c_str("ar-subar"),nl_primitive_wrap(nl_array_subarray),env);
//	nl_bind_new(nl_sym_from_c_str("ar-range"),nl_primitive_wrap(nl_array_range),env);
	//TODO: make and bind additional array subroutines
	
	//TODO: list concatenation
	
	//same as for arrays; size and length mean the same thing, sz is the official/recommended one
	nl_bind_new(nl_sym_from_c_str("list-sz"),nl_primitive_wrap(nl_list_size),env);
	nl_bind_new(nl_sym_from_c_str("list-len"),nl_primitive_wrap(nl_list_size),env);
	nl_bind_new(nl_sym_from_c_str("list-idx"),nl_primitive_wrap(nl_list_idx),env);
	nl_bind_new(nl_sym_from_c_str("list-cat"),nl_primitive_wrap(nl_list_cat),env);
	
	//struct stdlib subroutines
	nl_bind_new(nl_sym_from_c_str("struct-get"),nl_primitive_wrap(nl_struct_get),env);
	nl_bind_new(nl_sym_from_c_str("struct-replace"),nl_primitive_wrap(nl_struct_replace),env);
	
	nl_bind_new(nl_sym_from_c_str("outs"),nl_primitive_wrap(nl_outstr),env);
	nl_bind_new(nl_sym_from_c_str("outexp"),nl_primitive_wrap(nl_outexp),env);
	
	nl_bind_new(nl_sym_from_c_str("inexp"),nl_primitive_wrap(nl_inexp),env);
	nl_bind_new(nl_sym_from_c_str("inline"),nl_primitive_wrap(nl_inline),env);
	nl_bind_new(nl_sym_from_c_str("inchar"),nl_primitive_wrap(nl_inchar),env);
	
	//read in a file as a byte array (aka a string)
	nl_bind_new(nl_sym_from_c_str("file->ar"),nl_primitive_wrap(nl_str_from_file),env);
	
	
	nl_bind_new(nl_sym_from_c_str("num->byte"),nl_primitive_wrap(nl_num_to_byte),env);
	nl_bind_new(nl_sym_from_c_str("byte->num"),nl_primitive_wrap(nl_byte_to_num),env);
	nl_bind_new(nl_sym_from_c_str("ar->list"),nl_primitive_wrap(nl_array_to_list),env);
	nl_bind_new(nl_sym_from_c_str("list->ar"),nl_primitive_wrap(nl_list_to_array),env);
	nl_bind_new(nl_sym_from_c_str("val->memstr"),nl_primitive_wrap(nl_val_list_to_memstr),env);
	//TODO: all other sensical type conversions
	
	//TODO: any other bitwise operations that make sense
	//bitwise operations
	nl_bind_new(nl_sym_from_c_str("b|"),nl_primitive_wrap(nl_byte_or),env);
	nl_bind_new(nl_sym_from_c_str("b&"),nl_primitive_wrap(nl_byte_and),env);
	
	//for testing purposes, an assert function
	nl_bind_new(nl_sym_from_c_str("assert"),nl_primitive_wrap(nl_assert),env);
	
	//timing functions
	nl_bind_new(nl_sym_from_c_str("sleep"),nl_primitive_wrap(nl_sleep),env);
	
	//pre-defined variables for convenience
	nl_val *newline=nl_val_malloc(ARRAY);
	nl_val *newline_char=nl_val_malloc(BYTE);
	newline_char->d.byte.v=10;
	nl_array_push(newline,newline_char);
	
	nl_val *dquote=nl_val_malloc(ARRAY);
	nl_val *dquote_char=nl_val_malloc(BYTE);
	dquote_char->d.byte.v=34;
	nl_array_push(dquote,dquote_char);
	
	nl_val *squote=nl_val_malloc(ARRAY);
	nl_val *squote_char=nl_val_malloc(BYTE);
	squote_char->d.byte.v=39;
	nl_array_push(squote,squote_char);
	
	//TODO: make this \r\n on platforms for which that's the eol string
	nl_val *end_of_line=nl_val_malloc(ARRAY);
	nl_val *end_of_line_char=nl_val_malloc(BYTE);
	end_of_line_char->d.byte.v=10;
	nl_array_push(end_of_line,end_of_line_char);
	
	//newline (\n)
	nl_bind_new(nl_sym_from_c_str("newl"),newline,env);
	//double quote (")
	nl_bind_new(nl_sym_from_c_str("dquo"),dquote,env);
	//single quote (')
	nl_bind_new(nl_sym_from_c_str("squo"),squote,env);
	//end of line (\n on *nix)
	nl_bind_new(nl_sym_from_c_str("eol"),end_of_line,env);
}

//the repl for neulang; this is separated from main for embedding purposes
//the only thing you have to do outside this is give us an open file and close it when we're done
//arguments given are interpreted as command-line arguments and are bound to argv in the interpreter (NULL works)
int nl_repl(FILE *fp, nl_val *argv){
	//the global NULL is allocated in main() so that argv handling can use it
	//create and bind the global NULL
//	nl_null=nl_val_malloc(NL_NULL);
	
	exit_status=0;
	
	//allocate keywords
	nl_keyword_malloc();
	
	//create the global environment
	nl_env_frame *global_env=nl_env_frame_malloc(NULL);
	
	//bind the standard library functions in the global environment
	nl_bind_stdlib(global_env);
	
	//if we got arguments, bind those too
	if(argv!=NULL){
		nl_val *argv_symbol=nl_sym_from_c_str("argv");
		nl_bind(argv_symbol,argv,global_env);
		
		//free the arguments and the symbol
		nl_val_free(argv);
		nl_val_free(argv_symbol);
	}
	
	if(fp==stdin){
		printf("neulang version %s, compiled on %s %s\n",VERSION,__DATE__,__TIME__);
		printf("started with arguments: ");
		nl_out(stdout,argv);
		printf("\n");
		printf("[line %u] nl >> ",line_number);
	}
	
	//read in a little bit in case there's a shebang line
	//read a character from the given file
	char c=getc(fp);
	
	//peek a character after that, for two-char tokens
	char next_c=getc(fp);
	ungetc(next_c,fp);
	
	//initialize the line number
//	line_number=0;
	line_number=1;
	
	//ignore shebang (#!) line, if there is one
	if(c=='#' && next_c=='!'){
		while(c!='\n'){
			c=getc(fp);
		}
		line_number++;
	}else{
		ungetc(c,fp);
	}
	
	end_program=FALSE;
	while(!end_program){
		//only display prompt for interactive mode
		if(fp==stdin){
			printf("[line %u] nl >> ",line_number);
		}
		
		//read an expression in
		nl_val *exp=nl_read_exp(fp);
		
/*
#ifdef _DEBUG
		printf("\n");
		
		printf("Info [line %i]: evaluating ",line_number);
		nl_out(stdout,exp);
		printf("\n");
#endif
*/
		
		//evalute the expression in the global environment
		nl_val *result=nl_eval(exp,global_env,FALSE,NULL);
		
		//expressions should be free-d in nl_eval, unless they are self-evaluating
		
		//only output the result for interactive mode
		if(fp==stdin){
			//output (print) the result of evaluation
			nl_out(stdout,result);
			//pretty formatting
			printf("\n");
		}
		
		//free the resulting expression
		nl_val_free(result);
		
		//loop (completing the REPL)
	}

#ifdef _DEBUG
	printf("Info [line %i]: exited program\n",line_number);
#endif
	
	//de-allocate the global environment
	nl_env_frame_free(global_env);
	
	//free (de-allocate) keywords
	nl_keyword_free();
	
	//free the global null
	//NULL is not subject to reference counting
	//instead it has one, global, value
	free(nl_null);
//	nl_val_free(nl_null);
	
	//return back to main with interpreter exit status
	return exit_status;
}

//runtime!
int main(int argc, char *argv[]){
	FILE *fp=stdin;
	
	//if we were given a file, open it
	if(argc>1){
		fp=fopen(argv[1],"r");
		if(fp==NULL){
			fprintf(stderr,"Err: Could not open input file \"%s\"\n",argv[1]);
			return 1;
		}
	}
	
	//TODO: find a way to allocate this in nl_repl instead of here
	//but still allow it to be accessed from here
	nl_null=nl_val_malloc(NL_NULL);
	
	nl_val *nl_argv=nl_null;
	
	//if we got more arguments, then pass them to the interpreter as strings
	nl_argv=nl_val_malloc(PAIR);
	if(argc>1){
		nl_val *current_arg=nl_argv;
		int n;
		for(n=1;n<argc;n++){
			current_arg->d.pair.f=nl_val_malloc(ARRAY);
			int n2;
			for(n2=0;n2<strlen(argv[n]);n2++){
				nl_val *c=nl_val_malloc(BYTE);
				c->d.byte.v=argv[n][n2];
				nl_array_push(current_arg->d.pair.f,c);
			}
			
			if((n+1)<argc){
				current_arg->d.pair.r=nl_val_malloc(PAIR);
				current_arg=current_arg->d.pair.r;
			}else{
				current_arg->d.pair.r=nl_null;
			}
		}
	//no arguments, pass an empty arg list in
	}else{
		nl_argv->d.pair.f=nl_null;
		nl_argv->d.pair.r=nl_null;
	}
	
	//go into the read eval print loop!
	int ret=nl_repl(fp,nl_argv);
	
	//if we were reading from a file then close that file
	if((fp!=NULL) && (fp!=stdin)){
		fclose(fp);
	}
	
	//pass return up from repl
	return ret;
}

