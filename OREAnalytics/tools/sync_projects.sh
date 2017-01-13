#!/bin/bash

# execute this script from the root of an uncompressed QuantLib tarball

# get reference lists of distributed files (done with find; this is
# why this script should be run from an uncompressed tarball created
# with 'make dist', not from a working copy.)

find orea -name '*.[hc]pp' -or -name '*.[hc]' \
| grep -v 'orea/config\.hpp' | grep -v 'orea/ad\.hpp' | sort > orea.ref.files
find test -name '*.[hc]pp' \
| grep -v '/main\.cpp' \
| sort > test.ref.files

# extract file names from VC8 projects and clean up so that they
# have the same format as the reference lists.

# ...VC10 and above...

grep -o -E 'Include=".*\.[hc]p*"' OREAnalytics.vcxproj \
| awk -F'"' '{ print $2 }' | sed -e 's|\\|/|g' | sed -e 's|^./||' \
| sort > orea.vcx.files

grep -o -E 'Include=".*\.[hc]p*"' test/OREAnalyticsTestSuite.vcxproj \
| awk -F'"' '{ print $2 }' | sed -e 's|\\|/|g' | sed -e 's|^./||' \
| sed -e 's|^|test/|' | sort > test.vcx.files

grep -o -E 'Include=".*\.[hc]p*"' OREAnalytics.vcxproj.filters \
| awk -F'"' '{ print $2 }' | sed -e 's|\\|/|g' | sed -e 's|^./||' \
| sort > orea.vcx.filters

grep -o -E 'Include=".*\.[hc]p*"' test/OREAnalyticsTestSuite.vcxproj.filters \
| awk -F'"' '{ print $2 }' | sed -e 's|\\|/|g' | sed -e 's|^./||' \
| sed -e 's|^|test/|' | sort > test.vcx.filters

# write out differences...

echo 'Visual Studio 10 and above:'
echo 'project:'
diff -b orea.vcx.files orea.ref.files
diff -b test.vcx.files test.ref.files
echo 'filters:'
diff -b orea.vcx.filters orea.ref.files
diff -b test.vcx.filters test.ref.files

# ...and cleanup
rm -f orea.ref.files test.ref.files
rm -f orea.vcx.files test.vcx.files
rm -f orea.vcx.filters test.vcx.filters

