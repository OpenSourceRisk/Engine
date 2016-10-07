/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <test/yieldcurve.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/parsers.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;
using namespace openriskengine;
using namespace openriskengine::data;

namespace {

class MarketDataLoader : public Loader {
public:
    MarketDataLoader();
    const vector<boost::shared_ptr<MarketDatum>>& loadQuotes(const Date&) const;
    const boost::shared_ptr<MarketDatum>& get(const string& name, const Date&) const;
    const vector<Fixing>& loadFixings() const { return fixings_; }

private:
    map<Date, vector<boost::shared_ptr<MarketDatum>>> data_;
    vector<Fixing> fixings_;
};

const vector<boost::shared_ptr<MarketDatum>>& MarketDataLoader::loadQuotes(const Date& d) const {
    auto it = data_.find(d);
    QL_REQUIRE(it != data_.end(), "Loader has no data for date " << d);
    return it->second;
}

const boost::shared_ptr<MarketDatum>& MarketDataLoader::get(const string& name, const Date& d) const {
    for (auto& md : loadQuotes(d)) {
        if (md->name() == name)
            return md;
    }
    QL_FAIL("No MarketDatum for name " << name << " and date " << d);
}

MarketDataLoader::MarketDataLoader() {

    vector<string> data{"20150831 IR_SWAP/RATE/JPY/2D/6M/2Y 0.0022875"};

    for (auto s : data) {
        vector<string> tokens;
        boost::trim(s);
        boost::split(tokens, s, boost::is_any_of(" "), boost::token_compress_on);

        QL_REQUIRE(tokens.size() == 3, "Invalid market data line, 3 tokens expected " << s);
        Date date = parseDate(tokens[0]);
        const string& key = tokens[1];
        Real value = parseReal(tokens[2]);
        data_[date].push_back(parseMarketDatum(date, key, value));
    }
}
}

namespace testsuite {

void YieldCurveTest::testBootstrapAndFixings() {

    SavedSettings backup;

    Date asof(31, August, 2015);
    Settings::instance().evaluationDate() = asof;

    YieldCurveSpec spec("JPY", "JPY6M");

    CurveConfigurations curveConfigs;
    vector<boost::shared_ptr<YieldCurveSegment>> segments{boost::make_shared<SimpleYieldCurveSegment>(
        "Swap", "JPY-SWAP-CONVENTIONS", vector<string>(1, "IR_SWAP/RATE/JPY/2D/6M/2Y"))};
    boost::shared_ptr<YieldCurveConfig> jpyYieldConfig =
        boost::make_shared<YieldCurveConfig>("JPY6M", "JPY 6M curve", "JPY", "", segments);
    curveConfigs.yieldCurveConfig("JPY6M") = jpyYieldConfig;

    Conventions conventions;
    boost::shared_ptr<Convention> convention =
        boost::make_shared<IRSwapConvention>("JPY-SWAP-CONVENTIONS", "JP", "Semiannual", "MF", "A365", "JPY-LIBOR-6M");
    conventions.add(convention);

    MarketDataLoader loader;

    // Construction should fail due to missing fixing on August 28th, 2015
    auto checker = [](const Error& ex) -> bool {
        return string(ex.what()).find("Missing JPYLibor6M Actual/360 fixing for August 28th, 2015") != string::npos;
    };
    BOOST_CHECK_EXCEPTION(YieldCurve jpyYieldCurve(asof, spec, curveConfigs, loader, conventions), Error, checker);

    // Change calendar in conventions to Japan & London and the curve building should not throw an exception
    conventions.clear();
    convention = boost::make_shared<IRSwapConvention>("JPY-SWAP-CONVENTIONS", "JP,UK", "Semiannual", "MF", "A365",
                                                      "JPY-LIBOR-6M");
    conventions.add(convention);
    BOOST_CHECK_NO_THROW(YieldCurve jpyYieldCurve(asof, spec, curveConfigs, loader, conventions));

    // Reapply old convention but load necessary fixing and the curve building should still not throw an exception
    conventions.clear();
    convention =
        boost::make_shared<IRSwapConvention>("JPY-SWAP-CONVENTIONS", "JP", "Semiannual", "MF", "A365", "JPY-LIBOR-6M");
    conventions.add(convention);

    vector<Real> fixings{0.0013086};
    TimeSeries<Real> fixingHistory(Date(28, August, 2015), fixings.begin(), fixings.end());
    IndexManager::instance().setHistory("JPYLibor6M Actual/360", fixingHistory);

    BOOST_CHECK_NO_THROW(YieldCurve jpyYieldCurve(asof, spec, curveConfigs, loader, conventions));
}

test_suite* YieldCurveTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("YieldCurveTest");
    suite->add(BOOST_TEST_CASE(&YieldCurveTest::testBootstrapAndFixings));
    return suite;
}
}
