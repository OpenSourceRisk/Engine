/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file qle/instruments/commodityspreadoption.hpp
    \brief Option class
*/

#pragma once
#include <ql/exercise.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/option.hpp>
#include <ql/termstructures/volatility/volatilitytype.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/cashflows/commoditycashflow.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/indexes/fxindex.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Commodity Spread Option
/*! \ingroup instruments
 */
class CommoditySpreadOption : public Option {
public:
    class arguments;
    class engine;
    CommoditySpreadOption(const QuantLib::ext::shared_ptr<CommodityCashFlow>& longAssetCashflow,
                          const QuantLib::ext::shared_ptr<CommodityCashFlow>& shortAssetCashflow,
                          const ext::shared_ptr<Exercise>& exercise, const Real quantity, const Real strikePrice,
                          Option::Type type, const QuantLib::Date& paymentDate = Date(),
                          const QuantLib::ext::shared_ptr<FxIndex>& longAssetFxIndex = nullptr,
                          const QuantLib::ext::shared_ptr<FxIndex>& shortAssetFxIndex = nullptr,
                          Settlement::Type delivery = Settlement::Physical,
                          Settlement::Method settlementMethod = Settlement::PhysicalOTC);
          

    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(PricingEngine::arguments*) const override;
    //@}

    //! \name Inspectors
    //@{
    const QuantLib::ext::shared_ptr<CommodityCashFlow>& underlyingLongAssetFlow() const { return longAssetFlow_; }
    const QuantLib::ext::shared_ptr<CommodityCashFlow>& underlyingShortAssetFlow() const { return shortAssetFlow_; }

    const QuantLib::ext::shared_ptr<FxIndex>& longAssetFxIndex() const { return longAssetFxIndex_; }
    const QuantLib::ext::shared_ptr<FxIndex>& shortAssetFxIndex() const { return shortAssetFxIndex_; }
    Real effectiveStrike() const;
    //@}

    bool isCalendarSpread() const;

private:
    // arguments
    QuantLib::ext::shared_ptr<CommodityCashFlow> longAssetFlow_;
    QuantLib::ext::shared_ptr<CommodityCashFlow> shortAssetFlow_;
    Real quantity_;
    Real strikePrice_;
    Option::Type type_;
    Date paymentDate_;
    QuantLib::ext::shared_ptr<FxIndex> longAssetFxIndex_;
    QuantLib::ext::shared_ptr<FxIndex> shortAssetFxIndex_;
    QuantLib::Settlement::Type settlementType_;
    QuantLib::Settlement::Method settlementMethod_;
};

//! %Arguments for commodity spread option calculation
class CommoditySpreadOption::arguments : public Option::arguments {
public:
    arguments();
    QuantLib::ext::shared_ptr<CommodityCashFlow> longAssetFlow;
    QuantLib::ext::shared_ptr<CommodityCashFlow> shortAssetFlow;
    Real quantity;
    Real strikePrice;
    Real effectiveStrike;
    Option::Type type;
    Date paymentDate;
    QuantLib::ext::shared_ptr<FxIndex> longAssetFxIndex;
    QuantLib::ext::shared_ptr<FxIndex> shortAssetFxIndex;
    bool isCalendarSpread;
    Date longAssetLastPricingDate;
    Date shortAssetLastPricingDate;
    Settlement::Type settlementType;
    Settlement::Method settlementMethod;
    void validate() const override;
};

//! base class for commodity spread option engines
class CommoditySpreadOption::engine
    : public GenericEngine<CommoditySpreadOption::arguments, CommoditySpreadOption::results> {};

} // namespace QuantExt
