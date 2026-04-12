#!/bin/bash

files=$(find . -type f \( \
-name *.outast ! -path "*/examples/*" \
-o -name *.outderivation ! -path "*/examples/*" \
-o -name *.outlextokens ! -path "*/examples/*" \
-o -name *.outlextokensflaci ! -path "*/examples/*" \
-o -name *.outsymboltables ! -path "*/examples/*" \
-o -name *.outerrors ! -path "*/examples/*" \
-o -name *.moon ! -path "*/examples/*" \
\))

if [ -z "$files" ]; then
  echo "No files to clean found"
  exit 1
fi

ls -l $files

read -p "Clean? [Y/n]: " answer && [ -z "$answer" ] || case "$answer" in [Yy]*) ;; *) exit 1;; esac

rm $files
