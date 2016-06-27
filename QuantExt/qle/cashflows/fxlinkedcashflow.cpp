/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <ql/indexes/indexmanager.hpp>

namespace QuantExt {

    FXLinkedCashFlow::FXLinkedCashFlow
    (const Date& cashFlowDate, const Date& fxFixingDate, Real foreignAmount,
     boost::shared_ptr<FxIndex> fxIndex)
    : cashFlowDate_(cashFlowDate), fxFixingDate_(fxFixingDate), foreignAmount_(foreignAmount), fxIndex_(fxIndex)
    {}

    Real FXLinkedCashFlow::fxRate() const {
        return fxIndex_->fixing(fxFixingDate_);
    }

}
