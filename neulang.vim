" Vim syntax file
" Language: neulang
" Orig Author: neutrak
" Maintainer: neutrak
" Last Change:	2014-07-08

" Quit when a syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

" keywords
syn keyword nl_keywords exit let if else and or not xor lit sub begin recur while for after array return strout out null? f r assert
syn keyword nl_symbols TRUE FALSE NULL

" parens (highlighting included by matchgroup parameter)
syn region nl_parens matchgroup=Delimiter start="(" end=")" contains=ALL transparent

" evaluations
"syn region nl_evaluation start="\$" end="[ \n\(\)]\@<!"
syn match nl_evaluation "\$[a-zA-Z\-\+\/\*\%\_]*"

" strings
syn region nl_str start="\"" end="\""

" comments
syn region nl_comment start="//" skip="\\$" end="$"
syn region nl_multiline_comment start="/\*" end="\*/"

" now apply highlighting
highlight link nl_keywords Keyword
highlight link nl_symbols Type
highlight link nl_evaluation Macro
highlight link nl_str String
highlight link nl_comment Comment
highlight link nl_multiline_comment Comment

let b:current_syntax = "neulang"

