/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <ql/indexes/indexmanager.hpp>

namespace QuantExt {

    FXLinkedCashFlow::FXLinkedCashFlow
    (const Date& cashFlowDate, const Date& fxFixingDate, Real foreignAmount, const Handle<Quote>& fxSpot,
     const Handle<YieldTermStructure>& forTS, const Handle<YieldTermStructure>& domTS,
     const std::string& indexName)
    : cashFlowDate_(cashFlowDate), fxFixingDate_(fxFixingDate), foreignAmount_(foreignAmount), fxSpot_(fxSpot),
      forTS_(forTS), domTS_(domTS), indexName_(indexName) {
        QL_REQUIRE(!fxSpot.empty(), "FXLinkedCashFlow fxSpot is empty");
        QL_REQUIRE(!forTS.empty(), "FXLinkedCashFlow forTS is empty");
        QL_REQUIRE(!domTS.empty(), "FXLinkedCashFlow domTS is empty");
    }

    Real FXLinkedCashFlow::fxRate() const {
        Date today = Settings::instance().evaluationDate();

        if (fxFixingDate_ == today)
            return fxSpot_->value();
        else if (fxFixingDate_ < today) {
            // Get Historical fixing
            IndexManager& im = IndexManager::instance();
            QL_REQUIRE(im.hasHistory(indexName_), "No Index " << indexName_ << " found");
            Real fixing = im.getHistory(indexName_)[fxFixingDate_];
            QL_REQUIRE(fixing != Null<Real>(), "No fixing for " << fxFixingDate_ << " found in "
                       " index " << indexName_);
            return fixing;
        } else {
            // calculate FX Fwd
            return fxSpot_->value() * forTS_->discount(fxFixingDate_) / domTS_->discount(fxFixingDate_);
        }
    }

}
