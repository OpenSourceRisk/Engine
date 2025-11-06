#!/bin/sh

function validate {

# Get a list of the directories to check
dirs="Examples"
[ -d "OREAnalytics/test/input" ] && dirs="${dirs} OREAnalytics/test/input"
[ -d "OREData/test/input" ] && dirs="${dirs} OREData/test/input"
[ -d "QuantExt/test/input" ] && dirs="${dirs} QuantExt/test/input"

# test tests are intended to fail
exclude_dirs="OREData/test/input/calendaradjustment/invalid_calendaradjustments_1.xml OREData/test/input/calendaradjustment/invalid_calendaradjustments_2.xml"
exclude_conditions=""
# Loop through each directory in the exclude_dirs list
for dir in $exclude_dirs; do
  exclude_conditions="$exclude_conditions -not -path $dir "
done

# Check the XML files under the existing directories
find $dirs -name '*.xml' $exclude_conditions -print0 | \
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
