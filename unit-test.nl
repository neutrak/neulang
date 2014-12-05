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

// BEGIN comparison testing -------------------------------------------------------------------------------

(assert (= 2 2))
(assert (= -4 -4))
(assert (not (= 2 -2)))
(assert (!= 4 -4))
(assert (!= 4 3))

(assert (< 4 5))
(assert (not (< 5 4)))
(assert (<= 4 5))
(assert (<= 5 5))
(assert (not (<= 6 5)))

(assert (> 5 4))
(assert (not (> 4 5)))
(assert (>= 5 4))
(assert (>= 5 5))
(assert (not (>= 5 6)))

// END comparison testing ---------------------------------------------------------------------------------

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
(assert (= "asdf"
	(if (if 0 0 else 1)
		"asdf"
	)
))

//nested if as result
//EXPECT: "yo"
(assert (= "yo"
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
(assert (= $b "asdf"))

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
(assert (= $bool TRUE))
(let bool FALSE)
(assert (= $bool FALSE))

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

//let with lists; this allows multiple binds in a single let call
(let (i j k) (list 0 1 2))
(assert (= $i 0))
(assert (= $j 1))
(assert (= $k 2))

//let with lists from a sub return
(let (i j k) ((sub () (list 2 1 0))))
(assert (= $i 2))
(assert (= $j 1))
(assert (= $k 0))

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
(assert (= "b" ((sub () "b"))))

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

(assert (= FALSE ((sub () FALSE))))

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

(assert (= FALSE ($sub-a)))


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

//a for loop without an early return
(for n 0 (< $n 3000) (+ $n 1)
	(outs "n=" (val->memstr $n) $newl)
)

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

//a mutual recursion loop, to demonstrate that tailcalls are optimized even when they jump between functions (not just directly recursively)
(let a-mutual-recursion (sub (n)
	(if (> $n 0)
		($b-mutual-recursion $n)
	)
))

(let b-mutual-recursion (sub (n)
	($a-mutual-recursion (- $n 1))
))

($a-mutual-recursion 2000)

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
(assert (= (num->byte 7) (b| $byte-5 $byte-6)))
//bitwise AND test
(assert (= (num->byte 4) (b& $byte-5 $byte-6)))


//BEGIN standard library array testing --------------------------------------------------------------------

//array conversion from list(s)
(assert (= "abcd" (list->ar (list 'a' 'b' 'c' 'd'))))
(assert (= "abcd" (list->ar (list 'a' 'b') (list 'c' 'd'))))

//array concatenation
(assert (= "abcd" (, "a" "bc" "d")))

//array size, iteration
(let num-array (array 0 1 2 3 4 5))
(assert (= (ar-sz $num-array) 6))
(for n 0 (< $n (ar-sz $num-array)) (+ $n 1)
	(assert (= (ar-idx $num-array $n) $n))
)

//array replace (no side-effects)
(let a-string "abcd")
(assert (= "abce" (ar-replace $a-string 3 (num->byte 101))))
(assert (= "abcd" $a-string))

//array extend (no side-effects)
(assert (= "abcde" (ar-extend $a-string 'e')))
(assert (= (array 'a' 'b' 'c' 'd' 1 2 3) (ar-extend $a-string 1 2 3)))

//memory string (what this value looks in memory, as a string)
(assert (= "(a b c)" (val->memstr (lit ('a' 'b' 'c')))))

//array omit
(assert (= (array 1 2 4) (ar-omit (array 1 2 3 4) 2)))

//array chop (split, explode)
(assert (= (array (array) "abcd") (ar-chop "gabcd" "g")))
(assert (= (array "gab" "d") (ar-chop "gabcd" "c")))
(assert (= (array "gabc" (array)) (ar-chop "gabcd" "d")))

(assert (= (lit ('a' 'b' 'c' 'd')) (ar->list "abcd")))
(assert (= (list 1 2 3 4) (ar->list (array 1 2 3 4))))
(assert (= (list 1 2 3 4) (ar->list (array 1 2) (array 3 4))))

//array insert
//normal case
(assert (= (ar-ins "asdf" 0 (ar->list "bbb")) "bbbasdf"))
(assert (= (ar-ins "asdf" 1 (ar->list "bbb")) "abbbsdf"))
(assert (= (ar-ins "asdf" (ar-sz "asdf") (ar->list "bbb")) "asdfbbb"))
//exceptional (warning) case
(assert (= (ar-ins "asdf" -1 (ar->list "bbb")) "bbbasdf"))
(assert (= (ar-ins "asdf" (+ (ar-sz "asdf") 5) (ar->list "bbb")) "asdfbbb"))

//subarray operations
//subarray takes array, start, and length
(assert (= (ar-subar "asdfasdf" 0 4) "asdf"))
(assert (= (ar-subar "asdfasdf" 2 3) "dfa"))

//going off the end just stops pre-maturely and returns what it can
(assert (= (ar-subar "asdfasdf" 4 8) "asdf"))

//negative lengths go backwards
(assert (= (ar-subar "asdfasdf" 3 -4) "asdf"))

//END standard library array testing ----------------------------------------------------------------------


//BEGIN standard library list testing ---------------------------------------------------------------------

//list conversion from array
(assert (= (lit ('a' 'b' 'c' 'd')) (ar->list "abcd")))
(assert (= (list 1 2 3 4) (ar->list (array 1 2 3 4))))
(assert (= (list 1 2 3 4) (ar->list (array 1 2) (array 3 4))))

//list concatenation
(assert (= (lit ("a" "b" "c" "d")) (list-cat (list "a") (list "b") (list "c") (list "d"))))


//END standard library list testing -----------------------------------------------------------------------

//BEGIN proper struct testing -----------------------------------------------------------------------------

//(outs (val->memstr (struct (a 5) (b "c") (abc 'd'))) $newl)
//(exit)

//these native structs are implemented as environment frames and will hopefully be hash tables later

(let some-data (struct
	(a-var 5)
	(another-var "c")
	(a-third-var 'h')
))

(outs (val->memstr $some-data) $newl)

(assert (= 5 (struct-get $some-data a-var)))
(assert (= "c" (struct-get $some-data another-var)))

(let some-data (struct-replace $some-data a-var 20))
(assert (= 20 (struct-get $some-data a-var)))

(assert (= (struct (a 5)) (struct (a 5))))
(assert (not (= (struct (a 4)) (struct (a 5)))))
(assert (not (= (struct (a 5)) (struct (b 5)))))
(assert (= (struct (a 4) (b 5)) (struct (b 5) (a 4))))

//END proper struct testing -------------------------------------------------------------------------------

/*
//BEGIN closure-as-struct testing -------------------------------------------------------------------------

//this is a type that differs from what's within the struct; I'm just trying to ensure that this isn't ever used by struct calls
(let struct 'a')

//this is how you make a structure
(let new-struct (sub ()
	//an initial value is in the enclosing scope
	//note that if this was an argument it would get cleaned up after the call and so wouldn't be available to the sub-closure
	(let struct (array 0))
	
	//return a function with an environment (a closure) with a bound structure that can be accessed via arguments
	(return (sub (request value)
		//note that this could be extended to set individual array elements, rather than just the entire array as shown
		(if (= $request set)
			(let struct $value)
		else (if (= $request get)
			(return $struct)
		))
		//in case we got to the end and didn't return (we have to return something)
		$struct
	))
))

(let a-struct ($new-struct))
(assert (= ($a-struct get NULL) (array 0)))
($a-struct set (array 0 1 2 3 4))
(assert (= ($a-struct get NULL) (array 0 1 2 3 4)))
(outexp ($a-struct get NULL))
(outs $newl)
//ensure that the global value for "struct" is unaffected
(assert (= $struct 'a'))


//a closure-based structure for lines

//consists of members ret (character return), num (line number), and content (string)
(let line-struct (sub ()
	//constructor (initial data)
	//consists of character return (e.g. up-ret), line number, and line content
	(let struct-data (array 0 1 ""))
	
	(return (sub (rqst-type var new-val)
		//get operations return the existing value
		(if (= $rqst-type get)
			(if (= $var ret)
				(return (ar-idx $struct-data 0))
			else (if (= $var num)
				(return (ar-idx $struct-data 1))
			else (if (= $var content)
				(return (ar-idx $struct-data 2))
			else
				(outs "Err: symbol ")
				(outexp $var)
				(outs " is not a member of line-struct" $newl)
			)))
		//set operations replace the existing value with the new value
		else (if (= $rqst-type set)
			(if (= $var ret)
				(let struct-data (ar-replace $struct-data 0 $new-val))
			else (if (= $var num)
				(let struct-data (ar-replace $struct-data 1 $new-val))
			else (if (= $var content)
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
(assert (= ($a-line get content NULL) "asdf"))
(assert (= ($a-line get ret NULL) 0))
(assert (= ($a-line get num NULL) 1))

//END closure-as-struct testing ---------------------------------------------------------------------------
*/

//END standard library testing ----------------------------------------------------------------------------

//BEGIN array-sorting testing -----------------------------------------------------------------------------

//a very inefficient userspace subarray function
(let manual-ar-subar (sub (ar start length)
	(let ret (array))
	(let idx $start)
	
	//return ret directly out of the while loop
	//for all the length elements directly after the index
	(return (while (< (- $idx $start) $length)
		//add them to the return value
		(let ret (ar-extend $ret (ar-idx $ar $idx)))
		
		(let idx (+ $idx 1))
	after
		$ret
	))
))


//merge, aka zip
(let ar-merge (sub (ar-a ar-b cmp)
	(let ret (array))
	
	//zip up the arrays based on the comparison function
	(let ret (while (or (> (ar-sz $ar-a) 0) (> (ar-sz $ar-b) 0))
		//if there are still elements in both arrays
		(if (and (> (ar-sz $ar-a) 0) (> (ar-sz $ar-b) 0))
			
			//take the one chosen by the comparison
			(if ($cmp (ar-idx $ar-a 0) (ar-idx $ar-b 0))
				(let ret (ar-extend $ret (ar-idx $ar-a 0)))
				(let ar-a (ar-omit $ar-a 0))
			else
				(let ret (ar-extend $ret (ar-idx $ar-b 0)))
				(let ar-b (ar-omit $ar-b 0))
			)
		//if there are only elements left in a, then take one of those
		else (if (> (ar-sz $ar-a) 0)
			(let ret (ar-extend $ret (ar-idx $ar-a 0)))
			(let ar-a (ar-omit $ar-a 0))
		//if there are only elements left in b, then take one of those
		else
			(let ret (ar-extend $ret (ar-idx $ar-b 0)))
			(let ar-b (ar-omit $ar-b 0))
		))
	after
		$ret
	))
	
	//return the merged result
	(return $ret)
))

//merge sort
(let ar-merge-sort (sub (ar cmp)
	//fewer than 2 elements -> already sorted
	(if (< (ar-sz $ar) 2)
		(return $ar)
	)
	
	(let split-idx (floor (/ (ar-sz $ar) 2)))
	
	//get the lower half
	(let low (ar-subar $ar 0 $split-idx))
	//and the upper half
	(let high (ar-subar $ar $split-idx (- (ar-sz $ar) $split-idx)))
	
	//recursively sort the sub-arrays
	(let low ($ar-merge-sort $low $cmp))
	(let high ($ar-merge-sort $high $cmp))

	//return the (merged) sorted result
	(return ($ar-merge $low $high $cmp))
))

//insertion sort, just as a speed comparison
(let ar-ins-sort (sub (ar cmp)
	//fewer than 2 elements = already sorted
	(if (< (ar-sz $ar) 2)
		(return $ar)
	)
	
	//go through each index of the array
	(let idx 0)
	
	//note that we don't need a variable since we're returning directly out of the while loop
	(return (while (< $idx (ar-sz $ar))
		//find the lowest element in the array at or after the current element
		//initialized as the current element
		(let lowest (ar-idx $ar $idx))
		(let low-idx $idx)
		
		//iterate through the remainder of the array
		(let low-idx (while (< $idx (ar-sz $ar))
			//if something was lower (assuming cmp as <), update the lowest and the associated index
			(if ($cmp (ar-idx $ar $idx) $lowest)
				(let lowest (ar-idx $ar $idx))
				(let low-idx $idx)
			)
			
			//go to the next element
			(let idx (+ $idx 1))
		after
			//return the lowest element found
			$low-idx
		))
		
		//swap the current element for the lowest element found
		(let tmp (ar-idx $ar $idx))
		(let ar (ar-replace $ar $idx (ar-idx $ar $low-idx)))
		(let ar (ar-replace $ar $low-idx $tmp))
		
		//go to the next element
		(let idx (+ $idx 1))
	after
		//when at the end of all elements, return the array with replacements made
		$ar
	))
))


(assert (= (array 1 2 3 4 5 6 7 8 9) ($ar-merge (array 1 2 3 4 5) (array 6 7 8 9) (sub (a b) (< $a $b)))))
(assert (= (array 6 7 8 9 1 2 3 4 5) ($ar-merge (array 1 2 3 4 5) (array 6 7 8 9) (sub (a b) (> $a $b)))))

(assert (= (array 0 1 2 3 4 5 6 7 8 9) ($ar-ins-sort (array 2 3 4 1 0 5 8 9 7 6) (sub (a b) (< $a $b)))))

(assert (= (array 0 1 2 3 4 5 6 7 8 9) ($ar-merge-sort (array 2 3 4 1 0 5 8 9 7 6) (sub (a b) (< $a $b)))))
(outs "post merge-sort array is " (val->memstr ($ar-merge-sort (array 2 3 4 1 0 5 8 9 7 6) (sub (a b) (< $a $b)))) $newl)

//END array-sorting testing -------------------------------------------------------------------------------

// EXIT
//(exit)



