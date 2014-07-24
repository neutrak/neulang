
#ifndef _NL_STRUCTURES
#define _NL_STRUCTURES

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

#endif


#ifndef _NL_DECLARATIONS
#define _NL_DECLARATIONS

//BEGIN NL DECLARATIONS -------------------------------------------------------------------------------------------

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
nl_val *nl_eval_sequence(nl_val *body, nl_env_frame *env);

//bind all the symbols to corresponding values in the given environment
//returns TRUE on success, FALSE on failure
char nl_bind_list(nl_val *symbols, nl_val *values, nl_env_frame *env);

//apply a given subroutine to its arguments
//last_exp is set if we are currently executing the last expression of a body block, and can therefore re-use an existing application environment
nl_val *nl_apply(nl_val *sub, nl_val *arguments, nl_env_frame *env, char last_exp);

//evaluate an if statement with the given arguments
nl_val *nl_eval_if(nl_val *arguments, nl_env_frame *env, char last_exp);

//evaluate a sub statement with the given arguments
nl_val *nl_eval_sub(nl_val *arguments, nl_env_frame *env);

//proper evaluation of keywords!
//evaluate a keyword expression (or primitive function, if keyword isn't found)
nl_val *nl_eval_keyword(nl_val *keyword_exp, nl_env_frame *env, char last_exp);

//evaluate the given expression in the given environment
nl_val *nl_eval(nl_val *exp, nl_env_frame *env, char last_exp);

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

//runtime!
int main(int argc, char *argv[]);


// Forward declarations for standard library functions ----------------

//gcd-reduce a rational number
void nl_gcd_reduce(nl_val *v);

//compare two neulang values; returns -1 if a<b, 0 if a==b, and 1 if a>b
//this is value comparison, NOT pointer comparison
int nl_val_cmp(const nl_val *v_a, const nl_val *v_b);

//return a byte version of the given number constant, if possible
nl_val *nl_int_to_byte(nl_val *num_list);

//push a value onto the end of an array
void nl_array_push(nl_val *a, nl_val *v);

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
nl_val *nl_strout(nl_val *array_list);

//returns the length of a singly-linked list
//note that cyclic lists are infinite and this will never terminate on them
int nl_list_len(nl_val *list);

//returns the list element at the given index (we use 0-indexing)
nl_val *nl_list_idx(nl_val *list, nl_val *idx);

//add a list of (rational) numbers
nl_val *nl_add(nl_val *num_list);

//subtract a list of (rational) numbers
nl_val *nl_sub(nl_val *num_list);

//multiply a list of (rational) numbers
nl_val *nl_mul(nl_val *num_list);

//divide a list of (rational) numbers
nl_val *nl_div(nl_val *num_list);

//numeric equality operator =
//TWO ARGUMENTS ONLY!
nl_val *nl_eq(nl_val *val_list);

//numeric gt operator >
//if more than two arguments are given then this will only return true if a>b>c>... for (> a b c ...)
nl_val *nl_gt(nl_val *val_list);

//numeric lt operator <
//if more than two arguments are given then this will only return true if a<b<c<... for (< a b c ...)
nl_val *nl_lt(nl_val *val_list);

//END NL DECLARATIONS ---------------------------------------------------------------------------------------------

#endif

