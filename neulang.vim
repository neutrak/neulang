" Vim syntax file
" Language: neulang
" Orig Author: neutrak
" Maintainer: neutrak
" Last Change:	2014-07-08

" Quit when a syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

" support keywords including ? symbols (char 63)
"set iskeyword="@,65-90,97-122,63,_"

" keywords
syn keyword nl_keywords exit let if else and or not xor lit sub begin recur while for after array return outs outexp inexp inchar null? pair f r list assert type
syn keyword nl_symbols TRUE FALSE NULL BYTE_T NUM_T PAIR_T ARRAY_T PRI_T SUB_T STRUCT_T SYMBOL_T EVALUATION_T BIND_T NULL_T

" parens (highlighting included by matchgroup parameter)
syn region nl_parens matchgroup=Delimiter start="(" end=")" contains=ALL transparent

" evaluations
"syn region nl_evaluation start="\$" end="[ \n\(\)]\@<!"
syn match nl_evaluation "\$[a-zA-Z\-\+\/\*\%\_]*"

" strings
syn region nl_str start="\"" end="\""

" characters
syn region nl_char start="\'" end="\'"

" comments
syn region nl_comment start="//" skip="\\$" end="$" contains=nl_todo
syn region nl_multiline_comment start="/\*" end="\*/" contains=nl_todo

" TODO highlighting
syn keyword nl_todo TODO FIXME NOTE

" now apply highlighting
highlight link nl_keywords Keyword
highlight link nl_symbols Type
highlight link nl_evaluation Macro
highlight link nl_char String
highlight link nl_str String
highlight link nl_comment Comment
highlight link nl_multiline_comment Comment
highlight link nl_todo Todo

let b:current_syntax = "neulang"

