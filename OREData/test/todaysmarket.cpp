/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <boost/test/unit_test.hpp>
#include <ored/configuration/volatilityconfig.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/portfolio/builders/cms.hpp>
#include <ored/portfolio/builders/cmsspread.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/legbuilders.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/time/calendars/all.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>
#include <map>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;
using namespace ore;
using namespace ore::data;

using ore::test::TopLevelFixture;

namespace {

class MarketDataLoader : public Loader {
public:
    MarketDataLoader();
    std::vector<QuantLib::ext::shared_ptr<MarketDatum>> loadQuotes(const QuantLib::Date&) const override;
    std::set<Fixing> loadFixings() const override { return fixings_; }
    std::set<QuantExt::Dividend> loadDividends() const override { return dividends_; }
    void add(QuantLib::Date date, const string& name, QuantLib::Real value) {}
    void addFixing(QuantLib::Date date, const string& name, QuantLib::Real value) {}
    void addDividend(const QuantExt::Dividend& div) {}

private:
    std::map<QuantLib::Date, std::vector<QuantLib::ext::shared_ptr<MarketDatum>>> data_;
    std::set<Fixing> fixings_;
    std::set<QuantExt::Dividend> dividends_;
};

vector<QuantLib::ext::shared_ptr<MarketDatum>> MarketDataLoader::loadQuotes(const Date& d) const {
    auto it = data_.find(d);
    QL_REQUIRE(it != data_.end(), "Loader has no data for date " << d);
    return it->second;
}

MarketDataLoader::MarketDataLoader() {
    // clang-format off
    vector<string> data = boost::assign::list_of
        // borrow spread curve
        ("20160226 ZERO/YIELD_SPREAD/EUR/BANK_EUR_BORROW/A365/2Y -0.0010")
        ("20160226 ZERO/YIELD_SPREAD/EUR/BANK_EUR_BORROW/A365/5Y -0.0010")
        ("20160226 ZERO/YIELD_SPREAD/EUR/BANK_EUR_BORROW/A365/10Y -0.0010")
        ("20160226 ZERO/YIELD_SPREAD/EUR/BANK_EUR_BORROW/A365/20Y -0.0010")
        // lending spread curve
        ("20160226 ZERO/YIELD_SPREAD/EUR/BANK_EUR_LEND/A365/2Y 0.0050")
        ("20160226 ZERO/YIELD_SPREAD/EUR/BANK_EUR_LEND/A365/5Y 0.0050")
        ("20160226 ZERO/YIELD_SPREAD/EUR/BANK_EUR_LEND/A365/10Y 0.0050")
        ("20160226 ZERO/YIELD_SPREAD/EUR/BANK_EUR_LEND/A365/20Y 0.0050")
        // Eonia curve
        ("20160226 MM/RATE/EUR/0D/1D -0.0025")
        ("20160226 IR_SWAP/RATE/EUR/0D/1D/1D -0.0025")
        ("20160226 IR_SWAP/RATE/EUR/0D/1D/2D -0.0027")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/3D -0.003")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/1W -0.00245")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/2W -0.00245")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/3W -0.00245")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/1M -0.0030275")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/2M -0.003335")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/3M -0.003535")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/4M -0.00365")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/5M -0.0037925")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/6M -0.0037975")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/7M -0.00402")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/8M -0.0040475")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/9M -0.0041875")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/10M -0.004245")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/11M -0.00431")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/1Y -0.00436")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/2Y -0.004645")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/3Y -0.0043525")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/4Y -0.00375")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/5Y -0.0029")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/6Y -0.00185")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/7Y -0.00067")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/8Y 0.0005")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/9Y 0.00162")
        ("20160226 IR_SWAP/RATE/EUR/2D/1D/10Y 0.0026375")
        // USD Fed Funds curve
        ("20160226 MM/RATE/USD/0D/1D 0.00448")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/1M 0.004458")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/3M 0.004851")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/6M 0.005237")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/9M 0.005471")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/1Y 0.005614")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/2Y 0.006433")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/3Y 0.007101")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/4Y 0.008264")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/5Y 0.009269")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/7Y 0.011035")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/10Y 0.013318")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/12Y 0.01459")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/15Y 0.016029")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/20Y 0.01734")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/25Y 0.01804")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/30Y 0.018326")
        ("20160226 IR_SWAP/RATE/USD/2D/1D/50Y 0.0182")
        // USD 3M curve
        ("20160226 MM/RATE/USD/2D/3M 0.007961")
        ("20160226 FRA/RATE/USD/3M/3M 0.008132")
        ("20160226 FRA/RATE/USD/6M/3M 0.00858")
        ("20160226 FRA/RATE/USD/9M/3M 0.009141")
        ("20160226 FRA/RATE/USD/1Y/3M 0.009594")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/2Y 0.009268")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/3Y 0.010244")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/4Y 0.011307")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/5Y 0.012404")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/6Y 0.013502")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/7Y 0.014357")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/8Y 0.015181")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/9Y 0.016063")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/10Y 0.016805")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/12Y 0.018038")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/15Y 0.019323")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/20Y 0.020666")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/25Y 0.021296")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/30Y 0.021745")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/40Y 0.021951")
        ("20160226 IR_SWAP/RATE/USD/2D/3M/50Y 0.021741")
        // USD lognormal swaption quotes
        ("20160226 SWAPTION/RATE_LNVOL/USD/3M/10Y/ATM 0.548236")
        ("20160226 SWAPTION/RATE_LNVOL/USD/25Y/10Y/ATM 0.279322")
        ("20160226 SWAPTION/RATE_LNVOL/USD/10Y/10Y/ATM 0.343264")
        ("20160226 SWAPTION/RATE_LNVOL/USD/15Y/10Y/ATM 0.306509")
        ("20160226 SWAPTION/RATE_LNVOL/USD/7Y/10Y/ATM 0.378516")
        ("20160226 SWAPTION/RATE_LNVOL/USD/6M/10Y/ATM 0.541913")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3Y/10Y/ATM 0.451828")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1Y/10Y/ATM 0.522381")
        ("20160226 SWAPTION/RATE_LNVOL/USD/2Y/10Y/ATM 0.485922")
        ("20160226 SWAPTION/RATE_LNVOL/USD/5Y/10Y/ATM 0.413209")
        ("20160226 SWAPTION/RATE_LNVOL/USD/30Y/10Y/ATM 0.279684")
        ("20160226 SWAPTION/RATE_LNVOL/USD/20Y/10Y/ATM 0.280131")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1M/10Y/ATM 0.542948")
        ("20160226 SWAPTION/RATE_LNVOL/USD/4Y/10Y/ATM 0.428622")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3M/15Y/ATM 0.478372")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1M/15Y/ATM 0.471117")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1Y/15Y/ATM 0.456872")
        ("20160226 SWAPTION/RATE_LNVOL/USD/30Y/15Y/ATM 0.273396")
        ("20160226 SWAPTION/RATE_LNVOL/USD/6M/15Y/ATM 0.475296")
        ("20160226 SWAPTION/RATE_LNVOL/USD/20Y/15Y/ATM 0.265159")
        ("20160226 SWAPTION/RATE_LNVOL/USD/10Y/15Y/ATM 0.318263")
        ("20160226 SWAPTION/RATE_LNVOL/USD/4Y/15Y/ATM 0.383914")
        ("20160226 SWAPTION/RATE_LNVOL/USD/15Y/15Y/ATM 0.28198")
        ("20160226 SWAPTION/RATE_LNVOL/USD/2Y/15Y/ATM 0.433144")
        ("20160226 SWAPTION/RATE_LNVOL/USD/25Y/15Y/ATM 0.262587")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3Y/15Y/ATM 0.405347")
        ("20160226 SWAPTION/RATE_LNVOL/USD/5Y/15Y/ATM 0.370537")
        ("20160226 SWAPTION/RATE_LNVOL/USD/7Y/15Y/ATM 0.345499")
        ("20160226 SWAPTION/RATE_LNVOL/USD/4Y/1Y/ATM 0.606815")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3Y/1Y/ATM 0.731808")
        ("20160226 SWAPTION/RATE_LNVOL/USD/2Y/1Y/ATM 0.780075")
        ("20160226 SWAPTION/RATE_LNVOL/USD/5Y/1Y/ATM 0.562741")
        ("20160226 SWAPTION/RATE_LNVOL/USD/10Y/1Y/ATM 0.38573")
        ("20160226 SWAPTION/RATE_LNVOL/USD/7Y/1Y/ATM 0.475593")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1M/1Y/ATM 0.528012")
        ("20160226 SWAPTION/RATE_LNVOL/USD/25Y/1Y/ATM 0.277531")
        ("20160226 SWAPTION/RATE_LNVOL/USD/15Y/1Y/ATM 0.32467")
        ("20160226 SWAPTION/RATE_LNVOL/USD/6M/1Y/ATM 0.666817")
        ("20160226 SWAPTION/RATE_LNVOL/USD/20Y/1Y/ATM 0.296386")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3M/1Y/ATM 0.635173")
        ("20160226 SWAPTION/RATE_LNVOL/USD/30Y/1Y/ATM 0.289454")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1Y/1Y/ATM 0.742497")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3Y/20Y/ATM 0.383058")
        ("20160226 SWAPTION/RATE_LNVOL/USD/2Y/20Y/ATM 0.404186")
        ("20160226 SWAPTION/RATE_LNVOL/USD/25Y/20Y/ATM 0.259696")
        ("20160226 SWAPTION/RATE_LNVOL/USD/10Y/20Y/ATM 0.303547")
        ("20160226 SWAPTION/RATE_LNVOL/USD/30Y/20Y/ATM 0.270757")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1M/20Y/ATM 0.438879")
        ("20160226 SWAPTION/RATE_LNVOL/USD/7Y/20Y/ATM 0.330822")
        ("20160226 SWAPTION/RATE_LNVOL/USD/20Y/20Y/ATM 0.256717")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3M/20Y/ATM 0.444512")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1Y/20Y/ATM 0.427477")
        ("20160226 SWAPTION/RATE_LNVOL/USD/15Y/20Y/ATM 0.274453")
        ("20160226 SWAPTION/RATE_LNVOL/USD/6M/20Y/ATM 0.442455")
        ("20160226 SWAPTION/RATE_LNVOL/USD/4Y/20Y/ATM 0.363194")
        ("20160226 SWAPTION/RATE_LNVOL/USD/5Y/20Y/ATM 0.350917")
        ("20160226 SWAPTION/RATE_LNVOL/USD/30Y/25Y/ATM 0.271733")
        ("20160226 SWAPTION/RATE_LNVOL/USD/20Y/25Y/ATM 0.259564")
        ("20160226 SWAPTION/RATE_LNVOL/USD/4Y/25Y/ATM 0.355165")
        ("20160226 SWAPTION/RATE_LNVOL/USD/5Y/25Y/ATM 0.343885")
        ("20160226 SWAPTION/RATE_LNVOL/USD/15Y/25Y/ATM 0.271644")
        ("20160226 SWAPTION/RATE_LNVOL/USD/6M/25Y/ATM 0.431135")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1M/25Y/ATM 0.427615")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3M/25Y/ATM 0.433391")
        ("20160226 SWAPTION/RATE_LNVOL/USD/25Y/25Y/ATM 0.262762")
        ("20160226 SWAPTION/RATE_LNVOL/USD/10Y/25Y/ATM 0.304406")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3Y/25Y/ATM 0.372194")
        ("20160226 SWAPTION/RATE_LNVOL/USD/2Y/25Y/ATM 0.395398")
        ("20160226 SWAPTION/RATE_LNVOL/USD/7Y/25Y/ATM 0.326927")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1Y/25Y/ATM 0.41513")
        ("20160226 SWAPTION/RATE_LNVOL/USD/15Y/2Y/ATM 0.312514")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3Y/2Y/ATM 0.643934")
        ("20160226 SWAPTION/RATE_LNVOL/USD/6M/2Y/ATM 0.751427")
        ("20160226 SWAPTION/RATE_LNVOL/USD/30Y/2Y/ATM 0.282604")
        ("20160226 SWAPTION/RATE_LNVOL/USD/2Y/2Y/ATM 0.725701")
        ("20160226 SWAPTION/RATE_LNVOL/USD/7Y/2Y/ATM 0.45533")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1Y/2Y/ATM 0.750588")
        ("20160226 SWAPTION/RATE_LNVOL/USD/5Y/2Y/ATM 0.528093")
        ("20160226 SWAPTION/RATE_LNVOL/USD/4Y/2Y/ATM 0.578914")
        ("20160226 SWAPTION/RATE_LNVOL/USD/25Y/2Y/ATM 0.276083")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3M/2Y/ATM 0.752889")
        ("20160226 SWAPTION/RATE_LNVOL/USD/10Y/2Y/ATM 0.380044")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1M/2Y/ATM 0.722185")
        ("20160226 SWAPTION/RATE_LNVOL/USD/20Y/2Y/ATM 0.296735")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1M/30Y/ATM 0.418857")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3Y/30Y/ATM 0.367358")
        ("20160226 SWAPTION/RATE_LNVOL/USD/30Y/30Y/ATM 0.2718")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1Y/30Y/ATM 0.407524")
        ("20160226 SWAPTION/RATE_LNVOL/USD/25Y/30Y/ATM 0.263453")
        ("20160226 SWAPTION/RATE_LNVOL/USD/2Y/30Y/ATM 0.387553")
        ("20160226 SWAPTION/RATE_LNVOL/USD/4Y/30Y/ATM 0.35077")
        ("20160226 SWAPTION/RATE_LNVOL/USD/20Y/30Y/ATM 0.260409")
        ("20160226 SWAPTION/RATE_LNVOL/USD/5Y/30Y/ATM 0.339702")
        ("20160226 SWAPTION/RATE_LNVOL/USD/6M/30Y/ATM 0.422241")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3M/30Y/ATM 0.42532")
        ("20160226 SWAPTION/RATE_LNVOL/USD/15Y/30Y/ATM 0.272841")
        ("20160226 SWAPTION/RATE_LNVOL/USD/7Y/30Y/ATM 0.322472")
        ("20160226 SWAPTION/RATE_LNVOL/USD/10Y/30Y/ATM 0.300322")
        ("20160226 SWAPTION/RATE_LNVOL/USD/10Y/3Y/ATM 0.376155")
        ("20160226 SWAPTION/RATE_LNVOL/USD/5Y/3Y/ATM 0.504808")
        ("20160226 SWAPTION/RATE_LNVOL/USD/7Y/3Y/ATM 0.443113")
        ("20160226 SWAPTION/RATE_LNVOL/USD/4Y/3Y/ATM 0.545964")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3M/3Y/ATM 0.770113")
        ("20160226 SWAPTION/RATE_LNVOL/USD/30Y/3Y/ATM 0.283092")
        ("20160226 SWAPTION/RATE_LNVOL/USD/25Y/3Y/ATM 0.275506")
        ("20160226 SWAPTION/RATE_LNVOL/USD/20Y/3Y/ATM 0.293776")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3Y/3Y/ATM 0.598626")
        ("20160226 SWAPTION/RATE_LNVOL/USD/2Y/3Y/ATM 0.659808")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1M/3Y/ATM 0.760853")
        ("20160226 SWAPTION/RATE_LNVOL/USD/15Y/3Y/ATM 0.312797")
        ("20160226 SWAPTION/RATE_LNVOL/USD/6M/3Y/ATM 0.764493")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1Y/3Y/ATM 0.741367")
        ("20160226 SWAPTION/RATE_LNVOL/USD/5Y/4Y/ATM 0.483701")
        ("20160226 SWAPTION/RATE_LNVOL/USD/2Y/4Y/ATM 0.626629")
        ("20160226 SWAPTION/RATE_LNVOL/USD/7Y/4Y/ATM 0.430606")
        ("20160226 SWAPTION/RATE_LNVOL/USD/10Y/4Y/ATM 0.37399")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3M/4Y/ATM 0.755829")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1Y/4Y/ATM 0.690926")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1M/4Y/ATM 0.766932")
        ("20160226 SWAPTION/RATE_LNVOL/USD/4Y/4Y/ATM 0.522394")
        ("20160226 SWAPTION/RATE_LNVOL/USD/6M/4Y/ATM 0.740591")
        ("20160226 SWAPTION/RATE_LNVOL/USD/15Y/4Y/ATM 0.313507")
        ("20160226 SWAPTION/RATE_LNVOL/USD/30Y/4Y/ATM 0.282171")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3Y/4Y/ATM 0.562962")
        ("20160226 SWAPTION/RATE_LNVOL/USD/25Y/4Y/ATM 0.273649")
        ("20160226 SWAPTION/RATE_LNVOL/USD/20Y/4Y/ATM 0.291507")
        ("20160226 SWAPTION/RATE_LNVOL/USD/7Y/5Y/ATM 0.419719")
        ("20160226 SWAPTION/RATE_LNVOL/USD/25Y/5Y/ATM 0.272182")
        ("20160226 SWAPTION/RATE_LNVOL/USD/15Y/5Y/ATM 0.314468")
        ("20160226 SWAPTION/RATE_LNVOL/USD/10Y/5Y/ATM 0.368886")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1Y/5Y/ATM 0.665892")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1M/5Y/ATM 0.738673")
        ("20160226 SWAPTION/RATE_LNVOL/USD/30Y/5Y/ATM 0.282147")
        ("20160226 SWAPTION/RATE_LNVOL/USD/5Y/5Y/ATM 0.466931")
        ("20160226 SWAPTION/RATE_LNVOL/USD/20Y/5Y/ATM 0.289319")
        ("20160226 SWAPTION/RATE_LNVOL/USD/4Y/5Y/ATM 0.49694")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3M/5Y/ATM 0.719629")
        ("20160226 SWAPTION/RATE_LNVOL/USD/6M/5Y/ATM 0.709472")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3Y/5Y/ATM 0.535164")
        ("20160226 SWAPTION/RATE_LNVOL/USD/2Y/5Y/ATM 0.594412")
        ("20160226 SWAPTION/RATE_LNVOL/USD/4Y/6Y/ATM 0.478518")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3Y/6Y/ATM 0.512654")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1Y/6Y/ATM 0.627983")
        ("20160226 SWAPTION/RATE_LNVOL/USD/20Y/6Y/ATM 0.286969")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1M/6Y/ATM 0.675274")
        ("20160226 SWAPTION/RATE_LNVOL/USD/2Y/6Y/ATM 0.561415")
        ("20160226 SWAPTION/RATE_LNVOL/USD/10Y/6Y/ATM 0.36362")
        ("20160226 SWAPTION/RATE_LNVOL/USD/5Y/6Y/ATM 0.452013")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3M/6Y/ATM 0.67379")
        ("20160226 SWAPTION/RATE_LNVOL/USD/7Y/6Y/ATM 0.409059")
        ("20160226 SWAPTION/RATE_LNVOL/USD/25Y/6Y/ATM 0.2763")
        ("20160226 SWAPTION/RATE_LNVOL/USD/6M/6Y/ATM 0.657404")
        ("20160226 SWAPTION/RATE_LNVOL/USD/30Y/6Y/ATM 0.283231")
        ("20160226 SWAPTION/RATE_LNVOL/USD/15Y/6Y/ATM 0.311968")
        ("20160226 SWAPTION/RATE_LNVOL/USD/5Y/7Y/ATM 0.437809")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3M/7Y/ATM 0.628908")
        ("20160226 SWAPTION/RATE_LNVOL/USD/25Y/7Y/ATM 0.278409")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1M/7Y/ATM 0.629572")
        ("20160226 SWAPTION/RATE_LNVOL/USD/15Y/7Y/ATM 0.311755")
        ("20160226 SWAPTION/RATE_LNVOL/USD/7Y/7Y/ATM 0.40233")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1Y/7Y/ATM 0.588402")
        ("20160226 SWAPTION/RATE_LNVOL/USD/2Y/7Y/ATM 0.53681")
        ("20160226 SWAPTION/RATE_LNVOL/USD/10Y/7Y/ATM 0.357638")
        ("20160226 SWAPTION/RATE_LNVOL/USD/4Y/7Y/ATM 0.460946")
        ("20160226 SWAPTION/RATE_LNVOL/USD/20Y/7Y/ATM 0.285254")
        ("20160226 SWAPTION/RATE_LNVOL/USD/30Y/7Y/ATM 0.280821")
        ("20160226 SWAPTION/RATE_LNVOL/USD/6M/7Y/ATM 0.619567")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3Y/7Y/ATM 0.496016")
        ("20160226 SWAPTION/RATE_LNVOL/USD/25Y/8Y/ATM 0.278576")
        ("20160226 SWAPTION/RATE_LNVOL/USD/20Y/8Y/ATM 0.283873")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1Y/8Y/ATM 0.560732")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1M/8Y/ATM 0.591543")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3Y/8Y/ATM 0.479076")
        ("20160226 SWAPTION/RATE_LNVOL/USD/7Y/8Y/ATM 0.392865")
        ("20160226 SWAPTION/RATE_LNVOL/USD/30Y/8Y/ATM 0.280426")
        ("20160226 SWAPTION/RATE_LNVOL/USD/5Y/8Y/ATM 0.426418")
        ("20160226 SWAPTION/RATE_LNVOL/USD/4Y/8Y/ATM 0.448315")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3M/8Y/ATM 0.594923")
        ("20160226 SWAPTION/RATE_LNVOL/USD/15Y/8Y/ATM 0.311033")
        ("20160226 SWAPTION/RATE_LNVOL/USD/6M/8Y/ATM 0.591289")
        ("20160226 SWAPTION/RATE_LNVOL/USD/10Y/8Y/ATM 0.352116")
        ("20160226 SWAPTION/RATE_LNVOL/USD/2Y/8Y/ATM 0.518648")
        ("20160226 SWAPTION/RATE_LNVOL/USD/4Y/9Y/ATM 0.438637")
        ("20160226 SWAPTION/RATE_LNVOL/USD/6M/9Y/ATM 0.566963")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1M/9Y/ATM 0.56222")
        ("20160226 SWAPTION/RATE_LNVOL/USD/10Y/9Y/ATM 0.347195")
        ("20160226 SWAPTION/RATE_LNVOL/USD/1Y/9Y/ATM 0.539202")
        ("20160226 SWAPTION/RATE_LNVOL/USD/20Y/9Y/ATM 0.281429")
        ("20160226 SWAPTION/RATE_LNVOL/USD/2Y/9Y/ATM 0.50153")
        ("20160226 SWAPTION/RATE_LNVOL/USD/5Y/9Y/ATM 0.419976")
        ("20160226 SWAPTION/RATE_LNVOL/USD/15Y/9Y/ATM 0.308262")
        ("20160226 SWAPTION/RATE_LNVOL/USD/30Y/9Y/ATM 0.280027")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3Y/9Y/ATM 0.462502")
        ("20160226 SWAPTION/RATE_LNVOL/USD/7Y/9Y/ATM 0.384089")
        ("20160226 SWAPTION/RATE_LNVOL/USD/3M/9Y/ATM 0.569119")
        ("20160226 SWAPTION/RATE_LNVOL/USD/25Y/9Y/ATM 0.278568")
        // USD lognormal capfloor quotes
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/1Y/3M/0/0/0.015 0.44451")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/1Y/3M/0/0/0.010 0.447381")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/1Y/3M/0/0/0.025 0.452925")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/1Y/3M/0/0/0.020 0.450945")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/1Y/3M/0/0/0.030 0.447381")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/1Y/3M/0/0/0.005 0.570834")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/2Y/3M/0/0/0.015 0.484806")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/2Y/3M/0/0/0.010 0.51695")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/2Y/3M/0/0/0.025 0.459228")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/2Y/3M/0/0/0.020 0.468832")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/2Y/3M/0/0/0.030 0.440804")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/2Y/3M/0/0/0.005 0.661108")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/5Y/3M/0/0/0.015 0.5928")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/5Y/3M/0/0/0.010 0.670605")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/5Y/3M/0/0/0.025 0.50559")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/5Y/3M/0/0/0.020 0.54302")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/5Y/3M/0/0/0.030 0.472055")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/5Y/3M/0/0/0.005 0.87571")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/7Y/3M/0/0/0.015 0.584226")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/7Y/3M/0/0/0.010 0.686805")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/7Y/3M/0/0/0.025 0.470394")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/7Y/3M/0/0/0.020 0.518661")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/7Y/3M/0/0/0.030 0.431055")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/7Y/3M/0/0/0.005 0.931953")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/10Y/3M/0/0/0.015 0.54423")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/10Y/3M/0/0/0.010 0.65691")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/10Y/3M/0/0/0.025 0.423")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/10Y/3M/0/0/0.020 0.47358")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/10Y/3M/0/0/0.030 0.38394")
        ("20160226 CAPFLOOR/RATE_LNVOL/USD/10Y/3M/0/0/0.005 0.91791")
        // equity
        ("20160226 EQUITY/PRICE/SP5/USD 1500.00")
        ("20160226 EQUITY_FWD/PRICE/SP5/USD/1Y 1500.00")
        ("20160226 EQUITY_FWD/PRICE/SP5/USD/20180226 1500.00")
        ("20160226 EQUITY_DIVIDEND/RATE/SP5/USD/20170226 0.00")
        ("20160226 EQUITY_DIVIDEND/RATE/SP5/USD/2Y 0.00")
        ("20160226 EQUITY_OPTION/RATE_LNVOL/SP5/USD/1Y/ATMF 0.25")
        ("20160226 EQUITY_OPTION/RATE_LNVOL/SP5/USD/2018-02-26/ATMF 0.35")
        // commodity quotes
        ("20160226 COMMODITY/PRICE/GOLD/USD 1155.593")
        ("20160226 COMMODITY_FWD/PRICE/GOLD/USD/2016-08-31 1158.8")
        ("20160226 COMMODITY_FWD/PRICE/GOLD/USD/2017-02-28 1160.9")
        ("20160226 COMMODITY_FWD/PRICE/GOLD/USD/2017-08-31 1163.4")
        ("20160226 COMMODITY_FWD/PRICE/GOLD/USD/2017-12-29 1165.3")
        ("20160226 COMMODITY_FWD/PRICE/GOLD/USD/2018-12-31 1172.9")
        ("20160226 COMMODITY_FWD/PRICE/GOLD/USD/2021-12-31 1223")
        // correlation quotes
        ("20160226 CORRELATION/RATE/EUR-CMS-10Y/EUR-CMS-2Y/1Y/ATM 0.1")
        ("20160226 CORRELATION/RATE/EUR-CMS-10Y/EUR-CMS-2Y/2Y/ATM 0.2")
        ("20160226 CORRELATION/PRICE/USD-CMS-10Y/USD-CMS-2Y/1Y/ATM 0.0038614")
        ("20160226 CORRELATION/PRICE/USD-CMS-10Y/USD-CMS-2Y/2Y/ATM 0.0105279");
    // clang-format on

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

QuantLib::ext::shared_ptr<TodaysMarketParameters> marketParameters() {

    QuantLib::ext::shared_ptr<TodaysMarketParameters> parameters = QuantLib::ext::make_shared<TodaysMarketParameters>();

    // define three curves
    map<string, string> mDiscounting = {{"EUR", "Yield/EUR/EUR1D"}, {"USD", "Yield/USD/USD1D"}};
    parameters->addMarketObject(MarketObject::DiscountCurve, "ois", mDiscounting);

    // define three curves
    map<string, string> mYield = {{"EUR_LEND", "Yield/EUR/BANK_EUR_LEND"}, {"EUR_BORROW", "Yield/EUR/BANK_EUR_BORROW"}};
    parameters->addMarketObject(MarketObject::YieldCurve, "ois", mYield);

    map<string, string> mIndex = {
        {"EUR-EONIA", "Yield/EUR/EUR1D"}, {"USD-FedFunds", "Yield/USD/USD1D"}, {"USD-LIBOR-3M", "Yield/USD/USD3M"}};
    parameters->addMarketObject(MarketObject::IndexCurve, "ois", mIndex);

    parameters->addMarketObject(MarketObject::SwaptionVol, "ois", {{"USD", "SwaptionVolatility/USD/USD_SW_LN"}});
    parameters->addMarketObject(MarketObject::CapFloorVol, "ois", {{"USD", "CapFloorVolatility/USD/USD_CF_LN"}});

    map<string, string> swapIndexMap = {{"USD-CMS-1Y", "USD-FedFunds"},
                                        {"USD-CMS-30Y", "USD-LIBOR-3M"},
                                        {"USD-CMS-2Y", "USD-LIBOR-3M"},
                                        {"USD-CMS-10Y", "USD-LIBOR-3M"}};
    parameters->addMarketObject(MarketObject::SwapIndexCurve, "ois", swapIndexMap);

    map<string, string> equityMap = {{"SP5", "Equity/USD/SP5"}};
    parameters->addMarketObject(MarketObject::EquityCurve, "ois", equityMap);

    map<string, string> equityVolMap = {{"SP5", "EquityVolatility/USD/SP5"}};
    parameters->addMarketObject(MarketObject::EquityVol, "ois", equityVolMap);

    parameters->addMarketObject(MarketObject::CommodityCurve, "ois", {{"COMDTY_GOLD_USD", "Commodity/USD/GOLD_USD"}});

    map<string, string> correlationMap = {{"EUR-CMS-10Y/EUR-CMS-2Y", "Correlation/EUR-CORR"},
                                          {"USD-CMS-10Y/USD-CMS-2Y", "Correlation/USD-CORR"}};
    parameters->addMarketObject(MarketObject::Correlation, "ois", correlationMap);

    // all others empty so far
    map<string, string> emptyMap;
    parameters->addMarketObject(MarketObject::FXSpot, "ois", emptyMap);
    parameters->addMarketObject(MarketObject::FXVol, "ois", emptyMap);
    parameters->addMarketObject(MarketObject::DefaultCurve, "ois", emptyMap);

    // store this set of curves as "default" configuration
    MarketConfiguration config;
    config.setId(MarketObject::DiscountCurve, "ois");
    config.setId(MarketObject::YieldCurve, "ois");
    config.setId(MarketObject::IndexCurve, "ois");
    config.setId(MarketObject::SwapIndexCurve, "ois");
    config.setId(MarketObject::DefaultCurve, "ois");
    config.setId(MarketObject::SwaptionVol, "ois");
    config.setId(MarketObject::CapFloorVol, "ois");
    config.setId(MarketObject::FXSpot, "ois");
    config.setId(MarketObject::FXVol, "ois");
    config.setId(MarketObject::EquityCurve, "ois");
    config.setId(MarketObject::EquityVol, "ois");
    config.setId(MarketObject::CommodityCurve, "ois");
    config.setId(MarketObject::Correlation, "ois");

    parameters->addConfiguration("default", config);

    return parameters;
}

QuantLib::ext::shared_ptr<Conventions> conventions() {
    QuantLib::ext::shared_ptr<Conventions> conventions(new Conventions());

    QuantLib::ext::shared_ptr<Convention> zeroConv(new ZeroRateConvention("EUR-ZERO-CONVENTIONS-TENOR-BASED", "A365", "TARGET",
                                                                  "Continuous", "Daily", "2", "TARGET", "Following",
                                                                  "false"));
    conventions->add(zeroConv);

    QuantLib::ext::shared_ptr<Convention> depositConv(new DepositConvention("EUR-EONIA-CONVENTIONS", "EUR-EONIA"));
    conventions->add(depositConv);

    QuantLib::ext::shared_ptr<Convention> oisConv(new OisConvention("EUR-OIS-CONVENTIONS", "2", "EUR-EONIA", "A360", "TARGET",
                                                            "1", "false", "Annual", "Following", "Following",
                                                            "Backward"));
    conventions->add(oisConv);

    // USD Fed Funds curve conventions
    conventions->add(QuantLib::ext::make_shared<DepositConvention>("USD-FED-FUNDS-CONVENTIONS", "USD-FedFunds"));
    conventions->add(QuantLib::ext::make_shared<OisConvention>("USD-OIS-CONVENTIONS", "2", "USD-FedFunds", "A360", "US", "2",
                                                       "false", "Annual", "Following", "Following", "Backward"));

    // USD 3M curve conventions
    conventions->add(QuantLib::ext::make_shared<DepositConvention>("USD-LIBOR-CONVENTIONS", "USD-LIBOR"));
    conventions->add(QuantLib::ext::make_shared<FraConvention>("USD-3M-FRA-CONVENTIONS", "USD-LIBOR-3M"));
    conventions->add(QuantLib::ext::make_shared<IRSwapConvention>("USD-3M-SWAP-CONVENTIONS", "US", "Semiannual", "MF", "30/360",
                                                          "USD-LIBOR-3M"));

    // USD swap index conventions
    conventions->add(QuantLib::ext::make_shared<SwapIndexConvention>("USD-CMS-1Y", "USD-3M-SWAP-CONVENTIONS", "US"));
    conventions->add(QuantLib::ext::make_shared<SwapIndexConvention>("USD-CMS-30Y", "USD-3M-SWAP-CONVENTIONS", "US"));
    conventions->add(QuantLib::ext::make_shared<SwapIndexConvention>("USD-CMS-2Y", "USD-3M-SWAP-CONVENTIONS", "US"));
    conventions->add(QuantLib::ext::make_shared<SwapIndexConvention>("USD-CMS-10Y", "USD-3M-SWAP-CONVENTIONS", "US"));

    // USD CMS spread option conventions

    conventions->add(QuantLib::ext::make_shared<CmsSpreadOptionConvention>("USD-CMS-10Y-2Y-CONVENTION", "0M", "2D", "3M", "2",
                                                                   "TARGET", "A360", "MF"));

    return conventions;
}

QuantLib::ext::shared_ptr<CurveConfigurations> curveConfigurations() {
    QuantLib::ext::shared_ptr<CurveConfigurations> configs(new CurveConfigurations());

    // Vectors to hold quote strings and segments
    vector<string> quotes;
    vector<QuantLib::ext::shared_ptr<YieldCurveSegment>> segments;

    // Eonia curve
    quotes = {"MM/RATE/EUR/0D/1D"};
    segments.push_back(QuantLib::ext::make_shared<SimpleYieldCurveSegment>("Deposit", "EUR-EONIA-CONVENTIONS", quotes));

    // clang-format off
    quotes = {"IR_SWAP/RATE/EUR/2D/1D/1W",
              "IR_SWAP/RATE/EUR/2D/1D/2W",
              "IR_SWAP/RATE/EUR/2D/1D/1M",
              "IR_SWAP/RATE/EUR/2D/1D/2M",
              "IR_SWAP/RATE/EUR/2D/1D/3M",
              "IR_SWAP/RATE/EUR/2D/1D/4M",
              "IR_SWAP/RATE/EUR/2D/1D/5M",
              "IR_SWAP/RATE/EUR/2D/1D/6M",
              "IR_SWAP/RATE/EUR/2D/1D/7M",
              "IR_SWAP/RATE/EUR/2D/1D/8M",
              "IR_SWAP/RATE/EUR/2D/1D/9M",
              "IR_SWAP/RATE/EUR/2D/1D/10M",
              "IR_SWAP/RATE/EUR/2D/1D/11M",
              "IR_SWAP/RATE/EUR/2D/1D/1Y",
              "IR_SWAP/RATE/EUR/2D/1D/2Y",
              "IR_SWAP/RATE/EUR/2D/1D/3Y",
              "IR_SWAP/RATE/EUR/2D/1D/4Y",
              "IR_SWAP/RATE/EUR/2D/1D/5Y",
              "IR_SWAP/RATE/EUR/2D/1D/6Y",
              "IR_SWAP/RATE/EUR/2D/1D/7Y",
              "IR_SWAP/RATE/EUR/2D/1D/8Y",
              "IR_SWAP/RATE/EUR/2D/1D/9Y",
              "IR_SWAP/RATE/EUR/2D/1D/10Y"};
    // clang-format on
    segments.push_back(QuantLib::ext::make_shared<SimpleYieldCurveSegment>("OIS", "EUR-OIS-CONVENTIONS", quotes));

    configs->add(CurveSpec::CurveType::Yield, "EUR1D", 
        QuantLib::ext::make_shared<YieldCurveConfig>("EUR1D", "Eonia Curve", "EUR", "", segments));

    // Lending curve
    segments.clear();
    // clang-format off
    quotes = {"ZERO/YIELD_SPREAD/EUR/BANK_EUR_LEND/A365/2Y",
              "ZERO/YIELD_SPREAD/EUR/BANK_EUR_LEND/A365/5Y",
              "ZERO/YIELD_SPREAD/EUR/BANK_EUR_LEND/A365/10Y",
              "ZERO/YIELD_SPREAD/EUR/BANK_EUR_LEND/A365/20Y"};
    // clang-format on
    segments.push_back(QuantLib::ext::make_shared<ZeroSpreadedYieldCurveSegment>(
        "Zero Spread", "EUR-ZERO-CONVENTIONS-TENOR-BASED", quotes, "EUR1D"));

    configs->add(CurveSpec::CurveType::Yield, "BANK_EUR_LEND", 
        QuantLib::ext::make_shared<YieldCurveConfig>("BANK_EUR_LEND", "Eonia Curve", "EUR", "", segments));

    // Borrowing curve
    segments.clear();
    // clang-format off
    quotes = {"ZERO/YIELD_SPREAD/EUR/BANK_EUR_BORROW/A365/2Y",
              "ZERO/YIELD_SPREAD/EUR/BANK_EUR_BORROW/A365/5Y",
              "ZERO/YIELD_SPREAD/EUR/BANK_EUR_BORROW/A365/10Y",
              "ZERO/YIELD_SPREAD/EUR/BANK_EUR_BORROW/A365/20Y"};
    // clang-format on
    segments.push_back(QuantLib::ext::make_shared<ZeroSpreadedYieldCurveSegment>(
        "Zero Spread", "EUR-ZERO-CONVENTIONS-TENOR-BASED", quotes, "EUR1D"));

    configs->add(CurveSpec::CurveType::Yield, "BANK_EUR_BORROW",
        QuantLib::ext::make_shared<YieldCurveConfig>("BANK_EUR_LEND", "Eonia Curve", "EUR", "", segments));

    // USD Fed Funds curve
    segments.clear();
    quotes = {{"MM/RATE/USD/0D/1D"}};
    segments.push_back(QuantLib::ext::make_shared<SimpleYieldCurveSegment>("Deposit", "USD-FED-FUNDS-CONVENTIONS", quotes));

    // clang-format off
    quotes = {"IR_SWAP/RATE/USD/2D/1D/1M",
              "IR_SWAP/RATE/USD/2D/1D/3M",
              "IR_SWAP/RATE/USD/2D/1D/6M",
              "IR_SWAP/RATE/USD/2D/1D/9M",
              "IR_SWAP/RATE/USD/2D/1D/1Y",
              "IR_SWAP/RATE/USD/2D/1D/2Y",
              "IR_SWAP/RATE/USD/2D/1D/3Y",
              "IR_SWAP/RATE/USD/2D/1D/4Y",
              "IR_SWAP/RATE/USD/2D/1D/5Y",
              "IR_SWAP/RATE/USD/2D/1D/7Y",
              "IR_SWAP/RATE/USD/2D/1D/10Y",
              "IR_SWAP/RATE/USD/2D/1D/12Y",
              "IR_SWAP/RATE/USD/2D/1D/15Y",
              "IR_SWAP/RATE/USD/2D/1D/20Y",
              "IR_SWAP/RATE/USD/2D/1D/25Y",
              "IR_SWAP/RATE/USD/2D/1D/30Y",
              "IR_SWAP/RATE/USD/2D/1D/50Y"};
    // clang-format on
    segments.push_back(QuantLib::ext::make_shared<SimpleYieldCurveSegment>("OIS", "USD-OIS-CONVENTIONS", quotes));

    configs->add(CurveSpec::CurveType::Yield, "USD1D",
        QuantLib::ext::make_shared<YieldCurveConfig>("USD1D", "Fed Funds curve", "USD", "", segments));

    // USD 3M forward curve
    segments.clear();
    quotes = {"MM/RATE/USD/2D/3M"};
    segments.push_back(
        QuantLib::ext::make_shared<SimpleYieldCurveSegment>("Deposit", "USD-LIBOR-CONVENTIONS", quotes, "USD3M"));

    // clang-format off
    quotes = {"FRA/RATE/USD/3M/3M",
              "FRA/RATE/USD/6M/3M",
              "FRA/RATE/USD/9M/3M",
              "FRA/RATE/USD/1Y/3M"};
    // clang-format on
    segments.push_back(QuantLib::ext::make_shared<SimpleYieldCurveSegment>("FRA", "USD-3M-FRA-CONVENTIONS", quotes, "USD3M"));

    // clang-format off
    quotes = {"IR_SWAP/RATE/USD/2D/3M/2Y",
              "IR_SWAP/RATE/USD/2D/3M/3Y",
              "IR_SWAP/RATE/USD/2D/3M/4Y",
              "IR_SWAP/RATE/USD/2D/3M/5Y",
              "IR_SWAP/RATE/USD/2D/3M/6Y",
              "IR_SWAP/RATE/USD/2D/3M/7Y",
              "IR_SWAP/RATE/USD/2D/3M/8Y",
              "IR_SWAP/RATE/USD/2D/3M/9Y",
              "IR_SWAP/RATE/USD/2D/3M/10Y",
              "IR_SWAP/RATE/USD/2D/3M/12Y",
              "IR_SWAP/RATE/USD/2D/3M/15Y",
              "IR_SWAP/RATE/USD/2D/3M/20Y",
              "IR_SWAP/RATE/USD/2D/3M/25Y",
              "IR_SWAP/RATE/USD/2D/3M/30Y",
              "IR_SWAP/RATE/USD/2D/3M/40Y",
              "IR_SWAP/RATE/USD/2D/3M/50Y"};
    // clang-format on
    segments.push_back(QuantLib::ext::make_shared<SimpleYieldCurveSegment>("Swap", "USD-3M-SWAP-CONVENTIONS", quotes, "USD3M"));

    configs->add(CurveSpec::CurveType::Yield, "USD3M",
        QuantLib::ext::make_shared<YieldCurveConfig>("USD3M", "USD 3M curve", "USD", "USD1D", segments));

    // Swaption volatilities

    // Common variables for all volatility structures
    Actual365Fixed dayCounter;
    BusinessDayConvention bdc = Following;

    // Swaption volatility structure option and swap tenors
    // clang-format off
    vector<string> optionTenors{
        "1M",
        "3M",
        "6M",
        "1Y",
        "2Y",
        "3Y",
        "4Y",
        "5Y",
        "7Y",
        "10Y",
        "15Y",
        "20Y",
        "25Y",
        "30Y"
    };

    vector<string> swapTenors{
        "1Y",
        "2Y",
        "3Y",
        "4Y",
        "5Y",
        "7Y",
        "10Y",
        "15Y",
        "20Y",
        "25Y",
        "30Y"
    };
    // clang-format on

    // USD Lognormal swaption volatility "curve" configuration
    configs->add(CurveSpec::CurveType::SwaptionVolatility, "USD_SW_LN",
                 QuantLib::ext::make_shared<SwaptionVolatilityCurveConfig>(
                     "USD_SW_LN", "USD Lognormal swaption volatilities", SwaptionVolatilityCurveConfig::Dimension::ATM,
                     SwaptionVolatilityCurveConfig::VolatilityType::Lognormal,
                     SwaptionVolatilityCurveConfig::VolatilityType::Lognormal,
                     GenericYieldVolatilityCurveConfig::Interpolation::Linear,
                     GenericYieldVolatilityCurveConfig::Extrapolation::Flat, optionTenors, swapTenors, dayCounter,
                     UnitedStates(UnitedStates::Settlement), bdc, "USD-CMS-1Y", "USD-CMS-30Y"));

    // Capfloor volatility structure tenors and strikes
    vector<string> capTenors{"1Y", "2Y", "5Y", "7Y", "10Y"};
    vector<string> strikes{"0.005", "0.010", "0.015", "0.020", "0.025", "0.030"};

    // USD Lognormal capfloor volatility "curve" configuration
    configs->add(CurveSpec::CurveType::CapFloorVolatility, "USD_CF_LN", QuantLib::ext::make_shared<CapFloorVolatilityCurveConfig>(
        "USD_CF_LN", "USD Lognormal capfloor volatilities", CapFloorVolatilityCurveConfig::VolatilityType::Lognormal,
        true, false, false, capTenors, strikes, dayCounter, 0, UnitedStates(UnitedStates::Settlement), bdc,
        "USD-LIBOR-3M", 3 * Months, 0, "Yield/USD/USD1D"));

    vector<string> optionTenors2{"1Y"};

    vector<string> optionTenors3{"1Y", "2Y"};
    // Correlation curve
    configs->add(CurveSpec::CurveType::Correlation, "EUR-CORR", QuantLib::ext::make_shared<CorrelationCurveConfig>(
        "EUR-CORR", "EUR CMS Correlations", CorrelationCurveConfig::Dimension::Constant,
        CorrelationCurveConfig::CorrelationType::CMSSpread, "EUR-CMS-1Y-10Y-CONVENTION",
        MarketDatum::QuoteType::RATE, true, optionTenors2, dayCounter, UnitedStates(UnitedStates::Settlement), bdc,
        "EUR-CMS-10Y", "EUR-CMS-2Y", "EUR"));
    configs->add(CurveSpec::CurveType::Correlation, "USD-CORR", QuantLib::ext::make_shared<CorrelationCurveConfig>(
        "USD-CORR", "USD CMS Correlations", CorrelationCurveConfig::Dimension::ATM,
        CorrelationCurveConfig::CorrelationType::CMSSpread, "USD-CMS-10Y-2Y-CONVENTION",
        MarketDatum::QuoteType::PRICE, true, optionTenors3, Actual360(), TARGET(), ModifiedFollowing,
        "USD-CMS-10Y", "USD-CMS-2Y", "USD", "USD_SW_LN", "USD1D"));

    // clang-format off
    vector<string> eqFwdQuotes{
        "EQUITY_FWD/PRICE/SP5/USD/1Y",
        "EQUITY_FWD/PRICE/SP5/USD/20180226"
    };
    vector<string> equityVolExpiries{
        "1Y",
        "2018-02-26"
    };
    // clang-format on

    configs->add(CurveSpec::CurveType::Equity, "SP5", QuantLib::ext::make_shared<EquityCurveConfig>(
        "SP5", "", "USD1D", "USD", "USD", EquityCurveConfig::Type::ForwardPrice, "EQUITY/PRICE/SP5/USD", eqFwdQuotes));

    vector<string> eqVolQuotes = {"EQUITY_OPTION/RATE_LNVOL/SP5/USD/1Y/ATMF",
                                  "EQUITY_OPTION/RATE_LNVOL/SP5/USD/2018-02-26/ATMF"};
    vector<QuantLib::ext::shared_ptr<VolatilityConfig>> vcc;
    vcc.push_back(QuantLib::ext::make_shared<VolatilityCurveConfig>(eqVolQuotes, "Flat", "Flat"));

    configs->add(CurveSpec::CurveType::EquityVolatility, "SP5", QuantLib::ext::make_shared<EquityVolatilityCurveConfig>(
        "SP5", "", "USD", vcc, "SP5", "A365", "USD"));

    // clang-format off
    vector<string> commodityQuotes{
        "COMMODITY_FWD/PRICE/GOLD/USD/2016-08-31",
        "COMMODITY_FWD/PRICE/GOLD/USD/2017-02-28",
        "COMMODITY_FWD/PRICE/GOLD/USD/2017-08-31",
        "COMMODITY_FWD/PRICE/GOLD/USD/2017-12-29",
        "COMMODITY_FWD/PRICE/GOLD/USD/2018-12-31",
        "COMMODITY_FWD/PRICE/GOLD/USD/2021-12-31"
    };
    // clang-format on

    configs->add(CurveSpec::CurveType::Commodity, "GOLD_USD",
        QuantLib::ext::make_shared<CommodityCurveConfig>("GOLD_USD", "", "USD", commodityQuotes, "COMMODITY/PRICE/GOLD/USD"));

    return configs;
}

// Fixture to use for this test suite
class F : public TopLevelFixture {
public:
    QuantLib::ext::shared_ptr<TodaysMarket> market;

    F() {
        Date asof(26, February, 2016);
        Settings::instance().evaluationDate() = asof;

        auto loader = QuantLib::ext::make_shared<MarketDataLoader>();
        auto params = marketParameters();
        auto configs = curveConfigurations();
        auto convs = conventions();

        ore::data::InstrumentConventions::instance().setConventions(convs);

        BOOST_TEST_MESSAGE("Creating TodaysMarket Instance");
        market = QuantLib::ext::make_shared<TodaysMarket>(asof, params, loader, configs);
    }

    ~F() {
        BOOST_TEST_MESSAGE("Destroying TodaysMarket instance");
        market.reset();
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(OREDataTestSuite, TopLevelFixture)

BOOST_FIXTURE_TEST_SUITE(TodaysMarketTests, F)

BOOST_AUTO_TEST_CASE(testZeroSpreadedYieldCurve) {

    BOOST_TEST_MESSAGE("Testing zero spreaded yield curve rates...");

    Handle<YieldTermStructure> dts = market->discountCurve("EUR");
    Handle<YieldTermStructure> dtsLend = market->yieldCurve("EUR_LEND");
    Handle<YieldTermStructure> dtsBorrow = market->yieldCurve("EUR_BORROW");

    QL_REQUIRE(!dts.empty(), "EUR discount curve not found");
    QL_REQUIRE(!dtsLend.empty(), "EUR lending curve not found");
    QL_REQUIRE(!dtsBorrow.empty(), "EUR borrowing curve not found");

    Date today = Settings::instance().evaluationDate();
    DayCounter dc = Actual365Fixed();
    Real tolerance = 1.0e-5; // 0.1 bp
    Real expected1 = 0.005;
    Real expected2 = -0.001;
    for (Size i = 1; i <= 120; i++) {
        Date d = today + i * Months;
        Real z0 = dts->zeroRate(d, dc, Continuous);
        Real z1 = dtsLend->zeroRate(d, dc, Continuous);
        Real z2 = dtsBorrow->zeroRate(d, dc, Continuous);
        BOOST_CHECK_MESSAGE(fabs(z1 - z0 - expected1) < tolerance, "error in lending spread curve setup");
        BOOST_CHECK_MESSAGE(fabs(z2 - z0 - expected2) < tolerance, "error in borrowing spread curve setup");
    }
}

BOOST_AUTO_TEST_CASE(testNormalOptionletVolatility) {

    BOOST_TEST_MESSAGE("Testing normal optionlet volatilities...");

    Handle<OptionletVolatilityStructure> ovs = market->capFloorVol("USD");

    BOOST_CHECK_MESSAGE(!ovs.empty(), "USD lognormal optionlet volatility structure was not created");
    BOOST_CHECK_MESSAGE(ovs->volatilityType() == Normal,
                        "USD lognormal capfloor volatility was not converted to Normal optionlet volatility");

    // Test against some expected values
    Real tolerance = 1.0e-6;
    vector<Period> tenors{1 * Years, 2 * Years, 5 * Years, 7 * Years, 10 * Years};
    vector<Rate> strikes{0.005, 0.010, 0.015, 0.020, 0.025, 0.030};
    // clang-format off
    vector<vector<Real>> cachedValues{
        { 0.004336061, 0.004790686, 0.005541127, 0.006411979, 0.007242633, 0.007889790 },
        { 0.005904299, 0.006478381, 0.006929551, 0.007486301, 0.008139029, 0.008566001 },
        { 0.008871166, 0.009370956, 0.009723190, 0.010015776, 0.010243227, 0.010492463 },
        { 0.008517407, 0.008700672, 0.008661611, 0.008631934, 0.008657444, 0.008690871 },
        { 0.007641226, 0.007821393, 0.007889650, 0.007980682, 0.008075806, 0.008235808 }
    };
    // clang-format on

    for (Size i = 0; i < tenors.size(); ++i) {
        for (Size j = 0; j < strikes.size(); ++j) {
            Volatility v = ovs->volatility(tenors[i], strikes[j]);
            Real error = fabs(v - cachedValues[i][j]);
            // clang-format off
            BOOST_CHECK_MESSAGE(error < tolerance,
                "\ncap tenor:         " << tenors[i] <<
                "\nstrike:            " << io::rate(strikes[j]) <<
                "\ncached volatility: " << io::volatility(cachedValues[i][j]) <<
                "\nvolatility:        " << io::volatility(v) <<
                "\nerror:             " << io::rate(error) <<
                "\ntolerance:         " << io::rate(tolerance));
            // clang-format on
        }
    }
}

BOOST_AUTO_TEST_CASE(testEquityCurve) {

    BOOST_TEST_MESSAGE("Testing equity curve...");

    Handle<YieldTermStructure> divTs = market->equityDividendCurve("SP5");
    BOOST_CHECK(divTs.currentLink());
    Handle<YieldTermStructure> equityIrTs = market->discountCurve("USD");
    BOOST_CHECK(equityIrTs.currentLink());
    Handle<Quote> equitySpot = market->equitySpot("SP5");
    BOOST_CHECK(equitySpot.currentLink());
    Real spotVal = equitySpot->value();
    DayCounter divDc = divTs->dayCounter();

    Date today = Settings::instance().evaluationDate();
    Date d_1y = Date(27, Feb, 2017);
    Date d_2y = Date(26, Feb, 2018);
    Rate r_1y = equityIrTs->zeroRate(d_1y, divDc, Continuous);
    Rate r_2y = equityIrTs->zeroRate(d_2y, divDc, Continuous);
    Rate q_1y = divTs->zeroRate(d_1y, divDc, Continuous);
    Rate q_2y = divTs->zeroRate(d_2y, divDc, Continuous);
    Real f_1y = spotVal * std::exp((r_1y - q_1y) * (divDc.yearFraction(today, d_1y)));
    Real f_2y = spotVal * std::exp((r_2y - q_2y) * (divDc.yearFraction(today, d_2y)));
    BOOST_CHECK_CLOSE(1500.00, f_1y, 1.e-10); // hardcoded, to be the same as the input quote
    BOOST_CHECK_CLOSE(1500.0, f_2y, 1.e-10);  // hardcoded, to be the same as the input quote

    // test flat extrapolation of the dividend yield term structure (N.B. NOT FLAT ON FORWARDS!)
    Rate q_5y = divTs->zeroRate(5.0, Continuous);
    Rate q_50y = divTs->zeroRate(50.0, Continuous);
    BOOST_CHECK_CLOSE(q_5y, q_50y, 1.e-10);

    // test that the t=0 forward value is equal to the spot
    Rate r_0 = equityIrTs->zeroRate(0.0, Continuous);
    Rate q_0 = divTs->zeroRate(0.0, Continuous);
    Real fwd_0 = spotVal * std::exp((r_0 - q_0) * 0.0);
    BOOST_CHECK_EQUAL(spotVal, fwd_0);
}

BOOST_AUTO_TEST_CASE(testEquityVolCurve) {

    BOOST_TEST_MESSAGE("Testing equity vol curve...");

    Handle<BlackVolTermStructure> eqVol = market->equityVol("SP5");
    BOOST_CHECK(eqVol.currentLink());

    Date d_1y = Date(27, Feb, 2017);
    Date d_2y = Date(26, Feb, 2018);
    Volatility v_1y_atm = eqVol->blackVol(d_1y, 0.0);
    Volatility v_1y_smile = eqVol->blackVol(d_1y, 2000.0);
    BOOST_CHECK_EQUAL(v_1y_atm, v_1y_smile); // test ATM flat smile
    BOOST_CHECK_EQUAL(v_1y_atm, 0.25);       // test input = output
    Volatility v_2y_atm = eqVol->blackVol(d_2y, 0.0);
    Volatility v_2y_smile = eqVol->blackVol(d_2y, 2000.0);
    BOOST_CHECK_EQUAL(v_2y_atm, v_2y_smile); // test ATM flat smile
    BOOST_CHECK_EQUAL(v_2y_atm, 0.35);       // test input = output

    // test flat extrapolation
    Volatility v_5y_atm = eqVol->blackVol(5.0, 0.0);
    Volatility v_50y_atm = eqVol->blackVol(50.0, 0.0);
    BOOST_CHECK_CLOSE(v_5y_atm, v_50y_atm, 1.e-10);
}

BOOST_AUTO_TEST_CASE(testCommodityCurve) {

    BOOST_TEST_MESSAGE("Testing commodity price curve");

    // Just test that the building succeeded - the curve itself has been tested elsewhere
    Handle<PriceTermStructure> commodityCurve = market->commodityPriceCurve("COMDTY_GOLD_USD");
    BOOST_CHECK(*commodityCurve);
}

BOOST_AUTO_TEST_CASE(testCorrelationCurve) {

    BOOST_TEST_MESSAGE("Testing correlation curve");

    // Just test that the building succeeded - the curve itself has been tested elsewhere
    Handle<QuantExt::CorrelationTermStructure> correlationCurve1 =
        market->correlationCurve("EUR-CMS-10Y", "EUR-CMS-2Y");
    Handle<QuantExt::CorrelationTermStructure> correlationCurve2 =
        market->correlationCurve("USD-CMS-10Y", "USD-CMS-2Y");
    BOOST_CHECK(*correlationCurve1);
    BOOST_CHECK(*correlationCurve2);

    Calendar calendar = TARGET();
    Date qlStartDate = calendar.advance(calendar.advance(market->asofDate(), 2 * Days), 0 * Months);
    Date qlEndDate1Y = calendar.advance(qlStartDate, 1 * Years, ModifiedFollowing);
    Date qlEndDate2Y = calendar.advance(qlStartDate, 2 * Years, ModifiedFollowing);
    string startDate = ore::data::to_string(qlStartDate);
    string endDate1Y = ore::data::to_string(qlEndDate1Y);
    string endDate2Y = ore::data::to_string(qlEndDate2Y);

    BOOST_TEST_MESSAGE(startDate);
    BOOST_TEST_MESSAGE(endDate1Y);
    BOOST_TEST_MESSAGE(endDate2Y);

    ScheduleData cms1YSchedule(ScheduleRules(startDate, endDate1Y, "3M", "TARGET", "MF", "MF", "Forward", "N"));
    ScheduleData cms2YSchedule(ScheduleRules(startDate, endDate2Y, "3M", "TARGET", "MF", "MF", "Forward", "N"));

    Real fairSpread1Y = 0.00752401;
    Real fairSpread2Y = 0.00755509;

    LegData cms1YLeg(QuantLib::ext::make_shared<CMSSpreadLegData>(
                         "USD-CMS-10Y", "USD-CMS-2Y", 2, true, std::vector<Real>(1, 0.0), vector<string>(),
                         std::vector<Real>(1, fairSpread1Y), vector<string>(), vector<double>(), vector<string>(),
                         vector<double>(), vector<string>(), true),
                     false, "USD", cms1YSchedule, "A360", std::vector<Real>(1, 1));
    vector<LegData> legs1Y;
    legs1Y.push_back(cms1YLeg);

    LegData cms2YLeg(QuantLib::ext::make_shared<CMSSpreadLegData>(
                         "USD-CMS-10Y", "USD-CMS-2Y", 2, true, std::vector<Real>(1, 0.0), vector<string>(),
                         std::vector<Real>(1, fairSpread2Y), vector<string>(), vector<double>(), vector<string>(),
                         vector<double>(), vector<string>(), true),
                     false, "USD", cms2YSchedule, "A360", std::vector<Real>(1, 1));
    vector<LegData> legs2Y;
    legs2Y.push_back(cms2YLeg);

    Envelope env("CP1");

    ore::data::Swap cmsSpread1YCap(env, legs1Y);
    ore::data::Swap cmsSpread2YCap(env, legs2Y);

    Real expectedNpv1Y = 0.0038614;
    Real expectedNpv2Y = 0.0105279;

    // Build and price
    QuantLib::ext::shared_ptr<EngineData> engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("CMS") = "LinearTSR";
    engineData->engine("CMS") = "LinearTSRPricer";

    map<string, string> engineparams1;
    engineparams1["MeanReversion"] = "0.0";
    engineparams1["Policy"] = "RateBound";
    engineparams1["LowerRateBoundLogNormal"] = "0.0001";
    engineparams1["UpperRateBoundLogNormal"] = "2";
    engineparams1["LowerRateBoundNormal"] = "-2";
    engineparams1["UpperRateBoundNormal"] = "2";
    engineparams1["VegaRatio"] = "0.01";
    engineparams1["PriceThreshold"] = "0.0000001";
    engineparams1["BsStdDev"] = "3";
    engineData->engineParameters("CMS") = engineparams1;

    engineData->model("CMSSpread") = "BrigoMercurio";
    engineData->engine("CMSSpread") = "Analytic";
    map<string, string> engineparams2;
    engineparams2["IntegrationPoints"] = "16";
    engineData->engineParameters("CMSSpread") = engineparams2;

    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";

    QuantLib::ext::shared_ptr<EngineFactory> engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData, market);

    cmsSpread1YCap.build(engineFactory);
    cmsSpread2YCap.build(engineFactory);

    Real npvCash = cmsSpread1YCap.instrument()->NPV();

    BOOST_TEST_MESSAGE("NPV Cash 1Y             = " << npvCash);
    BOOST_CHECK_SMALL(npvCash - expectedNpv1Y, 0.000001);

    npvCash = cmsSpread2YCap.instrument()->NPV();

    BOOST_TEST_MESSAGE("NPV Cash 2Y             = " << npvCash);

    BOOST_CHECK_SMALL(npvCash - expectedNpv2Y, 0.000001);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
