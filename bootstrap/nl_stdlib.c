#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

#include "nl_structures.h"

//BEGIN STRING<->EXP I/O SUBROUTINES  -----------------------------------------------------------------------------

//gets an entry out of the string at the given position or returns NULL if position is out of bounds
//NOTE: this does NOT do type checking at the moment, use with care
char nl_str_char_or_null(nl_val *string, unsigned int pos){
	if(pos<(string->d.array.size)){
		return string->d.array.v[pos]->d.byte.v;
	}
	return '\0';
}

//skip past any leading whitespaces in the string
void nl_str_skip_whitespace(nl_val *input_string, unsigned int *persistent_pos){
	unsigned int pos=(*persistent_pos);
	
	char c=nl_str_char_or_null(input_string,pos);
	while(nl_is_whitespace(c) && (c!='\0')){
		if(c=='\n'){
			line_number++;
		}
		pos++;
		c=nl_str_char_or_null(input_string,pos);
	}
	
	(*persistent_pos)=pos;
}

//read a number from a string
nl_val *nl_str_read_num(nl_val *input_string, unsigned int *persistent_pos){
	unsigned int pos=(*persistent_pos);
	
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
	
	char c=nl_str_char_or_null(input_string,pos);
	pos++;
	
	//handle negative signs
	if(c=='-'){
		negative=TRUE;
		c=nl_str_char_or_null(input_string,pos);
		pos++;
	}
	
	//handle numerical expressions starting with . (absolute value less than 1)
	if(c=='.'){
		reading_decimal=TRUE;
		c=nl_str_char_or_null(input_string,pos);
		pos++;
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
			fprintf(stderr,"Err [line %u]: invalid character in numeric literal, \'%c\'\n",line_number,c);
#ifdef _STRICT
			exit(1);
#endif
		}
		
		c=nl_str_char_or_null(input_string,pos);
		if(c=='\0'){
			break;
		}
		pos++;
	}
	
	if(c==')'){
		if(pos>0){
			pos--;
		}
	}else if(c=='\n'){
		line_number++;
	}
	
	//incorporate negative values if a negative sign preceded the expression
	if(negative){
		(ret->d.num.n)*=(-1);
	}
	
	if(ret->d.num.d==0){
		ERR_EXIT(ret,"divide-by-0 in rational number; did you forget a denominator?",TRUE);
		nl_val_free(ret);
		ret=NULL;
	}else{
		//gcd reduce the number for faster computation
		nl_gcd_reduce(ret);
	}
	
	(*persistent_pos)=pos;
	return ret;
}

//read a string (byte array) from a string (trust me it makes sense)
nl_val *nl_str_read_string(nl_val *input_string, unsigned int *persistent_pos){
	unsigned int pos=(*persistent_pos);
	
	nl_val *ret=NULL;
	
	char c=nl_str_char_or_null(input_string,pos);
	if(c!='"'){
		fprintf(stderr,"Err [line %u]: string literal didn't start with \"; WHAT DID YOU DO? (started with \'%c\')\n",line_number,c);
#ifdef _STRICT
		exit(1);
#endif
		return ret;
	}
	pos++;
	
	//allocate a byte array
	ret=nl_val_malloc(ARRAY);
	
	if(pos<input_string->d.array.size){
		c=input_string->d.array.v[pos]->d.byte.v;
	}else{
		nl_val_free(ret);
		return NULL;
	}
	
	//read until end quote, THERE IS NO ESCAPE
	while(c!='"'){
		nl_val *ar_entry=nl_val_malloc(BYTE);
		ar_entry->d.byte.v=c;
		nl_array_push(ret,ar_entry);
		
		if(c=='\n'){
			line_number++;
		}
		
		pos++;
		if(pos<input_string->d.array.size){
			c=input_string->d.array.v[pos]->d.byte.v;
		}else{
			nl_val_free(ret);
			return NULL;
		}
	}
	if(c=='"'){
		pos++;
	}
	
	(*persistent_pos)=pos;
	return ret;
}

//read a single character (byte) from a string
nl_val *nl_str_read_char(nl_val *input_string, unsigned int *persistent_pos){
	unsigned int pos=(*persistent_pos);
	
	nl_val *ret=NULL;
	
	char c=nl_str_char_or_null(input_string,pos);
	pos++;
	if(c!='\''){
		fprintf(stderr,"Err [line %u]: character literal didn't start with \'; WHAT DID YOU DO? (started with \'%c\')\n",line_number,c);
#ifdef _STRICT
		exit(1);
#endif
		return ret;
	}
	
	//allocate a byte
	ret=nl_val_malloc(BYTE);
	
	c=nl_str_char_or_null(input_string,pos);
	pos++;
	ret->d.byte.v=c;
	
	c=nl_str_char_or_null(input_string,pos);
	pos++;
	if(c!='\''){
		fprintf(stderr,"Warn [line %u]: single-character literal didn't end with \'; (ended with \'%c\')\n",line_number,c);
#ifdef _STRICT
		exit(1);
#endif
		if(c=='\n'){
			line_number++;
		}
	}
	
	(*persistent_pos)=pos;
	return ret;
}

//read an expression list from a string
nl_val *nl_str_read_exp_list(nl_val *input_string, unsigned int *persistent_pos){
	unsigned int pos=(*persistent_pos);
	
	nl_val *ret=NULL;
	
	char c=nl_str_char_or_null(input_string,pos);
	if(c!='('){
		fprintf(stderr,"Err [line %u]: expression list didn't start with (; WHAT DID YOU DO? (started with \'%c\')\n",line_number,c);
#ifdef _STRICT
		exit(1);
#endif
		return ret;
	}
	pos++;
	
	//allocate a pair (the first element of a linked list)
	ret=nl_val_malloc(PAIR);
	
	nl_val *list_cell=ret;
	
	//read the first expression
	nl_val *next_exp=nl_str_read_exp(input_string,&pos);
	
	//continue reading expressions until we hit a NULL
	while(next_exp!=NULL){
		list_cell->d.pair.f=next_exp;
		list_cell->d.pair.r=NULL;
		
		next_exp=nl_str_read_exp(input_string,&pos);
		if(next_exp!=NULL){
			list_cell->d.pair.r=nl_val_malloc(PAIR);
			list_cell=list_cell->d.pair.r;
		}
	}
	//increment position to past the list termination character
	if((pos+1)<(input_string->d.array.size)){
		pos++;
	}
	
	//return a reference to the first list element
	(*persistent_pos)=pos;
	return ret;
}

//read a symbol (just a string with a wrapper) (with a rapper? drop them beats man) from a string
nl_val *nl_str_read_symbol(nl_val *input_string, unsigned int *persistent_pos){
	unsigned int pos=(*persistent_pos);
	
	nl_val *ret=NULL;
	
	char c;
	
	//allocate a symbol for the return
	ret=nl_val_malloc(SYMBOL);
	
	//allocate a byte array for the name
	nl_val *name=nl_val_malloc(ARRAY);
	
	c=nl_str_char_or_null(input_string,pos);
	pos++;
	
	//read until whitespace or list-termination character
	while(!nl_is_whitespace(c) && c!=')'){
		//an alternate list syntax has the symbol come first, followed by an open paren
		if(c=='('){
			ret->d.sym.name=name;
			nl_val *symbol=ret;
			
			//read the rest of the list
			pos--;
			nl_val *list_remainder=nl_str_read_exp_list(input_string,&pos);
			
			//return by pointer (must be set before a return)
			(*persistent_pos)=pos;
			
			ret=nl_val_malloc(PAIR);
			ret->d.pair.f=symbol;
			ret->d.pair.r=list_remainder;
			return ret;
		}
		nl_val *ar_entry=nl_val_malloc(BYTE);
		ar_entry->d.byte.v=c;
		nl_array_push(name,ar_entry);
		
		c=nl_str_char_or_null(input_string,pos);
		pos++;
	}
	
	if(c==')'){
		pos--;
	}else if(c=='\n'){
		line_number++;
	}
	
	(*persistent_pos)=pos;
	
	//encapsulate the name string in a symbol and return
	ret->d.sym.name=name;
	return ret;
}

//read an expression from an existing neulang string
//takes an input string, a position to start at (0 for whole string) and returns the new expression
//start_pos is set to the end of the first expression read when a full expression is found
nl_val *nl_str_read_exp(nl_val *input_string, unsigned int *persistent_pos){
	//position in string
	unsigned int pos=(*persistent_pos);
	
	//first ensure the string is valid
	if((input_string==NULL) || (input_string->t!=ARRAY)){
		ERR_EXIT(input_string,"null or incorrect type given to str_read_exp",TRUE);
		return NULL;
	}
	
	unsigned int n;
	for(n=0;n<input_string->d.array.size;n++){
		if((input_string->d.array.v[n]==NULL) || (input_string->d.array.v[n]->t!=BYTE)){
			ERR_EXIT(input_string->d.array.v[n],"invalid string entry (null or non-byte) given to str_read_exp",TRUE);
			return NULL;
		}
	}
	
	//a position past the end of the array is just a NULL value
	if((input_string->d.array.size)<=pos){
		return NULL;
	}
	
/*
#ifdef _DEBUG
	printf("nl_str_read_exp debug 0, reading an expression from ");
	nl_out(stdout,input_string);
	printf(" with starting position %u\n",pos);
#endif
*/
	
	//initialize the return to null
	nl_val *ret=NULL;
	
	//skip past any whitespace any time we go to read an expression
	nl_str_skip_whitespace(input_string,&pos);
	
	//if we hit the end of the string then exit
	if((input_string->d.array.size)<=pos){
		return NULL;
	}
	
	//read a character from the given string
	char c=nl_str_char_or_null(input_string,pos);
	
	//peek a character after that, for two-char tokens
	char next_c=nl_str_char_or_null(input_string,pos+1);
	
	//if it starts with a digit or '.' then it's a number, or if it starts with '-' followed by a digit
	if(isdigit(c) || (c=='.') || ((c=='-') && isdigit(next_c))){
		ret=nl_str_read_num(input_string,&pos);
	//if it starts with a quote read a string (byte array)
	}else if(c=='"'){
		ret=nl_str_read_string(input_string,&pos);
	//if it starts with a single quote, read a character (byte)
	}else if(c=='\''){
		ret=nl_str_read_char(input_string,&pos);
	//if it starts with a ( read a compound expression (a list of expressions)
	}else if(c=='('){
		ret=nl_str_read_exp_list(input_string,&pos);
	//an empty list is just a NULL value, so leave ret as NULL
	}else if(c==')'){
		
	//the double-slash denotes a comment, as in gnu89 C
	}else if(c=='/' && next_c=='/'){
		//ignore everything until the next newline, then try to read again
		while(c!='\n'){
			if((pos+1)<(input_string->d.array.size)){
				c=input_string->d.array.v[pos+1]->d.byte.v;
				pos++;
			}else{
				c='\n';
			}
		}
		line_number++;
		//skip past the newline that terminated the comment
		pos++;
		ret=nl_str_read_exp(input_string,&pos);
	//the /* ... */ multi-line comment style, as in gnu89 C
	}else if(c=='/' && next_c=='*'){
		pos++;
		pos++;
		c=nl_str_char_or_null(input_string,pos);
		pos++;
		next_c=nl_str_char_or_null(input_string,pos);
		
//		if(next_c=='\n'){
//			line_number++;
//		}
		
//		printf("reading multi-line comment, c=%c, next_c=%c\n",c,next_c);
		while(!((c=='*') && (next_c=='/'))){
			c=next_c;
			next_c=nl_str_char_or_null(input_string,pos+1);
			if((c=='\0') || (next_c=='\0')){
//				printf("hit a null byte\n");
				break;
			}
//			printf("next_c=%c\n",next_c);
			pos++;
			
			if(next_c=='\n'){
				line_number++;
			}
		}
		//if we didn't hit the end of the string, skip past the / character that terminated the comment
		if((pos+1)<input_string->d.array.size){
			pos++;
		}
		ret=nl_str_read_exp(input_string,&pos);
	//if it starts with a $ read an evaluation (a symbol to be looked up)
	}else if(c=='$'){
		//read an evaluation
		pos++;
		ret=nl_val_malloc(EVALUATION);
		nl_val *symbol=nl_str_read_symbol(input_string,&pos);
		
		if(symbol->t==SYMBOL){
			ret->d.eval.sym=symbol;
		//read symbol can also return a list, for alternate syntax calls with parens AFTER the symbol, so handle that
		}else if(symbol->t==PAIR){
			ret->d.eval.sym=symbol->d.pair.f;
			symbol->d.pair.f=ret;
			ret=symbol;
		}else{
			nl_val_free(ret);
			ret=NULL;
			ERR_EXIT(NULL,"could not read evaluation (internal parsing error)",FALSE);
		}
	//if it starts with anything not already handled read a symbol
	}else{
		ret=nl_str_read_symbol(input_string,&pos);
	}
	
	//skip past trailing whitespace just for good measure
	//(this is mostly for tracking line number consistently across multiple read_exp() calls, which may or may not contain trailing newlines)
	nl_str_skip_whitespace(input_string,&pos);
	
	(*persistent_pos)=pos;
	return ret;
}

//END STRING<->EXP I/O SUBROUTINES  -------------------------------------------------------------------------------


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
		//structs are equal iff they bind all the same symbols to all the same values (i.e. all elements of env are equal)
		case STRUCT:
			{
/*
				nl_env_frame *env_a=v_a->d.nl_struct.env;
				nl_env_frame *env_b=v_b->d.nl_struct.env;
				
				//first compare symbols (if this isn't 0 then they aren't even the same structure)
				int env_eq=nl_val_cmp(env_a->symbol_array,env_b->symbol_array);
				
				//if the symbols were equal then compare values and return the result of that
				if(env_eq==0){
					env_eq=nl_val_cmp(env_a->value_array,env_b->value_array);
				}
*/
				//TODO: remove this, replace with trie comparison (and you know, implement that)
				int env_eq=-1;
				
				//return the result
				return env_eq;
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

//returns the list equivalent to the given array(s)
//if multiple arrays are given they will be flattened into one sequential list
nl_val *nl_array_to_list(nl_val *arg_list){
	nl_val *ret=NULL;
	if(nl_c_list_size(arg_list)<1){
		ERR_EXIT(arg_list,"no arguments given to array_to_list, can't convert NULL (returning NULL)",TRUE);
		return NULL;
	}
	
	//start the list to return
	nl_val *current_cell=nl_val_malloc(PAIR);
	current_cell->d.pair.f=NULL;
	current_cell->d.pair.r=NULL;
	
	//set the return value to the head of this list (which we will build the rest of momentarily)
	ret=current_cell;
	
	//whether this is the first array or a continuation (this changes how we pair (or don't pair) to previously accumlated lists)
	char first_ar=TRUE;
	
	//for each argument
	while((arg_list!=NULL) && (arg_list->t==PAIR)){
		nl_val *current_ar=arg_list->d.pair.f;
		if((current_ar==NULL) || (current_ar->t!=ARRAY)){
			nl_val_free(ret);
			ERR_EXIT(current_ar,"non-array given to array_to_list conversion",TRUE);
			return NULL;
		}
		
		//handle for multiple arrays (current_cell should be updated accordingly)
		if(!first_ar){
			current_cell->d.pair.r=nl_val_malloc(PAIR);
			current_cell=current_cell->d.pair.r;
		}
		
		//go through each array element and add a copy to the list to return
		unsigned int n;
		for(n=0;n<current_ar->d.array.size;n++){
			current_cell->d.pair.f=nl_val_cp(current_ar->d.array.v[n]);
			//if this isn't the last element then make a container for the next element
			if((n+1)<current_ar->d.array.size){
				current_cell->d.pair.r=nl_val_malloc(PAIR);
				current_cell=current_cell->d.pair.r;
			//if this is the last element then null terminate
			}else{
				current_cell->d.pair.r=NULL;
			}
		}
		//I don't know if it was first_ar or not prior to this but it sure isn't now
		first_ar=FALSE;
		
		arg_list=arg_list->d.pair.r;
	}
	
	return ret;
}

//returns the array equivalent to the given list(s)
//if multiple lists are given they will be flattened into one sequential array
nl_val *nl_list_to_array(nl_val *arg_list){
	nl_val *ret=NULL;
	if(nl_c_list_size(arg_list)<1){
		ERR_EXIT(arg_list,"no arguments given to list_to_array, can't convert NULL (returning NULL)",TRUE);
		return NULL;
	}
	
	//create an array to return
	ret=nl_val_malloc(ARRAY);
	
	//for each argument
	while((arg_list!=NULL) && (arg_list->t==PAIR)){
		nl_val *current_list=arg_list->d.pair.f;
		if((current_list==NULL) || (current_list->t!=PAIR)){
			nl_val_free(ret);
			ERR_EXIT(current_list,"non-list given to list_to_array conversion",TRUE);
			return NULL;
		}
		
		//go through each list element and add a copy to the array to return
		while((current_list!=NULL) && (current_list->t==PAIR)){
			nl_array_push(ret,nl_val_cp(current_list->d.pair.f));
			current_list=current_list->d.pair.r;
		}
		
		arg_list=arg_list->d.pair.r;
	}
	
	return ret;
}

//returns the string-encoded version of any given expression
nl_val *nl_val_to_memstr(const nl_val *exp){
	nl_val *ret=nl_val_malloc(ARRAY);
	//output for sprintf operations
	char buffer[BUFFER_SIZE];
	//temporary string for sub-expressions
	nl_val *tmp_str;
	
/*
#ifdef _DEBUG
	printf("nl_val_to_memstr debug 0, converting ");
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
			
			tmp_str=nl_val_to_memstr(exp->d.pair.f);
			nl_str_push_nlstr(ret,tmp_str);
			nl_val_free(tmp_str);
			{
				exp=exp->d.pair.r;
				
				//linked-list conversion
				int len=0;
				while((exp!=NULL) && (exp->t==PAIR)){
					nl_str_push_cstr(ret," ");
					
					tmp_str=nl_val_to_memstr(exp->d.pair.f);
					nl_str_push_nlstr(ret,tmp_str);
					nl_val_free(tmp_str);
					
					len++;
					exp=exp->d.pair.r;
				}
				
				//normal pair conversion (a cons cell, in lisp terminology)
				if(len==0){
					nl_str_push_cstr(ret," . ");
					
					tmp_str=nl_val_to_memstr(exp);
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
//					tmp_str=nl_val_to_memstr(&(exp->d.array.v[n]));
					tmp_str=nl_val_to_memstr(exp->d.array.v[n]);
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
			nl_str_push_cstr(ret,"<closure/subroutine with args (");
			{
				nl_val *sub_args=exp->d.sub.args;
				while(sub_args!=NULL){
					tmp_str=nl_val_to_memstr(sub_args->d.pair.f);
					nl_str_push_nlstr(ret,tmp_str);
					nl_val_free(tmp_str);
					
					if(sub_args->d.pair.r!=NULL){
						nl_str_push_cstr(ret," ");
					}
					
					sub_args=sub_args->d.pair.r;
				}
			}
			nl_str_push_cstr(ret,")>");
			break;
		case STRUCT:
			nl_str_push_cstr(ret,"<struct ");
			{
/*
				unsigned int n;
				for(n=0;n<(exp->d.nl_struct.env->symbol_array->d.array.size);n++){
					nl_str_push_cstr(ret,"{");
					
					tmp_str=nl_val_to_memstr(exp->d.nl_struct.env->symbol_array->d.array.v[n]);
					nl_str_push_nlstr(ret,tmp_str);
					nl_val_free(tmp_str);
					
					nl_str_push_cstr(ret," ");
					
					tmp_str=nl_val_to_memstr(exp->d.nl_struct.env->value_array->d.array.v[n]);
					nl_str_push_nlstr(ret,tmp_str);
					nl_val_free(tmp_str);
					
					nl_str_push_cstr(ret,"}");
					
					if((n+1)<(exp->d.nl_struct.env->symbol_array->d.array.size)){
						nl_str_push_cstr(ret," ");
					}
				}
*/
				//TODO: implement pretty output for trie-based structs
			}
			nl_str_push_cstr(ret,">");
			break;
		case SYMBOL:
			nl_str_push_cstr(ret,"<symbol ");
			if(exp->d.sym.name!=NULL){
				unsigned int n;
				for(n=0;n<(exp->d.sym.name->d.array.size);n++){
//					tmp_str=nl_val_to_memstr(&(exp->d.sym.name->d.array.v[n]));
					tmp_str=nl_val_to_memstr(exp->d.sym.name->d.array.v[n]);
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
//					tmp_str=nl_val_to_memstr(&(exp->d.eval.sym->d.sym.name->d.array.v[n]));
					tmp_str=nl_val_to_memstr(exp->d.eval.sym->d.sym.name->d.array.v[n]);
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
nl_val *nl_val_list_to_memstr(nl_val *val_list){
	nl_val *ret=nl_val_malloc(ARRAY);
	//temporary string for sub-expressions
	nl_val *tmp_str;
	
	while(val_list!=NULL && val_list->t==PAIR){
		nl_val *exp=val_list->d.pair.f;
		
		tmp_str=nl_val_to_memstr(exp);
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
//			new_stored_size*=2;
			
			//this is an optimization to allow new allocations to use already-allocated memory from previous sizes
			new_stored_size=((3*new_stored_size)/2)+1;
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

//returns an array consisting of all elements of the starting array
//plus any elements given as arguments after that
nl_val *nl_array_extend(nl_val *arg_list){
	if(nl_c_list_size(arg_list)<1){
		ERR_EXIT(arg_list,"no arguments given to array extend operation",TRUE);
		return NULL;
	}
	
	if((arg_list->d.pair.f==NULL) || (arg_list->d.pair.f->t!=ARRAY)){
		ERR_EXIT(arg_list,"non-array given as first argument to array extend operation",TRUE);
		return NULL;
	}
	
	nl_val *ret=nl_val_cp(arg_list->d.pair.f);
	arg_list=arg_list->d.pair.r;
	
	while(arg_list!=NULL){
		nl_array_push(ret,nl_val_cp(arg_list->d.pair.f));
		arg_list=arg_list->d.pair.r;
	}
	
	return ret;
}

//returns an array consisting of all the elements of the starting array
//EXCEPT the element at the given position
nl_val *nl_array_omit(nl_val *arg_list){
	int argc=nl_c_list_size(arg_list);
	if(argc!=2){
		ERR_EXIT(arg_list,"incorrect number of arguments given to array omit operation",TRUE);
		return NULL;
	}
	nl_val *ar=arg_list->d.pair.f;
	nl_val *idx=arg_list->d.pair.r->d.pair.f;
	
	if((ar==NULL) || (ar->t!=ARRAY) || (idx==NULL) || (idx->t!=NUM)){
		ERR_EXIT(arg_list,"invalid type given to array omit operation",TRUE);
		return NULL;
	}
	
	if(idx->d.num.d!=NUM){
		ERR_EXIT(idx,"non-integer index given to array omit operation",TRUE);
		return NULL;
	}
	
	if(idx->d.num.n>=(ar->d.array.size)){
		ERR_EXIT(idx,"index given to array omit operation is larger than array size",TRUE);
		return NULL;
	}
	
	//okay now we're past the error handling and we can actually do something
	//make a new array
	nl_val *ret=nl_val_malloc(ARRAY);
	
	//push in copies of all the old values, substituting the new value where appropriate
	int n;
	for(n=0;n<(ar->d.array.size);n++){
		if(n==(idx->d.num.n)){
			//skip this one
		}else{
			nl_array_push(ret,nl_val_cp(ar->d.array.v[n]));
		}
	}
	
	return ret;
}

//TODO: write array find
//returns the index of the first occurance of the given subarray within the given array
//returns -1 for not found
nl_val *nl_array_find(nl_val *arg_list){
	nl_val *ret=NULL;
	return ret;
}

//array insert
//returns an array consisting of existing elements, plus given elements inserted at given 0-index-based position
nl_val *nl_array_insert(nl_val *arg_list){
	nl_val *ret=NULL;
	if(nl_c_list_size(arg_list)!=3){
		ERR_EXIT(arg_list,"too few arguments given to array insert operation (takes array, index, and a list of new values to insert)",TRUE);
		return NULL;
	}
	
	nl_val *base_array=arg_list->d.pair.f;
	if((base_array==NULL) || (base_array->t!=ARRAY)){
		ERR_EXIT(base_array,"non-array given as first argument to array insert operation",TRUE);
		return NULL;
	}
	
	nl_val *ins_idx=arg_list->d.pair.r->d.pair.f;
	if((ins_idx==NULL) || (ins_idx->t!=NUM) || (ins_idx->d.num.d!=1)){
		ERR_EXIT(ins_idx,"non-number or non-integer given as index argument to array insert operation",TRUE);
		return NULL;
	}
	
	//the list of new values to insert
	nl_val *new_val_list=arg_list->d.pair.r->d.pair.r->d.pair.f;
	if((new_val_list==NULL) && (new_val_list->t!=PAIR)){
		ERR_EXIT(new_val_list,"null or wrong type given to array insert, we need a LIST of new values to insert",TRUE);
		return NULL;
	}
	
	long int c_ins_idx=ins_idx->d.num.n;
	if(c_ins_idx<0){
		ERR(ins_idx,"negative index given to array insert, new elements will be added to start",TRUE);
		c_ins_idx=0;
	}
	if(c_ins_idx>(base_array->d.array.size)){
		ERR(ins_idx,"index given to array insert is past the end of the array (greater than array size), new elements will be added to end",TRUE);
		c_ins_idx=(base_array->d.array.size);
	}
	
	//do the actual insert
	//first allocate a new array to return
	ret=nl_val_malloc(ARRAY);
	
	unsigned int idx;
	//NOTE: the <= is intentional here and is what allows us to append to the end of the array
	for(idx=0;idx<=base_array->d.array.size;idx++){
		//if we hit the place to insert, then add all the given new elements here
		if(idx==c_ins_idx){
			while((new_val_list!=NULL) && (new_val_list->t==PAIR)){
				nl_array_push(ret,nl_val_cp(new_val_list->d.pair.f));
				new_val_list=new_val_list->d.pair.r;
			}
		}
		
		//if we're not past the end, then append a copy of the initial element here
		if(idx<base_array->d.array.size){
			nl_array_push(ret,nl_val_cp(base_array->d.array.v[idx]));
		}
	}
	
	return ret;
}

//array chop; takes an array and a delimiter
//returns a new array with elements of the original array in separate subarrays (i.e. explode, split)
nl_val *nl_array_chop(nl_val *arg_list){
	//check number of arguments is 2
	if(nl_c_list_size(arg_list)!=2){
		ERR_EXIT(arg_list,"incorrect number of arguments given to array chop operation (takes array and delimiter)",TRUE);
		return NULL;
	}
	
	//check no nulls
	if((arg_list->d.pair.f==NULL) || (arg_list->d.pair.r->d.pair.f==NULL)){
		ERR_EXIT(arg_list,"NULL given to array chop operation",TRUE);
		return NULL;
	}
	
	//check both are arrays
	if((arg_list->d.pair.f->t!=ARRAY) || (arg_list->d.pair.r->d.pair.f->t!=ARRAY)){
		ERR_EXIT(arg_list,"incorrect type given to array chop operation",TRUE);
		return NULL;
	}
	
	//okay, now the fun part
	//allocate a new array to store the result
	nl_val *ret=nl_val_malloc(ARRAY);
	unsigned int ret_idx=0;
	//create one entry on the array no matter what (if needle isn't found this will be haystack, if needle at pos 0 this will be empty)
	nl_array_push(ret,nl_val_malloc(ARRAY));
	
	nl_val *haystack=arg_list->d.pair.f;
	nl_val *needle=arg_list->d.pair.r->d.pair.f;
	
	//look through the haystack
	int n;
	for(n=0;n<(haystack->d.array.size);n++){
		//look through the needle to see if it corresponds to the haystack
		int n2;
		for(n2=0;n2<(needle->d.array.size);n2++){
			//if we found a non-matching entry then the needle wasn't found
			
			//needle goes past end of haystack, break early
			if((n+n2)>=(haystack->d.array.size)){
				break;
			//if they're both not null then check values
			}else if((haystack->d.array.v[n+n2]!=NULL) && (needle->d.array.v[n2]!=NULL)){
				//if the values are of different types they cannot be equal
				if((haystack->d.array.v[n+n2]->t)!=(needle->d.array.v[n2]->t)){
					break;
				}
				
				//if they values aren't equal they aren't equal (tautologically :P)
				if(nl_val_cmp(haystack->d.array.v[n+n2],needle->d.array.v[n2])!=0){
					break;
				}
				
			//if one, but not both, entries are null, then we DIDN'T find the needle
			}else if((haystack->d.array.v[n+n2]==NULL) || (needle->d.array.v[n2]==NULL)){
				break;
			//two nulls are equal
			}
		}
		//if we got through that whole above loop without breaking then we found the needle here
		if(n2==(needle->d.array.size)){
			//skip over the section with the needle
			if(needle->d.array.size>0){
				n+=((needle->d.array.size)-1);
			}
			
			//update the return to include a new array entry
			nl_array_push(ret,nl_val_malloc(ARRAY));
			ret_idx++;
			
			//and start again
			continue;
		}
		
		//if we got here and didn't continue, then we didn't find the needle
		//therefore shove the haystack element onto the return array
		nl_array_push(ret->d.array.v[ret_idx],nl_val_cp(haystack->d.array.v[n]));
	}
	
	return ret;
}

//TODO: write subarray
//subarray
//returns a new array consisting of a subset of the given array (all elements within the given inclusive range)
nl_val *nl_array_subarray(nl_val *arg_list){
	nl_val *ret=NULL;
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

//TODO: add an argument for line-editing here! and one for file to read from
//reads a line from stdin and returns the result as a [neulang] string
nl_val *nl_inline(nl_val *arg_list){
	char line_edit=FALSE;
	//line edit keyword
	nl_val *ledit_keyword=nl_sym_from_c_str("ledit");
	
	int argc=nl_c_list_size(arg_list);
	if((argc>=1) && (arg_list->d.pair.f!=NULL) && (arg_list->d.pair.f->t==SYMBOL) && (nl_val_cmp(arg_list->d.pair.f,ledit_keyword)==0)){
		line_edit=TRUE;
	}
	
	nl_val *ret=nl_val_malloc(ARRAY);
	
	int c=fgetc(stdin);
	while((c!='\n') && (c!='\r')){
		if(feof(stdin)){
			break;
		}
		
		//TODO: add line editing functions here (continue loop when done)
		if(line_edit){
			if(c & 0x80){
				printf("\ngot an escape char %c (%i)\n",c,c);
			}else{
				printf("\ngot a normal char %c (%i)\n",c,c);
			}
		}
		
		nl_val *next_char=nl_val_malloc(BYTE);
		next_char->d.byte.v=(unsigned char)(c);
		
		nl_array_push(ret,next_char);
		
		c=fgetc(stdin);
	}
	
	nl_val_free(ledit_keyword);
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

//BEGIN C-NL-STRUCT-LIST SUBROUTINES  -----------------------------------------------------------------------------

//get the given symbols from the struct
nl_val *nl_struct_get(nl_val *sym_list){
	//note that this returns a copy of the given value but to the outside world that shouldn't be a problem
	
	int arg_count=nl_c_list_size(sym_list);
	if((arg_count<2) || (sym_list->d.pair.f->t!=STRUCT)){
		ERR_EXIT(sym_list,"incorrect argument count or type given to struct get operation",TRUE);
		return NULL;
	}
	
	if(arg_count==2){
		return nl_val_cp(nl_lookup(sym_list->d.pair.r->d.pair.f,sym_list->d.pair.f->d.nl_struct.env));
	}
	
	//if we were given >1 symbol, then return a list of all the requested values
	nl_val *ret=nl_val_malloc(PAIR);
	nl_val *current_node=ret;
	
	nl_val *current_struct=sym_list->d.pair.f;
	sym_list=sym_list->d.pair.r;
	while((sym_list!=NULL) && (sym_list->t==PAIR)){
		current_node->d.pair.f=nl_val_cp(nl_lookup(sym_list->d.pair.f,current_struct->d.nl_struct.env));
		
		if(sym_list->d.pair.r!=NULL){
			current_node->d.pair.r=nl_val_malloc(PAIR);
			current_node=current_node->d.pair.r;
		}else{
			current_node->d.pair.r=NULL;
		}
		
		sym_list=sym_list->d.pair.r;
	}
	
	return ret;
}

//return the result of replacing the given symbol with the given value in the struct
nl_val *nl_struct_replace(nl_val *rqst_list){
	if((nl_c_list_size(rqst_list)!=3) || (rqst_list->t!=PAIR) || (rqst_list->d.pair.f==NULL) || (rqst_list->d.pair.f->t!=STRUCT)){
		ERR_EXIT(rqst_list,"incorrect use of struct set operation",TRUE);
		return NULL;
	}
	
	nl_val *current_struct=rqst_list->d.pair.f;
	nl_val *symbol=rqst_list->d.pair.r->d.pair.f;
	nl_val *new_value=rqst_list->d.pair.r->d.pair.r->d.pair.f;
	
	if(!nl_bind(symbol,new_value,current_struct->d.nl_struct.env)){
		ERR_EXIT(rqst_list,"could not bind symbol in struct",TRUE);
		return NULL;
	}
	
	//return this so it can be used (remember we don't do side-effects, referential transparency and whatnot)
	//note that what was passed in was a copy (from evaluating an evaluation type) so it's okay to modify it here and return it
	current_struct->ref++;
	return current_struct;
}

//END C-NL-STRUCT-LIST SUBROUTINES  -------------------------------------------------------------------------------

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

//take the floor of a (rational) number
nl_val *nl_floor(nl_val *num_list){
	nl_val *ret=NULL;
	
	int arg_count=nl_c_list_size(num_list);
	if(arg_count>=1){
		if(num_list->d.pair.f->t==NUM){
			//floor is just integer division
			ret=nl_val_malloc(NUM);
			ret->d.num.d=1;
			ret->d.num.n=((num_list->d.pair.f->d.num.n)/(num_list->d.pair.f->d.num.d));
			
		}else{
			ERR_EXIT(num_list,"wrong type given to floor (expected NUM)",TRUE);
		}
		
		if(arg_count>1){
			ERR(num_list,"too many arguments given to floor, ignoring all but the first...",TRUE);
		}
	}else{
		ERR_EXIT(num_list,"no arguments given to floor, can't convert NULL! (returning NULL)",TRUE);
	}
	
	return ret;
}

//take the ceiling of a (rational) number
nl_val *nl_ceil(nl_val *num_list){
	nl_val *ret=NULL;
	
	int arg_count=nl_c_list_size(num_list);
	if(arg_count>=1){
		if(num_list->d.pair.f->t==NUM){
			//run gcd to ensure that if the values evenly divide the denominator is 0
			nl_gcd_reduce(num_list->d.pair.f);
			
			ret=nl_val_malloc(NUM);
			ret->d.num.d=1;
			
			//ceiling is just integer division in cases where the numbers evenly divide
			if(num_list->d.pair.f->d.num.d==1){
				ret->d.num.n=((num_list->d.pair.f->d.num.n)/(num_list->d.pair.f->d.num.d));
			//when the numbers do not evenly divide, it is integer division +1
			}else{
				ret->d.num.n=(((num_list->d.pair.f->d.num.n)/(num_list->d.pair.f->d.num.d))+1);
			}
			
		}else{
			ERR_EXIT(num_list,"wrong type given to ceil (expected NUM)",TRUE);
		}
		
		if(arg_count>1){
			ERR(num_list,"too many arguments given to ceil, ignoring all but the first...",TRUE);
		}
	}else{
		ERR_EXIT(num_list,"no arguments given to ceil, can't convert NULL! (returning NULL)",TRUE);
	}
	
	return ret;
}

//TODO: write the rest of the math library

//END C-NL-STDLIB-MATH SUBROUTINES  -------------------------------------------------------------------------------


//BEGIN C-NL-STDLIB-CMP SUBROUTINES  ------------------------------------------------------------------------------

//equality operator =
//if more than two arguments are given then this will only return true if a==b==c==... for (= a b c ...)
//checks if the values of the same type within val_list are equal
nl_val *nl_generic_eq(nl_val *val_list){
	nl_val *ret=NULL;
	
	//garbage in, garbage out; if you give us NULL we return NULL
	if(nl_contains_nulls(val_list)){
		ERR_EXIT(val_list,"a NULL argument was given to equality operator, returning NULL",TRUE);
		return NULL;
	}
	if(!((nl_c_list_size(val_list)>=2) && (val_list->d.pair.f->t==val_list->d.pair.r->d.pair.f->t))){
		ERR_EXIT(val_list,"incorrect use of eq operator =; arg count < 2 or inconsistent type",TRUE);
		return NULL;
	}
	
	ret=nl_val_malloc(BYTE);
	ret->d.byte.v=FALSE;
	
	nl_val *last_value=val_list->d.pair.f;
	val_list=val_list->d.pair.r;
	
	while((val_list!=NULL) && (val_list->t==PAIR)){
		if(last_value->t!=val_list->d.pair.f->t){
			ERR_EXIT(val_list,"inconsistent types given to eq operator =",TRUE);
			nl_val_free(ret);
			return NULL;
		}
		
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

//gt operator >
//if more than two arguments are given then this will only return true if a>b>c>... for (> a b c ...)
//checks if the values of the same type within val_list are in descending order
nl_val *nl_generic_gt(nl_val *val_list){
	nl_val *ret=NULL;
	
	//garbage in, garbage out; if you give us NULL we return NULL
	if(nl_contains_nulls(val_list)){
		ERR_EXIT(val_list,"a NULL argument was given to greater than operator, returning NULL",TRUE);
		return NULL;
	}
	if(!((nl_c_list_size(val_list)>=2) && (val_list->d.pair.f->t==val_list->d.pair.r->d.pair.f->t))){
		ERR_EXIT(val_list,"incorrect use of gt operator >; arg count < 2 or inconsistent type",TRUE);
		return NULL;
	}
	
	ret=nl_val_malloc(BYTE);
	ret->d.byte.v=FALSE;
	
	nl_val *last_value=val_list->d.pair.f;
	val_list=val_list->d.pair.r;
	
	while((val_list!=NULL) && (val_list->t==PAIR)){
		if(last_value->t!=val_list->d.pair.f->t){
			ERR_EXIT(val_list,"inconsistent types given to gt operator >",TRUE);
			nl_val_free(ret);
			return NULL;
		}
		
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

//lt operator <
//if more than two arguments are given then this will only return true if a<b<c<... for (< a b c ...)
//checks if the values of the same type within val_list are in ascending order
nl_val *nl_generic_lt(nl_val *val_list){
	nl_val *ret=NULL;
	
	//garbage in, garbage out; if you give us NULL we return NULL
	if(nl_contains_nulls(val_list)){
		ERR_EXIT(val_list,"a NULL argument was given to less than operator, returning NULL",TRUE);
		return NULL;
	}
	if(!((nl_c_list_size(val_list)>=2) && (val_list->d.pair.f->t==val_list->d.pair.r->d.pair.f->t))){
		ERR_EXIT(val_list,"incorrect use of lt operator <; arg count < 2 or inconsistent type",TRUE);
		return NULL;
	}
	
	ret=nl_val_malloc(BYTE);
	ret->d.byte.v=FALSE;
	
	nl_val *last_value=val_list->d.pair.f;
	val_list=val_list->d.pair.r;
	
	while((val_list!=NULL) && (val_list->t==PAIR)){
		if(last_value->t!=val_list->d.pair.f->t){
			ERR_EXIT(val_list,"inconsistent types given to lt operator <",TRUE);
			nl_val_free(ret);
			return NULL;
		}
		
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

//ge operator >=
//checks if list is descending or equal, >=
nl_val *nl_generic_ge(nl_val *val_list){
	nl_val *gt_result=nl_generic_gt(val_list);
	if(nl_is_true(gt_result)){
		return gt_result;
	}else{
		nl_val_free(gt_result);
		return nl_generic_eq(val_list);
	}
}

//le operator <=
//checks if list is ascending or equal, <=
nl_val *nl_generic_le(nl_val *val_list){
	nl_val *gt_result=nl_generic_lt(val_list);
	if(nl_is_true(gt_result)){
		return gt_result;
	}else{
		nl_val_free(gt_result);
		return nl_generic_eq(val_list);
	}
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

//BEGIN C-NL-STDLIB-FILE SUBROUTINES  -----------------------------------------------------------------------------

//reads the given file and returns its contents as a byte array (aka a string)
//returns NULL if file wasn't found or couldn't be opened
nl_val *nl_str_from_file(nl_val *fname_list){
	if(nl_c_list_size(fname_list)!=1){
		ERR_EXIT(fname_list,"file->str operation takes exactly one argument",TRUE);
		return NULL;
	}
	
	if((fname_list->d.pair.f==NULL) || (fname_list->d.pair.f->t!=ARRAY)){
		ERR_EXIT(fname_list->d.pair.f,"wrong type argument given to file->str, expecting byte array",TRUE);
		return NULL;
	}
	
	char *fname=c_str_from_nl_str(fname_list->d.pair.f);
	
	FILE *fp=fopen(fname,"r");
	if(fp==NULL){
		free(fname);
		ERR_EXIT(fname_list->d.pair.f,"could not open file",TRUE);
		return NULL;
	}
	
	nl_val *ret=nl_val_malloc(ARRAY);
	
	while(!feof(fp)){
		char c=fgetc(fp);
		if(feof(fp)){
			break;
		}
		
		nl_val *current_byte=nl_val_malloc(BYTE);
		current_byte->d.byte.v=c;
		nl_array_push(ret,current_byte);
	}
	
	fclose(fp);
	
	free(fname);
	return ret;
}

//END C-NL-STDLIB-FILE SUBROUTINES  -------------------------------------------------------------------------------


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

