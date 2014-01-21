#!/usr/bin/nl

//this makes us search the $PATH environment variable when a function isn't found
//we will define the function as taking one string as an argument
//and when called it will be split by spaces and passed to the binary
//the return value of all shell functions is the value of stdout when execution completes
//(stdout is also output in real-time as the program outputs to it, because otherwise interactive programs wouldn't work well)
//note that the return value is not output by default, you have to explictly output it
(import-system-paths)

(ls)

//strings are passed to all shell commands
(ls "-a")

//pipes are done with the | keyword, it's just prefix notation; we do the stdin<->stdout thing in the interpreter
(| (cat "docs/file.txt") fgrep (, " -n " $qut "kittens" $qut))

//that was just to demonstrate; you can also call it correctly
(fgrep "kittens docs/file.txt"))

(vim "docs/file.txt")

(let[[byte]array] contents (cat "docs/file.txt")


