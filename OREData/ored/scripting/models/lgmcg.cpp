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

#include <ored/scripting/models/modelcgimpl.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/instruments/overnightindexedswap.hpp>
#include <ql/instruments/vanillaswap.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/swapindex.hpp>

namespace ore::data {

std::size_t LgmCG::numeraire(const Date& d, const std::size_t x, const Handle<YieldTermStructure>& discountCurve,
                             const std::string& discountCurveId, const Date& stateDate) const {

    ModelCG::ModelParameter id(ModelCG::ModelParameter::Type::lgm_numeraire, qualifier_, discountCurveId, d, stateDate);
    if (auto m = derivedModelParameters_.find(id); m != derivedModelParameters_.end())
        return m->node();

    auto p(p_);
    Real t = p()->termStructure()->timeFromReference(d);

    ModelCG::ModelParameter id_P0t(ModelCG::ModelParameter::Type::dsc, qualifier_, discountCurveId, d);
    ModelCG::ModelParameter id_H(ModelCG::ModelParameter::Type::lgm_H, qualifier_, {}, d);
    ModelCG::ModelParameter id_zeta(ModelCG::ModelParameter::Type::lgm_zeta, qualifier_, {}, d);

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
    derivedModelParameters_.insert(id);

    return n;
}

std::size_t LgmCG::discountBond(const Date& d, const Date& e, const std::size_t x,
                                const Handle<YieldTermStructure>& discountCurve, const std::string& discountCurveId,
                                const Date& stateDate) const {
    if (d == e)
        return cg_const(g_, 1.0);

    ModelCG::ModelParameter id(ModelCG::ModelParameter::Type::lgm_discountBond, qualifier_, discountCurveId, d, e,
                               stateDate);
    if (auto m = derivedModelParameters_.find(id); m != derivedModelParameters_.end())
        return m->node();

    std::size_t n = cg_mult(g_, numeraire(d, x, discountCurve, discountCurveId),
                            reducedDiscountBond(d, e, x, discountCurve, discountCurveId));

    id.setNode(n);
    derivedModelParameters_.insert(id);
    return n;
}

std::size_t LgmCG::reducedDiscountBond(const Date& d, Date e, const std::size_t x,
                                       const Handle<YieldTermStructure>& discountCurve,
                                       const std::string& discountCurveId, const Date& stateDate) const {
    e = std::max(d, e);
    if (d == e)
        return numeraire(d, x, discountCurve, discountCurveId);

    ModelCG::ModelParameter id(ModelCG::ModelParameter::Type::lgm_reducedDiscountBond, qualifier_, discountCurveId, d,
                               e, stateDate);
    if (auto m = derivedModelParameters_.find(id); m != derivedModelParameters_.end())
        return m->node();

    auto p = p_;
    Real t = p()->termStructure()->timeFromReference(d);
    Real T = p()->termStructure()->timeFromReference(e);

    ModelCG::ModelParameter id_P0T(ModelCG::ModelParameter::Type::dsc, qualifier_, discountCurveId, e);
    ModelCG::ModelParameter id_H(ModelCG::ModelParameter::Type::lgm_H, qualifier_, {}, e);
    ModelCG::ModelParameter id_zeta(ModelCG::ModelParameter::Type::lgm_zeta, qualifier_, {}, d);

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
    derivedModelParameters_.insert(id);
    return n;
}

/* Handles IborIndex and SwapIndex. Requires observation time t <= fixingDate */
std::size_t LgmCG::fixing(const QuantLib::ext::shared_ptr<InterestRateIndex>& index, const Date& fixingDate,
                          const Date& t, const std::size_t x, const Date& stateDate) const {

    ModelCG::ModelParameter id(ModelCG::ModelParameter::Type::fix, index->name(), {}, fixingDate, stateDate, {}, 0, 0);

    Date today = Settings::instance().evaluationDate();
    if (fixingDate <= today) {

        // handle historical fixing (this is handled as a model parameter)

        return addModelParameter(g_, modelParameters_, id, [index, fixingDate]() { return index->fixing(fixingDate); });

    } else if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborIndex>(index)) {

        // handle future fixing (this is a derived model parameter)

        if (auto m = derivedModelParameters_.find(id); m != derivedModelParameters_.end())
            return m->node();

        // Ibor Index

        Date d1 = std::max(t, ibor->valueDate(fixingDate));
        Date d2 = ibor->maturityDate(d1);

        Time dt = ibor->dayCounter().yearFraction(d1, d2);

        std::size_t disc1 = reducedDiscountBond(t, d1, x, ibor->forwardingTermStructure(), "fwd_" + index->name());
        std::size_t disc2 = reducedDiscountBond(t, d2, x, ibor->forwardingTermStructure(), "fwd_" + index->name());

        id.setNode(cg_div(g_, cg_subtract(g_, cg_div(g_, disc1, disc2), cg_const(g_, 1.0)), cg_const(g_, dt)));
        derivedModelParameters_.insert(id);
        return id.node();

    } else {
        QL_FAIL("LgmCG::fixing(): only ibor indices handled so far, index = " << index->name());
    }
}

} // namespace ore::data
