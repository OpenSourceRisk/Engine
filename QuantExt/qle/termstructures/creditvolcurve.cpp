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

#include <qle/utilities/time.hpp>

namespace QuantExt {

using namespace QuantLib;

CreditVolCurve::CreditVolCurve(BusinessDayConvention bdc, const DayCounter& dc) : VolatilityTermStructure(bdc, dc) {}

CreditVolCurve::CreditVolCurve(const Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc,
                               const DayCounter& dc, const std::vector<QuantLib::Period>& terms,
                               const std::vector<Handle<CreditCurve>>& termCurves, const Type& type)
    : VolatilityTermStructure(settlementDays, cal, bdc, dc), terms_(terms), termCurves_(termCurves), type_(type) {
    init();
}

CreditVolCurve::CreditVolCurve(const Date& referenceDate, const Calendar& cal, BusinessDayConvention bdc,
                               const DayCounter& dc, const std::vector<QuantLib::Period>& terms,
                               const std::vector<Handle<CreditCurve>>& termCurves, const Type& type)
    : VolatilityTermStructure(referenceDate, cal, bdc, dc), terms_(terms), termCurves_(termCurves), type_(type) {
    init();
}

void CreditVolCurve::init() const {
    QL_REQUIRE(terms_.size() == termCurves_.size(), "CreditVolCurve: terms size (" << terms_.size()
                                                                                   << ") must match termCurves size ("
                                                                                   << termCurves_.size());
}

Real CreditVolCurve::volatility(const Date& exerciseDate, const Period& underlyingTerm, const Real strike,
                                const Type& targetType) const {
    return volatility(exerciseDate, periodToTime(underlyingTerm), strike, targetType);
}

const std::vector<QuantLib::Period>& CreditVolCurve::terms() const { return terms_; }

const std::vector<Handle<CreditCurve>>& CreditVolCurve::termCurves() const { return termCurves_; }

const CreditVolCurve::Type& CreditVolCurve::type() const { return type_; }

Date CreditVolCurve::maxDate() const { return Date::maxDate(); }

Real CreditVolCurve::minStrike() const { return -QL_MAX_REAL; }

Real CreditVolCurve::maxStrike() const { return QL_MAX_REAL; }

Real CrediVolCurve::moneyness(const Real strike, const Real atmStrike) const {
    if (type() == Spread) {
        return strike - smile.atmStrike;
    } else if (type() == Price) {
        return std::log(strike / atmStrike);
    } else {
        QL_FAIL("InterpolatingCreditVolCurve::moneyness(): internal error, type not handled");
    }
}

Real CreditVolCurve::strike(const Real moneyness, const Real atmStrike) const {
    if (type() == Spread) {
        return atmStrike + moneyness;
    } else if (type() == Price) {
        return atmStrike * std::exp(moneyness);
    } else {
        QL_FAIL("InterpolatingCreditVolCurve::strike(): internal error, type not handled");
    }
}

Real CreditVolCurve::atmStrike(const QuantLib::Date& expiry, const QuantLib::Period& term) const {
    return atmStrike(expiry, periodToTime(term));
}

Real CreditVolCurve::atmStrike(const QuantLib::Date& expiry, const Real underlyingLength) const {
    QL_FAIL("atmStrike() to do.");
    // cache nach expiry / udnerlying length
    // require at least one curve
}

InterpolatingCreditVolCurve::InterpolatingCreditVolCurve(
    const Natural settlementDays, const Calendar& cal, BusinessDayConvention bdc, const DayCounter& dc,
    const std::vector<QuantLib::Period>& terms, const std::vector<Handle<CreditCurve>>& termCurves,
    const std::map<std::tuple<Period, Date, Real>, Handle<Quote>>& quotes, const Type& type)
    : CreditVolCurve(settlementDays, cal, bdc, dc, terms, termCurves, type), quotes_(quotes) {
    init();
}

InterpolatingCreditVolCurve::InterpolatingCreditVolCurve(
    const Date& referenceDate, const Calendar& cal, BusinessDayConvention bdc, const DayCounter& dc,
    const std::vector<QuantLib::Period>& terms, const std::vector<Handle<CreditCurve>>& termCurves,
    const std::map<std::tuple<Period, Date, Real>, Handle<Quote>>& quotes, const Type& type)
    : CreditVolCurve(referenceDate, cal, bdc, dc, terms, termCurves, type), quotes_(quotes) {
    init();
}

InterpolatingCreditVolCurve::init() const {
    for (auto const& c : termCurves_)
        registerWith(c);
    for (auto const& q : quotes_)
        registerWith(q.second);
}

Real InterpolatingCreditVolCurve::volatility(const Date& expiry, const Real underlyingLength, const Real strike,
                                             const Type& targetType) const {

    QL_REQUIRE(targetType == type(), "InterpolatingCreditVolCurve: targetType != type not supported yet.");

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

    Real atmVol_1_1 = smile_1_1->operator()(atm_1_1);
    Real atmVol_1_2 = smile_1_2->operator()(atm_1_2);
    Real atmVol_2_1 = smile_2_1->operator()(atm_2_1);
    Real atmVol_2_2 = smile_2_2->operator()(atm_2_2);

    // interpolated atm vol

    Real thisAtmVol = termAlpha_ * (expiry_alpha * atmVol_1_1 + (1.0 - expiry_alpha) * atmVol_1_2) +
                      (1.0 - termAlpha_) * (expiry_alpha * atmVol_2_1 + (1.0 - expiry_alpha) * atmVol_2_2);

    // moneyness

    Real m = moneyness(strike, atmStrike(expiry, underlyingLength));

    // vol spreads at target moneyness

    Real volSpread_1_1 = smile_1_1->operator()(strike(m, atm_1_1)) - atmVol_1_1;
    Real volSpread_1_2 = smile_1_2->operator()(strike(m, atm_1_2)) - atmVol_1_2;
    Real volSpread_2_1 = smile_2_1->operator()(strike(m, atm_2_1)) - atmVol_2_1;
    Real volSpread_2_2 = smile_2_2->operator()(strike(m, atm_2_2)) - atmVol_2_2;

    // result

    return thisAtmVol + termAlpha_ * (expiry_alpha * volSpread_1_1 + (1.0 - expiry_alpha) * volSpread_1_2) +
           (1.0 - termAlpha_) * (expiry_alpha * volSpread_2_1 + (1.0 - expiry_alpha) * volSpread_2_2);
}

void InterpolatingCreditVolCurve::performCalculations() const {

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
        if (term != currentTerm || expiry != currentExpiry || q == quotes_.back().first) {
            if (!currentStrikes.empty()) {
                if (currentStrikes.size() == 1) {
                    currentStrikes.push_back(currentStrikes.back());
                    currentVols.push_back(currentVols.back());
                }
                auto key = std::make_pair(currentTerm, currentExpiry);
                auto s = strikes_[key].insert(std::make_pair(key, currentStrikes)).first;
                auto v = vols_[key].insert(std::make_pair(key, currentVols)).first;
                smiles_[key] =
                    std::make_pair(atmStrike(currentExpiry, currentTerm),
                                   boost::make_shared<FlatExtrapolation>(boost::make_shared<LinearInterpolation>(
                                       s->second.begin(), s->second.end(), v->second.begin())));
                currentStrikes.clear();
                currentVols.clear();
                smileTerms.push_back(currentTerm);
                smileExpiries.push_back(currentExpiry);
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

    for (auto const& term : smileTerms) {
        for (auto const& expiry : smileExpiries) {
            auto key = std::make_pair(expiry, term);
            if (smiles_.find(key) == smiles_.end()) {

                // search neighboured expiries for same term

                Date expiry_m = Null<Date>();
                Date expiry_p = Null<Date>();
                for (auto const& s : smiles_) {
                    if (s->first.first != term)
                        continue;
                    if (s->first.second >= expiry) {
                        expiry_p = s->first.second;
                        break;
                    }
                    expiry_m = s->first.second;
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
    bool operator()(Real x, Real y) { return x < y && !close_enough(x, y); }
};
} // namespace

void InterpolatingCreditVolCurve::createSmile(const Date& expiry, const Period& term, const Date& expiry_m,
                                              const Date& expiry_p) {
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
                                        s->second.begin(), s->second.end(), v->second.begin())));
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
                                        s->second.begin(), s->second.end(), v->second.begin())));
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
        Real atmVol_m = smile_m->operator()(smile_m.first);
        Real atmVol_p = smile_p->operator()(smile_p.first);
        Real thisAtmVol = alpha * atmVol_m + (1.0 - alpha) * atmVol_p;
        for (auto const& k : strikes) {
            vols.push_back(thisAtmVol + alpha * (smile_m->operator()(k) - atmVol_m) +
                           (1.0 - alpha) * (smile_p->operator()(k) - atmVol_p));
        }
        auto s = strikes_.insert(std::make_pair(key, strikes));
        auto v = vols_.insert(std::make_pair(key, vols));
        smiles_[std::make_pair(expiry, term)] =
            std::make_pair(thisAtm, boost::make_shared<FlatExtrapolation>(boost::make_shared<LinearInterpolation>(
                                        s->second.begin(), s->second.end(), v->second.begin())));
    }
}

SpreadedCreditVolCurve::SpreadedCreditVolCurve(const Handle<CreditVolCurve> baseCurve, const std::vector<Date> expiries,
                                               const std::vector<Handle<Quote>> spreads, const bool stickyMoneyness)
    : CreditVolCurve(baseCurve->businessDayConvention(), baseCurve->dayCounter()), baseCurve_(baseCurve),
      expiries_(expiries), spreads_(spreads), stickyMoneyness_(stickyMoneyness) {}

const std::vector<QuantLib::Period>& SpreadedCreditVolCurve::terms() const { return terms_; }

const std::vector<Handle<CreditCurve>>& SpreadedCreditVolCurve::termCurves() const { return termCurves_; }

const Date& SpreadedCreditVolCurve::referenceDate() const { return baseCurve_->referenceDate(); }

Real SpreadedCreditVolCurve::volatility(const Date& exerciseDate, const Real underlyingLength, const Real strike,
                                        const Type& targetType) const {
    // TDODO
    return 0.0;
}

} // namespace QuantExt
