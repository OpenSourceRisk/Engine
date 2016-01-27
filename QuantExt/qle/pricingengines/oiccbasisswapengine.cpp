/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2012 - 2015 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/pricingengines/oiccbasisswapengine.hpp
    \brief Overnight Indexed Cross Currency Basis Swap Engine
*/

#include <qle/pricingengines/oiccbasisswapengine.hpp>
#include <ql/cashflows/cashflows.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <iostream>

namespace QuantLib {

    OvernightIndexedCrossCcyBasisSwapEngine::OvernightIndexedCrossCcyBasisSwapEngine(
     const Handle<YieldTermStructure>& ts1,
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

        QL_REQUIRE(ts1_->referenceDate() == ts2_->referenceDate(),
                   "reference dates do not match");

        Date npvDate = Settings::instance().evaluationDate();
        
        results_.valuationDate = npvDate;

        results_.legNPV.resize(arguments_.legs.size());
        results_.legBPS.resize(arguments_.legs.size());

        bool includeRefDateFlows =
            Settings::instance().includeReferenceDateEvents();

        for (Size i=0; i<arguments_.legs.size(); ++i) {
            try {
                Handle<YieldTermStructure> yts;
                Real fx;
                if (arguments_.currency[i] == ccy1_) {
                    yts = ts1_;
                    fx = 1.0;
                }
                else {
                    yts = ts2_;
                    fx = fx_->value();
                }
                results_.legNPV[i] = fx * arguments_.payer[i] *
                    CashFlows::npv(arguments_.legs[i],
                                   **yts,
                                   includeRefDateFlows,
                                   npvDate,
                                   npvDate);
                results_.legBPS[i] = fx * arguments_.payer[i] *
                    CashFlows::bps(arguments_.legs[i],
                                   **yts,
                                   includeRefDateFlows,
                                   npvDate,
                                   npvDate);
            } catch (std::exception &e) {
                QL_FAIL(io::ordinal(i+1) << " leg: " << e.what());
            }
            results_.value += results_.legNPV[i];
        }

        static Spread basisPoint = 1.0e-4;
        results_.fairPayLegSpread = arguments_.paySpread 
            - results_.value/(results_.legBPS[0]/basisPoint);
        results_.fairRecLegSpread = arguments_.recSpread 
            - results_.value/(results_.legBPS[1]/basisPoint);
    }

}
