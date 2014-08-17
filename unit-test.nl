#!/usr/bin/nl

//a unit test ensuring compliance to the neulang spec
//please run this in valgrind to ensure there are no memory leaks kthnx

// BEGIN parsing for various types ------------------------------------------------------------------------

//numeric constants (internally all numbers are stored as rationals)
5
-3/1
2.102
//0/0

//bytes (chars)
'a'
'B'
'9'

//strings (byte arrays)
//note that strings cannot contain any escape sequences; to use non-printable and quote chars, you need the string concatenation operator
"this is a test"
"there is `\'!@#$%^&*()_-=+ NO ESCAPE []{}"

//symbols
+
-
if
else
f
r

//special symbols
TRUE
FALSE
NULL

//evaluations (explicit literal required so we don't look them up in the environment)
(lit $test)
(lit $%-)

//the empty list
()

(//comment parsing
//test
)

/*/
this is only a multi-line comment test
k I think it works
*/

// END parsing for various types --------------------------------------------------------------------------

// BEGIN if statement testing -----------------------------------------------------------------------------

//null return from conditional
//(if)
//EXPECT: NULL
(assert (null? (if 1)))
//EXPECT: NULL
(assert (null? (if FALSE 5)))

//true condition, return 0
//EXPECT: 0
(assert (= 0 
	(if 1
		0
	)
))

//false condition, return 5
//EXPECT: 5
(assert (= 5 
	(if 0
		2
	else
		5
	)
))

//nested if as condition
//EXPECT: "asdf"
(assert (ar= "asdf"
	(if (if 0 0 else 1)
		"asdf"
	)
))

//nested if as result
//EXPECT: "yo"
(assert (ar= "yo"
	(if 0
		"hey"
	else (if 1
		"yo"
	else
		"aah"
	))
))

// END if statement testing -------------------------------------------------------------------------------

// BEGIN let statement testing ----------------------------------------------------------------------------

(let a-number /* this
is a very
weird test */ 5)
(assert (= $a-number 5))

//invalid syntax
//EXPECT: NULL
//(let)
//(let a)

//assign a numeric value
(let a 5)
//EXPECT: 5
(assert (= $a 5))

//assign a string
(let b "asdf")
//EXPECT: "asdf"
(assert (ar= $b "asdf"))

//copy a value
(let c $a)
//EXPECT: 5
(assert (= $c 5))

//re-set the value for a, ensure that c's value is unchanged
(let a -4.1)
//EXPECT: -4.1
(assert (= $a -4.1))
//EXPECT: 5
(assert (= $c 5))

//strict compilation mode causes the following few statements to just exit, so they are commented out so the rest of the test can run

//unassigned variables have value NULL, so set that now
//(let a $d)
//EXPECT: NULL
//$a

//variables cannot be re-bound to a different type
//(let b 1)
//EXPECT: "asdf"
//$b

//some special symbols evaluate to byte values, for boolean and char expressions
//so test those
(let bool TRUE)
(assert (b= $bool TRUE))
(let bool FALSE)
(assert (b= $bool FALSE))
//NULLs are generic and are always allowed
(let bool NULL)
(assert (null? $bool))
//but setting a variable to null doesn't re-set its type, so this should be a failure
//(let bool "asdf")
//EXPECT: NULL
//$bool

//assignment from nested if statements
(let if_check (if TRUE -4.2 else (if FALSE 4 else 3)))
(assert (= $if_check -4.2))

// END let statement testing ------------------------------------------------------------------------------

// BEGIN sub statement testing ----------------------------------------------------------------------------

//firstly, invalid syntax
//(sub)
//(sub "a")
//(sub 5)
(sub ())

//now something real, a subroutine with no args that returns the string "abc"
(sub () "abc")

//now assign a sub to a variable
(let a-test-sub (sub (b c e)
	"this is a test"
	(if TRUE
		"haha"
		(return 5)
	else
		"hehe"
	)
	(return 1)
	"abc"
))
$a-test-sub

//now apply a sub
(assert (ar= "b" ((sub () "b"))))

//((sub () 5 -1) 6 7 8)

//($a-test-sub "muhahahahaha" "a" "b")
(assert (= 5 ($a-test-sub "muhahahahaha" "a" "b")))

(let complex-test (sub (y)
	(+ 1 2)
	25
	FALSE
	3
))
$complex-test
(let return-value ($complex-test 1))
$return-value


((sub (x) $x) -3)
((sub (x y z) "asdf" (+ 4 5) -2) -4 -5 -6)

(assert (b= FALSE ((sub () FALSE))))

(assert (= 1 ((sub () FALSE 1))))

(assert (= 2 ((sub (x) (- $x 5)) 7)))

(let sub-sub (sub ()
	((sub (n)
		(if (< $n 5)
			"this is a test of a sub WITHIN a sub"
		else
			$n
		)
	) 0)
//	"this is after the sub-sub execution completed"
))

//($sub-sub)

(let sub-a (sub ()
	($sub-sub)
	(return FALSE)
))

(assert (b= FALSE ($sub-a)))


//just something I'm trying to see; don't mind this
(let arg-sub (sub (a)
	//NOTE: this let is not optional, it's the only thing binding a in the closure env (rather than just the apply env)
	// ^ maybe that should change? (vars passed as argument also auto-bound to closure env?) (okay I changed that, let's hope it didn't break anything)
//	(let a 10)
	(outexp $a)
	(outs $newl)
//	(return $a)
	//this used to return the $a from the environment that called the sub we're returning, instead of the environment the sub was declared in
	// ^ this was because let updated the apply environment but the sub-sub referenced the closure environment; now let updates both, so let's hope we didn't break anything else
	(return (sub () $a))
))
(assert (= 10 (($arg-sub 10))))
(outexp (($arg-sub 10)))
(outs $newl)

//ensure the value outside the sub didn't change
(assert (= $a -4.1))

//(exit)

// END sub statement testing ------------------------------------------------------------------------------

//BEGIN recursion testing ---------------------------------------------------------------------------------

//simple recursively-defined iteration
(let a-loop (sub (start max)
	(if (< $start $max)
		($a-loop (+ $start 1) $max)
	else
		$max
	)
))

($a-loop 1 5)

//recursive naive fibonacci sequence calculation
(let fib (sub (n)
	(if (> $n 2)
		(+ ($fib (- $n 2)) ($fib (- $n 1)))
	else
		1
	)
))

(assert (= 1 ($fib 1)))
(assert (= 1 ($fib 2)))
(assert (= 8 ($fib 6)))

//tail-recursive naive factorial calculation
(let ! (sub (n)
	(let iter-fact (sub (a n acc)
		(if (< $a $n)
			($iter-fact (+ $a 1) $n (* $acc $a))
		else
			(* $acc $a)
		)
	))
	($iter-fact 1 $n 1)
))

(assert (= 2432902008176640000 ($! 20)))

(let iter-loop (sub (min max)
	(if (< $min $max)
		($iter-loop (+ 1 $min) $max)
	else
		$max
	)
))

($iter-loop 4 64)

//a "dumb loop" to ensure that non-tailcall behavior is still handled accurately
//(this should return the FIRST argument given, even though it may recurse many times to figure that out)
//this is written in the alternate neulang syntax just to demonstrate that it works; in practice you should use one or the other, never mix them
let(dumb-loop sub((min max)
	if(<($min $max)
		$dumb-loop(+(1 $min) $max)
	else
		$max
	)
	$min
))

//EXPECT: 5
$dumb-loop(+(4 1) 10)

//the "recur" keyword allowing anonymous recursion!
(let recur-test (sub (min max)
//	(outexp $min)
//	(outs (array (num->byte 10)))
	(if (< $min $max)
		(recur (+ $min 1) $max)
	else
		$min
	)
))
(assert (= 500 ($recur-test 1 500)))

//END recursion testing -----------------------------------------------------------------------------------

//BEGIN loop testing --------------------------------------------------------------------------------------

(let n 0)

//a while loop! (internally this is converted into an anonymous sub)
(assert (= -3
	(while (< $n 1000)
		(outs "continuing..." $newl)
		(let n (+ $n 1))
	after
		-3
	)
))

//equivilent to a sub
(let a-loop (sub ()
	(if (< $n 5)
		(outs "continuing..." $newl)
		(let n (+ $n 1))
		(recur)
		//double recur just to make sure that's handled correctly
		(outs "recursing AGAIN" $newl)
		(recur)
	else
		-3
	)
)) (assert (= -3 ($a-loop)))

//anonymous sub (the above while loop internally gets converted into this exact code)
(assert (= -3 ((sub ()
	(if (< $n 10000)
		(outs "continuing... (n=")
		(outexp $n)
		(outs ")" $newl)
		(let n (+ $n 1))
//		(recur)
		(return (recur))
		//if this shows up then the early return above didn't work!
		(outs "this is broken" $newl)
		(exit 1)
	else
		-3
	)
))))

//(exit)

//note that unless it explicitly set after return the value in the outside environment didn't change
$n //0

//now a really long loop to demonstrate the fact that this uses tail recursion
(while (< $n 10000)
	(outexp $n)
	(outs $newl "here's johnny!!" $newl)
	(let n (+ $n 1))
)

//a for loop (this will not change the outside value of n, since it uses a single-argument closure
//so the arguments here are as follows
/*
<symbol that will be used in body, passed to recursive calls>
<initialization argument/first symbol value>
<condition to check (loop stops when it's false)>
<update to yeild new value for recursive call>
*/
(assert (= 12 (for n 0 (< $n 640) (+ $n 1)
	(outs "this is a for loop and n is ")
	(outexp $n)
	(outs $newl)
	
	//an early return acts as a break statement
	(if (= $n 600)
		(return 12)
	)
	
	(if (> $n 600)
		(outs "early return is broken" $newl)
	)
after
	13
)))

//0
(assert (= $n 0))

(assert (= 12 (while (< $n 640)
	(outs "this is a while loop and n is ")
	(outexp $n)
	(outs $newl)
	
	//an early return acts as a break statement
	(if (= $n 600)
		(if TRUE
			(return 12)
		)
	)
	
	(if (> $n 600)
		(outs "early return is broken" $newl)
	)
	
	(let n (+ $n 1))
after
	13
)))

(assert (= $n 0))

//this is a test of a loop within a sub
(let a-sub (sub ()
	(let n 0)
	(while (< $n 10)
		(outexp $n)
		(outs $newl)
		(let n (+ $n 1))
	)
	//this is just so the loop isn't a tailcall
	(return FALSE)
))

($a-sub)

//END loop testing ----------------------------------------------------------------------------------------

//BEGIN argument testing ----------------------------------------------------------------------------------

//output any arguments
(let output-list (sub (l)
	(if (not (null? $l))
		(outs (f $l))
		(outs " ")
		($output-list (r $l))
	else
		(outs $newl)
	)
))
($output-list $argv)
//$argv

//END argument testing ------------------------------------------------------------------------------------

//BEGIN boolean operator testing --------------------------------------------------------------------------

//boolean operators
//basic and case (FALSE)
(if (and 1 0)
	(outs "and failed" $newl)
	(assert FALSE)
else
	(outs "and worked" $newl)
)

//basic or case (TRUE)
(if (or 0 1)
	(outs "or worked" $newl)
else
	(outs "or failed" $newl)
	(assert FALSE)
)

//basic not case (TRUE)
(if (not 0)
	(outs "not worked" $newl)
else
	(outs "not failed" $newl)
	(assert FALSE)
)

//basic xor case (TRUE)
(if (xor 0 1 0)
	(outs "xor worked" $newl)
else
	(outs "xor failed" $newl)
	(assert FALSE)
)

//short-circuiting and case
(if (and 0 ((sub () (outs "and didn't short-circuit!" $newl) (assert FALSE))))
	(outs "short-circuiting and failed" $newl)
	(assert FALSE)
else
	(outs "short circuiting and worked" $newl)
)

//short-circuiting or case
(if (or 1 ((sub () (outs "or didn't short-circuit!" $newl) (assert FALSE))))
	(outs "short-circuiting or worked" $newl)
else
	(outs "short circuiting or failed" $newl)
	(assert FALSE)
)

//short-circuiting xor case
(if (xor 1 1 ((sub () (outs "xor didn't short-circuit!" $newl) (assert FALSE))))
	(outs "short circuiting xor failed" $newl)
	(assert FALSE)
else
	(outs "short-circuiting xor worked" $newl)
)

//END boolean operator testing ----------------------------------------------------------------------------

//BEGIN standard library testing --------------------------------------------------------------------------

//make a newline from other primitives (this is how you can output arbitrary characters, remember quotes and newlines cannot be escaped in strings)
(let newline (array (num->byte 10))) //\n
(let quote (array (num->byte 39))) //'
(let quotes (array (num->byte 34))) //"

(outs "this is " "a test" "..." $newline)
(outs (, "now with " "explicit concatenation" $newline))

//array operations, via strings
(let str-len $ar-sz)
(assert (= 6 ($str-len "a test")))

//allocate some byte types to test byte operations
(let byte-5 (num->byte 5))
(let byte-6 (num->byte 6))

//bitwise OR test
(assert (b= (num->byte 7) (b| $byte-5 $byte-6)))
//bitwise AND test
(assert (b= (num->byte 4) (b& $byte-5 $byte-6)))


//BEGIN standard library array testing --------------------------------------------------------------------
(assert (ar= "abcd" (, "a" "bc" "d")))

(let num-array (array 0 1 2 3 4 5))
(assert (= (ar-sz $num-array) 6))
(for n 0 (< $n (ar-sz $num-array)) (+ $n 1)
	(assert (= (ar-idx $num-array $n) $n))
)

(let a-string "abcd")
(assert (ar= "abce" (ar-replace $a-string 3 (num->byte 101))))
(assert (ar= "abcd" $a-string))

//END standard library array testing ----------------------------------------------------------------------



//BEGIN standard library list testing ---------------------------------------------------------------------
(assert (list= (lit ("a" "b" "c" "d")) (list-cat (list "a") (list "b") (list "c") (list "d"))))

//END standard library list testing -----------------------------------------------------------------------

//BEGIN closure-as-struct testing -------------------------------------------------------------------------

//this is a type that differs from what's within the struct; I'm just trying to ensure that this isn't ever used by struct calls
(let struct 'a')

//TODO: add some syntactic sugar to make structs super simple? it would be nice
//this is how you make a structure
(let new-struct (sub ()
	//an initial value is in the enclosing scope
	//note that if this was an argument it would get cleaned up after the call and so wouldn't be available to the sub-closure
	(let struct (array 0))
	
	//return a function with an environment (a closure) with a bound structure that can be accessed via arguments
	(return (sub (request value)
		//note that this could be extended to set individual array elements, rather than just the entire array as shown
		(if (sym= $request set)
			(let struct $value)
		else (if (sym= $request get)
			(return $struct)
		))
		//in case we got to the end and didn't return (we have to return something)
		$struct
	))
))

(let a-struct ($new-struct))
(assert (ar= ($a-struct get NULL) (array 0)))
($a-struct set (array 0 1 2 3 4))
(assert (ar= ($a-struct get NULL) (array 0 1 2 3 4)))
(outexp ($a-struct get NULL))
(outs $newl)
//ensure that the global value for "struct" is unaffected
(assert (b= $struct 'a'))


//a structure for lines (taken straight from the line editor at one point)

//consists of members ret (character return), num (line number), and content (string)
(let line-struct (sub ()
	//constructor (initial data)
	//consists of character return (e.g. up-ret), line number, and line content
	(let struct-data (array 0 1 ""))
	
	(return (sub (rqst-type var new-val)
		//get operations return the existing value
		(if (sym= $rqst-type get)
			(if (sym= $var ret)
				(return (ar-idx $struct-data 0))
			else (if (sym= $var num)
				(return (ar-idx $struct-data 1))
			else (if (sym= $var content)
				(return (ar-idx $struct-data 2))
			else
				(outs "Err: symbol ")
				(outexp $var)
				(outs " is not a member of line-struct" $newl)
			)))
		//set operations replace the existing value with the new value
		else (if (sym= $rqst-type set)
			(if (sym= $var ret)
				(let struct-data (ar-replace $struct-data 0 $new-val))
			else (if (sym= $var num)
				(let struct-data (ar-replace $struct-data 1 $new-val))
			else (if (sym= $var content)
				(let struct-data (ar-replace $struct-data 2 $new-val))
			else
				(outs "Err: symbol ")
				(outexp $var)
				(outs " is not a member of line-struct" $newl)
			)))
		else
			(outs "Err: not a valid operation for line-struct ")
			(outexp $rqst-type)
			(outs $newl)
		))
	))
))

(let a-line ($line-struct))
($a-line set content "asdf")
(assert (ar= ($a-line get content NULL) "asdf"))
(assert (= ($a-line get ret NULL) 0))
(assert (= ($a-line get num NULL) 1))

//END closure-as-struct testing ---------------------------------------------------------------------------

//END standard library testing ----------------------------------------------------------------------------

// EXIT
//(exit)



