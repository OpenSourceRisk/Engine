#!/bin/bash
export TEXINPUTS=.:$ORE/Docs/UserGuide:$ORE/DocsPlus::
#pdflatex --file-line-error --synctex=1 --shell-escape $1
pdflatex --shell-escape $1
