/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <qle/instruments/multilegoption.hpp>

#include <ql/cashflows/floatingratecoupon.hpp>

namespace QuantExt {

MultiLegOption::MultiLegOption(const std::vector<Leg>& legs, const std::vector<bool>& payer,
                               const std::vector<Currency>& currency, const boost::shared_ptr<Exercise>& exercise,
                               const Settlement::Type settlementType,
                               Settlement::Method settlementMethod = Settlement::PhysicalOTC)
    : legs_(legs), payer_(payer), currency_(currency), exercise_(exercise), settlementType_(settlementType) {

    QL_REQUIRE(legs_.size() > 0, "MultiLegOption: No legs are given");
    QL_REQUIRE(payer_.size() == legs_.size(),
               "MultiLegOption: payer size (" << payer.size() << ") does not match legs size (" << legs_.size() << ")");
    QL_REQUIRE(currency_.size() == legs_.size(), "MultiLegOption: currency size (" << currency_.size()
                                                                                   << ") does not match legs size ("
                                                                                   << legs_.size() << ")");
    // find maturity date

    maturity_ = Date::minDate();
    for (auto const& l : legs_) {
        if (!l.empty())
            maturity_ = std::max(maturity_, l.back()->date());
    }

} // MultiLegOption

Real MultiLegOption::underlyingNpv() const {
    calculate();
    QL_REQUIRE(underlyingNpv_ != Null<Real>(), "MultiLegOption: underlying npv not available");
    return underlyingNpv_;
}

void MultiLegOption::setupArguments(PricingEngine::arguments* args) const {
    MultiLegOption::arguments* tmp = dynamic_cast<MultiLegOption::arguments*>(args);
    QL_REQUIRE(tmp != nullptr, "MultiLegOption: wrong pricing engine argument type");
    tmp->legs = legs_;
    tmp->payer.resize(payer_.size());
    for (Size i = 0; i < payer_.size(); ++i)
        tmp->payer[i] = payer_[i] ? -1.0 : 1.0;
    tmp->currency = currency_;
    tmp->exercise = exercise_;
    tmp->settlementType = settlementType_;
}

void MultiLegOption::fetchResults(const PricingEngine::results* r) const {
    Instrument::fetchResults(r);
    const MultiLegOption::results* results = dynamic_cast<const MultiLegOption::results*>(r);
    if (results) {
        underlyingNpv_ = results->underlyingNpv;
    } else {
        underlyingNpv_ = Null<Real>();
    }
}

void MultiLegOption::results::reset() {
    Instrument::results::reset();
    underlyingNpv = Null<Real>();
}

} // namespace QuantExt
