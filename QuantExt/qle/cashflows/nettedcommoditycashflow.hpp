/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file qle/cashflows/nettedcommoditycashflow.hpp
    \brief Cash flow that nets multiple commodity floating leg cashflows for a given payment period
 */

#ifndef quantext_netted_commodity_cash_flow_hpp
#define quantext_netted_commodity_cash_flow_hpp

#include <ql/cashflow.hpp>
#include <ql/patterns/visitor.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <vector>

namespace QuantExt {

/*! Cash flow that aggregates multiple commodity floating leg cashflows for netting.

    This class takes a collection of commodity floating leg cashflows that have the same payment date
    and creates a single netted cashflow. The netting logic:
    1. Verifies all underlying cashflows have the same periodQuantity()
    2. Calculates sum of effective fixings: sum((isPayer ? -1 : 1) * cashflow->fixing())
    3. Rounds this sum to specified precision
    4. Multiplies rounded sum by common periodQuantity to get final amount
*/
class NettedCommodityCashFlow : public QuantLib::CashFlow, public QuantLib::Observer {

public:
    /*! Constructor
        \param underlyingCashflows Vector of pairs containing the underlying commodity cashflows and their payer flags
        \param paymentDate The payment date for this netted cashflow
        \param nettingPrecision Number of decimal places to round the total average fixing to (Null<Natural>() means no rounding)
    */
    NettedCommodityCashFlow(const std::vector<std::pair<QuantLib::ext::shared_ptr<QuantLib::CashFlow>, bool>>& underlyingCashflows,
                           const QuantLib::Date& paymentDate,
                           QuantLib::Natural nettingPrecision = QuantLib::Null<QuantLib::Natural>());

    //! \name Inspectors
    //@{
    const std::vector<std::pair<QuantLib::ext::shared_ptr<QuantLib::CashFlow>, bool>>& underlyingCashflows() const {
        return underlyingCashflows_;
    }
    QuantLib::Natural nettingPrecision() const { return nettingPrecision_; }
    QuantLib::Real commonQuantity() const { return commonQuantity_; }
    //@}

    //! \name Event interface
    //@{
    QuantLib::Date date() const override { return paymentDate_; }
    //@}

    //! \name CashFlow interface
    //@{
    QuantLib::Real amount() const override;
    //@}

    //! \name Observer interface
    //@{
    void update() override {
        calculated_ = false;
        notifyObservers();
    }
    //@}

    //! \name Visitability
    //@{
    void accept(QuantLib::AcyclicVisitor& v) override;
    //@}

private:
    void validateAndSetCommonQuantity();
    QuantLib::Real calculateTotalAverageFixing() const;

    std::vector<std::pair<QuantLib::ext::shared_ptr<QuantLib::CashFlow>, bool>> underlyingCashflows_;
    QuantLib::Date paymentDate_;
    QuantLib::Natural nettingPrecision_;
    QuantLib::Real commonQuantity_;
    mutable QuantLib::Real cachedAmount_;
    mutable bool calculated_;
};

} // namespace QuantExt

#endif
