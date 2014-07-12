
#ifndef _NL_STRUCTURES
#define _NL_STRUCTURES

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
void nl_val_free(nl_val *exp);

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

//evaluate all the elements in a list, replacing them with their evaluations
void nl_eval_elements(nl_val *list, nl_env_frame *env);

//apply a given subroutine to its arguments
nl_val *nl_apply(nl_val *sub, nl_val *arguments);

//evaluate an if statement with the given arguments
nl_val *nl_eval_if(nl_val *arguments, nl_env_frame *env);

//evaluate a sub statement with the given arguments
nl_val *nl_eval_sub(nl_val *arguments, nl_env_frame *env);

//evaluate a keyword expression (or primitive function, if keyword isn't found)
nl_val *nl_eval_keyword(nl_val *keyword_exp, nl_env_frame *env);

//evaluate the given expression in the given environment
nl_val *nl_eval(nl_val *exp, nl_env_frame *env);

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
void nl_out(FILE *fp, nl_val *exp);

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

//push a value onto the end of an array
void nl_array_push(nl_val *a, nl_val *v);

//pop a value off of the end of an array, resizing if needed
void nl_array_pop(nl_val *a);

//insert a value into an array, resizing if needed
void nl_array_ins(nl_val *a, nl_val *v, nl_val *index);

//remove a value from an array, resizing if needed
void nl_array_rm(nl_val *a, nl_val *index);

//add a list of (rational) numbers
nl_val *nl_add(nl_val *num_list);

//subtract a list of (rational) numbers
nl_val *nl_sub(nl_val *num_list);

//END NL DECLARATIONS ---------------------------------------------------------------------------------------------

#endif

