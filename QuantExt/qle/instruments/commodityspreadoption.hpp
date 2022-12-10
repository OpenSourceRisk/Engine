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
    CommoditySpreadOption(const boost::shared_ptr<CommodityCashFlow>& longAssetCashflow,
                          const boost::shared_ptr<CommodityCashFlow>& shortAssetCashflow,
                          const ext::shared_ptr<Exercise>& exercise, const Real quantity, const Real strikePrice,
                          Option::Type type, const QuantLib::Date& paymentDate = Date(),
                          const boost::shared_ptr<FxIndex>& longAssetFxIndex = nullptr,
                          const boost::shared_ptr<FxIndex>& shortAssetFxIndex = nullptr,
                          Settlement::Type delivery = Settlement::Physical,
                          Settlement::Method settlementMethod = Settlement::PhysicalOTC);
          

    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(PricingEngine::arguments*) const override;
    //@}

    //! \name Inspectors
    //@{
    const boost::shared_ptr<CommodityCashFlow>& underlyingLongAssetFlow() const { return longAssetFlow_; }
    const boost::shared_ptr<CommodityCashFlow>& underlyingShortAssetFlow() const { return shortAssetFlow_; }

    const boost::shared_ptr<FxIndex>& longAssetFxIndex() const { return longAssetFxIndex_; }
    const boost::shared_ptr<FxIndex>& shortAssetFxIndex() const { return shortAssetFxIndex_; }
    Real effectiveStrike() const;
    //@}

    bool isCalendarSpread() const;

private:
    // arguments
    boost::shared_ptr<CommodityCashFlow> longAssetFlow_;
    boost::shared_ptr<CommodityCashFlow> shortAssetFlow_;
    Real quantity_;
    Real strikePrice_;
    Option::Type type_;
    Date paymentDate_;
    boost::shared_ptr<FxIndex> longAssetFxIndex_;
    boost::shared_ptr<FxIndex> shortAssetFxIndex_;
    QuantLib::Settlement::Type settlementType_;
    QuantLib::Settlement::Method settlementMethod_;
};

//! %Arguments for commodity spread option calculation
class CommoditySpreadOption::arguments : public Option::arguments {
public:
    arguments();
    boost::shared_ptr<CommodityCashFlow> longAssetFlow;
    boost::shared_ptr<CommodityCashFlow> shortAssetFlow;
    Real quantity;
    Real strikePrice;
    Real effectiveStrike;
    Option::Type type;
    Date paymentDate;
    boost::shared_ptr<FxIndex> longAssetFxIndex;
    boost::shared_ptr<FxIndex> shortAssetFxIndex;
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
