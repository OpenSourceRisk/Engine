/*
 Copyright (C) 2019 Quaternion Risk Manaement Ltd
 All rights reserved.
*/

#include <ql/exercise.hpp>
#include <ql/shared_ptr.hpp>
#include <qle/instruments/commodityapo.hpp>

namespace QuantExt {

CommodityAveragePriceOption::CommodityAveragePriceOption(const boost::shared_ptr<CommodityIndexedAverageCashFlow>& flow,
                                                         const ext::shared_ptr<Exercise>& exercise,
                                                         const Real& quantity, const Real& strikePrice,
                                                         QuantLib::Option::Type type,
                                                         QuantLib::Settlement::Type delivery,
                                                         QuantLib::Settlement::Method settlementMethod)
    : Option(ext::shared_ptr<Payoff>(), exercise), flow_(flow), quantity_(quantity), strikePrice_(strikePrice),
      type_(type), settlementType_(delivery), settlementMethod_(settlementMethod) {
    registerWith(flow_);
}

bool CommodityAveragePriceOption::isExpired() const { return detail::simple_event(flow_->date()).hasOccurred(); }

void CommodityAveragePriceOption::setupArguments(PricingEngine::arguments* args) const {
    Option::setupArguments(args);

    CommodityAveragePriceOption::arguments* arguments = dynamic_cast<CommodityAveragePriceOption::arguments*>(args);

    QL_REQUIRE(arguments != 0, "wrong argument type");
    QL_REQUIRE(flow_->gearing() > 0.0, "The gearing on an APO must be positive");

    arguments->quantity = quantity_;
    arguments->strikePrice = strikePrice_;
    arguments->effectiveStrike = (strikePrice_ - flow_->spread()) / flow_->gearing();
    arguments->type = type_;
    arguments->settlementType = settlementType_;
    arguments->settlementMethod = settlementMethod_;
    arguments->exercise = exercise_;
    arguments->flow = flow_;
}

void CommodityAveragePriceOption::fetchResults(const PricingEngine::results* r) const {
    Option::fetchResults(r);
    const CommodityAveragePriceOption::results* results = dynamic_cast<const CommodityAveragePriceOption::results*>(r);
    QL_ENSURE(results != 0, "wrong results type");
    underlyingForwardValue_ = results->underlyingForwardValue;
    sigma_ = results->sigma;
}

void CommodityAveragePriceOption::results::reset() {
    Option::results::reset();
    underlyingForwardValue = Null<Real>();
    sigma = Null<Real>();
}

void CommodityAveragePriceOption::arguments::validate() const {
    QL_REQUIRE(flow, "underlying not set");
    QL_REQUIRE(exercise, "exercise not set");
    QuantLib::Settlement::checkTypeAndMethodConsistency(settlementType, settlementMethod);
}

} // namespace QuantExt
