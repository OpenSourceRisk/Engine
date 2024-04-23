/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <qle/termstructures/creditvolcurve.hpp>

#include <ql/instruments/creditdefaultswap.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <qle/utilities/interpolation.hpp>
#include <qle/utilities/time.hpp>

namespace QuantExt {

using namespace QuantLib;

CreditVolCurve::CreditVolCurve(BusinessDayConvention bdc, const DayCounter& dc, const std::vector<Period>& terms,
                               const std::vector<Handle<CreditCurve>>& termCurves, const Type& type)
    : VolatilityTermStructure(bdc, dc), terms_(terms), termCurves_(termCurves), type_(type) {
    init();
}

CreditVolCurve::CreditVolCurve(const Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc,
                               const DayCounter& dc, const std::vector<Period>& terms,
                               const std::vector<Handle<CreditCurve>>& termCurves, const Type& type)
    : VolatilityTermStructure(settlementDays, cal, bdc, dc), terms_(terms), termCurves_(termCurves), type_(type) {
    init();
}

CreditVolCurve::CreditVolCurve(const Date& referenceDate, const Calendar& cal, BusinessDayConvention bdc,
                               const DayCounter& dc, const std::vector<Period>& terms,
                               const std::vector<Handle<CreditCurve>>& termCurves, const Type& type)
    : VolatilityTermStructure(referenceDate, cal, bdc, dc), terms_(terms), termCurves_(termCurves), type_(type) {
    init();
}

void CreditVolCurve::init() {
    QL_REQUIRE(terms_.size() == termCurves_.size(), "CreditVolCurve: terms size (" << terms_.size()
                                                                                   << ") must match termCurves size ("
                                                                                   << termCurves_.size());
    // sort terms and curves

    std::vector<Size> p(terms_.size());
    std::iota(p.begin(), p.end(), 0);
    std::sort(p.begin(), p.end(), [this](Size i, Size j) { return this->terms_[i] < this->terms_[j]; });

    std::vector<Period> sortedTerms(terms_.size());
    std::vector<Handle<CreditCurve>> sortedCurves(terms_.size());
    std::transform(p.begin(), p.end(), sortedTerms.begin(), [this](Size i) { return this->terms_[i]; });
    std::transform(p.begin(), p.end(), sortedCurves.begin(), [this](Size i) { return this->termCurves_[i]; });

    terms_ = sortedTerms;
    termCurves_ = sortedCurves;

    // register with curves

    for (const auto& c : termCurves_)
        registerWith(c);
}

Real CreditVolCurve::volatility(const Date& exerciseDate, const Period& underlyingTerm, const Real strike,
                                const Type& targetType) const {
    return volatility(exerciseDate, periodToTime(underlyingTerm), strike, targetType);
}

Real CreditVolCurve::volatility(const Real exerciseTime, const Real underlyingLength, const Real strike,
                                const CreditVolCurve::Type& targetType) const {
    Date d = lowerDate(exerciseTime, referenceDate(), dayCounter());
    Real t1 = timeFromReference(d);
    Real t2 = timeFromReference(d + 1);
    Real alpha = (t2 - exerciseTime) / (t2 - t1);
    Real v1 = volatility(d, underlyingLength, strike, targetType);
    if (close_enough(alpha, 1.0))
        return v1;
    return alpha * v1 + (1.0 - alpha) * volatility(d + 1, underlyingLength, strike, targetType);
}

const std::vector<Period>& CreditVolCurve::terms() const { return terms_; }

const std::vector<Handle<CreditCurve>>& CreditVolCurve::termCurves() const { return termCurves_; }

const CreditVolCurve::Type& CreditVolCurve::type() const { return type_; }

Date CreditVolCurve::maxDate() const { return Date::maxDate(); }

Real CreditVolCurve::minStrike() const { return -QL_MAX_REAL; }

Real CreditVolCurve::maxStrike() const { return QL_MAX_REAL; }

Real CreditVolCurve::moneyness(const Real strike, const Real atmStrike) const {
    if (strike == Null<Real>())
        return 0.0;
    if (type() == Type::Spread) {
        return strike - atmStrike;
    } else if (type() == Type::Price) {
        return std::log(strike / atmStrike);
    } else {
        QL_FAIL("InterpolatingCreditVolCurve::moneyness(): internal error, type not handled");
    }
}

Real CreditVolCurve::strike(const Real moneyness, const Real atmStrike) const {
    if (type() == Type::Spread) {
        return atmStrike + moneyness;
    } else if (type() == Type::Price) {
        return atmStrike * std::exp(moneyness);
    } else {
        QL_FAIL("InterpolatingCreditVolCurve::strike(): internal error, type not handled");
    }
}

Real CreditVolCurve::atmStrike(const Date& expiry, const Period& term) const {
    return atmStrike(expiry, periodToTime(term));
}

Real CreditVolCurve::atmStrike(const Date& expiry, const Real underlyingLength) const {

    // do we have the desired value in the cache?

    calculate();
    auto v = atmStrikeCache_.find(std::make_pair(expiry, underlyingLength));
    if (v != atmStrikeCache_.end())
        return v->second;

    /* we need at least one term curve to compute the atm strike properly - we set it to 0 / 1 (spread, price) if
       we don't have a term, so that we can build strike-independent curves without curves. It is the user's
       responsibility to provide terms for strike-dependent curves. */
    if (terms().empty())
        return type() == Type::Price ? 1.0 : 0.0;

    // interpolate in term length

    std::vector<Real> termLengths;
    for (auto const& p : terms())
        termLengths.push_back(periodToTime(p));

    Size termIndex_m, termIndex_p;
    Real term_alpha;
    std::tie(termIndex_m, termIndex_p, term_alpha) = interpolationIndices(termLengths, underlyingLength);

    // construct cds underlying(s) for the terms we will use for the interpolation

    std::map<Period, QuantLib::ext::shared_ptr<QuantExt::CreditDefaultSwap>> underlyings;

    const CreditCurve::RefData& refData = termCurves_[termIndex_m]->refData();
    QL_REQUIRE(refData.runningSpread != Null<Real>(), "CreditVolCurve: need runningSpread for ATM strike computation. "
                                                      "Is the running spread in the term curve configuration?");

    /* Use index maturity date based on index start date. If no start date is given, assume this is a single name option
       running from today. */
    Date mat = std::max(cdsMaturity(refData.startDate != Null<Date>() ? refData.startDate : referenceDate(),
                                    terms_[termIndex_m], refData.rule),
                        referenceDate() + 1);
    Date effExp = std::min(mat - 1, expiry);
    Schedule schedule(effExp, mat, refData.tenor, refData.calendar, refData.convention, refData.termConvention,
                      refData.rule, refData.endOfMonth);
    Date protectionStartDate = (refData.rule == DateGeneration::CDS || refData.rule == DateGeneration::CDS2015)
                                   ? effExp
                                   : schedule.dates().front();
    DayCounter lastPeriodDayCounter = refData.lastPeriodDayCounter.empty() && refData.dayCounter == Actual360()
                                          ? Actual360(true)
                                          : refData.lastPeriodDayCounter;
    underlyings[terms_[termIndex_m]] = QuantLib::ext::make_shared<CreditDefaultSwap>(
        Protection::Buyer, 1.0, refData.runningSpread, schedule, refData.payConvention, refData.dayCounter, true,
        CreditDefaultSwap::atDefault, protectionStartDate, QuantLib::ext::shared_ptr<Claim>(), lastPeriodDayCounter, true,
        effExp, refData.cashSettlementDays);

    QL_REQUIRE(!termCurves_[termIndex_m]->rateCurve().empty() && !termCurves_[termIndex_p]->rateCurve().empty(),
               "CreditVolCurve: need discounting rate curve of index for ATM strike computation.");
    QL_REQUIRE(!termCurves_[termIndex_m]->recovery().empty() && !termCurves_[termIndex_p]->recovery().empty(),
               "CreditVolCurve: need recovery rate of index for ATM strike computation.");

    auto engine = QuantLib::ext::make_shared<MidPointCdsEngine>(termCurves_[termIndex_m]->curve(),
                                                        termCurves_[termIndex_m]->recovery()->value(),
                                                        termCurves_[termIndex_m]->rateCurve());

    underlyings[terms_[termIndex_m]]->setPricingEngine(engine);

    // compute (interpolated) ATM strike

    Real atmStrike;
    Real fep1, fep2, fairSpread1, fairSpread2, rpv01_1, rpv01_2, adjFairSpread1 = 0.0, adjFairSpread2 = 0.0,
                                                                 discToExercise = 1.0, forwardPrice1, forwardPrice2,
                                                                 adjForwardPrice1 = 1.0, adjForwardPrice2 = 1.0;

    discToExercise = termCurves_[termIndex_m]->rateCurve()->discount(effExp);
    fep1 = (1.0 - termCurves_[termIndex_m]->recovery()->value()) *
           termCurves_[termIndex_m]->curve()->defaultProbability(effExp) * discToExercise;
    if (type() == Type::Spread) {
        fairSpread1 = underlyings[terms_[termIndex_m]]->fairSpreadClean();
        rpv01_1 = std::abs(underlyings[terms_[termIndex_m]]->couponLegNPV() +
                           underlyings[terms_[termIndex_m]]->accrualRebateNPV()) /
                  underlyings[terms_[termIndex_m]]->runningSpread();
        adjFairSpread1 = fairSpread1 + fep1 / rpv01_1;
        atmStrike = adjFairSpread1;
    } else if (type() == Type::Price) {
        forwardPrice1 = 1.0 - underlyings[terms_[termIndex_m]]->NPV() / discToExercise;
        adjForwardPrice1 = forwardPrice1 - fep1 / discToExercise;
        atmStrike = adjForwardPrice1;
    } else {
        QL_FAIL("InterpolatingCreditVolCurve::atmStrike(): internal error, type not handled");
    }

    if (!close_enough(term_alpha, 1.0)) {
        Date mat = std::max(cdsMaturity(refData.startDate != Null<Date>() ? refData.startDate : referenceDate(),
                                        terms_[termIndex_p], refData.rule),
                            referenceDate() + 1);
        Date effExp = std::min(mat - 1, expiry);
        Schedule schedule(effExp, mat, refData.tenor, refData.calendar, refData.convention, refData.termConvention,
                          refData.rule, refData.endOfMonth);
        Date protectionStartDate = (refData.rule == DateGeneration::CDS || refData.rule == DateGeneration::CDS2015)
                                       ? effExp
                                       : schedule.dates().front();
        underlyings[terms_[termIndex_p]] = QuantLib::ext::make_shared<CreditDefaultSwap>(
            Protection::Buyer, 1.0, refData.runningSpread, schedule, refData.payConvention, refData.dayCounter, true,
            CreditDefaultSwap::atDefault, protectionStartDate, QuantLib::ext::shared_ptr<Claim>(), lastPeriodDayCounter, true,
            effExp, refData.cashSettlementDays);
        auto engine = QuantLib::ext::make_shared<MidPointCdsEngine>(termCurves_[termIndex_p]->curve(),
                                                            termCurves_[termIndex_p]->recovery()->value(),
                                                            termCurves_[termIndex_p]->rateCurve());
        underlyings[terms_[termIndex_p]]->setPricingEngine(engine);

        fep2 = (1.0 - termCurves_[termIndex_p]->recovery()->value()) *
               termCurves_[termIndex_p]->curve()->defaultProbability(effExp) * discToExercise;
        if (type() == Type::Spread) {
            fairSpread2 = underlyings[terms_[termIndex_p]]->fairSpreadClean();
            rpv01_2 = std::abs(underlyings[terms_[termIndex_p]]->couponLegNPV() +
                               underlyings[terms_[termIndex_p]]->accrualRebateNPV()) /
                      underlyings[terms_[termIndex_p]]->runningSpread();
            adjFairSpread2 = fairSpread2 + fep2 / rpv01_2;
            atmStrike = term_alpha * adjFairSpread1 + (1.0 - term_alpha * adjFairSpread2);
        } else if (type() == Type::Price) {
            forwardPrice2 = 1.0 - underlyings[terms_[termIndex_p]]->NPV() / discToExercise;
            adjForwardPrice2 = forwardPrice2 - fep2 / discToExercise;
            atmStrike = term_alpha * adjForwardPrice1 + (1.0 - term_alpha) * adjForwardPrice2;
        } else {
            QL_FAIL("InterpolatingCreditVolCurve::atmStrike(): internal error, type not handled");
        }
    }

    // add the result to the cache and return it

    atmStrikeCache_[std::make_pair(expiry, underlyingLength)] = atmStrike;
    return atmStrike;
}

void CreditVolCurve::performCalculations() const { atmStrikeCache_.clear(); }

InterpolatingCreditVolCurve::InterpolatingCreditVolCurve(
    const Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc, const DayCounter& dc,
    const std::vector<Period>& terms, const std::vector<Handle<CreditCurve>>& termCurves,
    const std::map<std::tuple<Date, Period, Real>, Handle<Quote>>& quotes, const Type& type)
    : CreditVolCurve(settlementDays, cal, bdc, dc, terms, termCurves, type), quotes_(quotes) {
    init();
}

InterpolatingCreditVolCurve::InterpolatingCreditVolCurve(
    const Date& referenceDate, const Calendar& cal, BusinessDayConvention bdc, const DayCounter& dc,
    const std::vector<Period>& terms, const std::vector<Handle<CreditCurve>>& termCurves,
    const std::map<std::tuple<Date, Period, Real>, Handle<Quote>>& quotes, const Type& type)
    : CreditVolCurve(referenceDate, cal, bdc, dc, terms, termCurves, type), quotes_(quotes) {
    init();
}

void InterpolatingCreditVolCurve::init() {
    for (auto const& q : quotes_)
        registerWith(q.second);
}

Real InterpolatingCreditVolCurve::volatility(const Date& expiry, const Real underlyingLength, const Real strike,
                                             const Type& targetType) const {

    calculate();

    QL_REQUIRE(targetType == type(), "InterpolatingCreditVolCurve: Vol type conversion between strike types 'Price' "
                                     "and 'Spread' is not supported. The vol "
                                     "surface used to price an option must have the same strike type as the option.");

    Real effStrike = strike == Null<Real>() ? atmStrike(expiry, underlyingLength) : strike;

    // term interpolation

    Size termIndex_m, termIndex_p;
    Real term_alpha;
    std::tie(termIndex_m, termIndex_p, term_alpha) = interpolationIndices(smileTermLengths_, underlyingLength);

    // expiry interpolation

    Size expiryIndex_m, expiryIndex_p;
    Real expiry_alpha;
    Real t = timeFromReference(expiry);
    std::tie(expiryIndex_m, expiryIndex_p, expiry_alpha) = interpolationIndices(smileExpiryTimes_, t);

    // smiles by expiry / term

    const Smile& smile_1_1 = smiles_[std::make_pair(smileExpiries_[expiryIndex_m], smileTerms_[termIndex_m])];
    const Smile& smile_1_2 = smiles_[std::make_pair(smileExpiries_[expiryIndex_m], smileTerms_[termIndex_p])];
    const Smile& smile_2_1 = smiles_[std::make_pair(smileExpiries_[expiryIndex_p], smileTerms_[termIndex_m])];
    const Smile& smile_2_2 = smiles_[std::make_pair(smileExpiries_[expiryIndex_p], smileTerms_[termIndex_p])];

    // atm levels by expiry / term

    Real atm_1_1 = smile_1_1.first;
    Real atm_1_2 = smile_1_2.first;
    Real atm_2_1 = smile_2_1.first;
    Real atm_2_2 = smile_2_2.first;

    // vols at target moneyness

    Real m = moneyness(effStrike, atmStrike(expiry, underlyingLength));
    Real vol_1_1 = smile_1_1.second->operator()(this->strike(m, atm_1_1));
    Real vol_1_2 = smile_1_2.second->operator()(this->strike(m, atm_1_2));
    Real vol_2_1 = smile_2_1.second->operator()(this->strike(m, atm_2_1));
    Real vol_2_2 = smile_2_2.second->operator()(this->strike(m, atm_2_2));

    // interpolate in term direction

    Real vol_1 = term_alpha * vol_1_1 + (1.0 - term_alpha) * vol_1_2;
    Real vol_2 = term_alpha * vol_2_1 + (1.0 - term_alpha) * vol_2_2;

    // interpolate in expiry direction

    return std::sqrt((expiry_alpha * (vol_1 * vol_1 * smileExpiryTimes_[expiryIndex_m]) +
                      (1.0 - expiry_alpha) * (vol_2 * vol_2 * smileExpiryTimes_[expiryIndex_p])) /
                     t);
}

void InterpolatingCreditVolCurve::performCalculations() const {

    CreditVolCurve::performCalculations();

    QL_REQUIRE(!quotes_.empty(), "InterpolatingCreditVolCurve: no quotes given, can not build a volatility curve.");

    // For each term and option expiry create an interpolation object representing a vol smile.

    smileTerms_.clear();
    smileExpiries_.clear();
    smileTermLengths_.clear();
    smileExpiryTimes_.clear();
    strikes_.clear();
    vols_.clear();
    smiles_.clear();

    Period currentTerm = 0 * Days;
    Date currentExpiry = Null<Date>();
    std::vector<Real> currentStrikes;
    std::vector<Real> currentVols;
    Date expiry;
    Period term;
    Real strike;
    Real vol;

    auto q = quotes_.begin();
    for (Size i = 0; i <= quotes_.size(); ++i) {
        if (i < quotes_.size()) {
            expiry = std::get<0>(q->first);
            term = std::get<1>(q->first);
            strike = std::get<2>(q->first);
            vol = q->second->value();
            ++q;
        } else {
            currentTerm = term;
            currentExpiry = expiry;
        }
        if (term != currentTerm || expiry != currentExpiry || i == quotes_.size()) {
            if (!currentStrikes.empty()) {
                if (currentStrikes.size() == 1) {
                    currentStrikes.push_back(currentStrikes.back() + 0.01);
                    currentVols.push_back(currentVols.back());
                }
                auto key = std::make_pair(currentExpiry, currentTerm);
                auto s = strikes_.insert(std::make_pair(key, currentStrikes)).first;
                auto v = vols_.insert(std::make_pair(key, currentVols)).first;
                auto tmp = QuantLib::ext::make_shared<FlatExtrapolation>(
                    QuantLib::ext::make_shared<LinearInterpolation>(s->second.begin(), s->second.end(), v->second.begin()));
                tmp->enableExtrapolation();
                smiles_[key] = std::make_pair(atmStrike(currentExpiry, currentTerm), tmp);
                currentStrikes.clear();
                currentVols.clear();
                smileTerms_.push_back(currentTerm);
                smileExpiries_.push_back(currentExpiry);
            }
            currentTerm = term;
            currentExpiry = expiry;
        }
        if (i < quotes_.size()) {
            currentStrikes.push_back(strike);
            currentVols.push_back(vol);
        }
    }

    // populate times vectors

    for (auto const& p : smileTerms_)
        smileTermLengths_.push_back(periodToTime(p));
    for (auto const& d : smileExpiries_)
        smileExpiryTimes_.push_back(timeFromReference(d));

    /* For each term, add missing option expiries that we saw for other terms by creating an interpolated smile.
       We interpolate in terms of
       - absolute moneyness (Type = Spread)
       - log-moneyness      (Type = Price)
    */

    for (auto const& term : smileTerms_) {
        for (auto const& expiry : smileExpiries_) {
            auto key = std::make_pair(expiry, term);
            if (smiles_.find(key) == smiles_.end()) {

                // search neighboured expiries for same term

                Date expiry_m = Null<Date>();
                Date expiry_p = Null<Date>();
                for (auto const& s : smiles_) {
                    if (s.first.second != term)
                        continue;
                    if (s.first.first >= expiry) {
                        expiry_p = s.first.first;
                        break;
                    }
                    expiry_m = s.first.first;
                }

                // if we have a smile for the expiry and term already, there is nothing to do

                if (expiry_m == expiry || expiry_p == expiry)
                    continue;

                // otherwise build an interpolated / extrapoalted smile

                if (expiry_m == Null<Date>() && expiry_p != Null<Date>()) {
                    // expiry <= smallest expiry for that term = expiry_p
                    createSmile(expiry, term, expiry_p, expiry_m);
                } else if (expiry_m != Null<Date>() && expiry_p != Null<Date>()) {
                    // expiry_m < expiry < expiry_p
                    createSmile(expiry, term, expiry_m, expiry_p);
                } else if (expiry_m != Null<Date>() && expiry_p == Null<Date>()) {
                    // expiry >= largest expiry for that term = expiry_m
                    createSmile(expiry, term, expiry_p, expiry_m);
                } else {
                    QL_FAIL("InterpolatingCreditVolCurve: internal error, expiry_m = expiry_p = null, i.e. there are "
                            "no smiles for term "
                            << term);
                }
            }
        }
    }
}

namespace {
struct CompClose {
    bool operator()(Real x, Real y) const { return x < y && !close_enough(x, y); }
};
} // namespace

void InterpolatingCreditVolCurve::createSmile(const Date& expiry, const Period& term, const Date& expiry_m,
                                              const Date& expiry_p) const {
    Real thisAtm = atmStrike(expiry, term);
    if (expiry_p == Null<Date>()) {
        auto key = std::make_pair(expiry_m, term);
        const Smile& smile = smiles_[key];
        std::vector<Real> strikes;
        std::vector<Real> vols;
        for (auto const& k : strikes_[key]) {
            strikes.push_back(strike(moneyness(k, smile.first), thisAtm));
        }
        for (auto const& k : strikes)
            vols.push_back(smile.second->operator()(k));
        auto s = strikes_.insert(std::make_pair(key, strikes));
        auto v = vols_.insert(std::make_pair(key, vols));
        auto tmp = QuantLib::ext::make_shared<FlatExtrapolation>(QuantLib::ext::make_shared<LinearInterpolation>(
            s.first->second.begin(), s.first->second.end(), v.first->second.begin()));
        tmp->enableExtrapolation();
        smiles_[std::make_pair(expiry, term)] = std::make_pair(thisAtm, tmp);
    } else if (expiry_m == Null<Date>()) {
        auto key = std::make_pair(expiry_p, term);
        const Smile& smile = smiles_[key];
        std::vector<Real> strikes;
        std::vector<Real> vols;
        for (auto const& k : strikes_[key]) {
            strikes.push_back(strike(moneyness(k, smile.first), thisAtm));
        }
        for (auto const& k : strikes)
            vols.push_back(smile.second->operator()(k));
        auto s = strikes_.insert(std::make_pair(key, strikes));
        auto v = vols_.insert(std::make_pair(key, vols));
        auto tmp = QuantLib::ext::make_shared<FlatExtrapolation>(QuantLib::ext::make_shared<LinearInterpolation>(
            s.first->second.begin(), s.first->second.end(), v.first->second.begin()));
        tmp->enableExtrapolation();
        smiles_[std::make_pair(expiry, term)] = std::make_pair(thisAtm, tmp);
    } else {
        auto key_m = std::make_pair(expiry_m, term);
        auto key_p = std::make_pair(expiry_p, term);
        const Smile& smile_m = smiles_[key_m];
        const Smile& smile_p = smiles_[key_p];
        std::set<Real, CompClose> strikes_set;
        for (auto const& k : strikes_[key_m]) {
            strikes_set.insert(strike(moneyness(k, smile_m.first), thisAtm));
        }
        for (auto const& k : strikes_[key_p]) {
            strikes_set.insert(strike(moneyness(k, smile_p.first), thisAtm));
        }
        std::vector<Real> strikes(strikes_set.begin(), strikes_set.end());
        std::vector<Real> vols;
        Real t = timeFromReference(expiry);
        Real t_m = timeFromReference(expiry_m);
        Real t_p = timeFromReference(expiry_m);
        Real alpha = (t_p - t) / (t_p - t_m);
        for (auto const& k : strikes) {
            Real vol_m = smile_m.second->operator()(k);
            Real vol_p = smile_p.second->operator()(k);
            vols.push_back(std::sqrt((alpha * (vol_m * vol_m * t_m) + (1.0 - alpha) * (vol_p * vol_p * t_p)) / t));
        }
        auto s = strikes_.insert(std::make_pair(std::make_pair(expiry, term), strikes));
        auto v = vols_.insert(std::make_pair(std::make_pair(expiry, term), vols));
        auto tmp = QuantLib::ext::make_shared<FlatExtrapolation>(QuantLib::ext::make_shared<LinearInterpolation>(
            s.first->second.begin(), s.first->second.end(), v.first->second.begin()));
        tmp->enableExtrapolation();
        smiles_[std::make_pair(expiry, term)] = std::make_pair(thisAtm, tmp);
    }
}

ProxyCreditVolCurve::ProxyCreditVolCurve(const QuantLib::Handle<CreditVolCurve>& source,
                                         const std::vector<QuantLib::Period>& terms,
                                         const std::vector<QuantLib::Handle<CreditCurve>>& termCurves)
    : CreditVolCurve(source->businessDayConvention(), source->dayCounter(), terms.empty() ? source->terms() : terms,
                     termCurves.empty() ? source->termCurves() : termCurves, source->type()),
      source_(source) {
    // check inputs to ctor for consistency
    QL_REQUIRE(terms.size() == termCurves.size(), "ProxyCreditVolCurve: given terms (" << terms.size()
                                                                                       << ") do not match term curves ("
                                                                                       << termCurves.size() << ")");
    registerWith(source);
}

QuantLib::Real ProxyCreditVolCurve::volatility(const QuantLib::Date& exerciseDate,
                                               const QuantLib::Real underlyingLength, const QuantLib::Real strike,
                                               const Type& targetType) const {

    // we read the vol from the source surface keeping the moneyness constant (if meaningful)

    Real effectiveStrike = strike;
    if (!this->terms().empty() && !source_->terms().empty()) {
        effectiveStrike = this->strike(this->moneyness(strike, this->atmStrike(exerciseDate, underlyingLength)),
                                       source_->atmStrike(exerciseDate, underlyingLength));
    }
    return source_->volatility(exerciseDate, underlyingLength, effectiveStrike, type());
}

const Date& ProxyCreditVolCurve::referenceDate() const { return source_->referenceDate(); }

SpreadedCreditVolCurve::SpreadedCreditVolCurve(const Handle<CreditVolCurve> baseCurve, const std::vector<Date> expiries,
                                               const std::vector<Handle<Quote>> spreads, const bool stickyMoneyness,
                                               const std::vector<Period>& terms,
                                               const std::vector<Handle<CreditCurve>>& termCurves)
    : CreditVolCurve(baseCurve->businessDayConvention(), baseCurve->dayCounter(), terms, termCurves, baseCurve->type()),
      baseCurve_(baseCurve), expiries_(expiries), spreads_(spreads), stickyMoneyness_(stickyMoneyness) {
    for (auto const& s : spreads)
        registerWith(s);
}

const Date& SpreadedCreditVolCurve::referenceDate() const { return baseCurve_->referenceDate(); }

void SpreadedCreditVolCurve::performCalculations() const {
    CreditVolCurve::performCalculations();
    times_.clear();
    spreadValues_.clear();
    for (auto const& d : expiries_) {
        times_.push_back(timeFromReference(d));
    }
    for (auto const& s : spreads_) {
        spreadValues_.push_back(s->value());
    }
    interpolatedSpreads_ = QuantLib::ext::make_shared<FlatExtrapolation>(
        QuantLib::ext::make_shared<LinearInterpolation>(times_.begin(), times_.end(), spreadValues_.begin()));
    interpolatedSpreads_->enableExtrapolation();
}

Real SpreadedCreditVolCurve::volatility(const Date& exerciseDate, const Real underlyingLength, const Real strike,
                                        const Type& targetType) const {
    calculate();
    Real effectiveStrike = strike;
    if (stickyMoneyness_ && !baseCurve_->terms().empty() && !this->terms().empty()) {
        effectiveStrike = this->strike(this->moneyness(strike, this->atmStrike(exerciseDate, underlyingLength)),
                                       baseCurve_->atmStrike(exerciseDate, underlyingLength));
    }
    Real base = baseCurve_->volatility(exerciseDate, underlyingLength, effectiveStrike, targetType);
    Real spread = interpolatedSpreads_->operator()(timeFromReference(exerciseDate));
    return base + spread;
}

CreditVolCurveWrapper::CreditVolCurveWrapper(const Handle<BlackVolTermStructure>& vol)
    : CreditVolCurve(vol->businessDayConvention(), vol->dayCounter(), {}, {}, Type::Spread), vol_(vol) {
    registerWith(vol_);
}

Real CreditVolCurveWrapper::volatility(const Date& exerciseDate, const Real underlyingLength, const Real strike,
                                       const Type& targetType) const {
    return vol_->blackVol(exerciseDate, strike, true);
}

const Date& CreditVolCurveWrapper::referenceDate() const { return vol_->referenceDate(); }

BlackVolFromCreditVolWrapper::BlackVolFromCreditVolWrapper(const Handle<QuantExt::CreditVolCurve>& vol,
                                                           const Real underlyingLength)
    : BlackVolatilityTermStructure(vol->businessDayConvention(), vol->dayCounter()), vol_(vol),
      underlyingLength_(underlyingLength) {}

const Date& BlackVolFromCreditVolWrapper::referenceDate() const { return vol_->referenceDate(); }

Real BlackVolFromCreditVolWrapper::minStrike() const { return vol_->minStrike(); }
Real BlackVolFromCreditVolWrapper::maxStrike() const { return vol_->maxStrike(); }
Date BlackVolFromCreditVolWrapper::maxDate() const { return vol_->maxDate(); }

Real BlackVolFromCreditVolWrapper::blackVolImpl(Real t, Real strike) const {
    return vol_->volatility(t, underlyingLength_, strike, vol_->type());
}

} // namespace QuantExt
