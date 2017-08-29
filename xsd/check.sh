#!/bin/sh

function validate {
find Examples -name $1 -print0 | \
{ 
fail=0
    while read -d $'\0' file
    do
        OUTPUT=$(xmllint --schema $2 --path xsd --noout $file 2>&1)
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
validate 'conventions.xml' conventions.xsd || status=1
validate 'curveconfig.xml' curveconfig.xsd || status=1
validate 'portfolio*.xml' instruments.xsd || status=1
validate 'netting*.xml' nettingsetdefinitions.xsd || status=1
validate 'simulation*.xml' simulation.xsd || status=1
validate 'pricingeng*.xml' pricingengines.xsd || status=1
validate 'todaysmarket*.xml' todaysmarket.xsd || status=1
validate 'sensi*.xml' sensitivity.xsd || status=1
validate 'stress*.xml' stress.xsd || status=1
validate 'ore*.xml' ore.xsd || status=1

exit $status
