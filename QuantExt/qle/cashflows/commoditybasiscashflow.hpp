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

/*! \file qle/cashflows/commoditybasiscashflow.hpp
    \brief Cash flow dependent on two commodity spot prices or future's settlement prices
 */

#pragma once

#include <boost/optional.hpp>
#include <ql/cashflow.hpp>
#include <ql/patterns/visitor.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/cashflows/commoditycashflow.hpp>

namespace QuantExt {

//! Cash flow dependent on a two commodity spot prices or futures settlement prices on a given pricing date
class CommodityBasisCashFlow : public CommodityCashFlow {

public:
    //! Constructor taking an explicit \p pricingDate and \p paymentDate
    CommodityBasisCashFlow(const boost::shared_ptr<CommodityCashFlow>& basisFlow,
                           const boost::shared_ptr<CommodityCashFlow>& baseFlow,
                           bool addBasis = true);

    boost::shared_ptr<CommodityCashFlow> basisFlow() const { return basisFlow_; }
    boost::shared_ptr<CommodityCashFlow> baseFlow() const { return baseFlow_; }
    bool addBasis() const { return addBasis_; }

    //@}
    //! \name CommodityCashFlow interface
    //@{
    const std::vector<std::pair<QuantLib::Date, ext::shared_ptr<CommodityIndex>>>& indices() const override { return indices_; }

    QuantLib::Date lastPricingDate() const override { return basisFlow_->lastPricingDate(); }

    QuantLib::Real fixing() const override;
    //@}
    
    //! \name Event interface
    //@{
    QuantLib::Date date() const override { return basisFlow_->date(); }
    //@}

    //! \name CashFlow interface
    //@{
    QuantLib::Real amount() const override;
    //@}

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor& v) override;
    //@}
private:
    void performCalculations() const override{};
    
    boost::shared_ptr<CommodityCashFlow> basisFlow_;
    boost::shared_ptr<CommodityCashFlow> baseFlow_;
    bool addBasis_;
    std::vector<std::pair<QuantLib::Date, ext::shared_ptr<CommodityIndex>>> indices_;

};


} // namespace QuantExt

