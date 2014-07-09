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
syn keyword nl_keywords exit let if else lit sub return
syn keyword nl_symbols TRUE FALSE NULL
syn keyword nl_parens ( )

" evaluations
syn region nl_evaluation start="\$" end="[ \n\(\)]\@<!"

" strings
syn region nl_str start="\"" end="\""

" comments
syn region nl_comment start="//" skip="\\$" end="$"
syn region nl_multiline_comment start="/\*" end="\*/"

" now apply highlighting
highlight link nl_keywords Keyword
highlight link nl_symbols Type
highlight link nl_parens Delimiter
highlight link nl_evaluation Macro
highlight link nl_str String
highlight link nl_comment Comment
highlight link nl_multiline_comment Comment

let b:current_syntax = "neulang"

