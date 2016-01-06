/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include "xassetmodel.hpp"
#include "utilities.hpp"
#include <qle/models/all.hpp>
#include <qle/pricingengines/all.hpp>

#include <boost/make_shared.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;
using namespace boost::accumulators;

void XAssetModelTest::testCcyLgm3fForeignPayouts() {
    BOOST_TEST_MESSAGE("Testing pricing of foreign payouts under domestic "
                       "measure in Ccy LGM 3F model...");

} // testCcyLgm3fForeignPayouts

test_suite *XAssetModelTest::suite() {
    test_suite *suite = BOOST_TEST_SUITE("XAsset model tests");
    suite->add(QUANTEXT_TEST_CASE(&XAssetModelTest::testCcyLgm3fForeignPayouts));
    return suite;
}
