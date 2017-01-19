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

#include <ored/utilities/parsers.hpp>

#include <algorithm>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

EquityCurve::EquityCurve(Date asof, EquityCurveSpec spec, const Loader& loader,
    const CurveConfigurations& curveConfigs, const Conventions& conventions) {

    try {

        const boost::shared_ptr<EquityCurveConfig>& config = curveConfigs.equityCurveConfig(spec.curveConfigID());

        dc_ = Actual365Fixed();
        if (config->dayCountID() == "") {
            DLOG("No Day Count convention specified for " << spec.curveConfigID() << ", using A365F as default");
        }
        else {
            dc_ = parseDayCounter(config->dayCountID());
        }

        // We loop over all market data, looking for quotes that match the configuration
        // until we found the whole set of quotes or do not have more quotes in the
        // market data

        quotes_ = std::vector<Real>(config->quotes().size(), Null<Real>()); // can be either dividend yields, or forward prices (depending upon the CurveConfig type)
        terms_ = std::vector<Date>(config->quotes().size(), Null<Date>());
        equitySpot_ = Null<Real>();
        Size quotesRead = 0;

        for (auto& md : loader.loadQuotes(asof)) {

            if (md->asofDate() == asof &&
                md->instrumentType() == MarketDatum::InstrumentType::EQUITY_SPOT &&
                md->quoteType() == MarketDatum::QuoteType::PRICE) {

                boost::shared_ptr<EquitySpotQuote> q = boost::dynamic_pointer_cast<EquitySpotQuote>(md);

                if (q->name() == config->equitySpotQuoteID()) {
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
                    Size pos = it1 - config->quotes().begin();
                    QL_REQUIRE(terms_[pos] == Null<Date>(), "duplicate market datum found for " << config->quotes()[pos]);
                    terms_[pos] = q->expiryDate();
                    quotes_[pos] = q->quote()->value();
                    quotesRead++;
                }
            }

            if (config->type() == EquityCurveConfig::Type::DividendYield &&
                md->asofDate() == asof &&
                md->instrumentType() == MarketDatum::InstrumentType::EQUITY_DIVIDEND &&
                md->quoteType() == MarketDatum::QuoteType::RATE) {

                boost::shared_ptr<EquityDividendYieldQuote> q = boost::dynamic_pointer_cast<EquityDividendYieldQuote>(md);

                vector<string>::const_iterator it1 =
                    std::find(config->quotes().begin(), config->quotes().end(), q->name());

                // is the quote one of the list in the config ?
                if (it1 != config->quotes().end()) {
                    Size pos = it1 - config->quotes().begin();
                    QL_REQUIRE(terms_[pos] == Null<Date>(), "duplicate market datum found for " << config->quotes()[pos]);
                    terms_[pos] = q->tenorDate();
                    quotes_[pos] = q->quote()->value();
                    quotesRead++;
                }
            }
        }
        string curveTypeStr = (config->type() == EquityCurveConfig::Type::ForwardPrice) ?
            "EQUITY_FWD" :
            "EQUITY_DIVIDEND";

        LOG("EquityCurve: read " << quotesRead << " quotes of type " << curveTypeStr);
        QL_REQUIRE(quotesRead == config->quotes().size(), "read " << quotesRead << ", but "
            << config->quotes().size() << " required.");
        QL_REQUIRE(equitySpot_ != Null<Real>(),
            "Equity spot quote not found for " << config->curveID());

        for (Size i = 0; i < terms_.size(); i++) {
            QL_REQUIRE(terms_[i] > asof,
                "Invalid Fwd Expiry " << terms_[i] << " vs. " << asof);
            if(i>0) {
                QL_REQUIRE(terms_[i] > terms_[i - 1], "terms must be increasing in curve config");
            }
        }
        curveType_ = config->type();
    }
    catch (std::exception& e) {
        QL_FAIL("equity curve building failed: " << e.what());
    }
    catch (...) {
        QL_FAIL("equity curve building failed: unknown error");
    }
}

boost::shared_ptr<YieldTermStructure> EquityCurve::divYieldTermStructure(
    const Date& asof,
    const Handle<YieldTermStructure>& equityIrCurve) const {

    Handle<YieldTermStructure> irCurve = equityIrCurve;
    if(irCurve.empty()) {
        QL_REQUIRE(!discountCurve_.empty(), "EquityCurve: discount curve and equityIrCurve are both empty");
        irCurve = discountCurve_;
    }

    try {
        vector<Rate> dividendRates;
        if (curveType_ == EquityCurveConfig::Type::ForwardPrice) {
            // Convert Fwds into dividends.
            // Fwd = Spot e^{(r-q)T}
            // => q = 1/T Log(Spot/Fwd) + r
            for (Size i = 0; i < quotes_.size(); i++) {
                QL_REQUIRE(quotes_[i] > 0,
                    "Invalid Fwd Price " << quotes_[i] << " for " << spec_.name());
                Time t = dc_.yearFraction(asof, terms_[i]);
                Rate ir_rate = irCurve->zeroRate(t, Continuous);
                dividendRates.push_back(::log(equitySpot_ / quotes_[i]) / t + ir_rate);
            }
        }
        else if (curveType_ == EquityCurveConfig::Type::DividendYield) {
            dividendRates = quotes_;
        }
        else
            QL_FAIL("Invalid Equity curve configuration type for " << spec_.name());

        QL_REQUIRE(dividendRates.size() > 0,
            "No dividend yield rates extracted for " << spec_.name());
        QL_REQUIRE(dividendRates.size() == terms_.size(),
            "vector size mismatch - dividend rates (" << dividendRates.size() <<
            ") vs terms (" << terms_.size() << ")");

        boost::shared_ptr<YieldTermStructure> divCurve;
        // Build Dividend Term Structure
        if (dividendRates.size() == 1) {
            // We only have 1 quote so we build a flat curve
            divCurve.reset(new FlatForward(asof, dividendRates[0], dc_));
        }
        else {
            // Build a ZeroCurve
            Size n = terms_.size();
            vector<Date> dates(n + 1); // n+1 so we include asof
            vector<Rate> rates(n + 1);
            for (Size i = 0; i < n; i++) {
                dates[i + 1] = terms_[i];
                rates[i + 1] = dividendRates[i];
            }
            dates[0] = asof;
            rates[0] = rates[1];
            if (irCurve->maxDate() > dates.back()) {
                dates.push_back(irCurve->maxDate());
                rates.push_back(rates.back());
            }
            // FIXME, interpolation should be part of config?
            divCurve.reset(new InterpolatedZeroCurve<QuantLib::Linear>(dates, rates, dc_, QuantLib::Linear()));
            divCurve->enableExtrapolation();
        }
        return divCurve;
    } catch (std::exception& e) {
        QL_FAIL("equity curve building failed: " << e.what());
    } catch (...) {
        QL_FAIL("equity curve building failed: unknown error");
    }
}
}
}
