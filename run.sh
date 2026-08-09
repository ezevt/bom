#!/bin/bash

files="src/main.c src/term.c src/editor.c"
flags="-Wall -Wextra -pedantic"
output="bin/editor"


gcc $flags $files -o $output \
    && ./$output
