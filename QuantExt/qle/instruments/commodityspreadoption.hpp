/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
 All rights reserved.
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
#include <qle/cashflows/commodityindexedcashflow.hpp>
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
    CommoditySpreadOption(const boost::shared_ptr<CommodityIndexedCashFlow>& longAssetCashflow,
                          const boost::shared_ptr<CommodityIndexedCashFlow>& shortAssetCashflow,
                          const ext::shared_ptr<Exercise>& exercise, const Real quantity, const Real strikePrice,
                          Option::Type type, Settlement::Type delivery = Settlement::Physical,
                          Settlement::Method settlementMethod = Settlement::PhysicalOTC,
                          const boost::shared_ptr<FxIndex>& fxIndex = nullptr);

    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(PricingEngine::arguments*) const override;
    //@}

    //! \name Inspectors
    //@{
    Settlement::Type settlementType() const { return settlementType_; }
    Settlement::Method settlementMethod() const { return settlementMethod_; }
    const boost::shared_ptr<CommodityIndexedCashFlow>& underlyingLongFlow() const { return longFlow_; }
    const boost::shared_ptr<CommodityIndexedCashFlow>& underlyingShortFlow() const { return shortFlow_; }

    const boost::shared_ptr<FxIndex>& fxIndex() const { return fxIndex_; }
    Real effectiveStrike() const;
    //@}

private:
    // arguments
    boost::shared_ptr<CommodityIndexedCashFlow> longFlow_;
    boost::shared_ptr<CommodityIndexedCashFlow> shortFlow_;
    Real quantity_;
    Real strikePrice_;
    Option::Type type_;
    QuantLib::Settlement::Type settlementType_;
    QuantLib::Settlement::Method settlementMethod_;
    boost::shared_ptr<FxIndex> fxIndex_;
};

//! %Arguments for commodity APO calculation
class CommoditySpreadOption::arguments : public Option::arguments {
public:
    arguments();
    boost::shared_ptr<CommodityIndexedCashFlow> longFlow;
    boost::shared_ptr<CommodityIndexedCashFlow> shortFlow;
    Real quantity;
    Real strikePrice;
    Real accrued;
    Real effectiveStrike;
    Option::Type type;
    boost::shared_ptr<FxIndex> fxIndex;
    Settlement::Type settlementType;
    Settlement::Method settlementMethod;
    void validate() const override;
};

//! base class for APO engines
class CommoditySpreadOption::engine
    : public GenericEngine<CommoditySpreadOption::arguments, CommoditySpreadOption::results> {};

} // namespace QuantExt
