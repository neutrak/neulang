#!/usr/bin/neul

//neulang, like the 4th attempt?
//at the moment I'm just playing with syntax and figuring out how I want it to be

//search for a string in a substring, the "sub"routine
//arguments needle (to find) and haystack (what to search in)
(let[sub] str-find (sub[num] (haystack needle)
	
	//note that $hay-index is a value (a num) and hay-index is a symbol; this is kind of tcl-like
	//because $haystack is a string we can use array functions on it, in this case size (length? php used count, idk)
	//the explicit return isn't strictly necessary, it's just for clarity
	(return (for[num] (let[num] hay-index 0) (< $hay-index (ar-size $haystack)) (inc hay-index)
		
		//we use words like and, or, and not for boolean conditionals
		//the condition is while we haven't run off the end of the string or run out of characters in they haystack to search through
		//note that we are setting the return value of the for loop to the not-found variable/symbol
		(let[byte] found (for[byte] (let[num] ned-index 0) (and (< $ned-index (ar-size $needle)) (< (+ (ar-size $needle) $hay-index) (ar-size $haystack))) (inc ned-index)
			
			//again we use an array function for a string (which is a byte array); now index
			//note this is a "byte" comparison (byte is also our boolean type)
			//if a character didn't match/was out of place
			(if (!= (ar-idx $haystack (+ $hay-index $ned-index) (ar-idx $needle $ned-index)))
				//remember that, or, more accurately, return it up
				
				//a return can be explicit (within a loop explicit returns act like break statements)
				(return FALSE)
				
				//or just the last statement
				//but this is within an if so I don't know if it's valid here?
				//haven't decided yet, depends on implementation details;
				//lisp did that but they didn't strongly type
				//I could just make all returns explicit and be done with it
//				FALSE
			)
		//this is a keyword which lets us execute code outside of the for loop iterations but within its scope (since for loops are closures)
		//it is a so-called "halt keyword" which stops evaluation prematurely, at least for the current iteration; it's kinda like a "continue" until the loop is finished; else is implemented in the same way
		after
			//if we got through the loop and didn't return then it was found!
			TRUE
		))
		
		(if $found
			//this return MUST be explicit, so the after clause doesn't get executed
			(return $hay-index)
		)
	after
		//if no matches were found return -1 as an error code
		-1
	))
))

//a little subset of fgrep to try to demonstrate the method
//returns an array of lines which matched
(let[sub] fgrep (sub[[[byte]]] (search-term filename)
	
	//contents is an array of lines, or you know, an array of byte arrays
	//maybe I should re-think that typing...
	//str-explode is named after php's string explode; shitty language, but that was a great name for a function
	(let[[[byte]]] contents (str-explode (in $filename) $eol))
	
	//note this is read-only to the for loop (since we want it before the first iteration), and will be re-set when the loop completes
	(let[[[byte]]] matches (ar-ar-byte-emp))
	
	//this loop returns all the matching lines
	(let[[[byte]]] matches (for[[[byte]]] (let[num] line-num 0) (< $line-num (ar-size $contents)) (inc line-num)
		
		//if this line matched
		(if (>= ($str-find (ar-idx $contents $line-num) $search-term) 0)
			
			//ar-push adds an element to the end of the array (in this case the matches array), increasing its size
			//since this function changes the value on the first iteration (when it's not defined in the current scope) a copy is made
			//then the copy (now in local closure env) is changed, and again on subsequent iterations
			(ar-push matches (ar-idx $contents $line-num))
		)
	after
		//return everything that matched
		matches
	))
	
	//return all the matches
	matches
))

//this binary acts as fgrep because we told it to with the runtime here
(if (>= (ar-size $argv 3))
	(out ($fgrep (ar-idx $argv 1) (ar-idx $argv 2)))
//another "halt" keyword which is pre-parsed before the if is evaluated
else
	(out (, "Usage: " (ar-idx $argv 0) "<search term> <filename>"))
)

