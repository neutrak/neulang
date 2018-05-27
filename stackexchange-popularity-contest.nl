#!/usr/bin/neul

//this was made as a demonstration for this stackexchange thread: https://codegolf.stackexchange.com/questions/48476/programming-languages-through-the-years
//although the thread was "codegolf" I didn't really go for minimalism here; rather for clarity

(outs "neulang was made in 2014" $newl)

(let ascii-n (sub (n)
	(for col 0 (< $col $n) (+ $col 1)
		(for row 0 (< $row $n) (+ $row 1)
			(if (or (= $row 0) (= $row (- $n 1)) (= $row $col))
				(outs "N")
			else
				(outs " ")
			)
		)
		(outs $newl)
	)
))

(for n 1 (<= $n 7) (+ $n 2)
	($ascii-n $n)
	(outs $newl)
)

(let gcd (sub (a b)
	(let a (while (>= $a $b)
		(let a (- $a $b))
	after
		$a
	))
	
	(if (= $a 0)
		$b
	else
		($gcd $b $a)
	)
))

(assert (= ($gcd 8 12) 4))
(assert (= ($gcd 12 8) 4))
(assert (= ($gcd 3 30) 3))
(assert (= ($gcd 5689 2) 1))
(assert (= ($gcd 234 876) 6))



