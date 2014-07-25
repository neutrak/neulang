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
char end_program;
unsigned int line_number;

//keywords
nl_val *true_keyword;
nl_val *false_keyword;
nl_val *null_keyword;

nl_val *if_keyword;
nl_val *else_keyword;
nl_val *exit_keyword;
nl_val *lit_keyword;
nl_val *let_keyword;
nl_val *sub_keyword;
nl_val *begin_keyword;
nl_val *recur_keyword;
nl_val *return_keyword;
nl_val *while_keyword;
nl_val *after_keyword;
nl_val *array_keyword;


//END GLOBAL DATA -------------------------------------------------------------------------------------------------

//BEGIN MEMORY MANAGEMENT SUBROUTINES -----------------------------------------------------------------------------

//fuck it, just reference count the damn thing; I don't even care anymore

//allocate a value, and initialize it so that we're not doing anything too crazy
nl_val *nl_val_malloc(nl_type t){
	nl_val *ret=(nl_val*)(malloc(sizeof(nl_val)));
	if(ret==NULL){
		fprintf(stderr,"Err: could not malloc a value (out of memory?)\n");
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
//			ret->d.sub.t=NUM;
			ret->d.sub.args=NULL;
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
//returns TRUE if successful, FALSE if there are still references
char nl_val_free(nl_val *exp){
	if(exp==NULL){
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
			nl_env_frame_free(exp->d.sub.env);
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
	return TRUE;
}

//copy a value data-wise into new memory, without changing the original
nl_val *nl_val_cp(nl_val *v){
	//a null value is copied as a null value
	if(v==NULL){
		return NULL;
	}
	
	nl_val *ret=NULL;
	
	//if we're not doing a data-wise copy don't allocate new memory
	//(primitive subroutines and closures are copied pointer-wise)
	if((v->t!=PRI) && (v->t!=SUB)){
		ret=nl_val_malloc(v->t);
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
	
	//environments are read/write by default (after all, this language is procedural, if only contextually)
	ret->rw=TRUE;
	
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
//^ there is one exception to that, which is for new vars in a non-rw env (an application frame, aka call stack entry)
//returns TRUE for success, FALSE for failure
char nl_bind(nl_val *symbol, nl_val *value, nl_env_frame *env){
	//can't bind to a null environment
	if(env==NULL){
		fprintf(stderr,"Err: cannot bind to a null environment\n");
		return FALSE;
/*
#ifdef _DEBUG
	}else if(env->rw==FALSE){
		printf("nl_bind debug 0, got a read-only environment while binding symbol ");
		nl_out(stdout,symbol);
		printf("...\n");
#endif
*/
	}
	
	if(symbol==NULL){
		fprintf(stderr,"Err: cannot bind a null symbol\n");
		return FALSE;
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
				return FALSE;
			}
			
			//update the value (removing a reference to the old value)
			nl_val_free(env->value_array->d.array.v[n]);
			env->value_array->d.array.v[n]=value;
			
			//this is a new reference to that value
			if(value!=NULL){
				value->ref++;
				
				//once bound values are no longer constant
				value->cnst=FALSE;
			}
			
			//note that the symbol is already stored
			//it will get free'd later by the calling code, we'll keep the existing version
			
/*
#ifdef _DEBUG
			printf("nl_bind debug 1, updated value for ");
			nl_out(stdout,symbol);
			printf(" to be ");
			nl_out(stdout,value);
			printf("\n");
#endif
*/
			found=TRUE;
			break;
		}
	}
	
	//if we didn't find this symbol in the environment, then go ahead and make it now
	if(!found){
		//if this environment isn't re-writable then don't even bother; just try a higher scope
		//this is the ONE time that we CAN change something in an above scope, and it's only done because we re-use scopes as a call stack (implementation detail)
		if(env->rw==FALSE){
			return(nl_bind(symbol,value,env->up_scope));
		}
		
		//bind symbol
		nl_array_push(env->symbol_array,symbol);
		symbol->ref++;
		
		nl_array_push(env->value_array,value);
		if(value!=NULL){
			//give the symbol the type that the bound value has (effectively inferring typing at runtime)
			//note that NULL is a generic value and doesn't change the type; (a value initially bound to NULL will be forever a symbol)
			symbol->d.sym.t=value->t;
			
			value->ref++;
			
			//once bound values are no longer constant
			value->cnst=FALSE;
			
/*
#ifdef _DEBUG
			printf("nl_bind debug 2, bound new value for ");
			nl_out(stdout,symbol);
			printf("; new value is ");
			nl_out(stdout,value);
			printf("\n");
#endif
*/
		}
	}
	
	return TRUE;
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

//make a neulang value out of a primitve function so we can bind it
nl_val *nl_primitive_wrap(nl_val *(*function)(nl_val *arglist)){
	nl_val *ret=nl_val_malloc(PRI);
	ret->d.pri.function=function;
	return ret;
}

//END MEMORY MANAGEMENT SUBROUTINES -------------------------------------------------------------------------------

//BEGIN EVALUTION SUBROUTINES -------------------------------------------------------------------------------------

//replace all instances of old with new in the given list (new CANNOT be a list itself, and old_val cannot be NULL)
//note that this is recursive and will also descend into lists within the list
//note also that this manages memory, and will remove references when needed
//returns TRUE if replacements were made, else FALSE
char nl_substitute_elements(nl_val *list, nl_val *old_val, nl_val *new_val){
	if(old_val==NULL){
		fprintf(stderr,"Err: cannot substitute NULLs in a list; that would just be silly\n");
		return FALSE;
	}
	
	char ret=FALSE;
	
	while((list!=NULL) && (list->t==PAIR)){
		//if we hit a nested list
		if((list!=NULL) && (list->t==PAIR) && (list->d.pair.f!=NULL) && (list->d.pair.f->t==PAIR)){
			//recurse! recurse!
			if(nl_substitute_elements(list->d.pair.f,old_val,new_val)){
				ret=TRUE;
			}
		//if we found the thing to replace
		}else if((list->d.pair.f!=NULL) && (list->d.pair.f->t==old_val->t) && (nl_val_cmp(old_val,list->d.pair.f)==0)){
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
	if(value==NULL){
		return 0;
	}
	
	//while we're not at list end
	while((list!=NULL) && (list->t==PAIR)){
		if(list->d.pair.f!=NULL){
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
	while((list!=NULL) && (list->t==PAIR)){
		nl_val *ev_result=nl_eval(list->d.pair.f,env,FALSE);
		
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
nl_val *nl_eval_sequence(nl_val *body, nl_env_frame *env){
	nl_val *ret=NULL;
	
	//whether or not this is the last statement in the body
	char on_last_exp=FALSE;
	
	nl_val *to_eval=NULL;
	while((body!=NULL) && (body->t==PAIR)){
		//if the next statement is the last, set that for the next loop iteration
		if(body->d.pair.r==NULL){
			on_last_exp=TRUE;
		}

		//this doesn't work because it'll substitute results in as body statements and fail on recursion or later calls
//		to_eval=body->d.pair.f;
//		to_eval->ref++;
		
		// make a copy so as not to ever change the actual subroutine body statements themselves
		to_eval=nl_val_cp(body->d.pair.f);
		//increment the references because this (to_eval) is a new reference and nl_eval will free it before we can if it's self-evaluating
		if(to_eval!=NULL){
			to_eval->ref++;
		}
		
/*
		//if we hit the "return" keyword then go ahead and treat this as the last expression (even if it wasn't properly)
		if((to_eval!=NULL) && (to_eval->t==PAIR) && (to_eval->d.pair.f!=NULL) && (to_eval->d.pair.f->t==SYMBOL) && (nl_val_cmp(return_keyword,to_eval->d.pair.f)==0)){
			on_last_exp=TRUE;
			//set the expression to evaluate to be the arguments to return, if there were any
			nl_val_free(to_eval->d.pair.f);
			to_eval->d.pair.f=nl_val_cp(begin_keyword);
			if(to_eval->d.pair.r!=NULL){
				to_eval->d.pair.r->ref++;
			}
		}
*/
		
		//if we're not going into a tailcall then keep this expression around and free it when we're done
		if(!on_last_exp){
			//always set this to ret, so ret ends up with the last-evaluated thing
			ret=nl_eval(to_eval,env,on_last_exp);
			
			//clean up our original expression (it got an extra reference if it was self-evaluating)
			nl_val_free(to_eval);
			
			//if we're not going to return this then we need to free the result since it will be unused
			nl_val_free(ret);
			ret=NULL;
		
		//if we ARE going into a tailcall then clean up our extra references and return
		}else{
/*
#ifdef _DEBUG
			printf("nl_eval_sequence debug 1, tailcalling into nl_eval...\n");
#endif
*/
			nl_val_free(to_eval);
			
			//NOTE: this depends on C's own TCO behavior and so requires compilation with -O3
			return nl_eval(to_eval,env,on_last_exp);
		}
		
		//go to the next body statement and eval again!
		body=body->d.pair.r;
	}
	return ret;
}

//bind all the symbols to corresponding values in the given environment
//returns TRUE on success, FALSE on failure
char nl_bind_list(nl_val *symbols, nl_val *values, nl_env_frame *env){
	//if there was nothing to bind
	if((values==NULL) && (symbols!=NULL) && (symbols->t==PAIR) && (symbols->d.pair.f==NULL) && (symbols->d.pair.r==NULL)){
		//we are successful at doing nothing (aren't you so proud?)
		return TRUE;
	}
	
	while((symbols!=NULL) && (symbols->t==PAIR) && (values!=NULL) && (values->t==PAIR)){
		//bind the argument to its associated symbol
		if((symbols->d.pair.f!=NULL) && (!nl_bind(symbols->d.pair.f,values->d.pair.f,env))){
			return FALSE;
		}
		
		symbols=symbols->d.pair.r;
		values=values->d.pair.r;
	}
	
	//if there weren't as many of one list as the other then return with error
	if((symbols!=NULL) || (values!=NULL)){
		return FALSE;
	}
	return TRUE;
}

//TODO: change this to support return statements (maybe last_exp can just force a tailcall behavior?)

//apply a given subroutine to its arguments
//last_exp is set if we are currently executing the last expression of a body block, and can therefore re-use an existing application environment
nl_val *nl_apply(nl_val *sub, nl_val *arguments, nl_env_frame *env, char last_exp){
	nl_val *ret=NULL;
	
	if(!(sub!=NULL && (sub->t==PRI || sub->t==SUB))){
		if(sub==NULL){
			fprintf(stderr,"Err: cannot apply NULL!!!\n");
		}else{
			fprintf(stderr,"Err: invalid type given to apply, expected subroutine or primitve procedure, got %i\n",sub->t);
		}
		return NULL;
	}
	
	//for primitive procedures just call out to the c function
	if(sub->t==PRI){
		ret=(*(sub->d.pri.function))(arguments);
	//bind arguments to internal sub symbols, substitute the body in, and actually do the apply
	}else if(sub->t==SUB){
		//create an apply environment with an up_scope of the closure environment
		nl_env_frame *apply_env;
		
		//TODO: change this whole thing; we're handling tailcalls differently now (in eval with eval_sequence)
		if(!last_exp){
			apply_env=nl_env_frame_malloc(sub->d.sub.env);
		}else{
			//in this case we came from the last statement in a closure, and so can re-use the environment we're already in
#ifdef _DEBUG
			printf("Info: detected tail-recursion; re-using existing application environment frame\n");
#endif
			apply_env=env;
			apply_env->rw=TRUE;
		}
		
		nl_val *arg_syms=sub->d.sub.args;
		nl_val *arg_vals=arguments;
		if(!nl_bind_list(arg_syms,arg_vals,apply_env)){
			fprintf(stderr,"Err: could not bind arguments to application environment (call stack) from apply\n");
		}
		//set the apply env read-only so any new vars go into the closure env
		apply_env->rw=FALSE;
		
		//TODO: handle return and recur keywords correctly in here somewhere/somehow
		
		//now evaluate the body in the application env
//		nl_val *body=sub->d.sub.body;
//		ret=nl_eval_sequence(body,apply_env);

/*
#ifdef _DEBUG
		printf("nl_apply debug 6, returning ret ");
		nl_out(stdout,ret);
		if(ret!=NULL){
			printf(" with %u references\n",ret->ref);
		}else{
			printf("\n");
		}
#endif
*/
		
		//now clean up the apply environment (call stack)
		//if we re-used a previous call stack entry then don't free that here (it'll be free'd after returning from this)
		if(!last_exp){
			//now evaluate the body in the application env
			nl_val *body=sub->d.sub.body;
			ret=nl_eval_sequence(body,apply_env);
			
			nl_env_frame_free(apply_env);
		//if we ARE the last expression, make a tailcall (this depends on C's own TCO (-O3))
		}else{
			nl_val *body=sub->d.sub.body;
			return nl_eval_sequence(body,apply_env);
		}
/*
#ifdef _DEBUG
		printf("nl_apply debug 7, free'd application environment and returning up\n");
		if(ret!=NULL){
			printf("nl_apply debug 7.1, after env_frame_free ret is ");
			nl_out(stdout,ret);
			if(ret!=NULL){
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
nl_val *nl_eval_if(nl_val *arguments, nl_env_frame *env, char last_exp){
/*
#ifdef _DEBUG
	printf("nl_eval_if debug 0, last_exp=%s\n",last_exp?"TRUE":"FALSE");
#endif
*/
	//return value
	nl_val *ret=NULL;
	
	if((arguments==NULL) || (arguments->t!=PAIR)){
		fprintf(stderr,"Err: invalid condition given to if statement\n");
		nl_val_free(else_keyword);
		return NULL;
	}
	
	nl_val *argument_start=arguments;
	
	//the condition is the first argument
	nl_val *cond=nl_eval(arguments->d.pair.f,env,FALSE);
	
	//replace the expression with its evaluation moving forward
	arguments->d.pair.f=cond;
	
	//a holder for temporary results
	//(these replace arguments as we move through the expression) (reference counting keeps memory handled)
	nl_val *tmp_result=NULL;
	
	//check if the condition is true
	if((cond!=NULL) && (((cond->t==BYTE) && (cond->d.byte.v!=0)) || ((cond->t==NUM) && (cond->d.num.n!=0)))){
		
		//advance past the condition itself
		arguments=arguments->d.pair.r;
		
		//true eval
		while((arguments!=NULL) && (arguments->t==PAIR)){
			//if we hit an else statement then break out (skipping over false case)
			if((arguments->d.pair.f->t==SYMBOL) && (nl_val_cmp(arguments->d.pair.f,else_keyword)==0)){
				break;
			}
			
			nl_val *next_exp=arguments->d.pair.r;
			//on the last expression pass last_exp through
			//an else case also counts as the end of expressions, since we don't execute after that
			if((next_exp==NULL) || ((next_exp->d.pair.f!=NULL) && (next_exp->d.pair.f->t==SYMBOL) && (nl_val_cmp(next_exp->d.pair.f,else_keyword)==0))){
				//we need a reference directly to the last expression since the containing expression (arguments) will be free'd
				nl_val *to_eval=arguments->d.pair.f;
				to_eval->ref++;
				nl_val_free(argument_start);
				
				//tailcall out to eval!
				return nl_eval(to_eval,env,last_exp);
			//otherwise it's not the last expression (we might still need the old env, for old argument values)
			}else{
				tmp_result=nl_eval(arguments->d.pair.f,env,FALSE);
			}
			arguments->d.pair.f=tmp_result;
			arguments=arguments->d.pair.r;
		}
		
		if(tmp_result!=NULL){
			tmp_result->ref++;
		}
		
		//when we're done with the list then break out
		ret=tmp_result;
	}else if(cond!=NULL){
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
				//on the last expression pass last_exp through
				if(arguments->d.pair.r==NULL){
					//we need a reference directly to the last expression since the containing expression (arguments) will be free'd
					nl_val *to_eval=arguments->d.pair.f;
					to_eval->ref++;
					nl_val_free(argument_start);
					
					//tailcall out to eval!
					return nl_eval(to_eval,env,last_exp);
				//otherwise it's not the last expression (we might still need the old env, for old argument values)
				}else{
					tmp_result=nl_eval(arguments->d.pair.f,env,FALSE);
				}
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
	
	//the calling context didn't free the arguments so do it here if we got to here
	//(we should really always end on a tailcall, only an empty body would result in us getting here)
	nl_val_free(argument_start);
	
	return ret;
}

//TODO: FIX THIS!!!! (it breaks hard when you try to immediately apply a sub (without binding it to a var) when that sub contains the recur keyword)
//evaluate a sub statement with the given arguments
nl_val *nl_eval_sub(nl_val *arguments, nl_env_frame *env){
	nl_val *ret=NULL;
	
	//invalid syntax case
	if(!((arguments!=NULL) && (arguments->t==PAIR) && (arguments->d.pair.f!=NULL) && (arguments->d.pair.f->t==PAIR))){
		fprintf(stderr,"Err: first argument to subroutine expression isn't a list (should be the list of arguments); invalid syntax!\n");
		nl_val_free(ret);
		return NULL;
	}
	
	//allocate some memory for this and start filling it
	ret=nl_val_malloc(SUB);
	ret->d.sub.args=arguments->d.pair.f;
	ret->d.sub.args->ref++;
	
	//create a new environment for this closure which links up to the existing environment
	ret->d.sub.env=nl_env_frame_malloc(env);
	
	//the rest of the arguments are the body
	ret->d.sub.body=arguments->d.pair.r;
	if(ret->d.sub.body!=NULL){
		ret->d.sub.body->ref++;
		
/*
#ifdef _DEBUG
		printf("nl_eval_sub debug 0, trying a substitution (changing the recur keyword into this closure)...\n");
#endif
*/
		//be sneaky about fixing recursion
		//check the body for "recur" statements; any time we find one, replace it with a reference to this closure
		//they'll never know!
		
		//if replacements were made
		if(nl_substitute_elements(ret->d.sub.body,recur_keyword,ret)){
/*
#ifdef _DEBUG
			printf("nl_eval_sub debug 1, substituted recur keyword for address %p=",ret);
			nl_out(stdout,ret);
			printf("\n");
#endif
*/
		}
	}
	
	return ret;
}

//proper evaluation of keywords!
//evaluate a keyword expression (or primitive function, if keyword isn't found)
nl_val *nl_eval_keyword(nl_val *keyword_exp, nl_env_frame *env, char last_exp){
	nl_val *keyword=keyword_exp->d.pair.f;
	nl_val *arguments=keyword_exp->d.pair.r;
	
	nl_val *ret=NULL;
	
	//TODO: make let require a type argument? (maybe the first argument is just its own symbol which represents the type?)
	// ^ for the moment we just check re-binds against first-bound type; this is closer to the strong inferred typing I initially envisioned anyway
	
	//check for if statements
	if(nl_val_cmp(keyword,if_keyword)==0){
		//handle memory to allow for TCO
		arguments->ref++;
		nl_val_free(keyword_exp);
		
		//handle if statements in a tailcall
		return nl_eval_if(arguments,env,last_exp);
		
		//handle if statements
//		ret=nl_eval_if(arguments,env,last_exp);
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
			nl_val *bound_value=nl_eval(arguments->d.pair.r->d.pair.f,env,last_exp);
			if(!nl_bind(arguments->d.pair.f,bound_value,env)){
				fprintf(stderr,"Err: let couldn't bind symbol to value\n");
			}
			
			//if this bind was unsuccessful (for example a type error) then re-set bound_value to NULL so we don't try to access it
			//(it was already free'd)
			if(nl_lookup(arguments->d.pair.f,env)!=bound_value){
				bound_value=NULL;
			}
			
			//return a copy of the value that was just bound (this is also sort of an internal test to ensure it was bound right)
			ret=nl_val_cp(nl_lookup(arguments->d.pair.f,env));
			
			//since what we just returned was a copy, the original won't be free'd by the calling code
			//so we're one reference too high at the moment
			nl_val_free(bound_value);
			
			//null-out the list elements we got rid of
			arguments->d.pair.r->d.pair.f=NULL;
		}else{
			fprintf(stderr,"Err: wrong syntax for let statement\n");
		}
	//check for subroutine definitions (lambda expressions which are used as closures)
	}else if(nl_val_cmp(keyword,sub_keyword)==0){
		//handle sub statements
		ret=nl_eval_sub(arguments,env);
		
	//check for begin statements (executed in-order, returning only the last)
	}else if(nl_val_cmp(keyword,begin_keyword)==0){
		//handle begin statements
//		ret=nl_eval_sequence(arguments,env);
		
		//NOTE: this is used for tailcalls and depends on C TCO (-O3)
		nl_val_free(keyword_exp);
		return nl_eval_sequence(arguments,env);
	//check for while statements (we'll convert this to tail recursion)
	}else if(nl_val_cmp(keyword,while_keyword)==0){
		if(nl_list_len(arguments)<2){
			fprintf(stderr,"Err: too few arguments given to while statement\n");
		}else{
			nl_val *cond=arguments->d.pair.f;
			nl_val *body=arguments->d.pair.r;
			nl_val *post_loop=NULL;
			
			arguments=arguments->d.pair.r;
			while((arguments!=NULL) && (arguments->t==PAIR)){
				//if we found an "after" then shove everything else in the post-loop and break
				if((arguments->d.pair.f!=NULL) && (arguments->d.pair.f->t==SYMBOL) && (nl_val_cmp(arguments->d.pair.f,after_keyword)==0)){
					post_loop=arguments->d.pair.r;
					//separate this list from the body list
					arguments->d.pair.r=NULL;
					break;
				}
				arguments=arguments->d.pair.r;
			}
			
			cond->ref++;
			body->ref++;
			if(post_loop!=NULL){
//				post_loop->ref++;
			}
			
			//build a sub expression to evaluate
			nl_val *sub_to_eval=nl_val_malloc(PAIR);
			//keyword
			sub_to_eval->d.pair.f=nl_val_cp(sub_keyword);
			sub_to_eval->d.pair.r=nl_val_malloc(PAIR);
			//no arguments (closures preserve values of new vars between calls)
			sub_to_eval->d.pair.r->d.pair.f=nl_val_malloc(PAIR);
			sub_to_eval->d.pair.r->d.pair.f->d.pair.f=NULL;
			sub_to_eval->d.pair.r->d.pair.f->d.pair.r=NULL;
			//add a condition encompassing the body and post_loop
			sub_to_eval->d.pair.r->d.pair.r=nl_val_malloc(PAIR);
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f=nl_val_malloc(PAIR);
			sub_to_eval->d.pair.r->d.pair.r->d.pair.r=NULL;
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.f=nl_val_cp(if_keyword);
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.r=nl_val_malloc(PAIR);
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.r->d.pair.f=cond;
			sub_to_eval->d.pair.r->d.pair.r->d.pair.f->d.pair.r->d.pair.r=body;
			while((body!=NULL) && (body->t==PAIR)){
				if(body->d.pair.r==NULL){
					//add in the recursive call that makes it, you know, loop
					body->d.pair.r=nl_val_malloc(PAIR);
					body->d.pair.r->d.pair.f=nl_val_malloc(PAIR);
					body->d.pair.r->d.pair.f->d.pair.f=nl_val_cp(recur_keyword);
					body->d.pair.r->d.pair.f->d.pair.r=NULL;
					body->d.pair.r->d.pair.r=NULL;
					
					//if we had a post-loop clause then put it in there
					if(post_loop!=NULL){
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
			to_eval->d.pair.r=NULL;
/*
#ifdef _DEBUG
			printf("nl_eval_keyword,while debug 0; going to eval ");
			nl_out(stdout,to_eval);
			printf("\n");
#endif
*/
			//evaluate the sub expression, thereby doing the while loop via tail recursion
			return nl_eval(to_eval,env,last_exp);
		}
	//check for array statements (turns the evaluated argument list into an array then returns that)
	}else if(nl_val_cmp(keyword,array_keyword)==0){
		//first evaluate arguements
		nl_eval_elements(arguments,env);
		
		//throw them in an array to return
		ret=nl_val_malloc(ARRAY);
		while((arguments!=NULL) && (arguments->t==PAIR)){
			nl_array_push(ret,nl_val_cp(arguments->d.pair.f));
			
			arguments=arguments->d.pair.r;
		}
	//TODO: check for all other keywords
	}else{
		//in the default case check for subroutines bound to this symbol
		nl_val *prim_sub=nl_lookup(keyword,env);
		if((prim_sub!=NULL) && (prim_sub->t==PRI)){
			//do eager evaluation on arguments
			nl_eval_elements(arguments,env);
			//call apply
			ret=nl_apply(prim_sub,arguments,env,last_exp);
		}else{
			fprintf(stderr,"Err: unknown keyword ");
			nl_out(stderr,keyword);
			fprintf(stderr,"\n");
		}
	}
	
	//free the original expression (frees the entire list, keyword, arguments, and all)
	//note that this list changed as we evaluated, and is likely not exactly what we started with
	//	^ that's a good thing, because what we started with was free'd in that case
	nl_val_free(keyword_exp);
	
	return ret;
}

//evaluate the given expression in the given environment
nl_val *nl_eval(nl_val *exp, nl_env_frame *env, char last_exp){
	nl_val *ret=NULL;
	
tailcall:
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
//			exp->ref++;
//			ret=exp;
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
					ret=NULL;
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
			if((exp->d.pair.f!=NULL) && (exp->d.pair.f->t==SYMBOL)){
				return nl_eval_keyword(exp,env,last_exp);
			//otherwise eagerly evaluate then call out to apply
			}else if(exp->d.pair.f!=NULL){
				//evaluate the first element, the thing we're going to apply to the arguments
				exp->d.pair.f->ref++;
				nl_val *sub=nl_eval(exp->d.pair.f,env,last_exp);
				
				//do eager evaluation on arguments
				nl_eval_elements(exp->d.pair.r,env);
				
				//if we were on the last expression then free it so the calling environment doesn't have to
/*
				if(last_exp){
					if(exp->d.pair.r!=NULL){
						exp->d.pair.r->ref++;
					}
					nl_val_free(exp);
				}
*/
				
				//if this is the last expression then it doesn't need any environment trickery and we can just execute the body directly
				if((last_exp) && (sub->t==SUB)){
					if(!nl_bind_list(sub->d.sub.args,exp->d.pair.r,env)){
						fprintf(stderr,"Err: could not bind arguments to application environment (call stack) from eval\n");
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
					printf("nl_eval debug 0, executing tailcall via goto with ");
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
					int recursive_refs=0;
					if(sub->t==SUB){
						//check for instances of recursion
						recursive_refs=nl_list_occur(sub->d.sub.body,sub);
					//primitive subs are never recursive, at least not from our point of view
					}
					
					//if this is the only reference to this sub and we'll free it after this call, but it's recurisve
					if((sub->ref==1) && (recursive_refs>0)){
						//add however many recursive calls
//						sub->ref+=recursive_refs;
						
						//add a self-reference
						sub->ref++;

						//call out to apply; this will run through the body (in the case of a closure)
						ret=nl_apply(sub,exp->d.pair.r,env,last_exp);
						
						//now clean up the sub, since we're done with the extra refs
						//if this is being free'd then recursive free calls should clean up ALL extra refs
//						nl_val_free(sub);
						
						//free the original sub expression and substitute in the closure so it can be free'd later
						nl_val_free(exp->d.pair.f);
						exp->d.pair.f=sub;
					}else{
						//call out to apply; this will run through the body (in the case of a closure)
						ret=nl_apply(sub,exp->d.pair.r,env,last_exp);
					}
				}
/*
#ifdef _DEBUG
				printf("nl_eval debug 1, returned from apply with value ");
				nl_out(stdout,ret);
				printf("\n");
#endif
*/
				//clean up the evaluation of the first element; if we use it again we'll re-make it with another eval call
				nl_val_free(sub);
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
			if(ret!=NULL && !((ret->t==PRI) || (ret->t==SUB))){
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
	printf("nl_eval debug 2, returning from eval...\n");
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

//skip fp past any leading whitespaces
void nl_skip_whitespace(FILE *fp){
	char c=getc(fp);
	while(nl_is_whitespace(c)){
		if(c=='\n'){
			line_number++;
		}
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
	}else if(c=='\n'){
		line_number++;
	}
	
	//incorporate negative values if a negative sign preceded the expression
	if(negative){
		(ret->d.num.n)*=(-1);
	}
	
	if(ret->d.num.d==0){
		fprintf(stderr,"Err: divide-by-0 in rational number; did you forget a denominator?\n");
		nl_val_free(ret);
		ret=NULL;
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
		
		if(c=='\n'){
			line_number++;
		}
		
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
		if(c=='\n'){
			line_number++;
		}
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

//read a symbol (just a string with a wrapper) (with a rapper? drop them beats man)
nl_val *nl_read_symbol(FILE *fp){
	nl_val *ret=NULL;
	
	char c;
	
	//allocate a symbol for the return
	ret=nl_val_malloc(SYMBOL);
	
	//allocate a byte array for the name
	nl_val *name=nl_val_malloc(ARRAY);
	
	c=getc(fp);
	
	//read until whitespace or list-termination character
	while(!nl_is_whitespace(c) && c!=')'){
		//an alternate list syntax has the symbol come first, followed by an open paren
		if(c=='('){
			ret->d.sym.name=name;
			nl_val *symbol=ret;
			
			//read the rest of the list
			ungetc(c,fp);
			nl_val *list_remainder=nl_read_exp_list(fp);
			
			ret=nl_val_malloc(PAIR);
			ret->d.pair.f=symbol;
			ret->d.pair.r=list_remainder;
			return ret;
		}
		nl_val *ar_entry=nl_val_malloc(BYTE);
		ar_entry->d.byte.v=c;
		nl_array_push(name,ar_entry);
		
		c=getc(fp);
	}
	
	if(c==')'){
		ungetc(c,fp);
	}else if(c=='\n'){
		line_number++;
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
		line_number++;
		ret=nl_read_exp(fp);
	//the /* ... */ multi-line comment style, as in gnu89 C
	}else if(c=='/' && next_c=='*'){
		next_c=getc(fp);
		next_c=getc(fp);
		
		while(!((c=='*') && (next_c=='/'))){
			c=next_c;
			next_c=getc(fp);
			if(next_c=='\n'){
				line_number++;
			}
		}
		ret=nl_read_exp(fp);
	//if it starts with a $ read an evaluation (a symbol to be looked up)
	}else if(c=='$'){
		//read an evaluation
		ret=nl_val_malloc(EVALUATION);
		nl_val *symbol=nl_read_symbol(fp);
		
		if(symbol->t==SYMBOL){
			ret->d.eval.sym=symbol;
		//read symbol can also return a list, for alternate syntax calls with parens AFTER the symbol, so handle that
		}else if(symbol->t==PAIR){
			ret->d.eval.sym=symbol->d.pair.f;
			symbol->d.pair.f=ret;
			ret=symbol;
		}else{
			fprintf(stderr,"Err: could not read evaluation (internal parsing error)\n");
			nl_val_free(ret);
			ret=NULL;
		}
	//if it starts with anything not already handled read a symbol
	}else{
		ungetc(c,fp);
		ret=nl_read_symbol(fp);
//	}else{
//		fprintf(stderr,"Err: invalid symbol at start of expression, \'%c\' (will start reading next expression)\n",c);
	}
	
	//NOTE: whitespace is discarded before we try to read the next expression
	
	return ret;
}

//output a neulang value
void nl_out(FILE *fp, const nl_val *exp){
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
			//this is also linked list output; it's a little more complex to make it pretty because this is good for debugging and just generally
			fprintf(fp,"(");
			nl_out(fp,exp->d.pair.f);
			{
				exp=exp->d.pair.r;
				
				//linked-list output
				int len=0;
				while((exp!=NULL) && (exp->t==PAIR)){
					fprintf(fp," ");
					nl_out(fp,exp->d.pair.f);
					
					len++;
					exp=exp->d.pair.r;
				}
				
				//normal pair output (a cons cell, in lisp terminology)
				if(len==0){
					fprintf(fp," . ");
					nl_out(fp,exp);
				}
			}
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
/*
#ifdef _DEBUG
	fprintf(fp," (allocated on line %u) ",exp->line);
#endif
*/
}

//END I/O SUBROUTINES ---------------------------------------------------------------------------------------------

//create global symbol data so it's not constantly being re-allocated (which is slow and unnecessary)
void nl_keyword_malloc(){
	true_keyword=nl_sym_from_c_str("TRUE");
	false_keyword=nl_sym_from_c_str("FALSE");
	null_keyword=nl_sym_from_c_str("NULL");
	
	if_keyword=nl_sym_from_c_str("if");
	else_keyword=nl_sym_from_c_str("else");
	exit_keyword=nl_sym_from_c_str("exit");
	lit_keyword=nl_sym_from_c_str("lit");
	let_keyword=nl_sym_from_c_str("let");
	sub_keyword=nl_sym_from_c_str("sub");
	begin_keyword=nl_sym_from_c_str("begin");
	recur_keyword=nl_sym_from_c_str("recur");
	return_keyword=nl_sym_from_c_str("return");
	while_keyword=nl_sym_from_c_str("while");
	after_keyword=nl_sym_from_c_str("after");
	array_keyword=nl_sym_from_c_str("array");
}

//free global symbol data for clean exit
void nl_keyword_free(){
	nl_val_free(true_keyword);
	nl_val_free(false_keyword);
	nl_val_free(null_keyword);
	
	nl_val_free(if_keyword);
	nl_val_free(else_keyword);
	nl_val_free(exit_keyword);
	nl_val_free(lit_keyword);
	nl_val_free(let_keyword);
	nl_val_free(sub_keyword);
	nl_val_free(begin_keyword);
	nl_val_free(recur_keyword);
	nl_val_free(return_keyword);
	nl_val_free(while_keyword);
	nl_val_free(after_keyword);
	nl_val_free(array_keyword);
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
	nl_bind_new(nl_sym_from_c_str("="),nl_primitive_wrap(nl_eq),env);
	nl_bind_new(nl_sym_from_c_str(">"),nl_primitive_wrap(nl_gt),env);
	nl_bind_new(nl_sym_from_c_str("<"),nl_primitive_wrap(nl_lt),env);
	nl_bind_new(nl_sym_from_c_str(","),nl_primitive_wrap(nl_array_cat),env);
	
	nl_bind_new(nl_sym_from_c_str("ar-sz"),nl_primitive_wrap(nl_array_size),env);
	
	nl_bind_new(nl_sym_from_c_str("strout"),nl_primitive_wrap(nl_strout),env);
	nl_bind_new(nl_sym_from_c_str("out"),nl_primitive_wrap(nl_output),env);
	
	nl_bind_new(nl_sym_from_c_str("int->byte"),nl_primitive_wrap(nl_int_to_byte),env);
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
	
	//allocate keywords
	nl_keyword_malloc();
	
	//create the global environment
	nl_env_frame *global_env=nl_env_frame_malloc(NULL);
	
	//bind the standard library functions in the global environment
	nl_bind_stdlib(global_env);
	
	//initialize the line number
//	line_number=0;
	line_number=1;
	
	end_program=FALSE;
	while(!end_program){
		printf("[line %u] nl >> ",line_number);
		
		//read an expression in
		nl_val *exp=nl_read_exp(fp);
		
//		printf("\n");
		
		//evalute the expression in the global environment
		nl_val *result=nl_eval(exp,global_env,FALSE);
		
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
	
	//free (de-allocate) keywords
	nl_keyword_free();
	
	//if we were reading from a file then close that file
	if((fp!=NULL) && (fp!=stdin)){
		fclose(fp);
	}
	
	//play nicely on *nix
	return 0;
}

