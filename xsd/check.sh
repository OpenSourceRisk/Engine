#!/bin/sh

function validate {

# Get a list of the directories to check
dirs="Examples"
[ -d "OREAnalytics/test/input" ] && dirs="${dirs} OREAnalytics/test/input"
[ -d "OREData/test/input" ] && dirs="${dirs} OREData/test/input"
[ -d "QuantExt/test/input" ] && dirs="${dirs} QuantExt/test/input"

# Check the XML files under the existing directories
find $dirs -name '*.xml' -print0 | \
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
