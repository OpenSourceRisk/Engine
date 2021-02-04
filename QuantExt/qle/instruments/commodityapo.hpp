/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file qle/instruments/commodityapo.hpp
    \brief Swaption class
*/

#ifndef quantext_instruments_commodityapo_hpp
#define quantext_instruments_commodityapo_hpp

#include <ql/instruments/swaption.hpp>
#include <ql/option.hpp>
#include <ql/termstructures/volatility/volatilitytype.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Commodity Average Price Option
/*! \ingroup instruments
 */
class CommodityAveragePriceOption : public Option {
public:
    class arguments;
    class engine;
    CommodityAveragePriceOption(const boost::shared_ptr<CommodityIndexedAverageCashFlow>& flow,
                                const ext::shared_ptr<Exercise>& exercise, const Real& quantity,
                                const Real& strikePrice, QuantLib::Option::Type type,
                                QuantLib::Settlement::Type delivery = QuantLib::Settlement::Physical,
                                QuantLib::Settlement::Method settlementMethod = QuantLib::Settlement::PhysicalOTC);
    
    //! \name Instrument interface
    //@{
    bool isExpired() const;
    void setupArguments(PricingEngine::arguments*) const;
    //@}

    //! \name Inspectors
    //@{
    Settlement::Type settlementType() const { return settlementType_; }
    Settlement::Method settlementMethod() const { return settlementMethod_; }
    const boost::shared_ptr<CommodityIndexedAverageCashFlow>& underlyingFlow() const { return flow_; }
    //@}

private:
    // arguments
    boost::shared_ptr<CommodityIndexedAverageCashFlow> flow_;
    Real quantity_;
    Real strikePrice_;
    Option::Type type_;
    QuantLib::Settlement::Type settlementType_;
    QuantLib::Settlement::Method settlementMethod_;
};

//! %Arguments for commodity APO calculation
class CommodityAveragePriceOption::arguments : public Option::arguments {
public:
    arguments();
    boost::shared_ptr<CommodityIndexedAverageCashFlow> flow;
    Real quantity;
    Real strikePrice;
    Real effectiveStrike;
    Option::Type type;
    QuantLib::Settlement::Type settlementType;
    QuantLib::Settlement::Method settlementMethod;
    void validate() const;
};

//! base class for APO engines
class CommodityAveragePriceOption::engine
    : public GenericEngine<CommodityAveragePriceOption::arguments, CommodityAveragePriceOption::results> {};

} // namespace QuantExt

#endif
