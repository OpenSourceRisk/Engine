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
#include <qle/termstructures/flatforwarddividendcurve.hpp>
#include <qle/termstructures/optionpricesurface.hpp>

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/wildcard.hpp>

#include <algorithm>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

EquityCurve::EquityCurve(Date asof, EquityCurveSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs,
                         const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& requiredYieldCurves,
                         const bool buildCalibrationInfo) {

    try {

        const QuantLib::ext::shared_ptr<EquityCurveConfig>& config = curveConfigs.equityCurveConfig(spec.curveConfigID());

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

        // declare spot and yields
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
        QuantLib::ext::shared_ptr<YieldCurve> yieldCurve = itr->second;
        forecastYieldTermStructure = yieldCurve->handle();

        // Set the interpolation variables
        dividendInterpVariable_ = parseYieldCurveInterpolationVariable(config->dividendInterpolationVariable());
        dividendInterpMethod_ = parseYieldCurveInterpolationMethod(config->dividendInterpolationMethod());

        // We loop over all market data, looking for quotes that match the configuration
        // until we found the whole set of quotes or do not have more quotes in the
        // market data

        vector<QuantLib::ext::shared_ptr<EquityForwardQuote>> qt; // for sorting quotes_/terms_ pairs
        vector<QuantLib::ext::shared_ptr<EquityOptionQuote>> oqt; // store any equity vol quotes
        Size quotesRead = 0;
        Size quotesExpired = 0;
        // in case of wild-card in config
        auto wildcard = getUniqueWildcard(config->fwdQuotes());

        if ((config->type() == EquityCurveConfig::Type::ForwardPrice ||
             config->type() == EquityCurveConfig::Type::ForwardDividendPrice ||
             config->type() == EquityCurveConfig::Type::OptionPremium) &&
            wildcard) {
            DLOG("Wild card quote specified for " << config->curveID())
        } else {
            if (config->type() == EquityCurveConfig::Type::OptionPremium) {
                oqt.reserve(config->fwdQuotes().size());
            } else {
                quotes_.reserve(config->fwdQuotes().size());
                terms_.reserve(config->fwdQuotes().size());
            }
        }

        // load the quotes

        auto q = QuantLib::ext::dynamic_pointer_cast<EquitySpotQuote>(loader.get(config->equitySpotQuoteID(), asof));
        QL_REQUIRE(q != nullptr, "expected '" << config->equitySpotQuoteID() << "' to be an EquitySpotQuote");
        Real spot = convertMinorToMajorCurrency(q->ccy(), q->quote()->value());
        // convert quote from minor to major currency if needed
        equitySpot = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(spot));

        if ((config->type() == EquityCurveConfig::Type::ForwardPrice ||
             config->type() == EquityCurveConfig::Type::ForwardDividendPrice)) {
            if (wildcard) {
                for (auto const& md : loader.get(*wildcard, asof)) {
                    auto q = QuantLib::ext::dynamic_pointer_cast<EquityForwardQuote>(md);
                    QL_REQUIRE(q != nullptr, "expected '" << md->name() << "' to be an EquityForwardQuote");
                    QL_REQUIRE(find(qt.begin(), qt.end(), q) == qt.end(),
                               "duplicate market datum found for " << q->name());
                    if (asof < q->expiryDate()) {
                        DLOG("EquityCurve Forward Price found for quote: " << q->name());
                        qt.push_back(q); // terms_ and quotes_
                        quotesRead++;
                    } else {
                        ++quotesExpired;
                        DLOG("Ignore expired ForwardPrice/ForwardDividendPrice quote "
                             << q->name() << ", expired at " << io::iso_date(q->expiryDate()));
                    }
                }
            } else {
                for (auto const& md :
                     loader.get(std::set<std::string>(config->fwdQuotes().begin(), config->fwdQuotes().end()), asof)) {
                    auto q = QuantLib::ext::dynamic_pointer_cast<EquityForwardQuote>(md);
                    QL_REQUIRE(q != nullptr, "expected '" << md->name() << "' to be an EquityForwardQuote");
                    if (asof < q->expiryDate()) {
                        bool isUnique = find(terms_.begin(), terms_.end(), q->expiryDate()) == terms_.end();
                        QL_REQUIRE(isUnique, "duplicate market datum found for " << q->name());
                        terms_.push_back(q->expiryDate());
                        // convert quote from minor to major currency if needed
                        quotes_.push_back(convertMinorToMajorCurrency(q->ccy(), q->quote()->value()));
                        quotesRead++;
                    } else {
                        DLOG("Ignore expired ForwardPrice/ForwardDividendPrice quote "
                             << q->name() << ", expired at " << io::iso_date(q->expiryDate()));
                        ++quotesExpired;
                    }
                }
            }
        }

        if (config->type() == EquityCurveConfig::Type::OptionPremium) {
            if (wildcard) {
                for (auto const& md : loader.get(*wildcard, asof)) {
                    auto q = QuantLib::ext::dynamic_pointer_cast<EquityOptionQuote>(md);
                    QL_REQUIRE(q != nullptr, "expected '" << md->name() << "' to be an EquityOptionQuote");
                    QL_REQUIRE(find(oqt.begin(), oqt.end(), q) == oqt.end(),
                               "duplicate market datum found for " << q->name());
                    if (asof < parseDate(q->expiry())) {
                        DLOG("EquityCurve Volatility Price found for quote: " << q->name());
                        oqt.push_back(q);
                        quotesRead++;
                    } else {
                        ++quotesExpired;
                        DLOG("Ignore expired OptionPremium  quote " << q->name() << ", expired at " << q->expiry());
                    }
                }
            } else {
                for (auto const& md :
                     loader.get(std::set<std::string>(config->fwdQuotes().begin(), config->fwdQuotes().end()), asof)) {
                    auto q = QuantLib::ext::dynamic_pointer_cast<EquityOptionQuote>(md);
                    QL_REQUIRE(q != nullptr, "expected '" << md->name() << "' to be an EquityOptionQuote");
                    if (asof < parseDate(q->expiry())) {
                        DLOG("EquityCurve Volatility Price found for quote: " << q->name());
                        oqt.push_back(q);
                        quotesRead++;
                    } else {
                        ++quotesExpired;
                        DLOG("Ignore expired OptionPremium  quote " << q->name() << ", expired at " << q->expiry());
                    }
                }
            }
        }

        if (config->type() == EquityCurveConfig::Type::DividendYield) {
            for (auto const& md :
                 loader.get(std::set<std::string>(config->fwdQuotes().begin(), config->fwdQuotes().end()), asof)) {
                QuantLib::ext::shared_ptr<EquityDividendYieldQuote> q =
                    QuantLib::ext::dynamic_pointer_cast<EquityDividendYieldQuote>(md);
                QL_REQUIRE(q != nullptr, "expected '" << md->name() << "' to be an EquityDividendYieldQuote");
                if (q->tenorDate() > asof) {
                    bool isUnique = find(terms_.begin(), terms_.end(), q->tenorDate()) == terms_.end();
                    QL_REQUIRE(isUnique, "duplicate market datum found for " << q->name());
                    DLOG("EquityCurve Dividend Yield found for quote: " << q->name());
                    terms_.push_back(q->tenorDate());
                    quotes_.push_back(q->quote()->value());
                    quotesRead++;
                } else {
                    DLOG("Ignore expired DividendYield quote " << q->name() << ", expired at "
                                                               << io::iso_date(q->tenorDate()));
                    ++quotesExpired;
                }
            }
        }

        // some checks on the quotes read
        DLOG("EquityCurve: read " << quotesRead + quotesExpired << " quotes of type " << config->type());
        DLOG("EquityCurve: ignored " << quotesExpired << " expired quotes.");

        if (!wildcard) {
            QL_REQUIRE(quotesRead + quotesExpired == config->fwdQuotes().size(),
                       "read " << quotesRead << "quotes and " << quotesExpired << " expire quotes, but "
                               << config->fwdQuotes().size() << " required.");
        }

        // Sort the quotes and terms by date

        QL_REQUIRE(terms_.size() == quotes_.size(), "Internal error: terms and quotes mismatch");
        if (!terms_.empty()) {
            vector<pair<Date, Real>> tmpSort;

            std::transform(terms_.begin(), terms_.end(), quotes_.begin(), back_inserter(tmpSort),
                           [](const Date& d, const Real& q) { return make_pair(d, q); });

            std::sort(tmpSort.begin(), tmpSort.end(), [](const pair<Date, Real>& left, const pair<Date, Real>& right) {
                return left.first < right.first;
            });

            for (Size i = 0; i < tmpSort.size(); ++i) {
                terms_[i] = tmpSort[i].first;
                quotes_[i] = tmpSort[i].second;
            }

            for (Size i = 0; i < terms_.size(); i++) {
                QL_REQUIRE(terms_[i] > asof, "Invalid Fwd Expiry " << terms_[i] << " vs. " << asof);
                if (i > 0) {
                    QL_REQUIRE(terms_[i] > terms_[i - 1], "terms must be increasing in curve config");
                }
            }
        }

        // the curve type that we will build
        EquityCurveConfig::Type buildCurveType = curveType_;

        // for curveType ForwardPrice or OptionPremium populate the terms_ and quotes_ with forward prices
        if (curveType_ == EquityCurveConfig::Type::ForwardPrice ||
            curveType_ == EquityCurveConfig::Type::ForwardDividendPrice) {

            if (qt.size() > 0) {
                DLOG("Building Equity Dividend Yield curve from Forward/Future prices");

                // sort quotes and terms in case of wild-card
                if (wildcard) {
                    QL_REQUIRE(quotesRead > 0, "Wild card quote specified, but no quotes read.");

                    // sort
                    std::sort(qt.begin(), qt.end(),
                              [](const QuantLib::ext::shared_ptr<EquityForwardQuote>& a,
                                 const QuantLib::ext::shared_ptr<EquityForwardQuote>& b) -> bool {
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
                DLOG("No Equity Forward quotes provided for " << config->curveID()
                                                              << ", continuing without dividend curve.");
                buildCurveType = EquityCurveConfig::Type::NoDividends;
            }
        } else if (curveType_ == EquityCurveConfig::Type::OptionPremium) {

            if (oqt.size() == 0) {
                DLOG("No Equity Option quotes provided for " << config->curveID()
                                                             << ", continuing without dividend curve.");
                buildCurveType = EquityCurveConfig::Type::NoDividends;
            } else {
                DLOG("Building Equity Dividend Yield curve from Option Volatilities");
                vector<QuantLib::ext::shared_ptr<EquityOptionQuote>> calls, puts;
                vector<Date> callDates, putDates;
                vector<Real> callStrikes, putStrikes;
                vector<Real> callPremiums, putPremiums;
                Calendar cal = NullCalendar();

                // Split the quotes into call and puts
                for (auto q : oqt) {
                    if (q->quote()->value() > 0) {
                        if (q->isCall()) {
                            calls.push_back(q);
                        } else {
                            puts.push_back(q);
                        }
                    }
                }

                // loop through the quotes and get the expiries, strikes, vols and types
                // We only want overlapping expiry/strike pairs
                for (Size i = 0; i < calls.size(); i++) {
                    for (Size j = 0; j < puts.size(); j++) {
                        if (calls[i]->expiry() == puts[j]->expiry()) {
                            auto callAbsoluteStrike = QuantLib::ext::dynamic_pointer_cast<AbsoluteStrike>(calls[i]->strike());
                            auto putAbsoluteStrike = QuantLib::ext::dynamic_pointer_cast<AbsoluteStrike>(puts[j]->strike());
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

                    DLOG("Building a Sparse Volatility surface for calls and puts");
                    // Build a Black Variance Sparse matrix
                    QuantLib::ext::shared_ptr<OptionPriceSurface> callSurface =
                        QuantLib::ext::make_shared<OptionPriceSurface>(asof, callDates, callStrikes, callPremiums, dc_);
                    QuantLib::ext::shared_ptr<OptionPriceSurface> putSurface =
                        QuantLib::ext::make_shared<OptionPriceSurface>(asof, putDates, putStrikes, putPremiums, dc_);
                    DLOG("CallSurface contains " << callSurface->expiries().size() << " expiries.");

                    DLOG("Stripping equity forwards from the option premium surfaces");
                    QuantLib::ext::shared_ptr<EquityForwardCurveStripper> efcs = QuantLib::ext::make_shared<EquityForwardCurveStripper>(
                        callSurface, putSurface, forecastYieldTermStructure, equitySpot, config->exerciseStyle());

                    // set terms and quotes from the stripper
                    terms_ = efcs->expiries();
                    quotes_ = efcs->forwards();
                } else {
                    DLOG("No overlapping call and put quotes for equity " << spec.curveConfigID()
                                                                          << " building NoDividends curve");
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
                QL_REQUIRE(quotes_[i] > 0, "Invalid Forward Price " << quotes_[i] << " for " << spec_.name()
                                                                    << ", expiry: " << terms_[i]);
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
            dividendYieldTermStructure = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(asof, 0.0, dc_));
            equityIndex_ =
                QuantLib::ext::make_shared<EquityIndex2>(spec.curveConfigID(), calendar, parseCurrency(config->currency()),
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

        QuantLib::ext::shared_ptr<YieldTermStructure> baseDivCurve;
        // Build Dividend Term Structure
        if (dividendRates.size() == 1) {
            // We only have 1 quote so we build a flat curve
            baseDivCurve.reset(new FlatForward(asof, dividendRates[0], dc_));
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
                baseDivCurve = zerocurve(dates, rates, dc_, dividendInterpMethod_);
            } else if (dividendInterpVariable_ == YieldCurve::InterpolationVariable::Discount) {
                baseDivCurve = discountcurve(dates, discounts, dc_, dividendInterpMethod_);
            } else {
                QL_FAIL("Unsupported interpolation variable for dividend yield curve");
            }
        }
        
        QuantLib::ext::shared_ptr<YieldTermStructure> divCurve;
        if (config->extrapolation()) {
            divCurve = baseDivCurve;
            divCurve->enableExtrapolation();
        } else {
            divCurve = QuantLib::ext::make_shared<FlatForwardDividendCurve>(
                asof, Handle<YieldTermStructure>(baseDivCurve), forecastYieldTermStructure);
            if (config->dividendExtrapolation())
                divCurve->enableExtrapolation();
        }

        dividendYieldTermStructure = Handle<YieldTermStructure>(divCurve);

        equityIndex_ =
            QuantLib::ext::make_shared<EquityIndex2>(spec.curveConfigID(), calendar, parseCurrency(config->currency()),
                                            equitySpot, forecastYieldTermStructure, dividendYieldTermStructure);

        if (buildCalibrationInfo) {

            // set calibration info

            calibrationInfo_ = QuantLib::ext::make_shared<YieldCurveCalibrationInfo>();
            calibrationInfo_->dayCounter = dc_.name();
            calibrationInfo_->currency = config->currency();
            for (auto const& p : YieldCurveCalibrationInfo::defaultPeriods) {
                Date d = asof + p;
                calibrationInfo_->pillarDates.push_back(d);
                calibrationInfo_->zeroRates.push_back(dividendYieldTermStructure->zeroRate(d, dc_, Continuous));
                calibrationInfo_->discountFactors.push_back(dividendYieldTermStructure->discount(d));
                calibrationInfo_->times.push_back(dividendYieldTermStructure->timeFromReference(d));
            }
        }

    } catch (std::exception& e) {
        QL_FAIL("equity curve building failed: " << e.what());
    } catch (...) {
        QL_FAIL("equity curve building failed: unknown error");
    }
};

} // namespace data
} // namespace ore
