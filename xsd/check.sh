#!/bin/sh

function validate {
find Examples -name '*.xml' -print0 | \
{ 
fail=0
    while read -d $'\0' file
    do
        OUTPUT=$(xmllint --schema input.xsd --path xsd --noout $file 2>&1)
        if [ "$?" -gt "0" ] 
        then 
        fail=1
        echo "${OUTPUT}"
        fi
    done
return $fail
}
}

validate || exit 1
