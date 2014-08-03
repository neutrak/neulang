//the preliminary code for a line editor written in neulang
//  I hope to eventually add an ncurses frontend to this

//this is being developed in tandem with the neulang interpreter itself (in the same way as C and unix)
//  hopefully that will help to identify deficiencies in the language and make it better in the process

//TODO: write like, all of this

(let VERSION "0.1.0")
(let newline (array (int->byte 10))) //\n
(let quote (array (int->byte 39))) //'
(let quotes (array (int->byte 34))) //"

//handle cli switches
(let switch-args (sub (argv)
	(strout "switch-args debug 0, argv is ")
	(out $argv)
	(strout $newline)
	
	(if (null? $argv)
		(return FALSE)
	)
	
/*
	(if (ar= (f $argv) "--help")
		(strout "[help] please see the manual page for detailed help" $newline)
		(return TRUE)
	)
	
	(while (not (null? (r $argv)))
//	(if (not (null? (r $argv)))
		(out $argv)
		(strout $newline)
		(strout "arguments continue" $newline)
		
		(let argv (r $argv))
		
		(out $argv)
		(strout $newline)
	)
	(return FALSE)
*/
	
	(return (for n 0 (< $n (list-sz $argv)) (+ $n 1)
		(if (ar= (list-idx $argv $n) "--help")
			(strout "[help] please see the manual page for detailed help" $newline)
			(return TRUE)
		else (if (ar= (list-idx $argv $n) "--version")
			(strout (, "[version] line editor (in neulang) v" $VERSION $newline))
			(return TRUE)
		))
	after
		(return FALSE)
	))
/*
	(return (while (not (null? $argv))
		(strout (, "skipping unknown argument " (f $argv) $newline))
		(let argv (r $argv))
	after
		(return FALSE)
	))
*/
))


//runtime entry point, with command line arguments
(let main (sub (argv)
	//first process special switches like --help and --version
	(if ($switch-args $argv)
		//if these were found return early
		(return 0)
	)
	
	//if we're given a file name, try to edit that
	(if (> (list-sz $argv) 1)
//		($edit (list-idx $argv 1))
	//no file name, create a new buffer
	else
//		($edit)
	)
	
	
	//return with no error on *nix
	(return 0)
))


//runtime!
//($main $argv)

($switch-args $argv)

(exit)

