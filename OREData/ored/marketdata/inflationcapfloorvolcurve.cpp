/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/marketdata/inflationcapfloorvolcurve.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/indexparser.hpp>
#include <qle/termstructures/yoyinflationoptionletvolstripper.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolcurve.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

InflationCapFloorVolCurve::InflationCapFloorVolCurve(Date asof, InflationCapFloorVolatilityCurveSpec spec, 
    const Loader& loader, const CurveConfigurations& curveConfigs,
    map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
    map<string, boost::shared_ptr<InflationCurve>>& inflationCurves) {
    try {

        const boost::shared_ptr<InflationCapFloorVolatilityCurveConfig>& config =
            curveConfigs.inflationCapFloorVolCurveConfig(spec.curveConfigID());
        
        Handle<YieldTermStructure> yts;
        auto it = yieldCurves.find(config->yieldTermStructure());
        if (it != yieldCurves.end()) {
            yts = it->second->handle();
        }
        else {
            QL_FAIL("The nominal term structure, " << config->yieldTermStructure()
                << ", required in the building "
                "of the curve, "
                << spec.name() << ", was not found.");
        }
        // Volatility type
        MarketDatum::QuoteType volatilityType;
        VolatilityType quoteVolatilityType;

        switch (config->volatilityType()) {
        case InflationCapFloorVolatilityCurveConfig::VolatilityType::Lognormal:
            volatilityType = MarketDatum::QuoteType::RATE_LNVOL;
            quoteVolatilityType = ShiftedLognormal;
            break;
        case InflationCapFloorVolatilityCurveConfig::VolatilityType::Normal:
            volatilityType = MarketDatum::QuoteType::RATE_NVOL;
            quoteVolatilityType = Normal;
            break;
        case InflationCapFloorVolatilityCurveConfig::VolatilityType::ShiftedLognormal:
            volatilityType = MarketDatum::QuoteType::RATE_SLNVOL;
            quoteVolatilityType = ShiftedLognormal;
            break;
        default:
            QL_FAIL("unexpected volatility type");
        }

        // Read in quotes matrix
        vector<Period> tenors = config->tenors();
        vector<double> strikes = config->strikes();
        QL_REQUIRE(!strikes.empty(), "Strikes should not be empty - expect a cap matrix");
        Matrix vols(tenors.size(), strikes.size());
        vector<vector<bool>> found(tenors.size(), vector<bool>(strikes.size()));
        Size remainingQuotes = tenors.size() * strikes.size();
        Size quotesRead = 0;

        // We take the first capfloor shift quote that we find in the file matching the
        // currency and index tenor
        for (auto& md : loader.loadQuotes(asof)) {
            if (md->asofDate() == asof &&
                (md->instrumentType() == MarketDatum::InstrumentType::ZC_INFLATIONCAPFLOOR ||
                    md->instrumentType() == MarketDatum::InstrumentType::YY_INFLATIONCAPFLOOR)) {

                boost::shared_ptr<InflationCapFloorQuote> q = boost::dynamic_pointer_cast<InflationCapFloorQuote>(md);

                if (config->type() == InflationCapFloorVolatilityCurveConfig::Type::ZC) {
                    q = boost::dynamic_pointer_cast<ZcInflationCapFloorQuote>(md);
                }
                else {
                    q = boost::dynamic_pointer_cast<YyInflationCapFloorQuote>(md);
                }

                if (q != NULL && q->index() == spec.index() && q->quoteType() == volatilityType) {

                    quotesRead++;

                    Real strike = parseReal(q->strike());
                    Size i = std::find(tenors.begin(), tenors.end(), q->term()) - tenors.begin();
                    Size j = std::find_if(strikes.begin(), strikes.end(),
                        [&strike](const double& s) { return close_enough(s, strike); }) -
                        strikes.begin();

                    if (i < tenors.size() && j < strikes.size()) {
                        vols[i][j] = q->quote()->value();

                        if (!found[i][j]) {
                            found[i][j] = true;
                            --remainingQuotes;
                        }
                    }

                }
            }
        }

        LOG("InflationCapFloorVolCurve: read " << quotesRead << " vols");

        // Check that we have all the data we need
        if (remainingQuotes != 0) {
            ostringstream m;
            m << "Not all quotes found for spec " << spec << endl;
            if (remainingQuotes != 0) {
                m << "Found vol data (*) and missing data (.):" << std::endl;
                for (Size i = 0; i < tenors.size(); ++i) {
                    for (Size j = 0; j < strikes.size(); ++j) {
                        m << (found[i][j] ? "*" : ".");
                    }
                    if (i < tenors.size() - 1)
                        m << endl;
                }
            }
            DLOGGERSTREAM << m.str() << endl;
            QL_FAIL("could not build cap/floor vol curve");
        }
        
        // Non-ATM cap/floor volatility surface
        boost::shared_ptr<CapFloorTermVolSurface> capVol = boost::make_shared<CapFloorTermVolSurface>(
            0, config->calendar(), config->businessDayConvention(), tenors, strikes, vols, config->dayCounter());

        // Only handle YoY Inflation at the moment
        if (config->type() == InflationCapFloorVolatilityCurveConfig::Type::YY) {

            boost::shared_ptr<YoYInflationIndex> index;
            auto it2 = inflationCurves.find(config->indexCurve());
            if (it2 != inflationCurves.end()) {
                boost::shared_ptr<InflationTermStructure> ts = it2->second->inflationTermStructure();
                // Check if the Index curve is a YoY curve - if not it must be a zero curve
                boost::shared_ptr<YoYInflationTermStructure> yyTs =
                    boost::dynamic_pointer_cast<YoYInflationTermStructure>(ts);
                QL_REQUIRE(yyTs, "YoY Inflation curve required for vol surface " << index->name());
                index = boost::make_shared<QuantExt::YoYInflationIndexWrapper>(
                    parseZeroInflationIndex(config->index(), true), true, Handle<YoYInflationTermStructure>(yyTs));
            }

            boost::shared_ptr<YoYInflationOptionletVolStripper> volStripper =
                boost::make_shared<YoYInflationOptionletVolStripper>(capVol, index, yts, quoteVolatilityType);
            yoyVolSurface_ = volStripper->yoyInflationCapFloorVolSurface();

        }
    }
    catch (std::exception& e) {
        QL_FAIL("inflation cap/floor vol curve building failed :" << e.what());
    }
    catch (...) {
        QL_FAIL("inflation cap/floor vol curve building failed: unknown error");
    }
}
} // namespace data
} // namespace ore
