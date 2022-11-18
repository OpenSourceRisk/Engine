/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <ql/exercise.hpp>
#include <ql/shared_ptr.hpp>
#include <qle/instruments/commodityspreadoption.hpp>

namespace QuantExt {

CommoditySpreadOption::CommoditySpreadOption(const boost::shared_ptr<CommodityIndexedCashFlow>& longFlow,
                                             const boost::shared_ptr<CommodityIndexedCashFlow>& shortFlow,
                                             const ext::shared_ptr<Exercise>& exercise, const Real quantity,
                                             const Real strikePrice, QuantLib::Option::Type type,
                                             QuantLib::Settlement::Type delivery,
                                             QuantLib::Settlement::Method settlementMethod,
                                             const boost::shared_ptr<FxIndex>& fxIndex)
    : Option(ext::shared_ptr<Payoff>(), exercise), longFlow_(longFlow), shortFlow_(shortFlow), quantity_(quantity),
      strikePrice_(strikePrice), type_(type), settlementType_(delivery), settlementMethod_(settlementMethod),
      fxIndex_(fxIndex) {
    registerWith(longFlow_);
    registerWith(shortFlow_);
}

bool CommoditySpreadOption::isExpired() const { return exercise_->lastDate() < Settings::instance().evaluationDate(); }

Real CommoditySpreadOption::effectiveStrike() const {
    return (strikePrice_ - longFlow_->spread()) / longFlow_->gearing();
}

void CommoditySpreadOption::setupArguments(PricingEngine::arguments* args) const {
    Option::setupArguments(args);

    CommoditySpreadOption::arguments* arguments = dynamic_cast<CommoditySpreadOption::arguments*>(args);

    QL_REQUIRE(arguments != 0, "wrong argument type");
    QL_REQUIRE(longFlow_->gearing() > 0.0, "The gearing on an APO must be positive");

    Date today = Settings::instance().evaluationDate();

    arguments->quantity = quantity_;
    arguments->strikePrice = strikePrice_;
    arguments->effectiveStrike = effectiveStrike();
    arguments->type = type_;
    arguments->settlementType = settlementType_;
    arguments->settlementMethod = settlementMethod_;
    arguments->exercise = exercise_;
    arguments->longFlow = longFlow_;
    arguments->shortFlow = shortFlow_;
    arguments->fxIndex = fxIndex_;
}

CommoditySpreadOption::arguments::arguments()
    : quantity(0.0), strikePrice(0.0), effectiveStrike(0.0), type(Option::Call), fxIndex(nullptr),
      settlementType(Settlement::Physical), settlementMethod(Settlement::PhysicalOTC) {}

void CommoditySpreadOption::arguments::validate() const {
    QL_REQUIRE(longFlow, "underlying not set");
    QL_REQUIRE(shortFlow, "underlying not set");
    QL_REQUIRE(exercise, "exercise not set");
    QuantLib::Settlement::checkTypeAndMethodConsistency(settlementType, settlementMethod);
}

} // namespace QuantExt
