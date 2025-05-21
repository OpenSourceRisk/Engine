/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <ored/scripting/models/lgmcg.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/instruments/overnightindexedswap.hpp>
#include <ql/instruments/vanillaswap.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/swapindex.hpp>

#include <boost/functional/hash.hpp>

namespace ore::data {

std::size_t LgmCG::numeraire(const Date& d, const std::size_t x, const Handle<YieldTermStructure>& discountCurve,
                             const std::string& discountCurveId) const {

    ModelCG::ModelParameter id(ModelCG::ModelParameter::Type::lgm_numeraire, qualifier_, discountCurveId, d);
    if (auto m = cachedParameters_.find(id); m != cachedParameters_.end())
        return m->node();

    auto p(p_);
    Real t = p()->termStructure()->timeFromReference(d);

    ModelCG::ModelParameter id_P0t(ModelCG::ModelParameter::Type::dsc, qualifier_, discountCurveId, {}, {}, {}, {}, {},
                                   {}, t);
    ModelCG::ModelParameter id_H(ModelCG::ModelParameter::Type::lgm_H, qualifier_, {}, {}, {}, {}, {}, {}, {}, t);
    ModelCG::ModelParameter id_zeta(ModelCG::ModelParameter::Type::lgm_zeta, qualifier_, {}, {}, {}, {}, {}, {}, {}, t);

    std::size_t P0t = addModelParameter(g_, modelParameters_, id_P0t, [p, discountCurve, t] {
        return (discountCurve.empty() ? p()->termStructure() : discountCurve)->discount(t);
    });
    std::size_t H = addModelParameter(g_, modelParameters_, id_H, [p, t] { return p()->H(t); });
    std::size_t zeta = addModelParameter(g_, modelParameters_, id_zeta, [p, t] { return p()->zeta(t); });

    std::size_t n = cg_div(
        g_,
        cg_exp(g_, cg_add(g_, cg_mult(g_, H, x), cg_mult(g_, cg_mult(g_, cg_const(g_, 0.5), zeta), cg_mult(g_, H, H)))),
        P0t);

    id.setNode(n);
    cachedParameters_.insert(id);

    return n;
}

std::size_t LgmCG::discountBond(const Date& d, const Date& e, const std::size_t x,
                                const Handle<YieldTermStructure>& discountCurve,
                                const std::string& discountCurveId) const {
    if (d == e)
        return cg_const(g_, 1.0);

    ModelCG::ModelParameter id(ModelCG::ModelParameter::Type::lgm_discountBond, qualifier_, discountCurveId, d, e);
    if (auto m = cachedParameters_.find(id); m != cachedParameters_.end())
        return m->node();

    std::size_t n = cg_mult(g_, numeraire(d, x, discountCurve, discountCurveId),
                            reducedDiscountBond(d, e, x, discountCurve, discountCurveId));

    id.setNode(n);
    cachedParameters_.insert(id);
    return n;
}

std::size_t LgmCG::reducedDiscountBond(const Date& d, Date e, const std::size_t x,
                                       const Handle<YieldTermStructure>& discountCurve,
                                       const std::string& discountCurveId, const Date& expiryDate) const {
    e = std::max(d, e);
    if (d == e)
        return cg_div(g_, cg_const(g_, 1.0), numeraire(d, x, discountCurve, discountCurveId));

    ModelCG::ModelParameter id(ModelCG::ModelParameter::Type::lgm_reducedDiscountBond, qualifier_, discountCurveId, d,
                               e, expiryDate);
    if (auto m = cachedParameters_.find(id); m != cachedParameters_.end())
        return m->node();

    auto p = p_;
    Real t = p()->termStructure()->timeFromReference(d);
    Real T = p()->termStructure()->timeFromReference(e);

    ModelCG::ModelParameter id_P0T(ModelCG::ModelParameter::Type::dsc, qualifier_, discountCurveId, e, expiryDate, {},
                                   {}, {}, {}, T);
    ModelCG::ModelParameter id_H(ModelCG::ModelParameter::Type::lgm_H, qualifier_, {}, {}, {}, {}, {}, {}, {}, T);
    ModelCG::ModelParameter id_zeta(ModelCG::ModelParameter::Type::lgm_zeta, qualifier_, {}, {}, {}, {}, {}, {}, {}, t);

    std::size_t H = addModelParameter(g_, modelParameters_, id_H, [p, T] { return p()->H(T); });
    std::size_t zeta = addModelParameter(g_, modelParameters_, id_zeta, [p, t] { return p()->zeta(t); });
    std::size_t P0T = addModelParameter(g_, modelParameters_, id_P0T, [p, discountCurve, T] {
        return (discountCurve.empty() ? p()->termStructure() : discountCurve)->discount(T);
    });

    std::size_t n = cg_mult(
        g_, P0T,
        cg_exp(g_, cg_negative(g_, cg_add(g_, cg_mult(g_, H, x),
                                          cg_mult(g_, cg_mult(g_, cg_const(g_, 0.5), zeta), cg_mult(g_, H, H))))));

    id.setNode(n);
    cachedParameters_.insert(id);
    return n;
}

std::size_t LgmCG::fixing(const QuantLib::ext::shared_ptr<InterestRateIndex>& index, const Date& fixingDate,
                          const Date& t, const std::size_t x) const {

    ModelCG::ModelParameter id(ModelCG::ModelParameter::Type::fix, index->name(), {}, fixingDate, t);

    Date today = Settings::instance().evaluationDate();
    if (fixingDate <= today) {

        // handle historical fixing (this is handled as a model parameter)

        return addModelParameter(g_, modelParameters_, id, [index, fixingDate]() { return index->fixing(fixingDate); });

    } else if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborIndex>(index)) {

        // handle future fixing (this is a derived model parameter)

        if (auto m = cachedParameters_.find(id); m != cachedParameters_.end())
            return m->node();

        // Ibor Index

        Date d1 = std::max(t, ibor->valueDate(fixingDate));
        Date d2 = ibor->maturityDate(d1);

        Time dt = ibor->dayCounter().yearFraction(d1, d2);

        std::size_t disc1 =
            reducedDiscountBond(t, d1, x, ibor->forwardingTermStructure(), "fwd_" + index->name(), fixingDate);
        std::size_t disc2 =
            reducedDiscountBond(t, d2, x, ibor->forwardingTermStructure(), "fwd_" + index->name(), fixingDate);

        id.setNode(cg_div(g_, cg_subtract(g_, cg_div(g_, disc1, disc2), cg_const(g_, 1.0)), cg_const(g_, dt)));
        cachedParameters_.insert(id);
        return id.node();

    } else {
        QL_FAIL("LgmCG::fixing(): only ibor indices handled so far, index = " << index->name());
    }
}

std::size_t LgmCG::compoundedOnRate(const QuantLib::ext::shared_ptr<OvernightIndex>& index,
                                    const std::vector<Date>& fixingDates, const std::vector<Date>& valueDates,
                                    const std::vector<Real>& dt, const Natural rateCutoff, const bool includeSpread,
                                    const Real spread, const Real gearing, const Period lookback, Real cap, Real floor,
                                    const bool localCapFloor, const bool nakedOption, const Date& t,
                                    const std::size_t x) const {

    // collect rate characteristics in hash value for caching

    std::size_t hash = 0;
    hash = std::accumulate(fixingDates.begin(), fixingDates.end(), hash,
                           [](std::size_t a, const Date& b) { return boost::hash_combine(a, b.serialNumber()), a; });
    hash = std::accumulate(valueDates.begin(), valueDates.end(), hash,
                           [](std::size_t a, const Date& b) { return boost::hash_combine(a, b.serialNumber()), a; });
    hash = std::accumulate(dt.begin(), dt.end(), hash,
                           [](std::size_t a, const double b) { return boost::hash_combine(a, b), a; });
    boost::hash_combine(hash, rateCutoff);
    boost::hash_combine(hash, includeSpread);
    boost::hash_combine(hash, spread);
    boost::hash_combine(hash, gearing);
    boost::hash_combine(hash, lookback.length());
    boost::hash_combine(hash, lookback.units());
    boost::hash_combine(hash, cap);
    boost::hash_combine(hash, floor);
    boost::hash_combine(hash, localCapFloor);
    boost::hash_combine(hash, nakedOption);

    // id for caching

    ModelCG::ModelParameter id(ModelCG::ModelParameter::Type::complexRate, index->name(), {}, t, {}, {}, 0, 0, hash);

    if (auto m = cachedParameters_.find(id); m != cachedParameters_.end())
        return m->node();

    // calculate

    QL_REQUIRE(!includeSpread || QuantLib::close_enough(gearing, 1.0),
               "LgmCG::compoundedOnRate(): if include spread = true, only a gearing 1.0 is allowed - scale "
               "the notional in this case instead.");

    QL_REQUIRE(rateCutoff < dt.size(), "LgmCG::compoundedOnRate(): rate cutoff ("
                                           << rateCutoff << ") must be less than number of fixings in period ("
                                           << dt.size() << ")");

    /* We allow the observation time t to be later than the value dates for which to project ON fixings.
       In this case we project the period from the first (future) value date to the last value date starting
       from t, but use the actual portion of the underlying curve.
       As a refinement, we might consider to scale x down to the volatility corresponding to the first future
       value date as well (TODO) - this is all experimental and an approximation to meet the requirements of
       an 1D backward solver, i.e. to be able to price e.g. Bermudan OIS swaptions in an efficient way. */

    // the following is similar to the code in the overnight index coupon pricer

    Size i = 0, n = dt.size();
    Size nCutoff = n - rateCutoff;
    std::size_t compoundFactor = cg_const(g_, 1.0), compoundFactorWithoutSpread = cg_const(g_, 1.0);

    Date today = Settings::instance().evaluationDate();

    while (i < n && fixingDates[std::min(i, nCutoff)] < today) {

        Date fixingDate = fixingDates[std::min(i, nCutoff)];

        std::size_t pastFixing = addModelParameter(
            g_, modelParameters_,
            ModelCG::ModelParameter(ModelCG::ModelParameter::Type::fix, index->name(), {}, fixingDate),
            [index, fixingDate]() { return index->fixing(fixingDate); });

        if (includeSpread) {
            compoundFactorWithoutSpread =
                cg_mult(g_, compoundFactorWithoutSpread,
                        cg_add(g_, cg_const(g_, 1.0), cg_mult(g_, pastFixing, cg_const(g_, dt[i]))));
            pastFixing = cg_add(g_, pastFixing, cg_const(g_, spread));
        }
        compoundFactor =
            cg_mult(g_, compoundFactor, cg_add(g_, cg_const(g_, 1.0), cg_mult(g_, pastFixing, cg_const(g_, dt[i]))));
        ++i;
    }

    // i < n && fixingDates[std::min(i, nCutoff)] == today is skipped, i.e. we assume that this fixing is projected

    std::size_t compoundFactorLgm = compoundFactor;
    std::size_t compoundFactorWithoutSpreadLgm = compoundFactorWithoutSpread;

    if (i < n) {

        Handle<YieldTermStructure> curve = index->forwardingTermStructure();
        QL_REQUIRE(!curve.empty(),
                   "LgmVectorised::compoundedOnRate(): null term structure set to this instance of " << index->name());

        // the dates associated to the projection on the T0 curve

        Date d1 = valueDates[i];
        Date d2 = valueDates[std::max(nCutoff, i)];

        double td1 = p_()->termStructure()->timeFromReference(d1);
        double td2 = p_()->termStructure()->timeFromReference(d2);

        std::size_t startDiscount =
            addModelParameter(g_, modelParameters_,
                              ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, qualifier_,
                                                      "fwd_" + index->name(), {}, {}, {}, {}, {}, {}, td1),
                              [curve, d1] { return curve->discount(d1); });
        std::size_t endDiscount =
            addModelParameter(g_, modelParameters_,
                              ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, qualifier_,
                                                      "fwd_" + index->name(), {}, {}, {}, {}, {}, {}, td2),
                              [curve, d2] { return curve->discount(d2); });

        if (nCutoff < n) {
            Date cutoffDate = valueDates[nCutoff];
            double t = p_()->termStructure()->timeFromReference(cutoffDate);
            double tp1 = p_()->termStructure()->timeFromReference(cutoffDate + 1);
            std::size_t discountCutoffDate =
                cg_div(g_,
                       addModelParameter(g_, modelParameters_,
                                         ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, qualifier_,
                                                                 "fwd_" + index->name(), {}, {}, {}, {}, {}, {}, tp1),
                                         [curve, cutoffDate] { return curve->discount(cutoffDate + 1); }),
                       addModelParameter(g_, modelParameters_,
                                         ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, qualifier_,
                                                                 "fwd_" + index->name(), {}, {}, {}, {}, {}, {}, t),
                                         [curve, cutoffDate] { return curve->discount(cutoffDate); }));
            endDiscount = cg_mult(g_, endDiscount, cg_pow(g_, discountCutoffDate, valueDates[n] - valueDates[nCutoff]));
        }

        // the times we use for the projection in the LGM model, if t > d1 they are displaced by (t-d1)

        Date d1_lgm = d1, d2_lgm = d2;
        if (t > d1) {
            d1_lgm += t - d1;
            d2_lgm += t - d2;
        }

        double td1_lgm = p_()->termStructure()->timeFromReference(d1_lgm);
        double td2_lgm = p_()->termStructure()->timeFromReference(d2_lgm);

        // the discount factors estimated in the lgm model

        std::size_t disc1 = reducedDiscountBond(t, d1_lgm, x, curve, "fwd_" + index->name());
        std::size_t disc2 = reducedDiscountBond(t, d2_lgm, x, curve, "fwd_" + index->name());

        // apply a correction to the discount factors

        disc1 = cg_mult(
            g_, disc1,
            cg_div(g_, startDiscount,
                   addModelParameter(g_, modelParameters_,
                                     ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, qualifier_,
                                                             "fwd_" + index->name(), {}, {}, {}, {}, {}, {}, td1_lgm),
                                     [curve, d1_lgm] { return curve->discount(d1_lgm); })));
        disc2 = cg_mult(
            g_, disc2,
            cg_div(g_, endDiscount,
                   addModelParameter(g_, modelParameters_,
                                     ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, qualifier_,
                                                             "fwd_" + index->name(), {}, {}, {}, {}, {}, {}, td2_lgm),
                                     [curve, d2_lgm] { return curve->discount(d2_lgm); })));

        // continue with the usual computation

        compoundFactorLgm = cg_mult(g_, compoundFactorLgm, cg_div(g_, disc1, disc2));

        if (includeSpread) {
            compoundFactorWithoutSpreadLgm = cg_mult(g_, compoundFactorWithoutSpreadLgm, cg_div(g_, disc1, disc2));
            Real tau = index->dayCounter().yearFraction(valueDates[i], valueDates.back()) /
                       (valueDates.back() - valueDates[i]);
            compoundFactorLgm = cg_mult(
                g_, compoundFactorLgm,
                cg_const(g_, std::pow(1.0 + tau * spread, static_cast<int>(valueDates.back() - valueDates[i]))));
        }
    }

    double tau = index->dayCounter().yearFraction(valueDates.front(), valueDates.back());
    std::size_t rate = cg_div(g_, cg_subtract(g_, compoundFactorLgm, cg_const(g_, 1.0)), cg_const(g_, tau));
    std::size_t swapletRate = cg_mult(g_, cg_const(g_, gearing), rate);
    std::size_t effectiveSpread, effectiveIndexFixing;
    if (!includeSpread) {
        swapletRate = cg_add(g_, swapletRate, cg_const(g_, spread));
        effectiveSpread = cg_const(g_, spread);
        effectiveIndexFixing = rate;
    } else {
        effectiveSpread = cg_subtract(
            g_, rate,
            cg_div(g_, cg_subtract(g_, compoundFactorWithoutSpreadLgm, cg_const(g_, 1.0)), cg_const(g_, tau)));
        effectiveIndexFixing = cg_subtract(g_, rate, effectiveSpread);
    }

    std::size_t resultNode;

    if (cap == Null<Real>() && floor == Null<Real>()) {

        // no cap / floor

        resultNode = swapletRate;

    } else {

        // handle cap / floor - we compute the intrinsic value only

        if (gearing < 0.0) {
            std::swap(cap, floor);
        }

        if (nakedOption)
            swapletRate = cg_const(g_, 0.0);

        std::size_t floorletRate = cg_const(g_, 0.0);
        std::size_t capletRate = cg_const(g_, 0.0);

        if (floor != Null<Real>()) {
            // ignore localCapFloor, treat as global
            std::size_t effectiveStrike =
                cg_div(g_, cg_subtract(g_, cg_const(g_, floor), effectiveSpread), cg_const(g_, gearing));
            floorletRate =
                cg_mult(g_, cg_const(g_, gearing),
                        cg_max(g_, cg_const(g_, 0.0), cg_subtract(g_, effectiveStrike, effectiveIndexFixing)));
        }

        if (cap != Null<Real>()) {
            std::size_t effectiveStrike =
                cg_div(g_, cg_subtract(g_, cg_const(g_, cap), effectiveSpread), cg_const(g_, gearing));
            capletRate = cg_mult(g_, cg_const(g_, gearing),
                                 cg_max(g_, cg_const(g_, 0.0), cg_subtract(g_, effectiveIndexFixing, effectiveStrike)));
            if (nakedOption && floor == Null<Real>())
                capletRate = cg_negative(g_, capletRate);
        }

        resultNode = cg_add(g_, {swapletRate, floorletRate, cg_negative(g_, capletRate)});
    }

    id.setNode(resultNode);
    cachedParameters_.insert(id);
    return id.node();
}

std::size_t LgmCG::averagedOnRate(const QuantLib::ext::shared_ptr<OvernightIndex>& index,
                                  const std::vector<Date>& fixingDates, const std::vector<Date>& valueDates,
                                  const std::vector<Real>& dt, const Natural rateCutoff, const bool includeSpread,
                                  const Real spread, const Real gearing, const Period lookback, Real cap, Real floor,
                                  const bool localCapFloor, const bool nakedOption, const Date& t,
                                  const std::size_t x) const {

    // collect rate characteristics in hash value for caching

    std::size_t hash = 0;
    hash = std::accumulate(fixingDates.begin(), fixingDates.end(), hash,
                           [](std::size_t a, const Date& b) { return boost::hash_combine(a, b.serialNumber()), a; });
    hash = std::accumulate(valueDates.begin(), valueDates.end(), hash,
                           [](std::size_t a, const Date& b) { return boost::hash_combine(a, b.serialNumber()), a; });
    hash = std::accumulate(dt.begin(), dt.end(), hash,
                           [](std::size_t a, const double b) { return boost::hash_combine(a, b), a; });
    boost::hash_combine(hash, rateCutoff);
    boost::hash_combine(hash, includeSpread);
    boost::hash_combine(hash, spread);
    boost::hash_combine(hash, gearing);
    boost::hash_combine(hash, lookback.length());
    boost::hash_combine(hash, lookback.units());
    boost::hash_combine(hash, cap);
    boost::hash_combine(hash, floor);
    boost::hash_combine(hash, localCapFloor);
    boost::hash_combine(hash, nakedOption);

    // id for caching

    ModelCG::ModelParameter id(ModelCG::ModelParameter::Type::complexRate, index->name(), {}, t, {}, {}, 0, 0, hash);

    if (auto m = cachedParameters_.find(id); m != cachedParameters_.end())
        return m->node();

    // calculate

    QL_REQUIRE(!includeSpread || QuantLib::close_enough(gearing, 1.0),
               "LgmCG::compoundedOnRate(): if include spread = true, only a gearing 1.0 is allowed - scale "
               "the notional in this case instead.");

    QL_REQUIRE(rateCutoff < dt.size(), "LgmCG::compoundedOnRate(): rate cutoff ("
                                           << rateCutoff << ") must be less than number of fixings in period ("
                                           << dt.size() << ")");

    /* See comment on t as in compoundedOnRate(), above applies here */

    // the following is similar to the code in the overnight index coupon pricer

    Size i = 0, n = dt.size();
    Size nCutoff = n - rateCutoff;
    std::size_t accumulatedRate = cg_const(g_, 0.0);

    Date today = Settings::instance().evaluationDate();

    while (i < n && fixingDates[std::min(i, nCutoff)] < today) {

        Date fixingDate = fixingDates[std::min(i, nCutoff)];

        std::size_t pastFixing = addModelParameter(
            g_, modelParameters_,
            ModelCG::ModelParameter(ModelCG::ModelParameter::Type::fix, index->name(), {}, fixingDate),
            [index, fixingDate]() { return index->fixing(fixingDate); });

        accumulatedRate = cg_add(g_, accumulatedRate, cg_mult(g_, pastFixing, cg_const(g_, dt[i])));
        ++i;
    }

    // i < n && fixingDates[std::min(i, nCutoff)] == today is skipped, i.e. we assume that this fixing is projected

    std::size_t accumulatedRateLgm = accumulatedRate;

    if (i < n) {

        Handle<YieldTermStructure> curve = index->forwardingTermStructure();
        QL_REQUIRE(!curve.empty(),
                   "LgmVectorised::compoundedOnRate(): null term structure set to this instance of " << index->name());

        // the dates associated to the projection on the T0 curve

        Date d1 = valueDates[i];
        Date d2 = valueDates[std::max(nCutoff, i)];

        double td1 = p_()->termStructure()->timeFromReference(d1);
        double td2 = p_()->termStructure()->timeFromReference(d2);

        std::size_t startDiscount =
            addModelParameter(g_, modelParameters_,
                              ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, qualifier_,
                                                      "fwd_" + index->name(), {}, {}, {}, {}, {}, {}, td1),
                              [curve, d1] { return curve->discount(d1); });
        std::size_t endDiscount =
            addModelParameter(g_, modelParameters_,
                              ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, qualifier_,
                                                      "fwd_" + index->name(), {}, {}, {}, {}, {}, {}, td2),
                              [curve, d2] { return curve->discount(d2); });

        if (nCutoff < n) {
            Date cutoffDate = valueDates[nCutoff];
            double t = p_()->termStructure()->timeFromReference(cutoffDate);
            double tp1 = p_()->termStructure()->timeFromReference(cutoffDate + 1);
            std::size_t discountCutoffDate =
                cg_div(g_,
                       addModelParameter(g_, modelParameters_,
                                         ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, qualifier_,
                                                                 "fwd_" + index->name(), {}, {}, {}, {}, {}, {}, tp1),
                                         [curve, cutoffDate] { return curve->discount(cutoffDate + 1); }),
                       addModelParameter(g_, modelParameters_,
                                         ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, qualifier_,
                                                                 "fwd_" + index->name(), {}, {}, {}, {}, {}, {}, t),
                                         [curve, cutoffDate] { return curve->discount(cutoffDate); }));
            endDiscount = cg_mult(g_, endDiscount, cg_pow(g_, discountCutoffDate, valueDates[n] - valueDates[nCutoff]));
        }

        // the times we use for the projection in the LGM model, if t > d1 they are displaced by (t-d1)

        Date d1_lgm = d1, d2_lgm = d2;
        if (t > d1) {
            d1_lgm += t - d1;
            d2_lgm += t - d2;
        }

        double td1_lgm = p_()->termStructure()->timeFromReference(d1_lgm);
        double td2_lgm = p_()->termStructure()->timeFromReference(d2_lgm);

        // the discount factors estimated in the lgm model

        std::size_t disc1 = reducedDiscountBond(t, d1_lgm, x, curve, "fwd_" + index->name());
        std::size_t disc2 = reducedDiscountBond(t, d2_lgm, x, curve, "fwd_" + index->name());

        // apply a correction to the discount factors

        disc1 = cg_mult(
            g_, disc1,
            cg_div(g_, startDiscount,
                   addModelParameter(g_, modelParameters_,
                                     ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, qualifier_,
                                                             "fwd_" + index->name(), {}, {}, {}, {}, {}, {}, td1_lgm),
                                     [curve, d1_lgm] { return curve->discount(d1_lgm); })));
        disc2 = cg_mult(
            g_, disc2,
            cg_div(g_, endDiscount,
                   addModelParameter(g_, modelParameters_,
                                     ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, qualifier_,
                                                             "fwd_" + index->name(), {}, {}, {}, {}, {}, {}, td2_lgm),
                                     [curve, d2_lgm] { return curve->discount(d2_lgm); })));

        // continue with the usual computation

        accumulatedRateLgm = cg_add(g_, accumulatedRateLgm, cg_log(g_, cg_div(g_, disc1, disc2)));
    }

    double tau = index->dayCounter().yearFraction(valueDates.front(), valueDates.back());
    std::size_t rate = cg_add(g_, cg_mult(g_, cg_const(g_, gearing / tau), accumulatedRateLgm), cg_const(g_, spread));

    std::size_t resultNode;

    if (cap == Null<Real>() && floor == Null<Real>()) {

        // no cap / floor

        resultNode = rate;

    } else {

        // handle cap / floor - we compute the intrinsic value only

        if (gearing < 0.0) {
            std::swap(cap, floor);
        }

        std::size_t forwardRate = cg_div(g_, cg_subtract(g_, rate, cg_const(g_, spread)), cg_const(g_, gearing));
        std::size_t floorletRate = cg_const(g_, 0.0);
        std::size_t capletRate = cg_const(g_, 0.0);

        if (nakedOption)
            rate = cg_const(g_, 0.0);

        if (floor != Null<Real>()) {
            // ignore localCapFloor, treat as global
            std::size_t effectiveStrike =
                cg_div(g_, cg_subtract(g_, cg_const(g_, floor), cg_const(g_, spread)), cg_const(g_, gearing));
            floorletRate = cg_mult(g_, cg_const(g_, gearing),
                                   cg_max(g_, cg_const(g_, 0.0), cg_subtract(g_, effectiveStrike, forwardRate)));
        }

        if (cap != Null<Real>()) {
            std::size_t effectiveStrike =
                cg_div(g_, cg_subtract(g_, cg_const(g_, cap), cg_const(g_, spread)), cg_const(g_, gearing));
            capletRate = cg_mult(g_, cg_const(g_, gearing),
                                 cg_max(g_, cg_const(g_, 0.0), cg_subtract(g_, forwardRate, effectiveStrike)));
            if (nakedOption && floor == Null<Real>())
                capletRate = cg_negative(g_, capletRate);
        }

        resultNode = cg_add(g_, {rate, floorletRate, cg_negative(g_, capletRate)});
    }

    id.setNode(resultNode);
    cachedParameters_.insert(id);
    return id.node();
}

} // namespace ore::data
