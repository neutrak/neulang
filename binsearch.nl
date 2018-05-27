//binary search for a needle in a sorted haystack (array)
//this is recursively implemented, but you could implement it iteratively as well
//return the index of the needle if found
//return -1 if the needle (search parameter) is not in the haystack (array)
(let ar-binsearch (sub (needle haystack)
	//tail recursive implementation requires a start idx
	(let iter-ar-binsearch (sub (needle haystack start-idx)
		(if (= (ar-sz $haystack) 0)
			-1
		else (if (= (ar-sz $haystack) 1)
			(if (= $needle (ar-idx $haystack 0))
				$start-idx
			else
				-1
			)
		else
			(let cmp-idx (floor (/ (ar-sz $haystack) 2)))
			(let cmp (ar-idx $haystack $cmp-idx))
			(if (< $needle $cmp)
				($iter-ar-binsearch $needle (ar-subar $haystack 0 $cmp-idx) $start-idx)
			else (if (> $needle $cmp)
				($iter-ar-binsearch $needle (ar-subar $haystack $cmp-idx (- (ar-sz $haystack) $cmp-idx)) (+ $start-idx $cmp-idx))
			else
				(+ $start-idx $cmp-idx)
			))
		))
	))
	($iter-ar-binsearch $needle $haystack 0)
))

//test to ensure ar-binsearch works properly
(for n 0 (<= $n 8) (+ $n 1)
	(assert (= ($ar-binsearch $n (array 1 2 3 4 5 6 7 8)) (- $n 1)))
)
(assert (= ($ar-binsearch 9) -1))
(assert (= ($ar-binsearch 4.3) -1))

(let needle (str->num (f (r $argv))))
(let haystack (ar-map (ar-chop (f (r (r $argv))) " ") (sub (exp) (str->num $exp))))
(outexp $needle)
(outs $newl)
(outexp $haystack)
(outs $newl)
(outexp ($ar-binsearch $needle $haystack))
(outs $newl)

