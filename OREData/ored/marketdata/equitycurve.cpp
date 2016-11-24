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

#include <ored/marketdata/equitycurve.hpp>
#include <ored/utilities/log.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/math/interpolations/backwardflatinterpolation.hpp>


#include <algorithm>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

EquityCurve::EquityCurve(Date asof, EquityCurveSpec spec, const Loader& loader,
                           const CurveConfigurations& curveConfigs, const Conventions& conventions,
                           map<string, boost::shared_ptr<YieldCurve>>& yieldCurves) {

    try {

        const boost::shared_ptr<EquityCurveConfig>& config = curveConfigs.equityCurveConfig(spec.curveConfigID());

        DayCounter conv_dc = Actual365Fixed();
        boost::shared_ptr<Convention> tmp = conventions.get(config->conventionID());
        if (tmp != NULL) {
            boost::shared_ptr<ZeroRateConvention> zeroConv = boost::dynamic_pointer_cast<ZeroRateConvention>(tmp);
            QL_REQUIRE(zeroConv != NULL, "Yield implied default curve requires ZERO convention (wrong type)");
            conv_dc = zeroConv->dayCounter();
        }

        equityIrCurveStr_ = config->equityInterestRateCurveID();
        auto it = yieldCurves.find(equityIrCurveStr_);
        QL_REQUIRE(it != yieldCurves.end(), 
            "The IR curve, " << equityIrCurveStr_ <<
            ", required in the building of the curve, " << spec.name() << 
            ", was not found.");
        equityIrCurve_ = it->second->handle().currentLink();

        // We loop over all market data, looking for quotes that match the configuration
        // until we found the whole set of quotes or do not have more quotes in the
        // market data

        vector<Real> quotes; // can be either dividend yields, or forward prices (depending upon the CurveConfig type)
        vector<Date> terms;
        equitySpot_ = Null<Real>();

        for (auto& md : loader.loadQuotes(asof)) {

            if (md->asofDate() == asof && 
                md->instrumentType() == MarketDatum::InstrumentType::EQUITY_SPOT &&
                md->quoteType() == MarketDatum::QuoteType::PRICE) {

                boost::shared_ptr<EquitySpotQuote> q = boost::dynamic_pointer_cast<EquitySpotQuote>(md);

                if (q->name() == equityIrCurveStr_) {
                    QL_REQUIRE(equitySpot_ == Null<Real>(), "duplicate equity spot quote " << q->name()
                                                                                               << " found.");
                    equitySpot_ = q->quote()->value();
                }
            }

            if (config->type() == EquityCurveConfig::Type::ForwardPrice && md->asofDate() == asof &&
                md->instrumentType() == MarketDatum::InstrumentType::EQUITY_FWD &&
                md->quoteType() == MarketDatum::QuoteType::PRICE) {

                boost::shared_ptr<EquityForwardQuote> q = boost::dynamic_pointer_cast<EquityForwardQuote>(md);

                vector<string>::const_iterator it1 =
                    std::find(config->quotes().begin(), config->quotes().end(), q->name());

                // is the quote one of the list in the config ?
                if (it1 != config->quotes().end()) {

                    vector<Date>::const_iterator it2 = std::find(terms.begin(), terms.end(), q->expiryDate());
                    QL_REQUIRE(it2 == terms.end(), "duplicate term in quotes found ("
                                                       << q->expiryDate() << ") while loading equity curve "
                                                       << config->curveID());
                    terms.push_back(q->expiryDate());
                    quotes.push_back(q->quote()->value());
                }
            }

            if (config->type() == EquityCurveConfig::Type::DividendYield && 
                md->asofDate() == asof &&
                md->instrumentType() == MarketDatum::InstrumentType::EQUITY_DIVIDEND) {
                
                boost::shared_ptr<EquityDividendYieldQuote> q = boost::dynamic_pointer_cast<EquityDividendYieldQuote>(md);

                vector<string>::const_iterator it1 =
                    std::find(config->quotes().begin(), config->quotes().end(), q->name());

                // is the quote one of the list in the config ?
                if (it1 != config->quotes().end()) {

                    vector<Date>::const_iterator it2 = std::find(terms.begin(), terms.end(), q->tenorDate());
                    QL_REQUIRE(it2 == terms.end(), "duplicate term in quotes found ("
                                                       << q->tenorDate() << ") while loading dividend yield curve "
                                                       << config->curveID());
                    terms.push_back(q->tenorDate());
                    quotes.push_back(q->quote()->value());
                }
            }
        }
        string curveTypeStr = (config->type() == EquityCurveConfig::Type::ForwardPrice) ? 
            "EQUITY_FWD" : 
            "EQUITY_DIVIDEND";
        LOG("EquityCurve: read " << quotes.size() << " quotes of type " << curveTypeStr);
        QL_REQUIRE(quotes.size() == config->quotes().size(), "read " << quotes.size() << ", but "
                                                                     << config->quotes().size() << " required.");
        QL_REQUIRE(quotes.size() == terms.size(), "read " << quotes.size() << "quotes, but "
            << terms.size() << " tenors.");
        QL_REQUIRE(equitySpot_ != Null<Real>(),
            "Equity spot quote not found for " << config->curveID());
        QL_REQUIRE(equityIrCurve_ != NULL,
            "Equity IR curve not found for " << config->curveID());

        vector<Rate> dividendRates;
        vector<Period> dividendTerms;
        if (config->type() == EquityCurveConfig::Type::ForwardPrice) {
            // Convert Fwds into dividends.
            // Fwd = Spot e^{(r-q)T}
            // => q = 1/T Log(Spot/Fwd) + r
            for (Size i = 0; i < quotes.size(); i++) {
                QL_REQUIRE(terms[i] > asof, "Invalid Fwd Expiry " << terms[i] << " vs. " << asof);
                QL_REQUIRE(quotes[i] > 0, "Invalid Fwd Price " << quotes[i] << " for " << spec.name());
                Time t = conv_dc.yearFraction(asof, terms[i]);
                dividendRates.push_back(::log(equitySpot_ / quotes[i]) / t + equityIrCurve_->zeroRate(t, Continuous));
                dividendTerms.push_back(Period(terms[i] - asof, Days));
            }
        }
        else if (config->type() == EquityCurveConfig::Type::DividendYield) {
            dividendRates = quotes;
            for (Size i = 0; i < terms.size(); i++)
                dividendTerms.push_back(Period(terms[i] - asof, Days));
        }
        else
            QL_FAIL("Invalid Equity curve configuration type for " << config->curveID());

        QL_REQUIRE(dividendRates.size() > 0, 
            "No dividend yield rates extracted for " << config->curveID());
        QL_REQUIRE(dividendRates.size() == dividendTerms.size(), 
            "vector size mismatch - dividend rates (" << dividendRates.size() << 
            ") vs terms (" << dividendTerms.size() << ")");

        // Build Dividend Term Structure
        if (dividendRates.size() == 1) {
            // We only have 1 quote so we build a flat curve
            divCurve_.reset(new FlatForward(asof, dividendRates[0], conv_dc));
        }
        else {
            // Build a ZeroCurve
            // For some reason, the order of the dates/rates is getting messed up above
            // so we use a map here and sort by dates.
            Size n = dividendTerms.size();
            map<Period, Rate> m;
            for (Size i = 0; i < n; i++)
                m[dividendTerms[i]] = dividendRates[i];
            sort(dividendTerms.begin(), dividendTerms.end());
            vector<Date> dates(n + 1); // n+1 so we include asof
            vector<Rate> rates(n + 1);
            for (Size i = 0; i < n; i++) {
                dates[i + 1] = asof + dividendTerms[i];
                rates[i + 1] = m[dividendTerms[i]];
            }
            dates[0] = asof;
            rates[0] = rates[1];
            divCurve_.reset(new InterpolatedZeroCurve<QuantLib::Linear>(dates, rates, conv_dc, QuantLib::Linear()));
            divCurve_->enableExtrapolation();
        }
    } catch (std::exception& e) {
        QL_FAIL("default curve building failed: " << e.what());
    } catch (...) {
        QL_FAIL("default curve building failed: unknown error");
    }
}
}
}
