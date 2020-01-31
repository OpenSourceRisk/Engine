/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file qle/instruments/commodityapo.hpp
    \brief Swaption class
*/

#ifndef quantext_instruments_commodityapo_hpp
#define quantext_instruments_commodityapo_hpp

#include <ql/option.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/termstructures/volatility/volatilitytype.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Commodity Average Price Option
/*! \ingroup instruments
 */
class CommodityAveragePriceOption : public Option {
public:
    class arguments;
    class results;
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
    Real underlyingForwardValue() const { return underlyingForwardValue_; }
    QuantLib::Real sigma() const { return sigma_; }
    //@}
private:
    void fetchResults(const PricingEngine::results*) const;
    // arguments
    boost::shared_ptr<CommodityIndexedAverageCashFlow> flow_;
    Real quantity_;
    Real strikePrice_;
    Option::Type type_;
    QuantLib::Settlement::Type settlementType_;
    QuantLib::Settlement::Method settlementMethod_;
    mutable Real underlyingForwardValue_;
    mutable QuantLib::Real sigma_;
};

//! %Arguments for commodity APO calculation
class CommodityAveragePriceOption::arguments : public Option::arguments {
public:
    arguments() : settlementType(Settlement::Physical) {}
    boost::shared_ptr<CommodityIndexedAverageCashFlow> flow;
    Real quantity;
    Real strikePrice;
    Real effectiveStrike;
    Option::Type type;
    QuantLib::Settlement::Type settlementType;
    QuantLib::Settlement::Method settlementMethod;
    void validate() const;
};

//! %Results from commodity APO calculation
class CommodityAveragePriceOption::results : public Option::results {
public:
    Real underlyingForwardValue;
    QuantLib::Real sigma;
    void reset();
};

//! base class for APO engines
class CommodityAveragePriceOption::engine : public GenericEngine<CommodityAveragePriceOption::arguments, CommodityAveragePriceOption::results> {};

} // namespace QuantExt

#endif
