//binary search for a needle in a sorted haystack (array)
//this is recursively implemented, but you could implement it iteratively as well
//return the index of the needle if found
//return -1 if the needle (search parameter) is not in the haystack (array)
(let ar-binsearch (sub (needle haystack)
	(if (= (ar-sz $haystack) 0)
		-1
	else (if (= (ar-sz $haystack) 1)
		(if (= $needle (ar-idx $haystack 0))
			0
		else
			-1
		)
	else
		(let cmp-idx (floor (/ (ar-sz $haystack) 2)))
		(let cmp (ar-idx $haystack $cmp-idx))
		(if (< $needle $cmp)
			($ar-binsearch $needle (ar-subar $haystack 0 $cmp-idx))
		else (if (> $needle $cmp)
			(let subsearch-idx ($ar-binsearch $needle (ar-subar $haystack $cmp-idx (- (ar-sz $haystack) $cmp-idx))))
			(if (< $subsearch-idx 0)
				$subsearch-idx
			else
				(+ $cmp-idx $subsearch-idx)
			)
		else
			$cmp-idx
		))
	))
))

//test to ensure ar-binsearch works properly
(for n 0 (<= $n 8) (+ $n 1)
	(assert (= ($ar-binsearch $n (array 1 2 3 4 5 6 7 8)) (- $n 1)))
)
(assert (= ($ar-binsearch 9) -1))
(assert (= ($ar-binsearch 4.3) -1))

//man I have got to add some proper string parsing functions; parsing command line arguments like this is a nightmare
//at the very lease I need to bind nl_str_read_num to a neulang subroutine for converting strings to numbers
(let needle (- (byte->num (ar-idx (f (r $argv)) 0)) (byte->num '0')))
(let haystack-strs (ar-chop (f (r (r $argv))) " "))
(let haystack "")
(let haystack (for n 0 (< $n (ar-sz $haystack-strs)) (+ $n 1)
	(let haystack (, $haystack (array (- (byte->num (ar-idx (ar-idx $haystack-strs $n) 0)) (byte->num '0')))))
after
	$haystack
))
(outexp $needle)
(outs $newl)
(outexp $haystack)
(outs $newl)
(outexp ($ar-binsearch $needle $haystack))
(outs $newl)

