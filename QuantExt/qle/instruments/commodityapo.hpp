/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file qle/instruments/commodityapo.hpp
    \brief Swaption class
*/

#ifndef quantext_instruments_commodityapo_hpp
#define quantext_instruments_commodityapo_hpp

#include <ql/instruments/barriertype.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/option.hpp>
#include <ql/exercise.hpp>
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
                                const ext::shared_ptr<Exercise>& exercise, const Real quantity, const Real strikePrice,
                                Option::Type type, Settlement::Type delivery = Settlement::Physical,
                                Settlement::Method settlementMethod = Settlement::PhysicalOTC,
                                const Real barrierLevel = Null<Real>(),
                                Barrier::Type barrierType = Barrier::Type::DownIn,
                                Exercise::Type barrierStyle = Exercise::American,
                                const boost::shared_ptr<FxIndex>& fxIndex= nullptr);

    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(PricingEngine::arguments*) const override;
    //@}

    //! \name Inspectors
    //@{
    Settlement::Type settlementType() const { return settlementType_; }
    Settlement::Method settlementMethod() const { return settlementMethod_; }
    const boost::shared_ptr<CommodityIndexedAverageCashFlow>& underlyingFlow() const { return flow_; }

    const boost::shared_ptr<FxIndex>& fxIndex() const { return fxIndex_; }
    Real barrierLevel() const { return barrierLevel_; }
    Barrier::Type barrierType() const { return barrierType_; }
    Exercise::Type barrierStyle() const { return barrierStyle_; }
    Real effectiveStrike() const;
    Real accrued(const Date& refDate) const;
    //@}

private:
    // arguments
    boost::shared_ptr<CommodityIndexedAverageCashFlow> flow_;
    Real quantity_;
    Real strikePrice_;
    Option::Type type_;
    QuantLib::Settlement::Type settlementType_;
    QuantLib::Settlement::Method settlementMethod_;
    boost::shared_ptr<FxIndex> fxIndex_;
    Real barrierLevel_;
    Barrier::Type barrierType_;
    Exercise::Type barrierStyle_;
};

//! %Arguments for commodity APO calculation
class CommodityAveragePriceOption::arguments : public Option::arguments {
public:
    arguments();
    boost::shared_ptr<CommodityIndexedAverageCashFlow> flow;
    Real quantity;
    Real strikePrice;
    Real accrued;
    Real effectiveStrike;
    Option::Type type;
    boost::shared_ptr<FxIndex> fxIndex;
    Settlement::Type settlementType;
    Settlement::Method settlementMethod;
    Real barrierLevel;
    Barrier::Type barrierType;
    Exercise::Type barrierStyle;
    void validate() const override;
};

//! base class for APO engines
class CommodityAveragePriceOption::engine
    : public GenericEngine<CommodityAveragePriceOption::arguments, CommodityAveragePriceOption::results> {};

} // namespace QuantExt

#endif
