
#ifndef _NL_STRUCTURES
#define _NL_STRUCTURES

//BEGIN GLOBAL CONSTANTS ------------------------------------------------------------------------------------------

#define VERSION "0.1.0"
#define TRUE 1
#define FALSE 0
#define BUFFER_SIZE 1024

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
	NL_TYPE_START,
	
	//external (user-visible) types
	BYTE=NL_TYPE_START, //single-byte value (characters, booleans)
	NUM, //rational number support, consists of 2 bignum ints; for an integer the denominator portion will be 1
	PAIR, //cons cell
	ARRAY, //contiguous memory section
	PRI, //primitive procedure (C code)
	SUB, //closure (subroutine)
	STRUCT, //structure (named array)
	
	//internal types (might still be user visible, but mostly an implementation detail)
	SYMBOL, //variable names, for the symbol table, internally this is a [byte]array
	EVALUATION, //to-be-evaluated symbol, denoted $symbol
	BIND, //delayed variable binding
	NL_NULL, //null type
	
	NL_TYPE_CNT,
} nl_type;

typedef struct nl_env_frame nl_env_frame;

//a primitive value structure, the basic unit of evalution in neulang
typedef struct nl_val nl_val;
struct nl_val {
	//type
	nl_type t;
	
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
			
			//named arguments, a list of pairs with default values
			nl_val *dflt_args;
			
			//body (linked list of statements to execute)
			nl_val *body;
			
			//environment (since this is a closure)
			nl_env_frame *env;
		} sub;
		
		struct {
			//environment (what to bind the various symbols in so we can look them up)
			//this should always link to NULL and isn't related to the evaluation environment, it's local-only
			nl_env_frame *env;
		} nl_struct;
		
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
		
		//delayed binding value
		struct {
			//symbol to bind
			nl_val *sym;
			//value to bind to symbol
			nl_val *v;
		} bind;
	} d;
};

//a trie to act as a hash table for environment frames
typedef struct nl_trie_node nl_trie_node;
struct nl_trie_node {
	int child_count;
	
	//an array of pointers to children
	nl_trie_node **children;
	
	char name;
	
	//true or false, whether this is a terminal node
	char end_node;
	
	//the value that's bound to this symbol (NULL for unbound)
	nl_val *value;
	
	//an array of booleans to define which types are allowed
	//this is for the (as far as I know unique) neulang multitype behavior
	char t[NL_TYPE_CNT];
};

//environment frame (one global, then one per closure)
struct nl_env_frame {
	//true if this environment is shared (closure or global)
	//otherwise false (application / call stack entry)
	char shared;
	
	//store a trie that maps symbols (really c strings) to values
	nl_trie_node *trie;
	
	//the environment above this one (THIS MUST BE FREE'D SEPERATELY)
	nl_env_frame *up_scope;
};

//END DATA STRUCTURES ---------------------------------------------------------------------------------------------

//BEGIN GLOBAL DATA -----------------------------------------------------------------------------------------------

//bookkeeping
char end_program;
int exit_status;
unsigned int line_number;

//global null
nl_val *nl_null;

//END GLOBAL DATA -------------------------------------------------------------------------------------------------

#endif


#ifndef _NL_DECLARATIONS
#define _NL_DECLARATIONS

//BEGIN NL DECLARATIONS -------------------------------------------------------------------------------------------

//error message function
void nl_err(const nl_val *v, const char *msg, char output);

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
//^ there is one exception to that, which is for new vars in a non-shared env (an application frame, aka call stack entry)
//returns TRUE for success, FALSE for failure
char nl_bind(nl_val *symbol, nl_val *value, nl_env_frame *env, const char chk_type);

//look up the symbol in the given environment frame
//note that this WILL go up to higher scopes, if there are any
nl_val *nl_lookup(nl_val *symbol, nl_env_frame *env);

//make a neulang string from a c string
nl_val *nl_str_from_c_str(const char *c_str);

//make a neulang symbol from a c string
nl_val *nl_sym_from_c_str(const char *c_str);

//make a c string from a neulang string (you must free this yourself!)
//returns NULL if not given a valid nl string
char *c_str_from_nl_str(nl_val *nl_str);

//make a neulang value out of a primitve function so we can bind it
nl_val *nl_primitive_wrap(nl_val *(*function)(nl_val *arglist));

//is this neulang value TRUE? (true values are nonzero numbers and nonzero bytes)
char nl_is_true(nl_val *v);

//replace all instances of old with new in the given list (new CANNOT be a list itself, and old_val cannot be NULL)
//note that this is recursive and will also descend into lists within the list (except if that list starts with sub, while, or for)
//note also that this manages memory, and will remove references when needed
//returns TRUE if replacements were made, else FALSE
char nl_substitute_elements_skipsub(nl_val *list, nl_val *old_val, nl_val *new_val);

//returns the number of times value occurred in the list (recursively checks sub-lists)
//(value CANNOT be a list itself, and also cannot be NULL)
int nl_list_occur(nl_val *list, nl_val *value);

//evaluate all the elements in a list, replacing them with their evaluations
void nl_eval_elements(nl_val *list, nl_env_frame *env);

//evaluate the list of values in order, returning the evaluation of the last statement only
nl_val *nl_eval_sequence(nl_val *body, nl_env_frame *env, char *early_ret);

//bind default arguments (symbols) to values in the given environment
//returns TRUE on success, FALSE on failure
char nl_bind_dflt(nl_val *dflt_args, nl_env_frame *env);

//bind all the symbols to corresponding values in the given environment
//returns TRUE on success, FALSE on failure
char nl_bind_list(nl_val *symbols, nl_val *values, nl_env_frame *env, char allow_delayed_binds, nl_val *named_args, const char chk_type);

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
int nl_repl(FILE *fp, nl_val *argv);

//runtime!
int main(int argc, char *argv[]);

// forward declarations for string<->expression functions -------------

//gets an entry out of the string at the given position or returns NULL if position is out of bounds
//NOTE: this does NOT do type checking at the moment, use with care
char nl_str_char_or_null(nl_val *string, unsigned int pos);

//skip past any leading whitespaces in the string
void nl_str_skip_whitespace(nl_val *input_string, unsigned int *persistent_pos);

//read a number from a string
nl_val *nl_str_read_num(nl_val *input_string, unsigned int *persistent_pos);

//read a string (byte array) from a string (trust me it makes sense)
nl_val *nl_str_read_string(nl_val *input_string, unsigned int *persistent_pos);

//read a single character (byte) from a string
nl_val *nl_str_read_char(nl_val *input_string, unsigned int *persistent_pos);

//read an expression list from a string
nl_val *nl_str_read_exp_list(nl_val *input_string, unsigned int *persistent_pos);

//read a symbol (just a string with a wrapper) (with a rapper? drop them beats man) from a string
nl_val *nl_str_read_symbol(nl_val *input_string, unsigned int *persistent_pos);

//read an expression from an existing neulang string
//takes an input string, a position to start at (0 for whole string) and returns the new expression
//start_pos is set to the end of the first expression read when a full expression is found
nl_val *nl_str_read_exp(nl_val *input_string, unsigned int *persistent_pos);

// forward declarations for standard library functions ----------------

//allocate a trie
nl_trie_node *nl_trie_malloc();

//recursively free a trie and associated values
void nl_trie_free(nl_trie_node *trie_root);

//allocate a pointer array for trie children
nl_trie_node **nl_trie_malloc_children(int count);

//make a copy of a trie with data-wise copies of all nodes and all values contained therein
nl_trie_node *nl_trie_cp(nl_trie_node *from);

//add a node to a trie that maps a c string to a value
//returns TRUE on success, FALSE on failure
char nl_trie_add_node(nl_trie_node *trie_root, const char *name, unsigned int start_idx, unsigned int length, nl_val *value, const char chk_type);

//check if a trie contains a given value; if so, return a pointer to the value
//returns a pointer to the value if a match was found, else NULL
//sets the memory at success to TRUE if successful, FALSE if not (because NULL is also a valid value)
//if reorder is true, re-orders memory so the thing matched will be slightly quicker to find next time, if possible
nl_val *nl_trie_match(nl_trie_node *trie_root, const char *name, unsigned int start_idx, unsigned int length, char *success, char reorder);

//find the NODE which contains the desired value (NULL if none is found)
nl_trie_node *nl_trie_match_node(nl_trie_node *trie_root, const char *name, unsigned int start_idx, unsigned int length);

//print out a trie structure
void nl_trie_print(nl_trie_node *trie_root, int level);

//check if two tries contain all the same elements
int nl_trie_cmp(nl_trie_node *a, nl_trie_node *b);

//return as a string a trie as an associative mapping
nl_val *nl_trie_associative_recursive_str(nl_trie_node *trie_root, char *history, int hist_length, char forget_node, nl_val *acc);

//return as a string a trie as an associative mapping (calls the recursive function that does same)
nl_val *nl_trie_associative_str(nl_trie_node *trie_root);

//emulate getch() behavior on *nix /without/ ncurses
int nix_getch();

//gcd-reduce a rational number
void nl_gcd_reduce(nl_val *v);

//compare two neulang values; returns -1 if a<b, 0 if a==b, and 1 if a>b
//this is value comparison, NOT pointer comparison
int nl_val_cmp(const nl_val *v_a, const nl_val *v_b);

//return whether or not this list contains null (this does NOT recurse to sub-lists)
char nl_contains_nulls(nl_val *val_list);

//push a c string onto a neulang string (note that this is side-effect based, it's an array push)
void nl_str_push_cstr(nl_val *nl_str, const char *cstr);

//push a neulang string onto another neulang string (only the first one is changed, the second is not, and data is copied in element by element)
void nl_str_push_nlstr(nl_val *nl_str, const nl_val *str_to_push);

//return a byte version of the given number constant, if possible
nl_val *nl_num_to_byte(nl_val *num_list);

//return an integer version of the given byte
nl_val *nl_byte_to_num(nl_val *byte_list);

//returns the list equivalent to the given array(s)
//if multiple arrays are given they will be flattened into one sequential list
nl_val *nl_array_to_list(nl_val *arg_list);

//returns the array equivalent to the given list(s)
//if multiple lists are given they will be flattened into one sequential array
nl_val *nl_list_to_array(nl_val *arg_list);

//returns a list of pairs corresponding to the struct entries for the given struct(s)
//if multiple structs are given they will be flattened into one sequential list
nl_val *nl_struct_to_list(nl_val *arg_list);

//returns the string-encoded version of any given expression
nl_val *nl_val_to_memstr(const nl_val *exp);

//returns the string-encoded version of any given list of expressions
nl_val *nl_val_list_to_memstr(nl_val *val_list);

//returns the symbol equivilent of the given string
nl_val *nl_str_to_sym(nl_val *str_list);

//returns the string equivilent of the given symbol
nl_val *nl_sym_to_str(nl_val *sym_list);

//returns the number equivilent of the given string, if possible
//this uses the same parsing as the underlying interpreter parsing of numeric constants
nl_val *nl_str_to_num(nl_val *str_list);

//push a value onto the end of an array
void nl_array_push(nl_val *a, nl_val *v);

//returns the entry in the array a (first arg) at index idx (second arg)
nl_val *nl_array_idx(nl_val *args);

//return the size of the first argument
//NOTE: subsequent arguments are IGNORED
nl_val *nl_array_size(nl_val *array_list);

//concatenate all the given arrays (a list) into one new larger array
nl_val *nl_array_cat(nl_val *array_list);

//returns a new array with the given value substituted for value at given index in the given array
//arguments array, index, new-value
nl_val *nl_array_replace(nl_val *arg_list);

//returns an array consisting of all elements of the starting array
//plus any elements given as arguments after that
nl_val *nl_array_extend(nl_val *arg_list);

//returns an array consisting of all the elements of the starting array
//EXCEPT the element at the given position
nl_val *nl_array_omit(nl_val *arg_list);

//returns the index of the first occurance of the given subarray within the given array
//returns -1 for not found
nl_val *nl_array_find(nl_val *arg_list);

//array insert
//returns an array consisting of existing elements, plus given elements inserted at given 0-index-based position
nl_val *nl_array_insert(nl_val *arg_list);

//array chop; takes an array and a delimiter
//returns a new array with elements of the original array in separate subarrays (i.e. explode, split)
nl_val *nl_array_chop(nl_val *arg_list);

//subarray
//returns a new array consisting of a subset of the given array (all elements within the given inclusive range)
nl_val *nl_array_subarray(nl_val *arg_list);

//subarray
//returns a new array consisting of a subset of the given array (length number of elements from given start)
nl_val *nl_array_subarray(nl_val *arg_list);

//array range
//returns a new array consisting of a subset of the given array (all elements within the given inclusive range)
nl_val *nl_array_range(nl_val *arg_list);

//array map
//returns a new array consisting of each original value in the array "mapped" by the given function to the new array
//i.e. the return value of this is the array of return values of the given subroutine applied to each of the original array arguments
nl_val *nl_array_map(nl_val *arg_list);

//output the given list of strings in sequence
//returns NULL (a void function)
nl_val *nl_outstr(nl_val *array_list);

//outputs the given value to stdout
//returns NULL (a void function)
nl_val *nl_outexp(nl_val *v_list);

//reads input from stdin and returns the resulting expression
nl_val *nl_inexp(nl_val *arg_list);

//TODO: add an argument for line-editing here!
//reads a line from stdin and returns the result as a [neulang] string
nl_val *nl_inline(nl_val *arg_list);

//reads a single keystroke from stdin and returns the result as a num
nl_val *nl_inchar(nl_val *arg_list);

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

//concatenates all the given lists
nl_val *nl_list_cat(nl_val *list_list);

//get the given symbols from the struct
nl_val *nl_struct_get(nl_val *sym_list);

//return the result of replacing the given symbol with the given value in the struct
nl_val *nl_struct_replace(nl_val *rqst_list);

//add a list of (rational) numbers
nl_val *nl_add(nl_val *num_list);

//subtract a list of (rational) numbers
nl_val *nl_sub(nl_val *num_list);

//multiply a list of (rational) numbers
nl_val *nl_mul(nl_val *num_list);

//divide a list of (rational) numbers
nl_val *nl_div(nl_val *num_list);

//take the floor of a (rational) number
nl_val *nl_floor(nl_val *num_list);

//take the ceiling of a (rational) number
nl_val *nl_ceil(nl_val *num_list);

//get the absolute value of a (rational) number
nl_val *nl_abs(nl_val *num_list);

//equality operator =
//if more than two arguments are given then this will only return true if a==b==c==... for (= a b c ...)
//checks if the values of the same type within val_list are equal
nl_val *nl_generic_eq(nl_val *val_list);

//not equal operator !=
//equivalent to (not (= <arg list>))
nl_val *nl_generic_neq(nl_val *val_list);

//gt operator >
//if more than two arguments are given then this will only return true if a>b>c>... for (> a b c ...)
//checks if the values of the same type within val_list are in descending order
nl_val *nl_generic_gt(nl_val *val_list);

//lt operator <
//if more than two arguments are given then this will only return true if a<b<c<... for (< a b c ...)
//checks if the values of the same type within val_list are in ascending order
nl_val *nl_generic_lt(nl_val *val_list);

//ge operator >=
//checks if list is descending or equal, >=
nl_val *nl_generic_ge(nl_val *val_list);

//le operator <=
//checks if list is ascending or equal, <=
nl_val *nl_generic_le(nl_val *val_list);

//null check null?
//returns TRUE iff all elements given in the list are NULL
nl_val *nl_is_null(nl_val *val_list);

//bitwise OR operation on the byte type
nl_val *nl_byte_or(nl_val *byte_list);

//bitwise AND operation on the byte type
nl_val *nl_byte_and(nl_val *byte_list);

//reads the given file and returns its contents as a byte array (aka a string)
//returns NULL if file wasn't found or couldn't be opened
nl_val *nl_str_from_file(nl_val *fname_list);

//assert that all conditions in the given list are true; if not, exit (if compiled _STRICT) or return false (not strict)
nl_val *nl_assert(nl_val *cond_list);

//sleep a given number of seconds (if multiple arguments are given they are added)
nl_val *nl_sleep(nl_val *time_list);

//TODO: write a "source" function that imports a different file and executes it (as our include/import/source equivalent)

//END NL DECLARATIONS ---------------------------------------------------------------------------------------------

#endif

