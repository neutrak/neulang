//if - else if - else statement
(if <condition>
	...
[else] (if <condition> 
	...
[else]
	...
//the number of parens needed here is dependent on the number of else clauses used, since else if is in reality nested in the outer if
))

//for loop
(for[return-type] <initialization> <condition> <update>
	...
//statements after this keyword do not get executed until the last iteration of the loop
//it's semantically equivilent to placing everything after it in a (if (not condition) ...) check, but is more efficient
[after]
	...
)

//while loop
(while[return-type] <condition>
	...
[after]
	...
)

//equivilent tail-recursive code for the above while loop, which gets generated transparently
//note this is defining an anonymous function then immediately calling it
//TODO: in a for loop the initialization clause would happen here (should it? that would be in the outer scope, I should maybe rethink that)
((sub[return-type] ()
	//note that even though no arguments are passed state gets updated on each iteration
	(if <condition>
		//execute main body
		...
		
		//in a for loop the update clause would go here
		
		//iterate again (note that this is tail recursion)
		(recurse)
	else
		//execute after clause
		...
	)
))	



//defining a function
(let[sub] <subroutine-name> ([return-type]sub <argument-list>
	...
))

//types by example

//bytes are used as both booleans and chars, and any other time you want a single-byte unsigned value
(let[byte] abyte FALSE)

//arrays are declared with an extra [] surrounding
(let[[byte]] astring "strings are byte arrays")

//numbers are all rational, integers are interpreted with denominator 1
(let[num] anum0 2) //stored as 2/1
(let[num] anum1 5/4) //stored as stated
(let[num] anum2 3.14159) //stored as 314159/100000 = 3.14159
//NOTE: gcd is run to reduce factors for more efficient operations

//a numeric array, initialized to a value
(let[[num]] anum-array (array $anum0 $anum1 $anum2))


