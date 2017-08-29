#!/bin/sh

function validate {
find $1 -name $2 -print0 | \
{ 
fail=0
    while read -d $'\0' file
    do
        OUTPUT=$(xmllint --schema $3 --path xsd --noout $file 2>&1)
        if [ "$?" -gt "0" ] 
        then 
        fail=1
        echo "${OUTPUT}"
        fi
    done
return $fail
}
}

status=0
validate Examples '*.xml' input.xsd || status=1

exit $status
