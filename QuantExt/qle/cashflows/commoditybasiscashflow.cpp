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

#include <ql/utilities/vectors.hpp>
#include <qle/cashflows/commoditybasiscashflow.hpp>

using namespace QuantLib;
using std::vector;

namespace QuantExt {

CommodityBasisCashFlow::CommodityBasisCashFlow(const boost::shared_ptr<CommodityCashFlow>& basisFlow,
                                               const boost::shared_ptr<CommodityCashFlow>& baseFlow,
                                               bool addBasis)
    : CommodityCashFlow(basisFlow->quantity(), basisFlow->spread(), basisFlow->gearing(), basisFlow->useFuturePrice(),
                        basisFlow->index(), basisFlow->fxIndex()),
      basisFlow_(basisFlow), baseFlow_(baseFlow), addBasis_(addBasis), indices_(basisFlow->indices())
       {
    registerWith(basisFlow_);
    registerWith(baseFlow_);   
    indices_.insert(indices_.end(), basisFlow_->indices().begin(), basisFlow_->indices().end());
}

Real CommodityBasisCashFlow::amount() const { return baseFlow_->fixing() + basisFlow_->fixing(); }

Real CommodityBasisCashFlow::fixing() const { return baseFlow_->amount() + basisFlow_->amount(); }

void CommodityBasisCashFlow::accept(AcyclicVisitor& v) {
    basisFlow_->accept(v);
    baseFlow_->accept(v);
    CashFlow::accept(v);
}


} // namespace QuantExt
