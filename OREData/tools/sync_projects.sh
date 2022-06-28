#!/bin/bash

# execute this script from the root of an uncompressed QuantLib tarball

# get reference lists of distributed files (done with find; this is
# why this script should be run from an uncompressed tarball created
# with 'make dist', not from a working copy.)

find ored -name '*.[hc]pp' -or -name '*.[hc]' \
| grep -v 'ored/config\.hpp' | grep -v 'ored/ad\.hpp' | sort > ored.ref.files
find test -name '*.[hc]pp' \
| grep -v '/main\.cpp' \
| sort > test.ref.files

# extract file names from VC8 projects and clean up so that they
# have the same format as the reference lists.

# ...VC10 and above...

grep -o -E 'Include=".*\.[hc]p*"' OREData.vcxproj \
| awk -F'"' '{ print $2 }' | sed -e 's|\\|/|g' | sed -e 's|^./||' \
| sort > ored.vcx.files

grep -o -E 'Include=".*\.[hc]p*"' test/OREDataTestSuite.vcxproj \
| awk -F'"' '{ print $2 }' | sed -e 's|\\|/|g' | sed -e 's|^./||' \
| sed -e 's|^|test/|' | sort > test.vcx.files

grep -o -E 'Include=".*\.[hc]p*"' OREData.vcxproj.filters \
| awk -F'"' '{ print $2 }' | sed -e 's|\\|/|g' | sed -e 's|^./||' \
| sort > ored.vcx.filters

grep -o -E 'Include=".*\.[hc]p*"' test/OREDataTestSuite.vcxproj.filters \
| awk -F'"' '{ print $2 }' | sed -e 's|\\|/|g' | sed -e 's|^./||' \
| sed -e 's|^|test/|' | sort > test.vcx.filters

# write out differences...

echo 'Visual Studio 10 and above:'
echo 'project:'
diff -b ored.vcx.files ored.ref.files
diff -b test.vcx.files test.ref.files
echo 'filters:'
diff -b ored.vcx.filters ored.ref.files
diff -b test.vcx.filters test.ref.files

# ...and cleanup
rm -f ored.ref.files test.ref.files
rm -f ored.vcx.files test.vcx.files
rm -f ored.vcx.filters test.vcx.filters

