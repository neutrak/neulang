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

//evaluations (explicit literal required so we don't look them up in the environment)
(lit $test)
(lit $%-)

//the empty list
()

// END parsing for various types --------------------------------------------------------------------------

// BEGIN if statement testing -----------------------------------------------------------------------------

//null return from conditional
//EXPECT: NULL
(if 1)

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

// END let statement testing ------------------------------------------------------------------------------

// EXIT
(exit)



