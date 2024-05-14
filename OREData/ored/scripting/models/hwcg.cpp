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

#include <ored/scripting/models/hwcg.hpp>
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

std::size_t HwCG::numeraire(const Date& d, const std::size_t x, const Handle<YieldTermStructure>& discountCurve,
                            const std::string& discountCurveId) const {
    std::string id = "__hw_" + qualifier_ + "_N_" + ore::data::to_string(d) + "_" + discountCurveId;
    std::size_t n;
    if (n = cg_var(g_, id, ComputationGraph::VarDoesntExist::Nan); n == ComputationGraph::nan) {
        auto p(p_);
        Date ds = getSloppyDate(d, sloppySimDates_, effSimDates_);
        Real t = p()->termStructure()->timeFromReference(d);
        // Real ts = p()->termStructure()->timeFromReference(ds);
        std::size_t I = cg_var(g_, "__hw_" + qualifier_ + "_I_" + ore::data::to_string(ds));

        std::string id_P0t = "__dsc_" + ore::data::to_string(ds) + "_" + discountCurveId;
        std::size_t P0t = addModelParameter(g_, modelParameters_, id_P0t, [p, discountCurve, t] {
            return (discountCurve.empty() ? p()->termStructure() : discountCurve)->discount(t);
        });
        n = cg_div(g_, cg_exp(g_, I), P0t);
        g_.setVariable(id, n);
    }
    return n;
}

std::size_t HwCG::discountBond(const Date& d, Date e, const std::size_t x,
                               const Handle<YieldTermStructure>& discountCurve,
                               const std::string& discountCurveId) const {
    e = std::max(d, e);
    std::string id =
        "__hw_" + qualifier_ + "_P_" + ore::data::to_string(d) + "_" + ore::data::to_string(e) + "_" + discountCurveId;
    std::size_t n;
    if (n = cg_var(g_, id, ComputationGraph::VarDoesntExist::Nan), n == ComputationGraph::nan) {
        auto p = p_;
        Date ds = getSloppyDate(d, sloppySimDates_, effSimDates_);
        Date es = getSloppyDate(e, sloppySimDates_, effSimDates_);
        Real t = p()->termStructure()->timeFromReference(d);
        Real T = p()->termStructure()->timeFromReference(e);
        Real ts = p()->termStructure()->timeFromReference(ds);
        Real Ts = p()->termStructure()->timeFromReference(es);
        std::string id_P0t = "__dsc_" + ore::data::to_string(ds) + "_" + discountCurveId;
        std::string id_P0T = "__dsc_" + ore::data::to_string(es) + "_" + discountCurveId;

        std::size_t y = cg_var(g_, "__hw_" + qualifier_ + "_y_" + ore::data::to_string(ds));
        std::size_t g = cg_const(g_, Ts - ts);
        std::size_t exponent = cg_negative(
            g_, cg_add(g_, cg_mult(g_, g, x), cg_mult(g_, cg_const(g_, 0.5), cg_mult(g_, y, cg_mult(g_, g, g)))));

        std::size_t P0t = addModelParameter(g_, modelParameters_, id_P0t, [p, discountCurve, t] {
            return (discountCurve.empty() ? p()->termStructure() : discountCurve)->discount(t);
        });
        std::size_t P0T = addModelParameter(g_, modelParameters_, id_P0T, [p, discountCurve, T] {
            return (discountCurve.empty() ? p()->termStructure() : discountCurve)->discount(T);
        });

        n = cg_mult(g_, cg_div(g_, P0T, P0t), cg_exp(g_, exponent));
        g_.setVariable(id, n);
    }
    return n;
}

std::size_t HwCG::reducedDiscountBond(const Date& d, const Date& e, const std::size_t x,
                                      const Handle<YieldTermStructure>& discountCurve,
                                      const std::string& discountCurveId) const {
    std::string id =
        "__hw_" + qualifier_ + "_Pr_" + ore::data::to_string(d) + "_" + ore::data::to_string(e) + "_" + discountCurveId;
    std::size_t n;
    if (n = cg_var(g_, id, ComputationGraph::VarDoesntExist::Nan), n == ComputationGraph::nan) {
        n = cg_div(g_, discountBond(d, e, x, discountCurve, discountCurveId),
                   numeraire(d, x, discountCurve, discountCurveId));
        g_.setVariable(id, n);
    }
    return n;
}

/* Handles IborIndex and SwapIndex. Requires observation time t <= fixingDate */
std::size_t HwCG::fixing(const QuantLib::ext::shared_ptr<InterestRateIndex>& index, const Date& fixingDate, const Date& t,
                         const std::size_t x) const {

    std::string id =
        "__irFix_" + index->name() + "_" + ore::data::to_string(fixingDate) + "_" + ore::data::to_string(t);
    std::size_t n;
    if (n = cg_var(g_, id, ComputationGraph::VarDoesntExist::Nan); n == ComputationGraph::nan) {

        // handle case where fixing is deterministic

        Date today = Settings::instance().evaluationDate();
        if (fixingDate <= today) {

            // handle historical fixing

            n = addModelParameter(g_, modelParameters_, id,
                                  [index, fixingDate]() { return index->fixing(fixingDate); });

        } else if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborIndex>(index)) {

            // Ibor Index

            Date d1 = std::max(t, ibor->valueDate(fixingDate));
            Date d2 = ibor->maturityDate(d1);

            Time dt = ibor->dayCounter().yearFraction(d1, d2);

            std::size_t disc1 = discountBond(t, d1, x, ibor->forwardingTermStructure(), "fwd_" + index->name());
            std::size_t disc2 = discountBond(t, d2, x, ibor->forwardingTermStructure(), "fwd_" + index->name());

            n = cg_div(g_, cg_subtract(g_, cg_div(g_, disc1, disc2), cg_const(g_, 1.0)), cg_const(g_, dt));

        } else {
            QL_FAIL("HwCG::fixing(): only ibor indices handled so far, index = " << index->name());
        }
        g_.setVariable(id, n);
    }
    return n;
}

} // namespace ore::data
