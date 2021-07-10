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
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/log.hpp>
#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <ql/math/interpolations/convexmonotoneinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>
#include <qle/termstructures/equityforwardcurvestripper.hpp>
#include <qle/termstructures/optionpricesurface.hpp>

#include <ored/utilities/currencycheck.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/wildcard.hpp>

#include <algorithm>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

EquityCurve::EquityCurve(Date asof, EquityCurveSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs,
                         const Conventions& conventions,
                         const map<string, boost::shared_ptr<YieldCurve>>& requiredYieldCurves) {

    try {

        const boost::shared_ptr<EquityCurveConfig>& config = curveConfigs.equityCurveConfig(spec.curveConfigID());

        dc_ = Actual365Fixed();
        if (config->dayCountID() == "") {
            DLOG("No Day Count convention specified for " << spec.curveConfigID() << ", using A365F as default");
        } else {
            dc_ = parseDayCounter(config->dayCountID());
        }

        // set the calendar to the ccy based calendar, if provided by config we try to use that
        Calendar calendar;
        if (!config->calendar().empty()) {
            try {
                calendar = parseCalendar(config->calendar());
            } catch (exception& ex) {
                WLOG("Failed to get Calendar name for  " << config->calendar() << ":" << ex.what());
            }
        }
        if (calendar.empty()) {
            calendar = parseCalendar(config->currency());
        }

        // Set the Curve type - EquityFwd / OptionPrice / DividendYield
        curveType_ = config->type();

        // declair spot and yields
        Handle<Quote> equitySpot;
        Handle<YieldTermStructure> forecastYieldTermStructure;
        Handle<YieldTermStructure> dividendYieldTermStructure;

        // Set the Equity Forecast curve
        YieldCurveSpec ycspec(config->currency(), config->forecastingCurve());

        // at this stage we should have built the curve already
        //  (consider building curve on fly if not? Would need to work around fact that requiredYieldCurves is currently
        //  const ref)
        auto itr = requiredYieldCurves.find(ycspec.name());
        QL_REQUIRE(itr != requiredYieldCurves.end(),
                   "Yield Curve Spec - " << ycspec.name() << " - not found during equity curve build");
        boost::shared_ptr<YieldCurve> yieldCurve = itr->second;
        forecastYieldTermStructure = yieldCurve->handle();

        // Set the interpolation variables
        dividendInterpVariable_ = parseYieldCurveInterpolationVariable(config->dividendInterpolationVariable());
        dividendInterpMethod_ = parseYieldCurveInterpolationMethod(config->dividendInterpolationMethod());

        // We loop over all market data, looking for quotes that match the configuration
        // until we found the whole set of quotes or do not have more quotes in the
        // market data

        vector<boost::shared_ptr<EquityForwardQuote>> qt; // for sorting quotes_/terms_ pairs
        vector<boost::shared_ptr<EquityOptionQuote>> oqt; // store any equity vol quotes
        Size quotesRead = 0;

        // in case of wild-card in config
        auto wildcard = getUniqueWildcard(config->fwdQuotes());

        if ((config->type() == EquityCurveConfig::Type::ForwardPrice ||
             config->type() == EquityCurveConfig::Type::ForwardDividendPrice ||
             config->type() == EquityCurveConfig::Type::OptionPremium) &&
            wildcard) {
            LOG("Wild card quote specified for " << config->curveID())
        } else {
            if (config->type() == EquityCurveConfig::Type::OptionPremium) {
                oqt.resize(config->fwdQuotes().size());
            } else {
                for (Size i = 0; i < config->fwdQuotes().size(); i++) {
                    quotes_.push_back(Null<Real>());
                    terms_.push_back(Null<Date>());
                }
            }
        }

        for (auto& md : loader.loadQuotes(asof)) {

            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::EQUITY_SPOT &&
                md->quoteType() == MarketDatum::QuoteType::PRICE) {

                boost::shared_ptr<EquitySpotQuote> q = boost::dynamic_pointer_cast<EquitySpotQuote>(md);

                if (q->name() == config->equitySpotQuoteID()) {
                    QL_REQUIRE(equitySpot.empty(), "duplicate equity spot quote " << q->name() << " found.");
                    // convert quote from minor to major currency if needed
                    Real spot = convertMinorToMajorCurrency(q->ccy(), q->quote()->value());
                    equitySpot = Handle<Quote>(boost::make_shared<SimpleQuote>(spot));
                }
            }

            if ((config->type() == EquityCurveConfig::Type::ForwardPrice ||
                 config->type() == EquityCurveConfig::Type::ForwardDividendPrice) &&
                md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::EQUITY_FWD &&
                md->quoteType() == MarketDatum::QuoteType::PRICE) {

                boost::shared_ptr<EquityForwardQuote> q = boost::dynamic_pointer_cast<EquityForwardQuote>(md);

                if (wildcard) {
                    // is the quote 'in' the config? (also check expiry not before asof)
                    if ((*wildcard).matches(q->name()) && asof < q->expiryDate()) {
                        QL_REQUIRE(find(qt.begin(), qt.end(), q) == qt.end(),
                                   "duplicate market datum found for " << q->name());
                        DLOG("EquityCurve Forward Price found for quote: " << q->name());
                        qt.push_back(q); // terms_ and quotes_
                        quotesRead++;
                    }
                } else {
                    vector<string>::const_iterator it1 =
                        std::find(config->fwdQuotes().begin(), config->fwdQuotes().end(), q->name());

                    // is the quote one of the list in the config ?
                    if (it1 != config->fwdQuotes().end()) {
                        Size pos = it1 - config->fwdQuotes().begin();
                        QL_REQUIRE(terms_[pos] == Null<Date>(),
                                   "duplicate market datum found for " << config->fwdQuotes()[pos]);
                        terms_[pos] = q->expiryDate();
                        // convert quote from minor to major currency if needed
                        quotes_[pos] = convertMinorToMajorCurrency(q->ccy(), q->quote()->value());
                        quotesRead++;
                    }
                }
            }

            if (config->type() == EquityCurveConfig::Type::OptionPremium && md->asofDate() == asof &&
                md->instrumentType() == MarketDatum::InstrumentType::EQUITY_OPTION &&
                md->quoteType() == MarketDatum::QuoteType::PRICE) {

                boost::shared_ptr<EquityOptionQuote> q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);
                Date expiryDate = getDateFromDateOrPeriod(q->expiry(), asof);

                if (wildcard) {
                    // is the quote 'in' the config? (also check expiry not before asof)
                    if ((*wildcard).matches(q->name()) && asof < expiryDate) {
                        QL_REQUIRE(find(oqt.begin(), oqt.end(), q) == oqt.end(),
                                   "duplicate market datum found for " << q->name());
                        DLOG("EquityCurve Volatility Price found for quote: " << q->name());
                        oqt.push_back(q);
                        quotesRead++;
                    }
                } else {
                    vector<string>::const_iterator it1 =
                        std::find(config->fwdQuotes().begin(), config->fwdQuotes().end(), q->name());

                    // is the quote one of the list in the config ?
                    if (it1 != config->fwdQuotes().end()) {
                        Size pos = it1 - config->fwdQuotes().begin();
                        QL_REQUIRE(!oqt[pos], "duplicate market datum found for " << config->fwdQuotes()[pos]);
                        oqt[pos] = q;
                        quotesRead++;
                    }
                }
            }

            if (config->type() == EquityCurveConfig::Type::DividendYield && md->asofDate() == asof &&
                md->instrumentType() == MarketDatum::InstrumentType::EQUITY_DIVIDEND &&
                md->quoteType() == MarketDatum::QuoteType::RATE) {

                boost::shared_ptr<EquityDividendYieldQuote> q =
                    boost::dynamic_pointer_cast<EquityDividendYieldQuote>(md);

                vector<string>::const_iterator it1 =
                    std::find(config->fwdQuotes().begin(), config->fwdQuotes().end(), q->name());

                // is the quote one of the list in the config ?
                if (it1 != config->fwdQuotes().end()) {
                    Size pos = it1 - config->fwdQuotes().begin();
                    QL_REQUIRE(terms_[pos] == Null<Date>(),
                               "duplicate market datum found for " << config->fwdQuotes()[pos]);
                    DLOG("EquityCurve Dividend Yield found for quote: " << q->name());
                    terms_[pos] = q->tenorDate();
                    quotes_[pos] = q->quote()->value();
                    quotesRead++;
                }
            }
        }

        // some checks on the quotes read
        LOG("EquityCurve: read " << quotesRead << " quotes of type " << config->type());
        QL_REQUIRE(!equitySpot.empty(), "Equity spot quote not found for " << config->curveID());

        if (!wildcard) {
            QL_REQUIRE(quotesRead == config->fwdQuotes().size(),
                       "read " << quotesRead << ", but " << config->fwdQuotes().size() << " required.");
        }

        for (Size i = 0; i < terms_.size(); i++) {
            QL_REQUIRE(terms_[i] > asof, "Invalid Fwd Expiry " << terms_[i] << " vs. " << asof);
            if (i > 0) {
                QL_REQUIRE(terms_[i] > terms_[i - 1], "terms must be increasing in curve config");
            }
        }

        // the curve type that we will build
        EquityCurveConfig::Type buildCurveType = curveType_;

        // for curveType ForwardPrice or OptionPremium poputuate the terms_ and quotes_ with forward prices
        if (curveType_ == EquityCurveConfig::Type::ForwardPrice ||
            curveType_ == EquityCurveConfig::Type::ForwardDividendPrice) {

            if (qt.size() > 0) {
                DLOG("Building Equity Dividend Yield curve from Forward/Future prices");

                // sort quotes and terms in case of wild-card
                if (wildcard) {
                    QL_REQUIRE(quotesRead > 0, "Wild card quote specified, but no quotes read.")

                        // sort
                        std::sort(qt.begin(), qt.end(),
                            [](const boost::shared_ptr<EquityForwardQuote>& a,
                                const boost::shared_ptr<EquityForwardQuote>& b) -> bool {
                        return a->expiryDate() < b->expiryDate();
                    });

                    // populate individual quote, term vectors
                    for (Size i = 0; i < qt.size(); i++) {
                        terms_.push_back(qt[i]->expiryDate());
                        // convert quote from minor to major currency if needed
                        quotes_.push_back(convertMinorToMajorCurrency(qt[i]->ccy(), qt[i]->quote()->value()));
                    }
                }
            }
            if (quotes_.size() == 0) {
                LOG("No Equity Forward quotes provided for " << config->curveID()
                    << ", continuing without dividend curve.");
                buildCurveType = EquityCurveConfig::Type::NoDividends;
            }
        } else if (curveType_ == EquityCurveConfig::Type::OptionPremium) {

            if (oqt.size() == 0) {
                LOG("No Equity Option quotes provided for " << config->curveID()
                                                             << ", continuing without dividend curve.");
                buildCurveType = EquityCurveConfig::Type::NoDividends;
            } else {
                DLOG("Building Equity Dividend Yield curve from Option Volatilities");
                vector<boost::shared_ptr<EquityOptionQuote>> calls, puts;
                vector<Date> callDates, putDates;
                vector<Real> callStrikes, putStrikes;
                vector<Real> callPremiums, putPremiums;
                Calendar cal = NullCalendar();

                // Split the quotes into call and puts
                for (auto q : oqt) {
                    if (q->quote()->value() > 0) {
                        if (q->isCall()) {
                            calls.push_back(q);
                        }
                        else {
                            puts.push_back(q);
                        }
                    }
                }

                // loop through the quotes and get the expiries, strikes, vols and types
                // We only want overlapping expiry/strike pairs
                for (Size i = 0; i < calls.size(); i++) {
                    for (Size j = 0; j < puts.size(); j++) {
                        if (calls[i]->expiry() == puts[j]->expiry()) {
                            auto callAbsoluteStrike = boost::dynamic_pointer_cast<AbsoluteStrike>(calls[i]->strike());
                            auto putAbsoluteStrike = boost::dynamic_pointer_cast<AbsoluteStrike>(puts[j]->strike());
                            QL_REQUIRE(callAbsoluteStrike, "Expected absolute strike for quote " << calls[i]->name());
                            QL_REQUIRE(putAbsoluteStrike, "Expected absolute strike for quote " << puts[j]->name());
                            if (*calls[i]->strike() == *puts[j]->strike()) {
                                TLOG("Adding Call and Put for strike/expiry pair : " << calls[i]->expiry() << "/"
                                                                                     << calls[i]->strike()->toString());
                                callDates.push_back(getDateFromDateOrPeriod(calls[i]->expiry(), asof));
                                putDates.push_back(getDateFromDateOrPeriod(puts[j]->expiry(), asof));

                                // convert strike to major currency if quoted in minor
                                Real callStrike =
                                    convertMinorToMajorCurrency(calls[i]->ccy(), callAbsoluteStrike->strike());
                                Real putStrike =
                                    convertMinorToMajorCurrency(puts[j]->ccy(), putAbsoluteStrike->strike());
                                callStrikes.push_back(callStrike);
                                putStrikes.push_back(putStrike);
                                // convert premium to major currency if quoted in minor
                                callPremiums.push_back(
                                    convertMinorToMajorCurrency(calls[i]->ccy(), calls[i]->quote()->value()));
                                putPremiums.push_back(
                                    convertMinorToMajorCurrency(puts[j]->ccy(), puts[j]->quote()->value()));
                            }
                        }
                    }
                }

                if (callDates.size() > 0 && putDates.size() > 0) {                    
                        DLOG("Found " << callDates.size() << " Call and Put Option Volatilities");

                        DLOG("Building a Sparce Volatility surface for calls and puts");
                        // Build a Black Variance Sparse matrix
                        boost::shared_ptr<OptionPriceSurface> callSurface =
                            boost::make_shared<OptionPriceSurface>(asof, callDates, callStrikes, callPremiums, dc_);
                        boost::shared_ptr<OptionPriceSurface> putSurface =
                            boost::make_shared<OptionPriceSurface>(asof, putDates, putStrikes, putPremiums, dc_);
                        DLOG("CallSurface contains " << callSurface->expiries().size() << " expiries.");

                        DLOG("Stripping equity forwards from the option premium surfaces");
                        boost::shared_ptr<EquityForwardCurveStripper> efcs = boost::make_shared<EquityForwardCurveStripper>(
                            callSurface, putSurface, forecastYieldTermStructure, equitySpot, config->exerciseStyle());

                        // set terms and quotes from the stripper
                        terms_ = efcs->expiries();
                        quotes_ = efcs->forwards();
                }
                else {
                    DLOG("No overlapping call and put quotes for equity " << spec.curveConfigID() << " building NoDividends curve");
                    buildCurveType = EquityCurveConfig::Type::NoDividends;
                }
            }
        }

        // Build the Dividend Yield curve from the quotes loaded
        vector<Rate> dividendRates;
        if (buildCurveType == EquityCurveConfig::Type::ForwardPrice ||
            buildCurveType == EquityCurveConfig::Type::ForwardDividendPrice ||
            buildCurveType == EquityCurveConfig::Type::OptionPremium) {
            // Convert Fwds into dividends.
            // Fwd = Spot e^{(r-q)T}
            // => q = 1/T Log(Spot/Fwd) + r
            for (Size i = 0; i < quotes_.size(); i++) {
                QL_REQUIRE(quotes_[i] > 0, "Invalid Fwd Price " << quotes_[i] << " for " << spec_.name());
                Time t = dc_.yearFraction(asof, terms_[i]);
                Rate ir_rate = forecastYieldTermStructure->zeroRate(t, Continuous);
                dividendRates.push_back(::log(equitySpot->value() / quotes_[i]) / t + ir_rate);
            }
        } else if (buildCurveType == EquityCurveConfig::Type::DividendYield) {
            DLOG("Building Equity Dividend Yield curve from Dividend Yield rates");
            dividendRates = quotes_;
        } else if (buildCurveType == EquityCurveConfig::Type::NoDividends) {
            DLOG("Building flat Equity Dividend Yield curve as no quotes provided");
            // Return a flat curve @ 0%
            dividendYieldTermStructure = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(asof, 0.0, dc_));
            equityIndex_ =
                boost::make_shared<EquityIndex>(spec.curveConfigID(), calendar, parseCurrency(config->currency()),
                                                equitySpot, forecastYieldTermStructure, dividendYieldTermStructure);
            return;
        } else
            QL_FAIL("Invalid Equity curve configuration type for " << spec_.name());

        QL_REQUIRE(dividendRates.size() > 0, "No dividend yield rates extracted for " << spec_.name());
        QL_REQUIRE(dividendRates.size() == terms_.size(), "vector size mismatch - dividend rates ("
                                                              << dividendRates.size() << ") vs terms (" << terms_.size()
                                                              << ")");

        // store "dividend discount factors" - in case we wish to interpolate according to discounts
        vector<Real> dividendDiscountFactors;
        for (Size i = 0; i < quotes_.size(); i++) {
            Time t = dc_.yearFraction(asof, terms_[i]);
            dividendDiscountFactors.push_back(std::exp(-dividendRates[i] * t));
        }

        boost::shared_ptr<YieldTermStructure> divCurve;
        // Build Dividend Term Structure
        if (dividendRates.size() == 1) {
            // We only have 1 quote so we build a flat curve
            divCurve.reset(new FlatForward(asof, dividendRates[0], dc_));
        } else {
            // Build a ZeroCurve
            Size n = terms_.size();
            vector<Date> dates(n + 1); // n+1 so we include asof
            vector<Rate> rates(n + 1);
            vector<Real> discounts(n + 1);
            for (Size i = 0; i < n; i++) {
                dates[i + 1] = terms_[i];
                rates[i + 1] = dividendRates[i];
                discounts[i + 1] = dividendDiscountFactors[i];
            }
            dates[0] = asof;
            rates[0] = rates[1];
            discounts[0] = 1.0;
            if (forecastYieldTermStructure->maxDate() > dates.back()) {
                dates.push_back(forecastYieldTermStructure->maxDate());
                rates.push_back(rates.back());
                Time maxTime = dc_.yearFraction(asof, forecastYieldTermStructure->maxDate());
                discounts.push_back(
                    std::exp(-rates.back() * maxTime)); // flat zero extrapolation used to imply dividend DF
            }
            if (dividendInterpVariable_ == YieldCurve::InterpolationVariable::Zero) {
                divCurve = zerocurve(dates, rates, dc_, dividendInterpMethod_);
            } else if (dividendInterpVariable_ == YieldCurve::InterpolationVariable::Discount) {
                divCurve = discountcurve(dates, discounts, dc_, dividendInterpMethod_);
            } else {
                QL_FAIL("Unsupported interpolation variable for dividend yield curve");
            }
            divCurve->enableExtrapolation();
        }
        dividendYieldTermStructure = Handle<YieldTermStructure>(divCurve);

        equityIndex_ =
            boost::make_shared<EquityIndex>(spec.curveConfigID(), calendar, parseCurrency(config->currency()),
                                            equitySpot, forecastYieldTermStructure, dividendYieldTermStructure);

        // set calibration info

        calibrationInfo_ = boost::make_shared<YieldCurveCalibrationInfo>();
        calibrationInfo_->dayCounter = dc_.name();
        calibrationInfo_->currency = config->currency();
        for (auto const& p : YieldCurveCalibrationInfo::defaultPeriods) {
            Date d = asof + p;
            calibrationInfo_->pillarDates.push_back(d);
            calibrationInfo_->zeroRates.push_back(dividendYieldTermStructure->zeroRate(d, dc_, Continuous));
            calibrationInfo_->discountFactors.push_back(dividendYieldTermStructure->discount(d));
            calibrationInfo_->times.push_back(dividendYieldTermStructure->timeFromReference(d));
        }

    } catch (std::exception& e) {
        QL_FAIL("equity curve building failed: " << e.what());
    } catch (...) {
        QL_FAIL("equity curve building failed: unknown error");
    }
};

} // namespace data
} // namespace ore
