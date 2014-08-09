//the preliminary code for a line editor written in neulang
//  I hope to eventually add an ncurses frontend to this

//this is being developed in tandem with the neulang interpreter itself (in the same way as C and unix)
//  hopefully that will help to identify deficiencies in the language and make it better in the process

//TODO: write like, all of this

(let VERSION "0.1.0")

//edit a single line, line-info also contains the line number (it's a pair)
(let edit-line (sub (line-info)
	//TODO: write this
	return -1
))

//edit a file (null for new file)
(let edit (sub (file)
	(if (null? $file)
		(strout "Info: no file given, opening new buffer" $newl)
	else
		(strout "Info: got filename " $file "; opening now..." $newl)
	)
	
	//the whole file
	(let edit-buffer "")
	//the currently active line, a pair of line number (0-based) and the text for that line
	(let current-line (pair 0 ""))
	
	(let finished FALSE)
	(while (not $finished)
		//TODO: get input from the user here, and adjust the edit buffer appropriately
		(let line-return ($edit-line $current-line))
		
		//check for special return values (which indicate moving a line)
		(if (= $line-return 0)
			//up
		else (if (= $line-return 1)
			//down
		else (if (= $line-return 2)
			//pgup
		else (if (= $line-return 3)
			//pgdn
		else (if (= $line-return 4)
			//save
		else (if (= $line-return 5)
			//exit
			(let finished TRUE)
		))))))
		
		//TODO: remove this, it's just for debugging
		(let finished TRUE)
	)
))

//handle cli switches
(let switch-args (sub (argv)
	(strout "switch-args debug 0, argv is ")
	(out $argv)
	(strout $newl)
	
	(if (null? $argv)
		(strout "switch-args debug 1, got a NULL argv value" $newl)
		(return FALSE)
	)
	
	(return (while (not (null? $argv))
		(if (ar= (f $argv) "--help")
			(strout "[help] please see the manual page for detailed help" $newl)
			(return TRUE)
		else (if (ar= (f $argv) "--version")
			(strout (, "[version] line editor (in neulang) v" $VERSION $newl))
			(return TRUE)
		else
			(strout (, "skipping unknown argument " (f $argv) $newl))
		))
		
		(let argv (r $argv))
	after
		(return FALSE)
	))
))


//runtime entry point, with command line arguments
(let main (sub (argv)
	(strout "main debug 0, argv is ")
	(out $argv)
	(strout $newl)
	
	//skip over the script name
	(if (not (null? $argv))
		(let argv (r $argv))
	)
	
	//first process special switches like --help and --version
	(if ($switch-args $argv)
		//if these were found return early
		(return 0)
	)
	(strout "main debug 1" $newl)
	
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

