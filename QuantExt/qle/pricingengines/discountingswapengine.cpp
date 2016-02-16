/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*
 Copyright (C) 2007, 2009 StatPro Italia srl
 Copyright (C) 2011 Ferdinando Ametrano

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/pricingengines/discountingswapengine.hpp>
#include <ql/cashflows/cashflows.hpp>
#include <ql/utilities/dataformatters.hpp>

namespace QuantExt {


    DiscountingSwapEngine::DiscountingSwapEngine(
                            const Handle<YieldTermStructure>& discountCurve,
                            boost::optional<bool> includeSettlementDateFlows,
                            Date settlementDate,
                            Date npvDate)
    : discountCurve_(discountCurve),
      includeSettlementDateFlows_(includeSettlementDateFlows),
      settlementDate_(settlementDate), npvDate_(npvDate),
      floatingLags_(false) {
        registerWith(discountCurve_);
    }

    DiscountingSwapEngine::DiscountingSwapEngine(
                            const Handle<YieldTermStructure>& discountCurve,
                            boost::optional<bool> includeSettlementDateFlows,
                            Period settlementDateLag,
                            Period npvDateLag,
                            Calendar calendar)
    : discountCurve_(discountCurve),
      includeSettlementDateFlows_(includeSettlementDateFlows),
      settlementDateLag_(settlementDateLag), npvDateLag_(npvDateLag),
      calendar_(calendar), floatingLags_(true) {
        registerWith(discountCurve_);
    }

    void DiscountingSwapEngine::calculate() const {
        QL_REQUIRE(!discountCurve_.empty(),
                   "discounting term structure handle is empty");

        results_.value = 0.0;
        results_.errorEstimate = Null<Real>();

        Date refDate = discountCurve_->referenceDate();

        Date settlementDate;
        if(floatingLags_) {
            QL_REQUIRE(settlementDateLag_.length() >= 0,
                       "non negative period required");
            settlementDate =  calendar_.advance(refDate, settlementDateLag_);
        }
        else {
            if (settlementDate_ == Date()) {
                settlementDate = refDate;
            } else {
                QL_REQUIRE(settlementDate >= refDate,
                           "settlement date ("
                               << settlementDate
                               << ") before "
                                  "discount curve reference date ("
                               << refDate << ")");
            }
        }

        if (floatingLags_) {
            QL_REQUIRE(npvDateLag_.length() >= 0,
                       "non negative period required");
            results_.valuationDate = calendar_.advance(refDate, npvDateLag_);
        } else {
            if (npvDate_ == Date()) {
                results_.valuationDate = refDate;
            } else {
                QL_REQUIRE(npvDate_ >= refDate,
                           "npv date (" << npvDate_
                                        << ") before "
                                           "discount curve reference date ("
                                        << refDate << ")");
            }
        }
        results_.npvDateDiscount =
            discountCurve_->discount(results_.valuationDate);

        Size n = arguments_.legs.size();
        results_.legNPV.resize(n);

        bool includeRefDateFlows =
            includeSettlementDateFlows_ ?
            *includeSettlementDateFlows_ :
            Settings::instance().includeReferenceDateEvents();

        for (Size i=0; i<n; ++i) {
            try {
                const YieldTermStructure& discount_ref = **discountCurve_;
                results_.legNPV[i] =
                    DiscountingSwapEngine::npv(
                        arguments_.legs[i], discount_ref, includeRefDateFlows,
                        settlementDate, results_.valuationDate) *
                    arguments_.payer[i];

            } catch (std::exception &e) {
                QL_FAIL(io::ordinal(i+1) << " leg: " << e.what());
            }
            results_.value += results_.legNPV[i];
        }
    }

}
