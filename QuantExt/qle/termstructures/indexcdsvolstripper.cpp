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

#include <qle/termstructures/indexcdsvolstripper.hpp>
#include <qle/pricingengines/blackindexcdsoptionengine.hpp>
#include <qle/pricingengines/numericalintegrationindexcdsoptionengine.hpp>
#include <qle/pricingengines/midpointindexcdsengine.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/claim.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <sstream>

namespace QuantExt {

using namespace QuantLib;
using std::map;
using std::string;
using std::vector;

IndexCdsVolStripper::IndexCdsVolStripper(Date referenceDate,
    Calendar calendar,
    BusinessDayConvention bdc,
    DayCounter dayCounter,
    const map<Period, Handle<CreditCurve>>& termCurves,
    map<Period, Date> termMaturities,
    QuoteCube quotes,
    TradeData tradeData,
    Solver1DOptions solverOptions)
    : referenceDate_(std::move(referenceDate)),
      calendar_(std::move(calendar)),
      bdc_(bdc),
      dayCounter_(std::move(dayCounter)),
      termMaturities_(std::move(termMaturities)),
      quotes_(std::move(quotes)),
      tradeData_(std::move(tradeData)),
      solverOptions_(std::move(solverOptions)),
      optionEngine_(tradeData_.engineOverride)
{
    // Choose the default engine here if no override was given.
    if (optionEngine_ == OptionEngine::None) {
        optionEngine_ = tradeData_.strikeType == CdsOption::Spread ? OptionEngine::Numerical : OptionEngine::Black;
    }

    terms_.reserve(quotes_.size());
    termCurves_.reserve(quotes_.size());
    for (auto& [term, surface] : quotes_) {
        // Perform checks up front to avoid performing them in the helper methods.
        QL_REQUIRE(termCurves.find(term) != termCurves.end(),
            "IndexCdsVolStripper: no credit curve provided for term " << term << ".");
        QL_REQUIRE(termMaturities_.find(term) != termMaturities_.end(),
            "IndexCdsVolStripper: no maturity for term " << term << ".");
        terms_.push_back(term);
        const auto& cch = termCurves.at(term);
        termCurves_.push_back(cch);
        populateEngineAndVols(term, cch);

        for (auto& [expiryDate, prices] : surface) {
            QL_REQUIRE(!prices.empty(), "IndexCdsVolStripper: empty vector or prices given for term " <<
                term << " and expiry " << expiryDate << ".");
            std::sort(prices.begin(), prices.end());

            // Check at least one quote and register with non-empty quotes.
            for (auto const& payRecPrice : prices) {
                QL_REQUIRE(!payRecPrice.payerPrice.empty() || !payRecPrice.receiverPrice.empty(),
                    "IndexCdsVolStripper: both payer and receiver quotes are empty for term " << term <<
                    ", expiry " << expiryDate << " and strike " << payRecPrice.strike << ".");
                if (!payRecPrice.payerPrice.empty())
                    registerWith(payRecPrice.payerPrice);
                if (!payRecPrice.receiverPrice.empty())
                    registerWith(payRecPrice.receiverPrice);
            }

            populateOptionHelpers(expiryDate, term, cch);
        }
    }
}

ext::shared_ptr<CreditVolCurve> IndexCdsVolStripper::creditVolCurve()
{
    calculate();
    return creditVolCurve_;
}

const vector<string>& IndexCdsVolStripper::errorMessages() const {
    return errorMessages_;
}

void IndexCdsVolStripper::performCalculations() const
{
    // Clear any previous error messages.
    errorMessages_.clear();

    // Determine option premium payment date.
    const auto& indexCdsData = tradeData_.indexCdsData;
    Date premiumPmtDate = indexCdsData.calendar.advance(referenceDate_, indexCdsData.cashSettlementDays,
        Days, indexCdsData.payConvention);

    // Use a copy of solverOptions_ member. We will update initial guess with last implied vol.
    Solver1DOptions solverOptions = solverOptions_;

    // Pass in the discount to premium date rather than calculating it each time below.
    DiscountFactor discToPremPmt = termCurves_.front()->rateCurve()->discount(premiumPmtDate);

    // Strip the volatilities from the prices and populate the volatility quotes for InterpolatingCreditVolCurve.
    using QuoteKey = InterpolatingCreditVolCurve::QuoteKey;
    InterpolatingCreditVolCurve::QuoteMap volQuotes;
    for (const auto& [term, surface] : quotes_) {
        const EngineVol& engineVol = termEngineVol_.at(term);
        const auto& helpersByExpiry = helpers_.at(term);
        for (const auto& [expiryDate, prices] : surface) {
            // Payer and receiver index CDS option helpers for the given expiry.
            const auto& payRecHelper = helpersByExpiry.at(expiryDate);
            // For the given expiry, find the strike to start at.
            Size startPos = findStartPos(prices);
            // Strip the volatilities from strike at startPos down (asc parameter set to false) to first strike.
            auto volStartPos = stripVols(prices, startPos, 0, volQuotes, payRecHelper, engineVol,
                solverOptions, expiryDate, term, discToPremPmt, false);
            if (startPos < prices.size() - 1) {
                // If any strikes on right of startPos strike, strip the volatilities from first such strike 
                // up (asc parameter set to true) to the last strike.
                solverOptions.initialGuess = volStartPos ? *volStartPos : solverOptions_.initialGuess;
                stripVols(prices, startPos + 1, prices.size(), volQuotes, payRecHelper, engineVol,
                    solverOptions, expiryDate, term, discToPremPmt, true);
            }
        }
    }

    // Update the credit volatility curve structure.
    using CVT = CreditVolCurve::Type;
    CVT volType = tradeData_.strikeType == CdsOption::Spread ? CVT::Spread : CVT::Price;
    creditVolCurve_ = ext::make_shared<InterpolatingCreditVolCurve>(referenceDate_, calendar_, bdc_, dayCounter_,
        terms_, termCurves_, volQuotes, volType);
}

void IndexCdsVolStripper::populateEngineAndVols(const Period& term, const Handle<CreditCurve>& cch)
{
    // Volatility value that will be solved for during stripping for each option with `term`.
    auto volPtr = ext::make_shared<SimpleQuote>(0.0);
    Handle<BlackVolTermStructure> volTs(ext::make_shared<BlackConstantVol>(0, NullCalendar(),
        Handle<Quote>(volPtr), Actual365Fixed()));
    Handle<CreditVolCurve> volCurve(ext::make_shared<CreditVolCurveWrapper>(volTs));

    // Create the engine used in the stripping.
    ext::shared_ptr<IndexCdsOptionBaseEngine> engine;
    if (optionEngine_ == OptionEngine::Black) {
        engine = ext::make_shared<QuantExt::BlackIndexCdsOptionEngine>(cch->curve(),
            cch->recovery()->value(), cch->rateCurve(), cch->rateCurve(), volCurve, false);
    } else {
        engine = ext::make_shared<QuantExt::NumericalIntegrationIndexCdsOptionEngine>(cch->curve(),
            cch->recovery()->value(), cch->rateCurve(), cch->rateCurve(), volCurve, false);
    }

    // Store engine and vol pair per term.
    termEngineVol_.try_emplace(term, EngineVol{engine, Handle<SimpleQuote>(volPtr)});
}

void IndexCdsVolStripper::populateOptionHelpers(const Date& expiryDate, const Period& term,
    const Handle<CreditCurve>& cch)
{
    const Date& underlyingMaturity = termMaturities_.at(term);
    const CreditCurve::RefData& indexCdsData = tradeData_.indexCdsData;
    Schedule schedule(expiryDate, underlyingMaturity, indexCdsData.tenor, indexCdsData.calendar,
        indexCdsData.convention, indexCdsData.termConvention, indexCdsData.rule, indexCdsData.endOfMonth);

    OptionPtr payer = createIndexCdsOption(Protection::Buyer, expiryDate, schedule, cch, term);
    OptionPtr receiver = createIndexCdsOption(Protection::Seller, expiryDate, schedule, cch, term);
    helpers_[term].emplace(expiryDate, PayRecHelper{ payer, receiver });
}

ext::shared_ptr<QuantExt::IndexCdsOption> IndexCdsVolStripper::createIndexCdsOption(Protection::Side side,
    const Date& expiryDate, const Schedule& schedule, const Handle<CreditCurve>& cch, const Period& term)
{
    // Effective index notional as of reference date i.e. the date on which we are performing the stripping.
    Real refDateNtl = tradeData_.indexFactor * notional_;

    // Create the index CDS underlying the option and set pricing engine.
    const CreditCurve::RefData& indexCdsData = tradeData_.indexCdsData;
    auto indexCds = ext::make_shared<QuantExt::IndexCreditDefaultSwap>(side, refDateNtl,
        vector<Real>{refDateNtl}, indexCdsData.runningSpread, schedule, indexCdsData.payConvention,
        indexCdsData.dayCounter, indexCdsData.settlesAccrual, indexCdsData.protPmtTime, expiryDate,
        ext::shared_ptr<FaceValueClaim>(), indexCdsData.lastPeriodDayCounter, indexCdsData.rebatesAccrual,
        expiryDate, indexCdsData.cashSettlementDays);

    indexCds->setPricingEngine(ext::make_shared<QuantExt::MidPointIndexCdsEngine>(
        cch->curve(), cch->recovery()->value(), cch->rateCurve()));

    // In the index CDS option below we start with the (upper) middle strike in the vector of striked prices.
    const auto& strikedPrices = quotes_.at(term).at(expiryDate);
    auto pos = strikedPrices.size() / 2;
    Real strike = strikedPrices[pos].strike;

    // Create the index CDS option.
    Real refDateRealisedFep = tradeData_.realisedFepFactor * notional_;
    Real ntlForStrike = tradeData_.indexFactorStrike * notional_;
    auto exercise = ext::make_shared<EuropeanExercise>(expiryDate);
    auto indexCdsOption = ext::make_shared<QuantExt::IndexCdsOption>(indexCds, exercise, strike,
        tradeData_.strikeType, Settlement::Cash, ntlForStrike, refDateRealisedFep, term);

    indexCdsOption->setPricingEngine(termEngineVol_.at(term).engine);

    return indexCdsOption;
}

Size IndexCdsVolStripper::findStartPos(const vector<OptionPrice>& optionPrices) const
{
    // The default result, the (upper) middle strike.
    Size pos = optionPrices.size() / 2;

    // If there are three or fewer strikes, not worth using the logic below, so return the default.
    if (optionPrices.size() <= 3)
        return pos;

    // Check if we have both payer and receiver quotes at all strikes. If not, use the default.
    auto hasBothPrices = [](const OptionPrice& op) {
        return !op.payerPrice.empty() && !op.receiverPrice.empty();
    };

    if (!std::ranges::all_of(optionPrices, hasBothPrices))
        return pos;

    // Check that the option premia are sensible before attempting the logic below.
    // If not, return the default. Argument for throwing an exception here also if these checks fail.
    bool strikeIsSpread = tradeData_.strikeType == CdsOption::Spread;
    const OptionPrice& firstOpt = optionPrices.front();
    bool firstOptOk = strikeIsSpread ? firstOpt.payerPrice->value() > firstOpt.receiverPrice->value() :
        firstOpt.payerPrice->value() < firstOpt.receiverPrice->value();
    if (!firstOptOk)
        return pos;

    // Payer prices should be montonically increasing (decreasing) with strike for price (spread) based strikes.
    // badPayer should therefore return `optionPrices.end()`.
    // Note: logic allows p_i == p_{i+1} i.e. not strict monotonicity.
    auto badPayer = std::ranges::adjacent_find(optionPrices, [strikeIsSpread](const auto& a, const auto& b) {
        return strikeIsSpread ? a.payerPrice->value() < b.payerPrice->value() :
            a.payerPrice->value() > b.payerPrice->value();
    });
    // Receiver prices should be montonically decreasing (increasing) with strike for price (spread) based strikes.
    // badReceiver should therefore return `optionPrices.end()`.
    auto badReceiver = std::ranges::adjacent_find(optionPrices, [strikeIsSpread](const auto& a, const auto& b) {
        return strikeIsSpread ? a.receiverPrice->value() > b.receiverPrice->value() :
            a.receiverPrice->value() < b.receiverPrice->value();
    });
    // If either payer or receiver prices violate monotonicity, return the default position.
    if (badPayer != optionPrices.end() || badReceiver != optionPrices.end())
        return pos;

    // Find the first strike where payer (receiver) price exceeds receiver (payer) price for price (spread) strikes.
    auto crossover = std::ranges::find_if(optionPrices, [strikeIsSpread](const OptionPrice& op) {
        Real payer = op.payerPrice->value();
        Real receiver = op.receiverPrice->value();
        return strikeIsSpread ? (payer < receiver) : (payer > receiver);
    });

    return crossover == optionPrices.end() ? pos : std::distance(optionPrices.begin(), crossover);
}

ext::optional<Volatility> IndexCdsVolStripper::stripVols(const vector<OptionPrice>& prices, Size startPos,
    Size endPos, VolQuotesMap& volQuotes, const PayRecHelper& payRecHelper, const EngineVol& engineVol,
    Solver1DOptions& solverOptions, const Date& expiryDate, const Period& term, DiscountFactor discToPremPmt,
    bool asc) const
{
    using QuoteKey = InterpolatingCreditVolCurve::QuoteKey;
    using ImpVolRes = IndexCdsOption::ImpliedVolatilityResult;

    // Small helper indicating whether to use payer or receiver.
    // If both not empty, use the one with smaller price (out of the money). Otherwise, use the non-empty one.
    // Note: we checked in the constructor that at least one of them is non-empty.
    auto usePayerOpt = [](const OptionPrice& op) {
        if (!op.payerPrice.empty() && !op.receiverPrice.empty()) {
            return op.payerPrice->value() < op.receiverPrice->value();
        } else {
            return !op.payerPrice.empty();
        }
    };

    // If the premia are BpsPerOutstandingNtl, we need to scale the premium value.
    Real scale = tradeData_.quoteDimension == QuoteDimension::BpsPerOptionNtl ? 1.0 : tradeData_.indexFactorStrike;

    // We want to discount the premium from cash settlement date back to reference date before using as target value.
    auto discPremium = [discToPremPmt, scale](Real premium) {
        return scale * premium * discToPremPmt;
    };

    // Flag that determines if we move in direction of increasing (`asc` true) or decreasing (`asc` false) strike.
    ptrdiff_t step = asc ? 1 : -1;

    // Engine and associated volatility handle used in the implied volatility calculation below.
    const EnginePtr& engine = engineVol.engine;
    const VolHandle& vol = engineVol.vol;

    // Store the first implied volatility - we can use it as initial guess when going in opposite strike direction.
    ext::optional<Volatility> firstVol;

    ptrdiff_t ePos = static_cast<ptrdiff_t>(endPos);
    for (ptrdiff_t i = static_cast<ptrdiff_t>(startPos); asc ? (i < ePos) : (i >= ePos); i += step) {
        const OptionPrice& op = prices[i];
        bool usePayer = usePayerOpt(op);
        Real targetPrice = usePayer ? op.payerPrice->value() : op.receiverPrice->value();

        // If the target price is 0, we use the previous solved volatility, which is in initial guess.
        if (close(targetPrice, 0.0)) {
            Handle<Quote> hvq(ext::make_shared<SimpleQuote>(solverOptions.initialGuess));
            volQuotes.try_emplace(QuoteKey{ expiryDate, term, op.strike }, hvq);
            continue;
        }

        Real targetPricePv = discPremium(targetPrice * notional_);
        const auto& helper = usePayer ? payRecHelper.payerHelper : payRecHelper.receiverHelper;
        helper->setStrike(op.strike);
        ImpVolRes res = helper->impliedVolatility(targetPricePv, engine, vol, solverOptions);
        if (!res.success) {
            std::ostringstream oss;
            oss << "Failed to imply vol for (expiry, term, strike) triplet (" << io::iso_date(expiryDate) << ", " <<
                term << ", " << op.strike << ") with error message: " << res.errorMessage << ". Final volatility is " <<
                res.volatility << " for target premium of " << targetPricePv << " with premium error of " <<
                res.error << ".";
            errorMessages_.push_back(oss.str());
        }

        solverOptions.initialGuess = res.volatility;
        Handle<Quote> hvq(ext::make_shared<SimpleQuote>(res.volatility));
        volQuotes.try_emplace(QuoteKey{ expiryDate, term, op.strike }, hvq);

        if (!firstVol && res.success)
            firstVol = res.volatility;
    }

    return firstVol;
}

} // namespace QuantExt
