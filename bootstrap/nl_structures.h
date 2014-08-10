
#ifndef _NL_STRUCTURES
#define _NL_STRUCTURES

//BEGIN GLOBAL CONSTANTS ------------------------------------------------------------------------------------------

#define VERSION "0.1.0"
#define TRUE 1
#define FALSE 0

//END GLOBAL CONSTANTS --------------------------------------------------------------------------------------------

//BEGIN GLOBAL MACROS ---------------------------------------------------------------------------------------------

#define ERR(val,msg,output) nl_err(val,msg,output)
#ifdef _STRICT
	#define ERR_EXIT(val,msg,output) ERR(val,msg,output); exit(1)
#else
	#define ERR_EXIT(val,msg,output) ERR(val,msg,output)
#endif

//END GLOBAL MACROS -----------------------------------------------------------------------------------------------

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
	
	//the line this value was allocated on
	unsigned int line;
	
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
		//TODO: make this continguous instead of an array of pointers to who-knows-where (I don't know how feasible this is due to other handling, I tried it once and it kinda failed hard, that might just be from trying to free the first entry or something; also I'm not sure how to store NULLs)
		//note that all neulang arrays internally store a size
		struct {
			//sub-type of the array
//			nl_type t;
			
			//the memory itself and the number of elements stored
			nl_val **v;
//			nl_val *v;
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
//			nl_type t;
			
			//arguments (linked list of symbols to bind to values during apply)
			nl_val *args;
			
			//body (linked list of statements to execute)
			nl_val *body;
			
			//environment (since this is a closure)
			nl_env_frame *env;
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
	//true if this environment is writable, otherwise false (read-only)
	char rw;
	
	//store symbols
	nl_val *symbol_array;
	//store values
	nl_val *value_array;
	
	//the environment above this one (THIS MUST BE FREE'D SEPERATELY)
	nl_env_frame* up_scope;
};


//END DATA STRUCTURES ---------------------------------------------------------------------------------------------

//BEGIN GLOBAL DATA -----------------------------------------------------------------------------------------------

//bookkeeping
char end_program;
unsigned int line_number;

//END GLOBAL DATA -------------------------------------------------------------------------------------------------

#endif


#ifndef _NL_DECLARATIONS
#define _NL_DECLARATIONS

//BEGIN NL DECLARATIONS -------------------------------------------------------------------------------------------

//error message function
void nl_err(nl_val *v, const char *msg, char output);

//returns a C string consisting of the name of the given type
const char *nl_type_name(nl_type t);

//allocate a value, and initialize it so that we're not doing anything too crazy
nl_val *nl_val_malloc(nl_type t);

//NOTE: reference decrementing is handled here as well
//free a value; this recursively frees complex data types
//returns TRUE if successful, FALSE if there are still references
char nl_val_free(nl_val *exp);

//copy a value data-wise into new memory, without changing the original
nl_val *nl_val_cp(nl_val *v);

//allocate an environment frame
nl_env_frame *nl_env_frame_malloc(nl_env_frame *up_scope);

//free an environment frame
void nl_env_frame_free(nl_env_frame *env);

//bind the given symbol to the given value in the given environment frame
//note that we do NOT change anything in the above scopes; this preserves referential transparency
//^ there is one exception to that, which is for new vars in a non-rw env (an application frame, aka call stack entry)
//returns TRUE for success, FALSE for failure
char nl_bind(nl_val *symbol, nl_val *value, nl_env_frame *env);

//look up the symbol in the given environment frame
//note that this WILL go up to higher scopes, if there are any
nl_val *nl_lookup(nl_val *symbol, nl_env_frame *env);

//make a neulang string from a c string
nl_val *nl_str_from_c_str(const char *c_str);

//make a neulang symbol from a c string
nl_val *nl_sym_from_c_str(const char *c_str);

//make a neulang value out of a primitve function so we can bind it
nl_val *nl_primitive_wrap(nl_val *(*function)(nl_val *arglist));

//is this neulang value TRUE? (true values are nonzero numbers and nonzero bytes)
char nl_is_true(nl_val *v);

//replace all instances of old with new in the given list (new CANNOT be a list itself, and old_val cannot be NULL)
//note that this is recursive and will also descend into lists within the list
//note also that this manages memory, and will remove references when needed
//returns TRUE if replacements were made, else FALSE
char nl_substitute_elements(nl_val *list, nl_val *old_val, nl_val *new_val);

//returns the number of times value occurred in the list (recursively checks sub-lists)
//(value CANNOT be a list itself, and also cannot be NULL)
int nl_list_occur(nl_val *list, nl_val *value);

//evaluate all the elements in a list, replacing them with their evaluations
void nl_eval_elements(nl_val *list, nl_env_frame *env);

//evaluate the list of values in order, returning the evaluation of the last statement only
nl_val *nl_eval_sequence(nl_val *body, nl_env_frame *env, char *early_ret);

//bind all the symbols to corresponding values in the given environment
//returns TRUE on success, FALSE on failure
char nl_bind_list(nl_val *symbols, nl_val *values, nl_env_frame *env);

//apply a given subroutine to its arguments
//note that returns are handled in eval_sequence
//also note tailcalls never get here (they are handled by eval turning body into begin, and then by eval_sequence tailcalling back into eval)
nl_val *nl_apply(nl_val *sub, nl_val *arguments, char *early_ret);

//evaluate an if statement with the given arguments
nl_val *nl_eval_if(nl_val *arguments, nl_env_frame *env, char last_exp, char *early_ret);

//evaluate a sub statement with the given arguments
nl_val *nl_eval_sub(nl_val *arguments, nl_env_frame *env);

//proper evaluation of keywords!
//evaluate a keyword expression (or primitive function, if keyword isn't found)
nl_val *nl_eval_keyword(nl_val *keyword_exp, nl_env_frame *env, char last_exp, char *early_ret);

//evaluate the given expression in the given environment
nl_val *nl_eval(nl_val *exp, nl_env_frame *env, char last_exp, char *early_ret);

//check if a givne character counts as whitespace in neulang
char nl_is_whitespace(char c);

//skip fp past any leading whitespaces
void nl_skip_whitespace(FILE *fp);

//read a number
nl_val *nl_read_num(FILE *fp);

//read a string (byte array)
nl_val *nl_read_string(FILE *fp);

//read a single character (byte)
nl_val *nl_read_char(FILE *fp);

//read an expression list
nl_val *nl_read_exp_list(FILE *fp);

//read a symbol (just a string with a wrapper) (with a rapper? drop them beats man)
nl_val *nl_read_symbol(FILE *fp);

//read an expression from the given input stream
nl_val *nl_read_exp(FILE *fp);

//output a neulang value
void nl_out(FILE *fp, const nl_val *exp);

//create global symbol data so it's not constantly being re-allocated (which is slow and unnecessary)
void nl_keyword_malloc();

//free global symbol data for clean exit
void nl_keyword_free();

//bind a newly alloc'd value (just removes an reference after bind to keep us memory-safe)
void nl_bind_new(nl_val *symbol, nl_val *value, nl_env_frame *env);

//bind all primitive subroutines in the given environment frame
void nl_bind_stdlib(nl_env_frame *env);

//the repl for neulang; this is separated from main for embedding purposes
//the only thing you have to do outside this is give us an open file and close it when we're done
//arguments given are interpreted as command-line arguments and are bound to argv in the interpreter (NULL works)
void nl_repl(FILE *fp, nl_val *argv);

//runtime!
int main(int argc, char *argv[]);


// Forward declarations for standard library functions ----------------

//gcd-reduce a rational number
void nl_gcd_reduce(nl_val *v);

//compare two neulang values; returns -1 if a<b, 0 if a==b, and 1 if a>b
//this is value comparison, NOT pointer comparison
int nl_val_cmp(const nl_val *v_a, const nl_val *v_b);

//return whether or not this list contains null (this does NOT recurse to sub-lists)
char nl_contains_nulls(nl_val *val_list);

//return a byte version of the given number constant, if possible
nl_val *nl_num_to_byte(nl_val *num_list);

//return an integer version of the given byte
nl_val *nl_byte_to_num(nl_val *byte_list);

//push a value onto the end of an array
void nl_array_push(nl_val *a, nl_val *v);

//returns the entry in the array a (first arg) at index idx (second arg)
nl_val *nl_array_idx(nl_val *args);

//pop a value off of the end of an array, resizing if needed
void nl_array_pop(nl_val *a);

//insert a value into an array, resizing if needed
void nl_array_ins(nl_val *a, nl_val *v, nl_val *index);

//remove a value from an array, resizing if needed
void nl_array_rm(nl_val *a, nl_val *index);

//return the size of the first argument
//NOTE: subsequent arguments are IGNORED
nl_val *nl_array_size(nl_val *array_list);

//concatenate all the given arrays (a list) into one new larger array
nl_val *nl_array_cat(nl_val *array_list);

//output the given list of strings in sequence
//returns NULL (a void function)
nl_val *nl_outstr(nl_val *array_list);

//outputs the given value to stdout
//returns NULL (a void function)
nl_val *nl_outexp(nl_val *v_list);

//reads input from stdin and returns the resulting expression
nl_val *nl_inexp(nl_val *arg_list);

//returns the length (size) of a singly-linked list
//note that cyclic lists are infinite and this will never terminate on them
int nl_c_list_size(nl_val *list);

//returns the list element at the given index (we use 0-indexing)
nl_val *nl_c_list_idx(nl_val *list, nl_val *idx);

//return the size of the first argument
//NOTE: subsequent arguments are IGNORED
nl_val *nl_list_size(nl_val *list_list);

//returns the list element at the given index (we use 0-indexing)
//this calls out to the c version which cannot be used directly due to argument count
nl_val *nl_list_idx(nl_val *arg_list);

//add a list of (rational) numbers
nl_val *nl_add(nl_val *num_list);

//subtract a list of (rational) numbers
nl_val *nl_sub(nl_val *num_list);

//multiply a list of (rational) numbers
nl_val *nl_mul(nl_val *num_list);

//divide a list of (rational) numbers
nl_val *nl_div(nl_val *num_list);

//checks if the values of given type t within val_list are equal
nl_val *nl_generic_eq(nl_val *val_list, nl_type t);

//checks if the values of a given type t within val_list are in descending order
nl_val *nl_generic_gt(nl_val *val_list, nl_type t);

//checks if the values of a given type t within val_list are in ascending order
nl_val *nl_generic_lt(nl_val *val_list, nl_type t);

//checks if list is descending or equal, >=
nl_val *nl_generic_ge(nl_val *val_list, nl_type t);

//checks if list is ascending or equal, <=
nl_val *nl_generic_le(nl_val *val_list, nl_type t);

//numeric equality operator =
//if more than two arguments are given then this will only return true if a==b==c==... for (= a b c ...)
nl_val *nl_eq(nl_val *val_list);

//numeric gt operator >
//if more than two arguments are given then this will only return true if a>b>c>... for (> a b c ...)
nl_val *nl_gt(nl_val *val_list);

//numeric lt operator <
//if more than two arguments are given then this will only return true if a<b<c<... for (< a b c ...)
nl_val *nl_lt(nl_val *val_list);

//numeric ge operator >=
nl_val *nl_ge(nl_val *val_list);

//numeric le operator <=
nl_val *nl_le(nl_val *val_list);

//array equality operator ar=
nl_val *nl_ar_eq(nl_val *val_list);

//array greater than operator ar>
nl_val *nl_ar_gt(nl_val *val_list);

//array less than operator ar<
nl_val *nl_ar_lt(nl_val *val_list);

//array greater than or equal to operator ar>=
nl_val *nl_ar_ge(nl_val *val_list);

//array less than or equal to operator ar<=
nl_val *nl_ar_le(nl_val *val_list);

//list equality operator list=
nl_val *nl_list_eq(nl_val *val_list);

//list greater than operator list>
nl_val *nl_list_gt(nl_val *val_list);

//list less than operator list<
nl_val *nl_list_lt(nl_val *val_list);

//list greater than or equal to operator list>=
nl_val *nl_list_ge(nl_val *val_list);

//list less than or equal to operator list<=
nl_val *nl_list_le(nl_val *val_list);

//byte equality operator b=
//if more than two arguments are given then this will only return true if a==b==c==... for (b= a b c ...)
nl_val *nl_byte_eq(nl_val *val_list);

//byte greater than operator b>
nl_val *nl_byte_gt(nl_val *val_list);

//byte less than operator b<
nl_val *nl_byte_lt(nl_val *val_list);

//byte greater than or equal to operator b>=
nl_val *nl_byte_ge(nl_val *val_list);

//byte less than or equal to operator b<=
nl_val *nl_byte_le(nl_val *val_list);

//null check null?
//returns TRUE iff all elements given in the list are NULL
nl_val *nl_is_null(nl_val *val_list);

//assert that all conditions in the given list are true; if not, exit (if compiled _STRICT) or return false (not strict)
nl_val *nl_assert(nl_val *cond_list);

//bitwise OR operation on the byte type
nl_val *nl_byte_or(nl_val *byte_list);

//bitwise AND operation on the byte type
nl_val *nl_byte_and(nl_val *byte_list);

//END NL DECLARATIONS ---------------------------------------------------------------------------------------------

#endif

