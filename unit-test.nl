//a unit test ensuring compliance to the neulang spec
//please run this in valgrind to ensure there are no memory leaks kthnx

// BEGIN parsing for various types ------------------------------------------------------------------------

//numeric constants (internally all numbers are stored as rationals)
5
-3/1
2.102
0/0

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
//EXPECT: NULL
(if 1)
//EXPECT: NULL
(if FALSE 5)

//true condition, return 0
//EXPECT: 0
(if 1
	0
)

//false condition, return 5
//EXPECT: 5
(if 0
	2
else
	5
)

//nested if as condition
//EXPECT: "asdf"
(if (if 0 0 else 1)
	"asdf"
)

//nested if as result
//EXPECT: "yo"
(if 0
	"hey"
else (if 1
	"yo"
else
	"aah"
))


// END if statement testing -------------------------------------------------------------------------------

// BEGIN let statement testing ----------------------------------------------------------------------------

(let a-number /* this
is a very
weird test */ 5)
$a-number

//invalid syntax
//EXPECT: NULL
(let)
(let a)

//assign a numeric value
(let a 5)
//EXPECT: 5
$a

//assign a string
(let b "asdf")
//EXPECT: "asdf"
$b

//copy a value
(let c $a)
//EXPECT: 5
$c

//re-set the value for a, ensure that c's value is unchanged
(let a -4.1)
//EXPECT: -4.1
$a
//EXPECT: 5
$c

//unassigned variables have value NULL, so set that now
(let a $d)
//EXPECT: NULL
$a

//variables cannot be re-bound to a different type
(let b 1)
//EXPECT: "asdf"
$b

//some special symbols evaluate to byte values, for boolean and char expressions
//so test those
(let bool TRUE)
$bool
(let bool FALSE)
$bool
//NULLs are generic and are always allowed
(let bool NULL)
$bool
//but setting a variable to null doesn't re-set its type, so this should be a failure
(let bool "asdf")
//EXPECT: NULL
$bool

//assignment from nested if statements
(let if_check (if TRUE -4.2 else (if FALSE 4 else 3)))
$if_check

// END let statement testing ------------------------------------------------------------------------------

// BEGIN sub statement testing ----------------------------------------------------------------------------

//firstly, invalid syntax
(sub)
(sub "a")
(sub 5)
(sub ())

//now something real, a subroutine with no args that returns the string "abc"
(sub () "abc")

//now assign a sub to a variable
(let a-test-sub (sub (b c e)
	"this is a test"
	(if TRUE
		"haha"
	else
		"hehe"
	)
	(return 1)
	13
))
$a-test-sub

//now apply a sub
((sub () "b"))

((sub () 5 -1) 6 7 8)

($a-test-sub "muhahahahaha")

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

((sub () FALSE))

((sub () FALSE 1))

((sub (x) (- $x 5)) 7)


// END sub statement testing ------------------------------------------------------------------------------

// EXIT
(exit)



