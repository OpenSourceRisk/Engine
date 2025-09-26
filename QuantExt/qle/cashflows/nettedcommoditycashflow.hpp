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

#include <ql/patterns/visitor.hpp>
#include <qle/cashflows/commoditycashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <vector>

namespace QuantExt {

/*! \brief Helper function to perform precise rounding with pre-rounding to avoid floating point issues.

    \param value The value to round
    \param precision Number of decimal places for final rounding
    \param preRoundPrecision Number of decimal places for pre-rounding (default 8)
    \return The rounded value
*/
QuantLib::Real roundWithPrecision(QuantLib::Real value, QuantLib::Natural precision,
                                 QuantLib::Natural preRoundPrecision = 8);

/*! Cash flow that aggregates multiple commodity floating leg cashflows for netting.

    This class takes a collection of commodity floating leg cashflows that have the same payment date
    and creates a single netted cashflow. The netting logic:
    1. Verifies all underlying cashflows have the same periodQuantity()
    2. Calculates sum of effective fixings: sum((isPayer ? -1 : 1) * cashflow->fixing())
    3. Rounds this sum to specified precision
    4. Multiplies rounded sum by common periodQuantity to get final amount
*/
class NettedCommodityCashFlow : public CommodityCashFlow {

public:
    /*! Constructor
        \param underlyingCashflows Vector of pairs containing the underlying commodity cashflows and their payer flags
        \param nettingPrecision Number of decimal places to round the total average fixing to (Null<Natural>() means no rounding)
    */
    NettedCommodityCashFlow(const std::vector<std::pair<QuantLib::ext::shared_ptr<CommodityCashFlow>, bool>>& underlyingCashflows,
                           QuantLib::Natural nettingPrecision = QuantLib::Null<QuantLib::Natural>());

    //! \name Inspectors
    //@{
    const std::vector<std::pair<QuantLib::ext::shared_ptr<CommodityCashFlow>, bool>>& underlyingCashflows() const {
        return underlyingCashflows_;
    }
    QuantLib::Natural nettingPrecision() const { return nettingPrecision_; }
    QuantLib::Real commonQuantity() const { return commonQuantity_; }
    QuantLib::Real roundedFixing() const;
    //@}

    //! \name Event interface
    //@{
    QuantLib::Date date() const override;
    //@}

    //! \name CommodityCashFlow interface
    //@{
    const std::vector<std::pair<QuantLib::Date, QuantLib::ext::shared_ptr<CommodityIndex>>>& indices() const override;
    QuantLib::Date lastPricingDate() const override;
    QuantLib::Real periodQuantity() const override;
    QuantLib::Real fixing() const override;
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
    void validateAndSetCommonQuantityAndDate();

    std::vector<std::pair<QuantLib::ext::shared_ptr<CommodityCashFlow>, bool>> underlyingCashflows_;
    QuantLib::Natural nettingPrecision_;
    QuantLib::Real commonQuantity_;
};

} // namespace QuantExt

#endif
