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
([return-type]for <initialization> <condition> <update>
	...
//statements after this keyword do not get executed until the last iteration of the loop
//it's semantically equivilent to placing everything after it in a (if (not condition) ...) check, but is more efficient
[after]
	...
)

//while loop
([return-type]while <condition>
	...
[after]
	...
)

//equivilent tail-recursive code for the above while loop, which gets generated transparently
//note this is defining an anonymous function then immediately calling it
//TODO: in a for loop the initialization clause would happen here (should it? that would be in the outer scope, I should maybe rethink that)
(([return-type]sub ()
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



