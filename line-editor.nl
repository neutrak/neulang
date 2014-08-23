//the preliminary code for a line editor written in neulang
//  I hope to eventually add an ncurses frontend to this

//this is being developed in tandem with the neulang interpreter itself (in the same way as C and unix)
//  hopefully that will help to identify deficiencies in the language and make it better in the process

//TODO: write like, all of this

//global constants
(let VERSION "0.1.0")

(let up-ret 0)
(let down-ret 1)
(let pgup-ret 2)
(let pgdn-ret 3)
(let save-ret 4)
(let exit-ret 5)

//I realize that this struct thing is sort of round-about and nasty; I'm seriously considering adding some syntactic sugar for struct definitions

//a structure for lines
//consists of members ret (character return), num (line number), and content (string)
(let line-struct (sub ()
	//constructor (initial data)
	//consists of character return (e.g. up-ret), line number, current index (the current location we are at within the line), and line content
	(let struct-data (array 0 1 0 ""))
	
	(return (sub (rqst-type var new-val)
		//get operations return the existing value
		(if (sym= $rqst-type get)
			(if (sym= $var ret)
				(return (ar-idx $struct-data 0))
			else (if (sym= $var num)
				(return (ar-idx $struct-data 1))
			else (if (sym= $var idx)
				(return (ar-idx $struct-data 2))
			else (if (sym= $var content)
				(return (ar-idx $struct-data 3))
			else
				(outs "Err: symbol ")
				(outexp $var)
				(outs " is not a member of line-struct" $newl)
			))))
		//set operations replace the existing value with the new value
		else (if (sym= $rqst-type set)
			(outs "line-struct debug 0, trying to re-set structure " (val->str $struct-data) $newl)
			(outs "$new-val is " (val->str $new-val) $newl)
			
			(if (sym= $var ret)
				(let struct-data (ar-replace $struct-data 0 $new-val))
			else (if (sym= $var num)
				(let struct-data (ar-replace $struct-data 1 $new-val))
			else (if (sym= $var idx)
				(let struct-data (ar-replace $struct-data 2 $new-val))
			else (if (sym= $var content)
				(let struct-data (ar-replace $struct-data 3 $new-val))
			else
				(outs "Err: symbol ")
				(outexp $var)
				(outs " is not a member of line-struct" $newl)
			))))
			
			(outs "line-struct debug 1, struct-data is now " (val->str $struct-data) $newl)
		//return the array version of this data structure as it exists in memory
		else (if (sym= $rqst-type ret)
			$struct-data
		else
			(outs "Err: not a valid operation for line-struct ")
			(outexp $rqst-type)
			(outs $newl)
		)))
	))
))

/*
//this is the syntax I'm hoping to implement to replace the above structure definition (functionally the same though)
(let line-struct (struct ()
	(ret 0)
	(num 1)
	(idx 0)
	(content "")
))
*/

(let output-line (sub (line-info)
	(outs "[line ")
	(outexp ($line-info get num NULL))
	(outs "] [idx ")
	(outexp ($line-info get idx NULL))
	(outs "] " ($line-info get content NULL) $newl)
))

//edit a single line, line-info also contains the line number (it's a pair)
(let edit-line (sub (line-info)
	//constants
	(let up-char (list 65 27))
	(let down-char (list 66 27))
	(let pgup-char (list 53 126))
	(let pgdn-char (list 54 126))
	//TODO: figure out real values for these constants, they are NOT all 128
	(let save-char (list 128))
	(let exit-char (list 128))
	
	(let left-char (list 68))
	(let right-char (list 67))
	(let home-char (list 49 126))
	(let end-char (list 52 126))
	
	//read a character
	(let c (inchar))
	
	(outs "edit-line debug -1, going into while loop..." $newl)
	//while the user hasn't hit enter (a newline character)
	(let line-return (while (not (or (= $c 10) (= $c 13)))
		(let escaped FALSE)
		//if we got an escape sequence then note that and read the next character
		(if (= $c 91)
			(let escaped TRUE)
			(let c (inchar))
		)
		
		(if (b= $escaped TRUE)
			//check for special escapes (movement that can't be handled in one line)
			(if (= $c (f $up-char))
//				(inchar)
//				($line-info set num (- ($line-info get num NULL) 1))
				(outs "edit-line debug 0, returning $up-ret..." $newl)
				(return $up-ret)
			else (if (= $c (f $down-char))
//				(inchar)
//				($line-info set num (+ ($line-info get num NULL) 1))
				(outs "edit-line debug 0.5, returning $down-ret..." $newl)
				(return $down-ret)
			else (if (= $c (f $pgup-char))
				(let c (inchar))
				(if (= $c (f (r $pgup-char)))
					(return $pgup-ret)
				)
			else (if (= $c (f $pgdn-char))
				(let c (inchar))
				(if (= $c (f (r $pgdn-char)))
					(return $pgdn-ret)
				)
			else (if (= $c (f $save-char))
				(return $save-ret)
			else (if (= $c (f $exit-char))
				(return $exit-ret)
			))))))
			
			(outs "edit-line debug 0.75..." $newl)
			
			//check for local escapes (movement that CAN be handled on one line)
			(if (= $c (f $left-char))
				(let line-idx ($line-info get idx NULL))
				(if (> $line-idx 0)
					($line-info set idx (- $line-idx 1))
				)
			else (if (= $c (f $right-char))
				(let line-idx ($line-info get idx NULL))
				(if (< $line-idx (- (ar-sz ($line-info get content NULL)) 1))
					($line-info set idx (+ $line-idx 1))
				)
			))
			
			(outs "edit-line debug 1, got an escape but didn't yet return; skipping to the next char" $newl)
			
			//skip over unknown escape sequences, (return (recur)) acts as continue within a while loop
			(return (recur))
		)
		
		(if (= $c 127)
			(outs "edit-line debug, got a backspace..." $newl)
		)
		
/*
		//if we got a newline, then skip the rest of the function
		(if (or (= $c 10) (= $c 13))
			(return (recur))
		)
*/
		
		//TODO: default case, add the char to the line and output the line
		($line-info set content (, ($line-info get content NULL) (array (num->byte $c))))
		($line-info set idx (+ ($line-info get idx NULL) 1))
		($output-line $line-info)
		
		//debug, just output the damn thing
		(outexp $c)
		(outs " (" (array (num->byte $c)) ")" $newl)
		
		(outs "edit-line debug 3..." $newl)
		
		(outs $newl)
		
		//read in another character
		(let c (inchar))
	after
		(return -1)
	))
	
	(return $line-return)
))

//edit a file (null for new file)
(let edit (sub (file)
	//the whole file
	(let edit-buffer "")
	(if (null? $file)
		(outs "Info: no file given, opening new buffer" $newl)
	else
		(outs "Info: got filename " $file "; opening now..." $newl)
//		(let edit-buffer (infile $file))
	)
	
	//the currently active line
	(let current-line ($line-struct))
	//TODO: set the current line content correctly
	($current-line set content "")
	
	(let finished FALSE)
	(while (not $finished)
		//TODO: get input from the user here, and adjust the edit buffer appropriately
		(let line-return ($edit-line $current-line))
		(outs "edit debug 0; returned from edit-line with value " (val->str $line-return) $newl)
		(outs "current line data is " (val->str ($current-line ret NULL NULL)) $newl)
//		(exit)
		
		//check for special return values (which indicate moving a line)
		(if (= $line-return $up-ret)
			//up
//			($current-line set num (- ($current-line get num NULL) 1))
		else (if (= $line-return $down-ret)
			//down
//			($current-line set num (+ ($current-line get num NULL) 1))
		else (if (= $line-return $pgup-ret)
			//pgup
		else (if (= $line-return $pgdn-ret)
			//pgdn
		else (if (= $line-return $save-ret)
			//save
		else (if (= $line-return $exit-ret)
			//exit
			(let finished TRUE)
		))))))
		
		//TODO: remove this, it's just for debugging
		(let finished TRUE)
	)
	
	//TODO: remove this, it's just for debugging
	(outs ($current-line get content NULL) $newl)
))

//handle cli switches
(let switch-args (sub (argv)
	(outs "switch-args debug 0, argv is ")
	(outexp $argv)
	(outs $newl)
	
	(if (null? $argv)
		(outs "switch-args debug 1, got a NULL argv value" $newl)
		(return FALSE)
	)
	
	(return (while (not (null? $argv))
		(if (ar= (f $argv) "--help")
			(outs "[help] please see the manual page for detailed help" $newl)
			(return TRUE)
		else (if (ar= (f $argv) "--version")
			(outs (, "[version] line editor (in neulang) v" $VERSION $newl))
			(return TRUE)
		else
			(outs (, "skipping unknown argument " (f $argv) $newl))
		))
		
		(let argv (r $argv))
	after
		(return FALSE)
	))
))


//runtime entry point, with command line arguments
(let main (sub (argv)
/*
	(outs "main debug 0, argv is ")
	(outexp $argv)
	(outs $newl)
*/
	
	//skip over the script name
	(if (not (null? $argv))
		(let argv (r $argv))
	)
	
	//first process special switches like --help and --version
	(if ($switch-args $argv)
		//if these were found return early
		(return 0)
	)
	
	//if we're given a file name, try to edit that
	(if (not (null? $argv))
		($edit (f $argv))
	//no file name, create a new buffer
	else
		($edit NULL)
	)
	
	//return with no error on *nix
	(return 0)
))


//runtime!
(let exit-code ($main $argv))

//TODO: make the interpreter actually take arguments to exit
(exit $exit-code)

