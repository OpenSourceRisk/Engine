/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

#include <qle/termstructures/bondfuturevolstripper.hpp>
#include <qle/pricingengines/analyticeuropeanengine.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>
#include <ql/exercise.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <sstream>

namespace QuantExt {

using namespace QuantLib;
using std::map;
using std::string;
using std::vector;

BondFutureVolStripper::BondFutureVolStripper(Date referenceDate,
    Calendar calendar,
    BusinessDayConvention bdc,
    DayCounter dayCounter,
    Handle<Quote> futurePrice,
    Handle<YieldTermStructure> discountCurve,
    QuoteSurface quotes,
    Solver1DOptions solverOptions,
    Exercise::Type type,
    bool lowerStrikeConstExtrap,
    bool upperStrikeConstExtrap,
    TimeExtrapType timeExtrapolationType,
    bool preferOutOfTheMoney)
    : referenceDate_(std::move(referenceDate)),
      calendar_(std::move(calendar)),
      bdc_(bdc),
      dayCounter_(std::move(dayCounter)),
      type_(type),
      lowerStrikeConstExtrap_(lowerStrikeConstExtrap),
      upperStrikeConstExtrap_(upperStrikeConstExtrap),
      timeExtrapolationType_(timeExtrapolationType),
      futurePrice_(std::move(futurePrice)),
      discountCurve_(std::move(discountCurve)),
      quotes_(std::move(quotes)),
      solverOptions_(std::move(solverOptions)),
      preferOutOfTheMoney_(preferOutOfTheMoney) {

    // This will be relaxed in future.
    QL_REQUIRE(type_ == Exercise::European, "BondFutureVolStripper: only European exercise is supported for now.");

    QL_REQUIRE(futurePrice_->isValid(), "BondFutureVolStripper: needs a valid future price quote.");
    QL_REQUIRE(!discountCurve_.empty(), "BondFutureVolStripper: discount curve is empty");

    // Check that there is at least one quote at each expiry and strike and register with non-empty quotes.
    for (auto& [expiryDate, prices] : quotes_) {
        QL_REQUIRE(!prices.empty(), "BondFutureVolStripper: empty vector or prices given for expiry date " <<
            expiryDate << ".");
        std::sort(prices.begin(), prices.end());

        for (auto const& callPutPrice : prices) {
            QL_REQUIRE(!callPutPrice.callPrice.empty() || !callPutPrice.putPrice.empty(),
                "BondFutureVolStripper: both call and put quotes are empty for expiry date " <<
                expiryDate << " and strike " << callPutPrice.strike << ".");
            if (!callPutPrice.callPrice.empty())
                registerWith(callPutPrice.callPrice);
            if (!callPutPrice.putPrice.empty())
                registerWith(callPutPrice.putPrice);
        }
    }

    // Register with the future price and discount curve as well.
    registerWith(futurePrice_);
    registerWith(discountCurve_);

    setUpSolver();
}

BondFutureVolStripper::PriceError::PriceError(const VanillaOption& option, SimpleQuote& volatility, Real targetPrice)
    : option_(option), volatility_(volatility), targetPrice_(targetPrice) {
}

Real BondFutureVolStripper::PriceError::PriceError::operator()(Volatility x) const {
    volatility_.setValue(x);
    // If we update and use Barone Adesi Whaley for American options, we need to wrap in a try catch as it fails 
    // for very small variance.
    Real npv = option_.NPV();
    return npv - targetPrice_;
}

ext::shared_ptr<BlackVolTermStructure> BondFutureVolStripper::volSurface()
{
    calculate();
    return volSurface_;
}

const vector<string>& BondFutureVolStripper::errorMessages() const {
    return errorMessages_;
}

void BondFutureVolStripper::performCalculations() const {

    // Clear any previous error messages.
    errorMessages_.clear();

    // Data that needs to be populated to create the BlackVarianceSurfaceSparse below.
    vector<Date> expiries;
    vector<Real> strikes;
    vector<Volatility> vols;

    // Create the process and engine used by the instruments in the stripping.
    ext::shared_ptr<SimpleQuote> volQuote = ext::make_shared<SimpleQuote>(0.1);
    auto volPtr = QuantLib::ext::make_shared<QuantLib::BlackConstantVol>(
        referenceDate_, calendar_, Handle<Quote>(volQuote), dayCounter_);
    auto vol = QuantLib::Handle<QuantLib::BlackVolTermStructure>(volPtr);
    ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp = ext::make_shared<QuantLib::BlackProcess>(
        futurePrice_, discountCurve_, vol);
    ext::shared_ptr<PricingEngine> engine = ext::make_shared<QuantExt::AnalyticEuropeanEngine>(gbsp);

    // Strip the volatilities from the prices and populate the expiries, strikes, and vols.
    for (const auto& [expiryDate, prices] : quotes_) {
        // For the given expiry, find the strike to start at i.e. first strike greater than ATM, current future price.
        Size startPos = findStartPos(prices);
        // Create the exercise.
        ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(expiryDate);
        // Strip the volatilities from strike at startPos down (asc parameter set to false) to first strike.
        ext::optional<Volatility> volStartPos;
        if (startPos > 0) {
            volStartPos = stripVols(prices, startPos, 0, expiries, strikes, vols, solverOptions_.initialGuess,
                exercise, engine, *volQuote, false);
        }
        // If any strikes on right of startPos strike, strip the volatilities from first such strike 
        // up (asc parameter set to true) to the last strike.
        if (startPos < prices.size() - 1) {
            Real initialGuess = volStartPos ? *volStartPos : solverOptions_.initialGuess;
            stripVols(prices, startPos + 1, prices.size(), expiries, strikes, vols, initialGuess, exercise,
                engine, *volQuote, true);
        }
    }

    // Populate the variance surface.
    volSurface_ = ext::make_shared<BlackVarianceSurfaceSparse<>>(referenceDate_, calendar_, expiries, strikes, vols,
        dayCounter_, lowerStrikeConstExtrap_, upperStrikeConstExtrap_, timeExtrapolationType_);
}

void BondFutureVolStripper::setUpSolver() {

    bool useMinMax;
    std::tie(brent_, useMinMax) = createSolver1D<Brent>(solverOptions_);

    Real accuracy = solverOptions_.accuracy;
    using std::placeholders::_1;
    using std::placeholders::_2;
    if (useMinMax) {
        auto [min, max] = solverOptions_.minMax;
        typedef Real(Brent::* MinMaxSolver)(const PriceError&, Real, Real, Real, Real) const;
        solver_ = std::bind(static_cast<MinMaxSolver>(&Brent::solve), &brent_, _1, accuracy, _2, min, max);
    } else {
        Real step = solverOptions_.step;
        typedef Real(Brent::* StepSolver)(const PriceError&, Real, Real, Real) const;
        solver_ = std::bind(static_cast<StepSolver>(&Brent::solve), &brent_, _1, accuracy, _2, step);
    }
}

Size BondFutureVolStripper::findStartPos(const vector<OptionPrice>& optionPrices) const {
    // Find first strike greater than future price (if one exists).
    auto itFirstGreater = std::upper_bound(optionPrices.begin(), optionPrices.end(), futurePrice_->value(),
        [](Real value, const OptionPrice& op) { return value < op.strike; });
    return itFirstGreater == optionPrices.end() ? 0 : std::distance(optionPrices.begin(), itFirstGreater);
}

ext::optional<Volatility> BondFutureVolStripper::stripVols(const vector<OptionPrice>& prices, Size startPos,
    Size endPos, vector<Date>& expiries, vector<Real>& strikes, vector<Volatility>& vols,
    Real initialGuess, const ext::shared_ptr<Exercise>& exercise, const ext::shared_ptr<PricingEngine>& engine,
    SimpleQuote& volQuote, bool asc) const {

    // Small helper indicating whether to use call or put option.
    // Note: we checked in the constructor that at least one of them is non-empty.
    auto useCallOpt = [this](const OptionPrice& op) {
        if (!op.callPrice.empty() && !op.putPrice.empty()) {
            if (this->preferOutOfTheMoney_)
                return op.strike > this->futurePrice_->value();
            else
                return op.strike < this->futurePrice_->value();
        } else {
            return !op.callPrice.empty();
        }
    };

    // Flag that determines if we move in direction of increasing (`asc` true) or decreasing (`asc` false) strike.
    ptrdiff_t step = asc ? 1 : -1;

    // Store the first implied volatility - we can use it as initial guess when going in opposite strike direction.
    ext::optional<Volatility> firstVol;

    // Store expiry date. Adding it below in the loop.
    Date expiryDate = exercise->lastDate();

    ptrdiff_t ePos = static_cast<ptrdiff_t>(endPos);
    for (ptrdiff_t i = static_cast<ptrdiff_t>(startPos); asc ? (i < ePos) : (i >= ePos); i += step) {

        const OptionPrice& op = prices[i];
        bool useCall = useCallOpt(op);
        Real targetPrice = useCall ? op.callPrice->value() : op.putPrice->value();
        Option::Type optType = useCall ? Option::Call : Option::Put;

        // Create the option instrument.
        ext::shared_ptr<StrikedTypePayoff> payoff = ext::make_shared<PlainVanillaPayoff>(optType, op.strike);
        VanillaOption option(payoff, exercise);
        option.setPricingEngine(engine);

        // Attempt to calculate the implied volatility.
        PriceError f(option, volQuote, targetPrice);
        bool success = true;
        Real vol;
        try {
            vol = solver_(f, initialGuess);
        } catch (const Error& e) {
            success = false;
            vol = volQuote.value();
            std::ostringstream oss;
            oss << "Failed to imply vol for (expiry, strike) = (" << io::iso_date(expiryDate) << ", " <<
                op.strike << ") with error message: " << e.what() << ". Final volatility is " <<
                vol << " for target premium of " << targetPrice << " with premium error of " << f(vol) << ".";
            errorMessages_.push_back(oss.str());
        }

        // Add to results.
        expiries.push_back(expiryDate);
        strikes.push_back(op.strike);
        vols.push_back(vol);

        // Update initial guess for next iteration and the starting volatility.
        initialGuess = vol;
        if (!firstVol && success)
            firstVol = vol;
    }

    return firstVol;
}

} // namespace QuantExt
