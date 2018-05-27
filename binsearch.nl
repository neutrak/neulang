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
(outs $newl)

(if (>= (list-sz $argv) 2)
	(let needle (str->num (f (r $argv))))
	(let haystack (ar-map (ar-chop (f (r (r $argv))) " ") (sub (exp) (str->num $exp))))
	(outexp $needle)
	(outs $newl)
	(outexp $haystack)
	(outs $newl)
	(outexp ($ar-binsearch $needle $haystack))
	(outs $newl)
	(exit 0)
)


//find how much a previously sorted array has been rotated by in log(n) time
(let ar-rot-search (sub (haystack)
	//tail-recursive implementation
	//largest-difference should be initialized to the difference at the bounds, and start-idx should be initialized at 0
	(let iter-ar-rot-search (sub (largest-difference haystack start-idx)
		//an empty or one-element array hasn't been rotated at all, because rotation doesn't make sense on it
		(if (<= (ar-sz $haystack) 1)
			$start-idx
		else
			(let cmp-idx (floor (/ (ar-sz $haystack) 2)))
			(let cmp (ar-idx $haystack $cmp-idx))
			(let low-difference (abs (- $cmp (ar-idx $haystack 0))))
			(let high-difference (abs (- $cmp (ar-idx $haystack (- (ar-sz $haystack) 1)))))
			
			//if the difference between the 0th index item and the cmp-idx index item is larger than our previously largest difference
			//the the largest difference (and therefore the rotation index) is within the range [0,cmp-idx+1]
			(if (and (>= $low-difference $high-difference) (< $largest-difference $low-difference))
				($iter-ar-rot-search $low-difference (ar-subar $haystack 0 (+ $cmp-idx 1)) $start-idx)
			//if the difference between the last item and the cmp-idx index item is larger than our previously largest difference
			//then the largest difference (and therefore the rotation index) is within the range [cmp-idx, ar-sz - 1]
			else (if (< $largest-difference $high-difference)
				($iter-ar-rot-search $high-difference (ar-subar $haystack $cmp-idx (- (ar-sz $haystack) $cmp-idx)) (+ $start-idx $cmp-idx))
			//otherwise no difference was larger than our previously largest difference
			//that difference is defined as the difference between elements at the search bounds (0th and (size-1)th entries)
			//so return the index for whichever of those elements is smaller (that should be where the start of the array was prior to rotation)
			else
				(+ $start-idx (if (< (ar-idx $haystack 0) (ar-idx $haystack (- (ar-sz $haystack) 1)))
					0
				else
					(- (ar-sz $haystack) 1)
				))
			))
		)
	))
	($iter-ar-rot-search (abs (- (ar-idx $haystack (- (ar-sz $haystack) 1)) (ar-idx $haystack 0))) $haystack 0)
))

//verify that rotation index binary search works as it should
(assert (= ($ar-rot-search (array 1 2 3 4 5 6 7)) 0))
(assert (= ($ar-rot-search (array 7 1 2 3 4 5 6)) 1))
(assert (= ($ar-rot-search (array 6 7 1 2 3 4 5)) 2))
(assert (= ($ar-rot-search (array 5 6 7 1 2 3 4)) 3))
(assert (= ($ar-rot-search (array 4 5 6 7 1 2 3)) 4))
(assert (= ($ar-rot-search (array 3 4 5 6 7 1 2)) 5))
(assert (= ($ar-rot-search (array 2 3 4 5 6 7 1)) 6))
(outs $newl)




