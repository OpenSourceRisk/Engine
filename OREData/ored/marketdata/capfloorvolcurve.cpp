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

#include <ored/marketdata/capfloorvolcurve.hpp>
#include <ored/utilities/log.hpp>

#include <qle/termstructures/datedstrippedoptionlet.hpp>
#include <qle/termstructures/datedstrippedoptionletadapter.hpp>
#include <qle/termstructures/optionletstripper1.hpp>
#include <qle/termstructures/optionletstripper2.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolcurve.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolsurface.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletadapter.hpp>

using namespace QuantLib;
using QuantExt::DatedStrippedOptionlet;
using QuantExt::DatedStrippedOptionletAdapter;
using QuantExt::OptionletStripper1;
using QuantExt::OptionletStripper2;
using namespace std;

namespace ore {
namespace data {

CapFloorVolCurve::CapFloorVolCurve(Date asof, CapFloorVolatilityCurveSpec spec, const Loader& loader,
                                   const CurveConfigurations& curveConfigs, boost::shared_ptr<IborIndex> iborIndex,
                                   Handle<YieldTermStructure> discountCurve) {
    try {

        const boost::shared_ptr<CapFloorVolatilityCurveConfig>& config =
            curveConfigs.capFloorVolCurveConfig(spec.curveConfigID());

        // Volatility type
        MarketDatum::QuoteType volatilityType;
        VolatilityType quoteVolatilityType;

        switch (config->volatilityType()) {
        case CapFloorVolatilityCurveConfig::VolatilityType::Lognormal:
            volatilityType = MarketDatum::QuoteType::RATE_LNVOL;
            quoteVolatilityType = ShiftedLognormal;
            break;
        case CapFloorVolatilityCurveConfig::VolatilityType::Normal:
            volatilityType = MarketDatum::QuoteType::RATE_NVOL;
            quoteVolatilityType = Normal;
            break;
        case CapFloorVolatilityCurveConfig::VolatilityType::ShiftedLognormal:
            volatilityType = MarketDatum::QuoteType::RATE_SLNVOL;
            quoteVolatilityType = ShiftedLognormal;
            break;
        default:
            QL_FAIL("unexpected volatility type");
        }

        // Read in quotes matrix. If we hit any ATM quotes that match one of the tenors, store those also.
        vector<Period> tenors = config->tenors();
        vector<double> strikes = config->strikes();
        QL_REQUIRE(!strikes.empty(), "Strikes should not be empty - expect a cap matrix");
        Matrix vols(tenors.size(), strikes.size());
        map<Period, Volatility> atmVolCurve;
        vector<vector<bool>> found(tenors.size(), vector<bool>(strikes.size()));
        Size remainingQuotes = tenors.size() * strikes.size();
        Size quotesRead = 0;

        // We take the first capfloor shift quote that we find in the file matching the
        // currency and index tenor
        bool isSln = volatilityType == MarketDatum::QuoteType::RATE_SLNVOL;
        bool haveShiftQuote = false;
        Real shift = 0.0;

        for (auto& md : loader.loadQuotes(asof)) {
            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::CAPFLOOR) {

                boost::shared_ptr<CapFloorQuote> q = boost::dynamic_pointer_cast<CapFloorQuote>(md);
                boost::shared_ptr<CapFloorShiftQuote> qs = boost::dynamic_pointer_cast<CapFloorShiftQuote>(md);

                if (q != NULL && q->ccy() == spec.ccy() && q->underlying() == iborIndex->tenor() &&
                    q->quoteType() == volatilityType) {

                    quotesRead++;

                    if (!q->atm()) {
                        Size i = std::find(tenors.begin(), tenors.end(), q->term()) - tenors.begin();
                        Size j = std::find_if(strikes.begin(), strikes.end(), [&q](const double& s) {
                            return close_enough(s, q->strike());
                        }) - strikes.begin();

                        if (i < tenors.size() && j < strikes.size()) {
                            vols[i][j] = q->quote()->value();

                            if (!found[i][j]) {
                                found[i][j] = true;
                                --remainingQuotes;
                            }
                        }
                    } else if (config->includeAtm()) {
                        // This forces us to read all cap/floor quotes
                        atmVolCurve.emplace(q->term(), q->quote()->value());
                    }
                } else if (isSln && !haveShiftQuote && qs && qs->ccy() == spec.ccy() &&
                           qs->indexTenor() == iborIndex->tenor() && qs->quoteType() == MarketDatum::QuoteType::SHIFT) {
                    // Shift quote for shifted lognormal capfloor volatility
                    shift = qs->quote()->value();
                    haveShiftQuote = true;
                }
            }
        }

        LOG("CapFloorVolCurve: read " << quotesRead << " vols");

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
            LOG(m.str());
            QL_FAIL("could not build cap/floor vol curve");
        }

        // Non-ATM cap/floor volatility surface
        boost::shared_ptr<CapFloorTermVolSurface> capVol = boost::make_shared<CapFloorTermVolSurface>(
            0, config->calendar(), config->businessDayConvention(), tenors, strikes, vols, config->dayCounter());

        // Build the stripped caplet/floorlet surface
        // Hardcoded target volatility type to Normal - decision made to always work with a Normal optionlet surface
        boost::shared_ptr<OptionletStripper1> optionletStripper =
            boost::make_shared<OptionletStripper1>(capVol, iborIndex, Null<Rate>(), 1.0e-6, 100, discountCurve,
                                                   quoteVolatilityType, shift, false, Normal, 0.0);
        boost::shared_ptr<DatedStrippedOptionlet> datedOptionletStripper =
            boost::make_shared<DatedStrippedOptionlet>(asof, optionletStripper);
        capletVol_ = boost::make_shared<DatedStrippedOptionletAdapter>(datedOptionletStripper);
        capletVol_->enableExtrapolation(config->extrapolate());

        // If given, add the ATM cap/floor volatility curve
        if (!atmVolCurve.empty()) {
            // Ordered list of ATM tenors and corresponding values
            vector<Period> atmTenors;
            vector<Volatility> atmVols;
            for (const auto& kv : atmVolCurve) {
                atmTenors.push_back(kv.first);
                atmVols.push_back(kv.second);
            }

            boost::shared_ptr<CapFloorTermVolCurve> atmCapVol = boost::make_shared<CapFloorTermVolCurve>(
                0, config->calendar(), config->businessDayConvention(), atmTenors, atmVols, config->dayCounter());
            Handle<CapFloorTermVolCurve> hAtmCapVol(atmCapVol);
            boost::shared_ptr<OptionletStripper> atmOptionletStripper =
                boost::make_shared<OptionletStripper2>(optionletStripper, hAtmCapVol, quoteVolatilityType, shift);
            datedOptionletStripper = boost::make_shared<DatedStrippedOptionlet>(asof, atmOptionletStripper);
            capletVol_ = boost::make_shared<DatedStrippedOptionletAdapter>(datedOptionletStripper);
            capletVol_->enableExtrapolation(config->extrapolate());
        }
    } catch (std::exception& e) {
        QL_FAIL("cap/floor vol curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("cap/floor vol curve building failed: unknown error");
    }
}
}
}
