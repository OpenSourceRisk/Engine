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

/*! \file qle/pricingengines/oiccbasisswapengine.hpp
    \brief Overnight Indexed Cross Currency Basis Swap Engine
    \ingroup
*/

#include <iostream>
#include <ql/cashflows/cashflows.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <qle/pricingengines/oiccbasisswapengine.hpp>

using namespace QuantLib;

namespace QuantExt {

OvernightIndexedCrossCcyBasisSwapEngine::OvernightIndexedCrossCcyBasisSwapEngine(const Handle<YieldTermStructure>& ts1,
                                                                                 const Currency& ccy1,
                                                                                 const Handle<YieldTermStructure>& ts2,
                                                                                 const Currency& ccy2,
                                                                                 const Handle<Quote>& fx)
    : ts1_(ts1), ccy1_(ccy1), ts2_(ts2), ccy2_(ccy2), fx_(fx) {
    registerWith(ts1_);
    registerWith(ts2_);
    registerWith(fx_);
}

void OvernightIndexedCrossCcyBasisSwapEngine::calculate() const {
    results_.value = 0.0;
    results_.errorEstimate = Null<Real>();

    QL_REQUIRE(!fx_.empty(), "fx handle not set");
    QL_REQUIRE(!ts1_.empty(), "ts1 handle not set");
    QL_REQUIRE(!ts2_.empty(), "ts2 handle not set");

    QL_REQUIRE(ts1_->referenceDate() == ts2_->referenceDate(), "reference dates do not match");

    Date npvDate = Settings::instance().evaluationDate();

    results_.valuationDate = npvDate;

    results_.legNPV.resize(arguments_.legs.size());
    results_.legBPS.resize(arguments_.legs.size());

    bool includeRefDateFlows = Settings::instance().includeReferenceDateEvents();

    for (Size i = 0; i < arguments_.legs.size(); ++i) {
        try {
            Handle<YieldTermStructure> yts;
            Real fx;
            if (arguments_.currency[i] == ccy1_) {
                yts = ts1_;
                fx = 1.0;
            } else {
                yts = ts2_;
                fx = fx_->value();
            }
            results_.legNPV[i] = fx * arguments_.payer[i] *
                                 CashFlows::npv(arguments_.legs[i], **yts, includeRefDateFlows, npvDate, npvDate);
            results_.legBPS[i] = fx * arguments_.payer[i] *
                                 CashFlows::bps(arguments_.legs[i], **yts, includeRefDateFlows, npvDate, npvDate);
        } catch (std::exception& e) {
            QL_FAIL(io::ordinal(i + 1) << " leg: " << e.what());
        }
        results_.value += results_.legNPV[i];
    }

    static Spread basisPoint = 1.0e-4;
    results_.fairPayLegSpread = arguments_.paySpread - results_.value / (results_.legBPS[0] / basisPoint);
    results_.fairRecLegSpread = arguments_.recSpread - results_.value / (results_.legBPS[1] / basisPoint);
}
} // namespace QuantExt
