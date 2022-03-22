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

#include <qle/instruments/creditdefaultswap.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <qle/pricingengines/midpointcdsengine.hpp>
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

    /* we need at least on term curve to compute the atm strike properly - we set it to 0 / 1 (spread, price) if
       we don't have a term, so that we can build strike-independent curves without curves. It is the user's
       responsibility to provide terms for strike-dependent curves. */
    if (terms().empty())
        return type() == Type::Price ? 1.0 : 0.0;

    // interpolate in term length

    std::vector<Real> termLengths;
    for (auto const& p : terms())
        termLengths.push_back(periodToTime(p));

    Size termIndex = std::max<Size>(
        1, std::min<Size>(termLengths.size() - 1,
                          std::distance(termLengths.begin(), std::lower_bound(termLengths.begin(), termLengths.end(),
                                                                              underlyingLength, [](Real t1, Real t2) {
                                                                                  return t1 < t2 &&
                                                                                         !close_enough(t1, t2);
                                                                              }))));

    Real term_t1 = termLengths[termIndex - 1];
    Real term_t2 = termLengths[termIndex];
    Real term_alpha = (term_t2 - underlyingLength) / (term_t2 - term_t1);

    // construct cds underlying(s) for the terms we will use for the interpolation

    std::map<Period, boost::shared_ptr<QuantExt::CreditDefaultSwap>> underlyings;

    const CreditCurve::RefData& refData = termCurves_[termIndex - 1]->refData();
    QL_REQUIRE(refData.startDate != Null<Date>(),
               "CreditVolCurve: need start date of index for ATM strike computation. Is this set in the term curve?");
    QL_REQUIRE(
        refData.runningSpread != Null<Real>(),
        "CreditVolCurve: need runningSpread of index for ATM strike computation. Is this set in the term curve?");

    Schedule schedule(expiry, cdsMaturity(refData.startDate, terms_[termIndex - 1], refData.rule), refData.tenor,
                      refData.calendar, refData.convention, refData.termConvention, refData.rule, refData.endOfMonth);
    underlyings[terms_[termIndex - 1]] = boost::make_shared<CreditDefaultSwap>(
        Protection::Buyer, 1.0, refData.runningSpread, schedule, refData.payConvention, refData.dayCounter, true,
        CreditDefaultSwap::atDefault, Date(), boost::shared_ptr<Claim>(), refData.lastPeriodDayCounter, expiry,
        refData.cashSettlementDays);

    QL_REQUIRE(!termCurves_[termIndex - 1]->rateCurve().empty() && !termCurves_[termIndex]->rateCurve().empty(),
               "CreditVolCurve: need discounting rate curve of index for ATM strike computation.");
    QL_REQUIRE(!termCurves_[termIndex - 1]->recovery().empty() && !termCurves_[termIndex]->recovery().empty(),
               "CreditVolCurve: need recovery rate of index for ATM strike computation.");

    auto engine = boost::make_shared<QuantExt::MidPointCdsEngine>(termCurves_[termIndex - 1]->curve(),
                                                                  termCurves_[termIndex - 1]->recovery()->value(),
                                                                  termCurves_[termIndex - 1]->rateCurve());

    underlyings[terms_[termIndex - 1]]->setPricingEngine(engine);

    if (!close_enough(term_alpha, 1.0)) {
        Schedule schedule(expiry, cdsMaturity(refData.startDate, terms_[termIndex - 1], refData.rule), refData.tenor,
                          refData.calendar, refData.convention, refData.termConvention, refData.rule,
                          refData.endOfMonth);
        underlyings[terms_[termIndex]] = boost::make_shared<CreditDefaultSwap>(
            Protection::Buyer, 1.0, refData.runningSpread, schedule, refData.payConvention, refData.dayCounter, true,
            CreditDefaultSwap::atDefault, Date(), boost::shared_ptr<Claim>(), refData.lastPeriodDayCounter, expiry,
            refData.cashSettlementDays);
        auto engine = boost::make_shared<QuantExt::MidPointCdsEngine>(termCurves_[termIndex]->curve(),
                                                                      termCurves_[termIndex]->recovery()->value(),
                                                                      termCurves_[termIndex]->rateCurve());
        underlyings[terms_[termIndex - 1]]->setPricingEngine(engine);
    }

    // compute (interpolated) ATM strike

    Real atmStrike;

    Real fep1 = (1.0 - termCurves_[termIndex - 1]->recovery()->value()) *
                termCurves_[termIndex - 1]->curve()->defaultProbability(expiry);
    Real fep2 = (1.0 - termCurves_[termIndex]->recovery()->value()) *
                termCurves_[termIndex]->curve()->defaultProbability(expiry);
    if (type() == Type::Spread) {
        Real fairSpread1 = underlyings[terms_[termIndex - 1]]->fairSpreadClean();
        Real fairSpread2 = underlyings[terms_[termIndex]]->fairSpreadClean();
        Real rpv01_1 = std::abs(underlyings[terms_[termIndex - 1]]->couponLegNPV() +
                                underlyings[terms_[termIndex - 1]]->accrualRebateNPV()) /
                       underlyings[terms_[termIndex - 1]]->runningSpread();
        Real rpv01_2 = std::abs(underlyings[terms_[termIndex]]->couponLegNPV() +
                                underlyings[terms_[termIndex]]->accrualRebateNPV()) /
                       underlyings[terms_[termIndex]]->runningSpread();
        Real adjFairSpread1 = fairSpread1 + fep1 / rpv01_1;
        Real adjFairSpread2 = fairSpread2 + fep2 / rpv01_2;
        atmStrike = term_alpha * adjFairSpread1 + (1.0 - term_alpha * adjFairSpread2);
    } else if (type() == Type::Price) {
        Real discToExercise = termCurves_[termIndex - 1]->rateCurve()->discount(expiry);
        Real forwardPrice1 = (1.0 - underlyings[terms_[termIndex - 1]]->NPV()) / discToExercise;
        Real forwardPrice2 = (1.0 - underlyings[terms_[termIndex]]->NPV()) / discToExercise;
        Real adjForwardPrice1 = forwardPrice1 - fep1 / discToExercise;
        Real adjForwardPrice2 = forwardPrice2 - fep2 / discToExercise;
        atmStrike = term_alpha * adjForwardPrice1 + (1.0 - term_alpha) * adjForwardPrice2;
    } else {
        QL_FAIL("InterpolatingCreditVolCurve::atmStrike(): internal error, type not handled");
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

    QL_REQUIRE(targetType == type(), "InterpolatingCreditVolCurve: targetType != type not supported yet.");

    Real effStrike = strike == Null<Real>() ? atmStrike(expiry, underlyingLength) : strike;

    // term interpolation

    Size termIndex = std::max<Size>(
        1, std::min<Size>(
               smileTermLengths_.size() - 1,
               std::distance(smileTermLengths_.begin(),
                             std::lower_bound(smileTermLengths_.begin(), smileTermLengths_.end(), underlyingLength,
                                              [](Real t1, Real t2) { return t1 < t2 && !close_enough(t1, t2); }))));

    Real term_t1 = smileTermLengths_[termIndex - 1];
    Real term_t2 = smileTermLengths_[termIndex];
    Real term_alpha = (term_t2 - underlyingLength) / (term_t2 - term_t1);

    // expiry interpolation

    Size expiryIndex = std::max<Size>(
        1, std::min<Size>(
               smileTermLengths_.size() - 1,
               std::distance(smileTermLengths_.begin(),
                             std::lower_bound(smileTermLengths_.begin(), smileTermLengths_.end(), underlyingLength,
                                              [](Real t1, Real t2) { return t1 < t2 && !close_enough(t1, t2); }))));

    Real expiry_t1 = smileTermLengths_[expiryIndex - 1];
    Real expiry_t2 = smileTermLengths_[expiryIndex];
    Real expiry_alpha = (expiry_t2 - underlyingLength) / (expiry_t2 - expiry_t1);

    // smiles by expiry / term

    const Smile& smile_1_1 = smiles_[std::make_pair(smileExpiries_[expiryIndex - 1], smileTerms_[termIndex - 1])];
    const Smile& smile_1_2 = smiles_[std::make_pair(smileExpiries_[expiryIndex - 1], smileTerms_[termIndex])];
    const Smile& smile_2_1 = smiles_[std::make_pair(smileExpiries_[expiryIndex], smileTerms_[termIndex - 1])];
    const Smile& smile_2_2 = smiles_[std::make_pair(smileExpiries_[expiryIndex], smileTerms_[termIndex])];

    // atm levels by expiry / term

    Real atm_1_1 = smile_1_1.first;
    Real atm_1_2 = smile_1_2.first;
    Real atm_2_1 = smile_2_1.first;
    Real atm_2_2 = smile_2_2.first;

    // atm vols

    Real atmVol_1_1 = smile_1_1.second->operator()(atm_1_1);
    Real atmVol_1_2 = smile_1_2.second->operator()(atm_1_2);
    Real atmVol_2_1 = smile_2_1.second->operator()(atm_2_1);
    Real atmVol_2_2 = smile_2_2.second->operator()(atm_2_2);

    // interpolated atm vol

    Real thisAtmVol = term_alpha * (expiry_alpha * atmVol_1_1 + (1.0 - expiry_alpha) * atmVol_1_2) +
                      (1.0 - term_alpha) * (expiry_alpha * atmVol_2_1 + (1.0 - expiry_alpha) * atmVol_2_2);

    // moneyness

    Real m = moneyness(effStrike, atmStrike(expiry, underlyingLength));

    // vol spreads at target moneyness

    Real volSpread_1_1 = smile_1_1.second->operator()(this->strike(m, atm_1_1)) - atmVol_1_1;
    Real volSpread_1_2 = smile_1_2.second->operator()(this->strike(m, atm_1_2)) - atmVol_1_2;
    Real volSpread_2_1 = smile_2_1.second->operator()(this->strike(m, atm_2_1)) - atmVol_2_1;
    Real volSpread_2_2 = smile_2_2.second->operator()(this->strike(m, atm_2_2)) - atmVol_2_2;

    // result

    return thisAtmVol + term_alpha * (expiry_alpha * volSpread_1_1 + (1.0 - expiry_alpha) * volSpread_1_2) +
           (1.0 - term_alpha) * (expiry_alpha * volSpread_2_1 + (1.0 - expiry_alpha) * volSpread_2_2);
}

void InterpolatingCreditVolCurve::performCalculations() const {

    CreditVolCurve::performCalculations();

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

    for (auto const& q : quotes_) {
        Date expiry = std::get<0>(q.first);
        Period term = std::get<1>(q.first);
        Real strike = std::get<2>(q.first);
        Real vol = q.second->value();
        if (term != currentTerm || expiry != currentExpiry || q.first == quotes_.rbegin()->first) {
            if (!currentStrikes.empty()) {
                if (currentStrikes.size() == 1) {
                    currentStrikes.push_back(currentStrikes.back());
                    currentVols.push_back(currentVols.back());
                }
                auto key = std::make_pair(currentExpiry, currentTerm);
                auto s = strikes_.insert(std::make_pair(key, currentStrikes)).first;
                auto v = vols_.insert(std::make_pair(key, currentVols)).first;
                smiles_[key] =
                    std::make_pair(atmStrike(currentExpiry, currentTerm),
                                   boost::make_shared<FlatExtrapolation>(boost::make_shared<LinearInterpolation>(
                                       s->second.begin(), s->second.end(), v->second.begin())));
                currentStrikes.clear();
                currentVols.clear();
                smileTerms_.push_back(currentTerm);
                smileExpiries_.push_back(currentExpiry);
            }
            currentTerm = term;
            currentExpiry = expiry;
        }
        currentStrikes.push_back(strike);
        currentVols.push_back(vol);
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
        smiles_[std::make_pair(expiry, term)] =
            std::make_pair(thisAtm, boost::make_shared<FlatExtrapolation>(boost::make_shared<LinearInterpolation>(
                                        s.first->second.begin(), s.first->second.end(), v.first->second.begin())));
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
        smiles_[std::make_pair(expiry, term)] =
            std::make_pair(thisAtm, boost::make_shared<FlatExtrapolation>(boost::make_shared<LinearInterpolation>(
                                        s.first->second.begin(), s.first->second.end(), v.first->second.begin())));
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
        Real atmVol_m = smile_m.second->operator()(smile_m.first);
        Real atmVol_p = smile_p.second->operator()(smile_p.first);
        Real thisAtmVol = alpha * atmVol_m + (1.0 - alpha) * atmVol_p;
        for (auto const& k : strikes) {
            vols.push_back(thisAtmVol + alpha * (smile_m.second->operator()(k) - atmVol_m) +
                           (1.0 - alpha) * (smile_p.second->operator()(k) - atmVol_p));
        }
        auto s = strikes_.insert(std::make_pair(std::make_pair(expiry, term), strikes));
        auto v = vols_.insert(std::make_pair(std::make_pair(expiry, term), vols));
        smiles_[std::make_pair(expiry, term)] =
            std::make_pair(thisAtm, boost::make_shared<FlatExtrapolation>(boost::make_shared<LinearInterpolation>(
                                        s.first->second.begin(), s.first->second.end(), v.first->second.begin())));
    }
}

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
    times_.clear();
    spreadValues_.clear();
    for (auto const& d : expiries_) {
        times_.push_back(timeFromReference(d));
    }
    for (auto const& s : spreads_) {
        spreadValues_.push_back(s->value());
    }
    interpolatedSpreads_ = boost::make_shared<FlatExtrapolation>(
        boost::make_shared<LinearInterpolation>(times_.begin(), times_.end(), spreadValues_.begin()));
}

Real SpreadedCreditVolCurve::volatility(const Date& exerciseDate, const Real underlyingLength, const Real strike,
                                        const Type& targetType) const {
    Real thisAtm =
        (strike == Null<Real>() || stickyMoneyness_) ? this->atmStrike(exerciseDate, underlyingLength) : Null<Real>();
    Real effStrike = strike == Null<Real>() ? thisAtm : strike;
    Real adj = stickyMoneyness_ ? baseCurve_->atmStrike(exerciseDate, underlyingLength) - thisAtm : 0.0;
    return baseCurve_->volatility(exerciseDate, underlyingLength, effStrike + adj, targetType) +
           interpolatedSpreads_->operator()(timeFromReference(exerciseDate));
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
