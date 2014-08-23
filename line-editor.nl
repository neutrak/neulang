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

//a structure for lines
//consists of members ret (character return), num (line number), idx (column within line), and content (string)
(let line-struct (sub ()
	(struct
		(ret -1)
		(num 1)
		(idx 0)
		(content "")
	)
))

(let output-line (sub (line-info)
	(outs "[line ")
	(outexp (struct-get $line-info num))
	(outs "] [idx ")
	(outexp (struct-get $line-info idx))
	(outs "] " (struct-get $line-info content) $newl)
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
	(let line-info (while (not (or (= $c 10) (= $c 13)))
		(let escaped FALSE)
		//if we got an escape sequence then note that and read the next character
		(if (= $c 91)
			(let escaped TRUE)
			(let c (inchar))
		)
		
		(outs "edit-line debug -0.5, escaped is " (val->str $escaped) $newl)
		
		(if (b= $escaped TRUE)
			//check for special escapes (movement that can't be handled in one line)
			(if (= $c (f $up-char))
//				(inchar)
//				(let line-info (struct-replace $line-info num (- (struct-get $line-info num) 1)))
				(outs "edit-line debug 0, returning $up-ret..." $newl)
				(let line-info (struct-replace $line-info ret $up-ret))
				(return $line-info)
			else (if (= $c (f $down-char))
//				(inchar)
//				(let line-info (struct-replace $line-info num (+ (struct-get $line-info num) 1)))
				(outs "edit-line debug 0.5, returning $down-ret..." $newl)
				(let line-info (struct-replace $line-info ret $down-ret))
				(return $line-info)
			else (if (= $c (f $pgup-char))
				(let c (inchar))
				(if (= $c (f (r $pgup-char)))
					(let line-info (struct-replace $line-info ret $pgup-ret))
					(return $line-info)
				)
			else (if (= $c (f $pgdn-char))
				(let c (inchar))
				(if (= $c (f (r $pgdn-char)))
					(let line-info (struct-replace $line-info ret $pgdn-ret))
					(return $line-info)
				)
			else (if (= $c (f $save-char))
				(let line-info (struct-replace $line-info ret $save-ret))
				(return $line-info)
			else (if (= $c (f $exit-char))
				(let line-info (struct-replace $line-info ret $exit-ret))
				(return $line-info)
			))))))
			
			(outs "edit-line debug 0.75..." $newl)
			
			//check for local escapes (movement that CAN be handled on one line)
			(let line-idx (struct-get $line-info idx))
			(if (= $c (f $left-char))
//				(inchar) //skip a control character
				(if (> $line-idx 0)
					(outs "edit-line debug 0.85, line-idx is " (val->str $line-idx) $newl)
					(let line-info (struct-replace $line-info idx (- $line-idx 1)))
					(outs "edit-line debug 0.85, line info idx is " (val->str (struct-get $line-info idx)) $newl)
				)
			else (if (= $c (f $right-char))
//				(inchar) //skip a control character
				(if (< $line-idx (- (ar-sz (struct-get $line-info content)) 1))
					(let line-info (struct-replace $line-info idx (+ $line-idx 1)))
				)
			))
			
			(outs "edit-line debug 1, got an escape but didn't yet return; skipping to the next char" $newl)
			
			//skip over unknown escape sequences, (return (recur)) acts as continue within a while loop
			(return (recur))
		)
		
		(outs "edit-line debug 2" $newl)
		
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
		(let line-info (struct-replace $line-info content (, (struct-get $line-info content) (array (num->byte $c)))))
		(let line-info (struct-replace $line-info idx (+ (struct-get $line-info idx) 1)))
		($output-line $line-info)
		
		//debug, just output the damn thing
		(outexp $c)
		(outs " (" (array (num->byte $c)) ")" $newl)
		
		(outs "edit-line debug 3..." $newl)
		
		(outs $newl)
		
		//read in another character
		(let c (inchar))
	after
		(let line-info (struct-replace $line-info ret -1))
		(return $line-info)
	))
	
	(return $line-info)
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
	(let current-line (struct-replace $current-line content ""))
	
	(let finished FALSE)
	(while (not $finished)
		//TODO: get input from the user here, and adjust the edit buffer appropriately
		(let current-line ($edit-line $current-line))
		(let line-return (struct-get $current-line ret))
		
		(outs "edit debug 0; returned from edit-line with value " (val->str $line-return) $newl)
		(outs "current line data is " (val->str $current-line) $newl)
		
		//check for special return values (which indicate moving a line)
		(if (= $line-return $up-ret)
			//up
//			(let current-line (struct-replace $current-line num (- (struct-get $current-line num) 1)))
		else (if (= $line-return $down-ret)
			//down
//			(let current-line (struct-replace $current-line num (+ (struct-get $current-line num) 1)))
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
	(outs (struct-get $current-line content) $newl)
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

