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
//	SYMBOL, //variable names, for the symbol table, internally this is a [byte]array
//	EVALUATION, //to-be-evaluated symbol, denoted $symbol
} nl_type;

//environment frame (one global, then one per closure)
typedef struct nl_env_frame {
	//TODO: store symbols
	//TODO: store values
} nl_env_frame;

//a primitive value structure, the basic unit of evalution in neulang
typedef struct nl_val nl_val;
struct nl_val {
	nl_type t;
	
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
	} d;
};

//END DATA STRUCTURES ---------------------------------------------------------------------------------------------

//BEGIN FORWARD DECLARATIONS --------------------------------------------------------------------------------------
void nl_out(FILE *fp, nl_val *exp);
//END FORWARD DECLARATIONS ----------------------------------------------------------------------------------------


//BEGIN GLOBAL DATA -----------------------------------------------------------------------------------------------
//END GLOBAL DATA -------------------------------------------------------------------------------------------------

//BEGIN MEMORY MANAGEMENT SUBROUTINES -----------------------------------------------------------------------------

nl_val *nl_val_malloc(nl_type t){
	nl_val *ret=malloc(sizeof(nl_val));
	ret->t=t;
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
		default:
			break;
	}
	
	return ret;
}

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
		default:
			break;
	}
	
	free(exp);
}

//END MEMORY MANAGEMENT SUBROUTINES -------------------------------------------------------------------------------

//BEGIN C-NL-STDLIB SUBROUTINES -----------------------------------------------------------------------------------

//gcd-reduce a rational number
void nl_gcd_reduce(nl_val *v){
	//ignore null and non-rational values
	if((v==NULL) || (v->t!=NUM)){
		return;
	}
	
	//find greatest common divisor of numerator and denominator
	long long int a=v->d.num.n;
	long long int b=v->d.num.d;
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
		free(a->d.array.v);
		a->d.array.v=new_array_v;
		
		//update size parameters
		a->d.array.stored_size=new_stored_size;
	}
	
	//update size
	a->d.array.size=new_size;
	
	//copy in the new data
	a->d.array.v[(new_size-1)]=v;
}

//pop a value off of the end of an array
void nl_array_pop(nl_val *a){
	
}



//END C-NL-STDLIB SUBROUTINES -------------------------------------------------------------------------------------


//BEGIN EVALUTION SUBROUTINES -------------------------------------------------------------------------------------
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
	
	//read a number
	while(!nl_is_whitespace(c)){
		//denominator
		if(isdigit(c) && !(reading_denom) && !(reading_decimal)){
			(ret->d.num.n)*=10;
			(ret->d.num.n)+=(c-'0');
		//numerator
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

//TODO: WRITE THIS!!!!
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
/*
	ret->d.array.size=3;
	ret->d.array.v=malloc((ret->d.array.size)*(sizeof(nl_val*)));
	ret->d.array.stored_size=3;
	int n;
	for(n=0;n<(ret->d.array.size);n++){
		ret->d.array.v[n]=nl_val_malloc(BYTE);
	}
	ret->d.array.v[1]->d.byte.v=1;
	ret->d.array.v[2]->d.byte.v='a';
*/
	
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

//TODO: WRITE THIS!!!!
//read an expression from the given input stream
nl_val *nl_read_exp(FILE *fp){

	
/*
	//TODO: remove this, it's just for debugging
	nl_val *ret=nl_val_malloc(ARRAY);
	ret->d.array.size=3;
	ret->d.array.v=malloc((ret->d.array.size)*(sizeof(nl_val*)));
	int n;
	for(n=0;n<(ret->d.array.size);n++){
		ret->d.array.v[n]=nl_val_malloc(BYTE);
	}
	ret->d.array.v[1]->d.byte.v=1;
	ret->d.array.v[2]->d.byte.v='a';
*/
	
/*
	//TODO: remove this, it's just for debugging
	nl_val *ret=nl_val_malloc(PAIR);
	ret->d.pair.f=nl_val_malloc(BYTE);
	ret->d.pair.f->d.byte.v='B';
	ret->d.pair.r=NULL;
	
*/
	//initialize the return to null
	nl_val *ret=NULL;
	
	//skip past any whitespace any time we go to read an expression
	nl_skip_whitespace(fp);
	
	//read a character from the given file
	char c=getc(fp);
	
	//if it starts with a digit or '-' then it's a number
	if(isdigit(c) || (c=='-')){
		ungetc(c,fp);
		ret=nl_read_num(fp);
	//if it starts with a quote read a string (byte array)
	}else if(c=='"'){
		ungetc(c,fp);
		ret=nl_read_string(fp);
	//if it starts with a single quote, read a character (byte)
	}else if(c=='\''){
		ungetc(c,fp);
//		ret=nl_read_char(fp);
	//if it starts with a ( it's a compound expression (a list of expressions)
	}else if(c=='('){
		ungetc(c,fp);
//		ret=nl_read_exp_list(fp);
		
	//TODO: if it starts with a $ read an evaluation
//	}else if(){
		
	//TODO: if it starts with a letter read a symbol
//	}else if(){
		
	}else{
		fprintf(stderr,"Err: invalid symbol at start of expression, \'%c\' (will start reading again from next character)\n",c);
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
			fprintf(fp,"primitive procedure");
			break;
		case SUB:
			fprintf(fp,"closure");
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
	nl_env_frame *global_env=malloc(sizeof(nl_env_frame));
	
	char end_program=FALSE;
	while(!end_program){
		printf("nl >> ");
		
		//read an expression in
		nl_val *exp=nl_read_exp(stdin);
		
		printf("\n");
		
		//TODO: remove this it's only for debugging
		nl_out(stdout,exp);
		nl_val_free(exp);
		
/*
		//evalute the expression in the global environment
		nl_val *result=nl_eval(exp,global_env);
		
		//free the original expression
		nl_free(exp);
		
		//output the result of evaluation
		nl_out(stdout,result);
		
		//free the resulting expression
		nl_val_free(result);
*/
		printf("\n");
		
		//TODO: remove this, it's just for debugging
//		end_program=TRUE;
	}
	
	//de-allocate the global environment
	free(global_env);
	
	//play nicely on *nix
	return 0;
}

