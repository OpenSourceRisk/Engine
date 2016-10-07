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

#include <ored/marketdata/swaptionvolcurve.hpp>
#include <ored/utilities/log.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <algorithm>

using namespace QuantLib;
using namespace std;

namespace openriskengine {
namespace data {

SwaptionVolCurve::SwaptionVolCurve(Date asof, SwaptionVolatilityCurveSpec spec, const Loader& loader,
                                   const CurveConfigurations& curveConfigs) {

    try {

        const boost::shared_ptr<SwaptionVolatilityCurveConfig>& config =
            curveConfigs.swaptionVolCurveConfig(spec.curveConfigID());

        QL_REQUIRE(config->dimension() == SwaptionVolatilityCurveConfig::Dimension::ATM,
                   "Unsupported Swaption vol curve building dimension");

        // We loop over all market data, looking for quotes that match the configuration
        // until we found the whole matrix or do not have more quotes in the market data

        MarketDatum::QuoteType volatilityType;
        switch (config->volatilityType()) {
        case SwaptionVolatilityCurveConfig::VolatilityType::Lognormal:
            volatilityType = MarketDatum::QuoteType::RATE_LNVOL;
            break;
        case SwaptionVolatilityCurveConfig::VolatilityType::Normal:
            volatilityType = MarketDatum::QuoteType::RATE_NVOL;
            break;
        case SwaptionVolatilityCurveConfig::VolatilityType::ShiftedLognormal:
            volatilityType = MarketDatum::QuoteType::RATE_SLNVOL;
            break;
        default:
            QL_FAIL("unexpected volatility type");
        }
        bool isSln = volatilityType == MarketDatum::QuoteType::RATE_SLNVOL;
        vector<Period> optionTenors = config->optionTenors();
        vector<Period> swapTenors = config->swapTenors();
        Matrix vols(optionTenors.size(), swapTenors.size());
        Matrix shifts(isSln ? vols.rows() : 0, isSln ? vols.columns() : 0);
        vector<vector<bool>> found(optionTenors.size(), vector<bool>(swapTenors.size()));
        vector<bool> shiftFound(swapTenors.size());
        Size remainingQuotes = optionTenors.size() * swapTenors.size();
        Size remainingShiftQuotes = isSln ? swapTenors.size() : 0;
        Size quotesRead = 0, shiftQuotesRead = 0;

        for (auto& md : loader.loadQuotes(asof)) {

            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::SWAPTION) {

                boost::shared_ptr<SwaptionQuote> q = boost::dynamic_pointer_cast<SwaptionQuote>(md);
                boost::shared_ptr<SwaptionShiftQuote> qs = boost::dynamic_pointer_cast<SwaptionShiftQuote>(md);

                if (remainingQuotes > 0 && q != NULL && q->ccy() == spec.ccy() && q->quoteType() == volatilityType &&
                    q->dimension() == "ATM") {

                    quotesRead++;

                    Size i = std::find(optionTenors.begin(), optionTenors.end(), q->expiry()) - optionTenors.begin();
                    Size j = std::find(swapTenors.begin(), swapTenors.end(), q->term()) - swapTenors.begin();

                    if (i < optionTenors.size() && j < swapTenors.size()) {
                        vols[i][j] = q->quote()->value();

                        if (!found[i][j]) {
                            found[i][j] = true;
                            if (--remainingQuotes == 0 && remainingShiftQuotes == 0)
                                break;
                        }
                    }
                }

                if (isSln && remainingShiftQuotes > 0 && qs != NULL && qs->ccy() == spec.ccy() &&
                    qs->quoteType() == MarketDatum::QuoteType::SHIFT) {

                    shiftQuotesRead++;

                    Size j = std::find(swapTenors.begin(), swapTenors.end(), qs->term()) - swapTenors.begin();

                    if (j < swapTenors.size()) {
                        for (Size i = 0; i < shifts.rows(); ++i)
                            shifts[i][j] = qs->quote()->value();

                        if (!shiftFound[j]) {
                            shiftFound[j] = true;
                            if (--remainingShiftQuotes == 0 && remainingQuotes == 0)
                                break;
                        }
                    }
                }
            }
        }

        LOG("SwaptionVolCurve: read " << quotesRead << " vols");

        // Check that we have all the data we need
        if (remainingQuotes != 0 || remainingShiftQuotes != 0) {
            ostringstream m, missingQuotes;
            m << "Not all quotes found for spec " << spec << endl;
            if (remainingQuotes != 0) {
                m << "Found vol data (*) and missing data (.):" << std::endl;
                for (Size i = 0; i < optionTenors.size(); ++i) {
                    for (Size j = 0; j < swapTenors.size(); ++j) {
                        if (found[i][j]) {
                            m << "*";
                        } else {
                            m << ".";
                            missingQuotes << "Missing vol: (" << optionTenors[i] << ", " << swapTenors[j] << ")"
                                          << endl;
                        }
                    }
                    if (i < optionTenors.size() - 1)
                        m << endl;
                }
            }
            if (remainingShiftQuotes != 0) {
                if (remainingQuotes != 0)
                    m << endl;
                m << "Found shift data (*) and missing data (.):" << std::endl;
                for (Size j = 0; j < swapTenors.size(); ++j) {
                    if (shiftFound[j]) {
                        m << "*";
                    } else {
                        m << ".";
                        missingQuotes << "Missing shifted vol: (" << swapTenors[j] << ")" << endl;
                    }
                }
                m << endl;
            }
            DLOGGERSTREAM << m.str() << endl << missingQuotes.str();
            QL_FAIL("could not build swaption vol curve");
        }

        vol_ = boost::shared_ptr<SwaptionVolatilityStructure>(new SwaptionVolatilityMatrix(
            asof, config->calendar(), config->businessDayConvention(), optionTenors, swapTenors, vols,
            config->dayCounter(), config->flatExtrapolation(),
            config->volatilityType() == SwaptionVolatilityCurveConfig::VolatilityType::Normal
                ? QuantLib::Normal
                : QuantLib::ShiftedLognormal,
            shifts));

        vol_->enableExtrapolation(config->extrapolate());

    } catch (std::exception& e) {
        QL_FAIL("swaption vol curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("swaption vol curve building failed: unknown error");
    }
}
}
}
