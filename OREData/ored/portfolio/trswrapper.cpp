/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/portfolio/trswrapper.hpp>
#include <qle/indexes/compositeindex.hpp>

#include <ored/utilities/to_string.hpp>

#include <qle/instruments/cashflowresults.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/currencies/exchangeratemanager.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

TRSWrapper::TRSWrapper(
    const std::vector<QuantLib::ext::shared_ptr<ore::data::Trade>>& underlying,
    const std::vector<QuantLib::ext::shared_ptr<Index>>& underlyingIndex, const std::vector<Real> underlyingMultiplier,
    const bool includeUnderlyingCashflowsInReturn, const Real initialPrice, const Currency& initialPriceCurrency,
    const std::vector<Currency>& assetCurrency, const Currency& returnCurrency,
    const std::vector<Date>& valuationSchedule, const std::vector<Date>& paymentSchedule,
    const std::vector<Leg>& fundingLegs, const std::vector<TRS::FundingData::NotionalType>& fundingNotionalTypes,
    const Currency& fundingCurrency, const Size fundingResetGracePeriod, const bool paysAsset, const bool paysFunding,
    const Leg& additionalCashflowLeg, const bool additionalCashflowLegPayer, const Currency& additionalCashflowCurrency,
    const std::vector<QuantLib::ext::shared_ptr<FxIndex>>& fxIndexAsset, const QuantLib::ext::shared_ptr<FxIndex>& fxIndexReturn,
    const QuantLib::ext::shared_ptr<FxIndex>& fxIndexAdditionalCashflows,
    const std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& addFxIndices)
    : underlying_(underlying), underlyingIndex_(underlyingIndex), underlyingMultiplier_(underlyingMultiplier),
      includeUnderlyingCashflowsInReturn_(includeUnderlyingCashflowsInReturn), initialPrice_(initialPrice),
      initialPriceCurrency_(initialPriceCurrency), assetCurrency_(assetCurrency), returnCurrency_(returnCurrency),
      valuationSchedule_(valuationSchedule), paymentSchedule_(paymentSchedule), fundingLegs_(fundingLegs),
      fundingNotionalTypes_(fundingNotionalTypes), fundingCurrency_(fundingCurrency),
      fundingResetGracePeriod_(fundingResetGracePeriod), paysAsset_(paysAsset), paysFunding_(paysFunding),
      additionalCashflowLeg_(additionalCashflowLeg), additionalCashflowLegPayer_(additionalCashflowLegPayer),
      additionalCashflowCurrency_(additionalCashflowCurrency), fxIndexAsset_(fxIndexAsset),
      fxIndexReturn_(fxIndexReturn), fxIndexAdditionalCashflows_(fxIndexAdditionalCashflows),
      addFxIndices_(addFxIndices) {

    QL_REQUIRE(!paymentSchedule_.empty(), "TRSWrapper::TRSWrapper(): payment schedule must not be empty()");

    QL_REQUIRE(valuationSchedule_.size() == paymentSchedule_.size() + 1,
               "TRSWrapper::TRSWrapper(): valuation schedule size ("
                   << valuationSchedule_.size() << ") must be payment schedule size (" << paymentSchedule_.size()
                   << ") plus one");

    for (Size i = 0; i < valuationSchedule_.size() - 1; ++i) {
        QL_REQUIRE(valuationSchedule_[i] < valuationSchedule_[i + 1],
                   "TRSWrapper::TRSWrapper(): valuation schedule dates must be monotonic, at "
                       << i << ": " << valuationSchedule_[i] << ", " << valuationSchedule_[i + 1]);
    }

    for (Size i = 0; i < paymentSchedule_.size() - 1; ++i) {
        QL_REQUIRE(paymentSchedule_[i] < paymentSchedule_[i + 1],
                   "TRSWrapper::TRSWrapper(): payment schedule dates must be monotonic, at "
                       << i << ": " << paymentSchedule_[i] << ", " << paymentSchedule_[i + 1]);
    }

    for (Size i = 0; i < paymentSchedule_.size(); ++i) {
        QL_REQUIRE(paymentSchedule_[i] >= valuationSchedule_[i + 1], "TRSWrapper::TRSWrapper(): payment date at "
                                                                         << i << " (" << paymentSchedule_[i]
                                                                         << ") must be >= valuation date ("
                                                                         << valuationSchedule_[i + 1]);
    }

    QL_REQUIRE(fundingLegs_.size() == fundingNotionalTypes_.size(), "TRSWrapper::TRSWrapper(): number of funding legs ("
                                                                        << fundingLegs_.size()
                                                                        << ") must match funding notitional types ("
                                                                        << fundingNotionalTypes_.size() << ")");

    QL_REQUIRE(!underlying_.empty(), "TRSWrapper::TRSWrapper(): no underlying given, at least one is required");
    QL_REQUIRE(underlying.size() == underlyingIndex.size(),
               "TRSWrapper::TRSWrapper(): number of underlyings ("
                   << underlying.size() << ") does not match underlying index size (" << underlyingIndex.size() << ")");
    QL_REQUIRE(underlying.size() == underlyingMultiplier.size(), "TRSWrapper::TRSWrapper(): number of underlyings ("
                                                                     << underlying.size()
                                                                     << ") does not match underlying index size ("
                                                                     << underlyingMultiplier.size() << ")");
    QL_REQUIRE(underlying.size() == assetCurrency.size(),
               "TRSWrapper::TRSWrapper(): number of underlyings ("
                   << underlying.size() << ") does not match asset currency size (" << assetCurrency.size() << ")");
    QL_REQUIRE(underlying.size() == fxIndexAsset.size(),
               "TRSWrapper::TRSWrapper(): number of underlyings ("
                   << underlying.size() << ") does not match fx index asset  size (" << fxIndexAsset.size() << ")");

    for (Size i = 0; i < underlying_.size(); ++i) {
        registerWith(underlying_[i]->instrument()->qlInstrument());
        registerWith(underlyingIndex_[i]);
    }

    for (Size i = 0; i < fundingLegs_.size(); ++i) {
        for (Size j = 0; j < fundingLegs_[i].size(); ++j) {
            registerWith(fundingLegs_[i][j]);
        }
    }

    for (auto const& f : fxIndexAsset)
        registerWith(f);
    registerWith(fxIndexReturn);
    registerWith(fxIndexAdditionalCashflows);

    // compute last payment date, after this date the TRS is considered expired

    lastDate_ = Date::minDate();
    for (auto const& d : paymentSchedule_)
        lastDate_ = std::max(lastDate_, d);
    for (auto const& l : fundingLegs_)
        for (auto const& c : l)
            lastDate_ = std::max(lastDate_, c->date());
    for (auto const& c : additionalCashflowLeg_)
        lastDate_ = std::max(lastDate_, c->date());
}

bool TRSWrapper::isExpired() const { return detail::simple_event(lastDate_).hasOccurred(); }

void TRSWrapper::setupArguments(PricingEngine::arguments* args) const {
    TRSWrapper::arguments* a = dynamic_cast<TRSWrapper::arguments*>(args);
    QL_REQUIRE(a != nullptr, "wrong argument type in TRSWrapper");
    a->underlying_ = underlying_;
    a->underlyingIndex_ = underlyingIndex_;
    a->underlyingMultiplier_ = underlyingMultiplier_;
    a->includeUnderlyingCashflowsInReturn_ = includeUnderlyingCashflowsInReturn_;
    a->initialPrice_ = initialPrice_;
    a->initialPriceCurrency_ = initialPriceCurrency_;
    a->assetCurrency_ = assetCurrency_;
    a->returnCurrency_ = returnCurrency_;
    a->valuationSchedule_ = valuationSchedule_;
    a->paymentSchedule_ = paymentSchedule_;
    a->fundingLegs_ = fundingLegs_;
    a->fundingNotionalTypes_ = fundingNotionalTypes_;
    a->fundingCurrency_ = fundingCurrency_;
    a->fundingResetGracePeriod_ = fundingResetGracePeriod_;
    a->paysAsset_ = paysAsset_;
    a->paysFunding_ = paysFunding_;
    a->additionalCashflowLeg_ = additionalCashflowLeg_;
    a->additionalCashflowLegPayer_ = additionalCashflowLegPayer_;
    a->additionalCashflowCurrency_ = additionalCashflowCurrency_;
    a->fxIndexAsset_ = fxIndexAsset_;
    a->fxIndexReturn_ = fxIndexReturn_;
    a->fxIndexAdditionalCashflows_ = fxIndexAdditionalCashflows_;
    a->addFxIndices_ = addFxIndices_;
}

void TRSWrapper::arguments::validate() const {
    QL_REQUIRE(!initialPriceCurrency_.empty(), "empty initial price currency");
    for (auto const& a : assetCurrency_)
        QL_REQUIRE(!a.empty(), "empty asset currency");
    QL_REQUIRE(!returnCurrency_.empty(), "empty return currency");
    QL_REQUIRE(!fundingCurrency_.empty(), "empty funding currency");
}

void TRSWrapper::fetchResults(const PricingEngine::results* r) const { Instrument::fetchResults(r); }

bool TRSWrapperAccrualEngine::computeStartValue(std::vector<Real>& underlyingStartValue,
                                                std::vector<Real>& fxConversionFactor, QuantLib::Date& startDate,
                                                QuantLib::Date& endDate, bool& usingInitialPrice,
                                                const Size nth) const {
    Date today = Settings::instance().evaluationDate();
    Size payIdx =
        std::distance(arguments_.paymentSchedule_.begin(),
                      std::upper_bound(arguments_.paymentSchedule_.begin(), arguments_.paymentSchedule_.end(), today)) +
        nth;
    Date v0 = payIdx < arguments_.valuationSchedule_.size() ? arguments_.valuationSchedule_[payIdx] : Date::maxDate();
    Date v1 =
        payIdx < arguments_.valuationSchedule_.size() - 1 ? arguments_.valuationSchedule_[payIdx + 1] : Date::maxDate();

    /* Check whether there is a "nth" current valuation period, nth > 0. */
    if (nth > 0 && (payIdx >= arguments_.paymentSchedule_.size() || v0 > today))
        return false;

    std::fill(underlyingStartValue.begin(), underlyingStartValue.end(), 0.0);
    std::fill(fxConversionFactor.begin(), fxConversionFactor.end(), 1.0);
    startDate = Null<Date>();
    endDate = Null<Date>();
    usingInitialPrice = false;

    for (Size i = 0; i < arguments_.underlying_.size(); ++i) {
        if (payIdx < arguments_.paymentSchedule_.size()) {
            if (v0 > today) {
                // The start valuation date is > today: we return null, except an initial price is given, in which case
                // we return this price (possibly converted with todays FX rate to return ccy). This allows for a
                // reasonable asset leg npv estimation, which would otherwise be zero and jump to its actual value on v0
                // + 1. Internal consistency check: make sure that v0 is the initial date of the valuation schedule
                QL_REQUIRE(payIdx == 0, "TRSWrapper: internal error, expected valuation date "
                                            << v0 << " for pay date = " << arguments_.paymentSchedule_[payIdx]
                                            << " to be the first valuation date, since it is > today (" << today
                                            << ")");
                if (nth == 0 && arguments_.initialPrice_ != Null<Real>()) {
                    if (i == 0) {
                        Real s0 =
                            arguments_.initialPrice_ *
                            (arguments_.underlyingMultiplier_.size() == 1 ? arguments_.underlyingMultiplier_[i] : 1.0);
                        Real fx0 = getFxConversionRate(today, arguments_.initialPriceCurrency_,
                                                       arguments_.returnCurrency_, false);
                        DLOG("start value (underlying "
                             << std::to_string(i + 1) << "): s0=" << s0 << " (from fixed initial price), fx0=" << fx0
                             << " => " << fx0 * s0 << " on today (valuation start date is " << v0 << ")");
                        underlyingStartValue[i] = s0;
                        fxConversionFactor[i] = fx0;
                        startDate = v0;
                        if(v1 <= today)
                            endDate = v1;
                        usingInitialPrice = true;
                    } else {
                        underlyingStartValue[i] = 0.0;
                        fxConversionFactor[i] = 1.0;
                    }
                } else {
                    DLOG("start value (underlying " << std::to_string(i + 1) << ") is null, because eval date ("
                                                    << today << ") is <= start valuation date (" << v0
                                                    << ") for nth current period " << nth
                                                    << " and no intiial price is given");
                    underlyingStartValue[i] = Null<Real>();
                    fxConversionFactor[i] = Null<Real>();
                    startDate = Null<Date>();
                }
            } else {
                // The start valuation date is <= today, we determine the start value from the initial price or a
                // historical fixing
                Real s0 = 0.0, fx0 = 1.0;
                if (nth == 0 && arguments_.initialPrice_ != Null<Real>() &&
                    v0 == arguments_.valuationSchedule_.front()) {
                    if (i == 0) {
                        DLOG("initial price is given as " << arguments_.initialPrice_ << " "
                                                          << arguments_.initialPriceCurrency_);
                        s0 = arguments_.initialPrice_ *
                             (arguments_.underlying_.size() == 1 ? arguments_.underlyingMultiplier_[i] : 1.0);
                        fx0 = getFxConversionRate(v0, arguments_.initialPriceCurrency_, arguments_.returnCurrency_,
                                                  false);
                        usingInitialPrice = true;
                    }
                } else {
                    s0 = getUnderlyingFixing(i, v0, false) * arguments_.underlyingMultiplier_[i];
                    fx0 = getFxConversionRate(v0, arguments_.assetCurrency_[i], arguments_.returnCurrency_, false);
                }
                DLOG("start value (underlying " << std::to_string(i + 1) << "): s0=" << s0 << " fx0=" << fx0 << " => "
                                                << fx0 * s0 << " on " << v0 << " in nth current period " << nth);
                underlyingStartValue[i] = s0;
                fxConversionFactor[i] = fx0;
                startDate = v0;
                if(v1 <= today)
                    endDate = v1;
            }
        } else {
            // we are beyond the last date in the payment schedule => return null
            DLOG("start value (underlying " << std::to_string(i + 1) << ") is null, because eval date (" << today
                                            << ") is >= last date in payment schedule ("
                                            << arguments_.paymentSchedule_.back() << ") in nth current period " << nth);
            underlyingStartValue[i] = Null<Real>();
            fxConversionFactor[i] = Null<Real>();
            startDate = Null<Date>();
        }
    } // loop over underlyings

    return true;
}

namespace {
Real getFxIndexFixing(const QuantLib::ext::shared_ptr<FxIndex>& fx, const Currency& source, const Date& d,
                      const bool enforceProjection) {
    bool invert = fx->targetCurrency() == source;
    Real res;
    if (enforceProjection) {
        res = fx->forecastFixing(0.0);
    } else {
        Date adjustedDate = fx->fixingCalendar().adjust(d, Preceding);
        res = fx->fixing(adjustedDate, false);
    }
    return invert ? 1.0 / res : res;
}
} // namespace

Real TRSWrapperAccrualEngine::getFxConversionRate(const Date& date, const Currency& source, const Currency& target,
                                                  const bool enforceProjection) const {

    if (source == target)
        return 1.0;

    Real result1 = 1.0;
    if (source != arguments_.fundingCurrency_) {
        bool found = false;
        for (Size i = 0; i < arguments_.fxIndexAsset_.size(); ++i) {
            if (arguments_.fxIndexAsset_[i] == nullptr)
                continue;
            if (source == arguments_.fxIndexAsset_[i]->sourceCurrency() ||
                source == arguments_.fxIndexAsset_[i]->targetCurrency()) {
                result1 = getFxIndexFixing(arguments_.fxIndexAsset_[i], source, date, enforceProjection);
                found = true;
            }
        }
        if (!found) {
            if (arguments_.fxIndexReturn_ != nullptr && (source == arguments_.fxIndexReturn_->sourceCurrency() ||
                                                         source == arguments_.fxIndexReturn_->targetCurrency())) {
                result1 = getFxIndexFixing(arguments_.fxIndexReturn_, source, date, enforceProjection);
            } else if (arguments_.fxIndexAdditionalCashflows_ != nullptr &&
                       (source == arguments_.fxIndexAdditionalCashflows_->sourceCurrency() ||
                        source == arguments_.fxIndexAdditionalCashflows_->targetCurrency())) {
                result1 = getFxIndexFixing(arguments_.fxIndexAdditionalCashflows_, source, date, enforceProjection);
            } else {
                QL_FAIL("TRSWrapperAccrualEngine: could not convert " << source.code() << " to funding currency "
                                                                      << arguments_.fundingCurrency_
                                                                      << ", are all required FXTerms set up?");
            }
        }
    }

    Real result2 = 1.0;
    if (target != arguments_.fundingCurrency_) {
        bool found = false;
        for (Size i = 0; i < arguments_.fxIndexAsset_.size(); ++i) {
            if (arguments_.fxIndexAsset_[i] == nullptr)
                continue;
            if (target == arguments_.fxIndexAsset_[i]->sourceCurrency() ||
                target == arguments_.fxIndexAsset_[i]->targetCurrency()) {
                result2 = getFxIndexFixing(arguments_.fxIndexAsset_[i], target, date, enforceProjection);
                found = true;
            }
        }
        if (!found) {
            if (arguments_.fxIndexReturn_ != nullptr && (target == arguments_.fxIndexReturn_->sourceCurrency() ||
                                                         target == arguments_.fxIndexReturn_->targetCurrency())) {
                result2 = getFxIndexFixing(arguments_.fxIndexReturn_, target, date, enforceProjection);
            } else if (arguments_.fxIndexAdditionalCashflows_ != nullptr &&
                       (target == arguments_.fxIndexAdditionalCashflows_->sourceCurrency() ||
                        target == arguments_.fxIndexAdditionalCashflows_->targetCurrency())) {
                result2 = getFxIndexFixing(arguments_.fxIndexAdditionalCashflows_, target, date, enforceProjection);
            } else {
                QL_FAIL("TRSWrapperAccrualEngine: could not convert " << source.code() << " to funding currency "
                                                                      << arguments_.fundingCurrency_
                                                                      << ", are all required FXTerms set up?");
            }
        }
    }

    return result1 / result2;
}

Real TRSWrapperAccrualEngine::getUnderlyingFixing(const Size i, const Date& date, const bool enforceProjection) const {
    Date today = Settings::instance().evaluationDate();
    QL_REQUIRE(date <= today, "TRSWrapperAccrualEngine: internal error, getUnderlyingFixing("
                                  << date << ") for future date requested (today=" << today << ")");
    if (enforceProjection) {
        return arguments_.underlying_[i]->instrument()->NPV() / arguments_.underlyingMultiplier_[i];
    }
    Date adjustedDate = arguments_.underlyingIndex_[i]->fixingCalendar().adjust(date, Preceding);
    try {
        auto tmp = arguments_.underlyingIndex_[i]->fixing(adjustedDate);
        return tmp;
    } catch (const std::exception&) {
        if (adjustedDate == today)
            return arguments_.underlying_[i]->instrument()->NPV() / arguments_.underlyingMultiplier_[i];
        else
            throw;
    }
}

void TRSWrapperAccrualEngine::calculate() const {

    Date today = Settings::instance().evaluationDate();

    DLOG("TRSWrapperAccrualEngine: today = " << today << ", paysAsset = " << std::boolalpha << arguments_.paysAsset_
                                             << ", paysFunding = " << std::boolalpha << arguments_.paysFunding_);

    Real assetMultiplier = (arguments_.paysAsset_ ? -1.0 : 1.0);
    Real fundingMultiplier = (arguments_.paysFunding_ ? -1.0 : 1.0);

    results_.additionalResults["returnCurrency"] = arguments_.returnCurrency_.code();
    results_.additionalResults["fundingCurrency"] = arguments_.fundingCurrency_.code();
    results_.additionalResults["returnLegInitialPrice"] = arguments_.initialPrice_;
    results_.additionalResults["returnLegInitialPriceCurrency"] = arguments_.initialPriceCurrency_.code();

    // asset leg valuation (accrual method)

    Real assetLegNpv = 0.0;
    Size nthCurrentPeriod = 0;
    std::vector<CashFlowResults> cfResults;

    std::vector<Real> underlyingStartValue(arguments_.underlying_.size(), 0.0);
    std::vector<Real> fxConversionFactor(arguments_.underlying_.size(), 1.0);
    Date startDate = Null<Date>();
    Date endDate = Null<Date>();
    bool usingInitialPrice;

    while (computeStartValue(underlyingStartValue, fxConversionFactor, startDate, endDate, usingInitialPrice,
                             nthCurrentPeriod)) {

        // vector holding cashflow results, we store these as an additional result

        for (Size i = 0; i < arguments_.underlying_.size(); ++i) {

            std::string resultSuffix = arguments_.underlying_.size() > 1 ? "_" + std::to_string(i + 1) : "";
            if (nthCurrentPeriod > 0)
                resultSuffix += "_nth(" + std::to_string(nthCurrentPeriod) + ")";

            results_.additionalResults["underlyingCurrency" + resultSuffix] = arguments_.assetCurrency_[i].code();

            if (underlyingStartValue[i] != Null<Real>()) {
                Real s1, fx1;
                if (endDate == Null<Date>()) {
                    s1 = arguments_.underlying_[i]->instrument()->NPV();
                    fx1 = getFxConversionRate(today, arguments_.assetCurrency_[i], arguments_.returnCurrency_, true);
                } else {
                    s1 = getUnderlyingFixing(i, endDate, false) * arguments_.underlyingMultiplier_[i];
                    fx1 = getFxConversionRate(endDate, arguments_.assetCurrency_[i], arguments_.returnCurrency_, false);
                }
                assetLegNpv += fx1 * s1 - underlyingStartValue[i] * fxConversionFactor[i];
                DLOG("end value (underlying " << std::to_string(i + 1) << "): s1=" << s1 << " fx1=" << fx1 << " => "
                                              << fx1 * s1 << " on "
                                              << io::iso_date(endDate == Null<Date>() ? today : endDate));

                // add details  return leg valuation to additional results
                results_.additionalResults["s0" + resultSuffix] = underlyingStartValue[i];
                results_.additionalResults["fx0" + resultSuffix] = fxConversionFactor[i];
                results_.additionalResults["s1" + resultSuffix] = s1;
                results_.additionalResults["fx1" + resultSuffix] = fx1;
                results_.additionalResults["underlyingMultiplier" + resultSuffix] = arguments_.underlyingMultiplier_[i];

                // add return cashflow to additional results
                cfResults.emplace_back();
                cfResults.back().amount = fx1 * s1;
                if (arguments_.underlying_.size() == 1 || !usingInitialPrice) {
                    cfResults.back().amount -= underlyingStartValue[i] * fxConversionFactor[i];
                }
                cfResults.back().amount *= assetMultiplier;
                cfResults.back().payDate = today;
                cfResults.back().currency = arguments_.returnCurrency_.code();
                cfResults.back().legNumber = 0;
                cfResults.back().type = "AccruedReturn" + resultSuffix;
                cfResults.back().accrualStartDate = startDate;
                cfResults.back().accrualEndDate = endDate == Null<Date>() ? today : endDate;
                cfResults.back().fixingValue = s1 / arguments_.underlyingMultiplier_[i];
                cfResults.back().notional = underlyingStartValue[i] * fxConversionFactor[i];

                // if initial price is used and there is more than one underlying, add cf for initialPrice
                if (arguments_.underlying_.size() > 1 && usingInitialPrice && i == 0) {
                    cfResults.emplace_back();
                    cfResults.back().amount = -underlyingStartValue[i] * fxConversionFactor[i];
                    cfResults.back().payDate = today;
                    cfResults.back().currency = arguments_.returnCurrency_.code();
                    cfResults.back().legNumber = 0;
                    cfResults.back().type = "AccruedReturn" + resultSuffix;
                    cfResults.back().accrualStartDate = startDate;
                    cfResults.back().accrualEndDate = endDate == Null<Date>() ? today : endDate;
                    cfResults.back().notional = underlyingStartValue[i] * fxConversionFactor[i];
                }

                // startDate might be >= today, if an initial price is given, see the comment in startValue() above
                if (arguments_.includeUnderlyingCashflowsInReturn_ && startDate != Null<Date>() && startDate < today) {
                    // add cashflows in return period
                    Real cf = 0.0;
                    for (auto const& l : arguments_.underlying_[i]->legs()) {
                        for (auto const& c : l) {
                            if (!c->hasOccurred(startDate) && c->hasOccurred(today)) {
                                Real tmp = c->amount() * arguments_.underlyingMultiplier_[i];
                                cf += tmp;
                                // add intermediate cashflows to additional results
                                cfResults.emplace_back();
                                cfResults.back().amount = assetMultiplier * (tmp * fx1);
                                cfResults.back().payDate = c->date();
                                cfResults.back().currency = arguments_.returnCurrency_.code();
                                cfResults.back().legNumber = 1;
                                cfResults.back().type = "UnderlyingCashFlow" + resultSuffix;
                                cfResults.back().notional = underlyingStartValue[i] * fxConversionFactor[i];
                            }
                        }
                    }
                    // account for dividends
                    Real dividends = 0.0;
                    if (auto e = QuantLib::ext::dynamic_pointer_cast<EquityIndex2>(arguments_.underlyingIndex_[i])) {
                        dividends +=
                            e->dividendsBetweenDates(startDate + 1, today) * arguments_.underlyingMultiplier_[i];
                    } else if (auto e = QuantLib::ext::dynamic_pointer_cast<CompositeIndex>(arguments_.underlyingIndex_[i])) {
                        dividends +=
                            e->dividendsBetweenDates(startDate + 1, today) * arguments_.underlyingMultiplier_[i];
                    }
                    cf += dividends;
                    if (!close_enough(dividends, 0.0)) {
                        // add dividends as one cashflow to additional results
                        cfResults.emplace_back();
                        cfResults.back().amount = assetMultiplier * (dividends * fx1);
                        cfResults.back().payDate = today;
                        cfResults.back().currency = arguments_.returnCurrency_.code();
                        cfResults.back().legNumber = 2;
                        cfResults.back().type = "UnderlyingDividends" + resultSuffix;
                        cfResults.back().notional = underlyingStartValue[i] * fxConversionFactor[i];
                    }

                    DLOG("add cashflows in return period (" << io::iso_date(startDate) << ", " << io::iso_date(today)
                                                            << "]: amount in asset ccy = " << cf << ", fx conversion "
                                                            << fx1 << " => " << cf * fx1);

                    results_.additionalResults["underlyingCashflows" + resultSuffix] = cf;

                    assetLegNpv += cf * fx1;
                }
            }
        } // loop over underlyings

        ++nthCurrentPeriod;
    } // loop over nth current period

    results_.additionalResults["assetLegNpv"] = assetMultiplier * assetLegNpv;
    results_.additionalResults["assetLegNpvCurency"] = arguments_.returnCurrency_.code();
    DLOG("asset leg npv = " << assetMultiplier * assetLegNpv << " " << arguments_.returnCurrency_.code());

    // funding leg valuation (accrual method)

    Real fundingLegNpv = 0.0;

    for (Size i = 0; i < arguments_.fundingLegs_.size(); ++i) {

        Size nthCpn = 0;
        for (Size cpnNo = 0; cpnNo < arguments_.fundingLegs_[i].size(); ++cpnNo) {

            Real localFundingLegNpv = 0.0; // local per funding coupon

            auto cpn = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(arguments_.fundingLegs_[i][cpnNo]);
            if (cpn == nullptr || cpn->date() <= today || cpn->accrualStartDate() >= today)
                continue;

            // look up latest valuation date <= funding start date, fall back to first valuation date, if no such
            // date exists
            Date fundingStartDate = cpn->accrualStartDate();
            Real fundingCouponNotional = cpn->nominal();
            Size currentIdx = std::distance(arguments_.valuationSchedule_.begin(),
                                            std::upper_bound(arguments_.valuationSchedule_.begin(),
                                                             arguments_.valuationSchedule_.end(),
                                                             fundingStartDate + arguments_.fundingResetGracePeriod_));
            if (currentIdx > 0)
                --currentIdx;

            if (arguments_.valuationSchedule_[currentIdx] > today) {
                DLOG("fundingLegNpv = 0 for funding leg #" << (i + 1) << ", because last relevant valuation date ("
                                                           << arguments_.valuationSchedule_[currentIdx]
                                                           << ") is >= eval date (" << today << ")");
                continue;
            }

            localFundingLegNpv = cpn->accruedAmount(today);
            Real fundingLegNotionalFactor = 0.0;
            std::string resultSuffix = arguments_.fundingLegs_.size() > 1 ? "_" + std::to_string(i + 1) : "";

            try {
                results_.additionalResults["fundingCouponRate" + resultSuffix] = cpn->rate();
            } catch (...) {
            }

            bool isOvernightCoupon = QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(cpn) != nullptr;

            for (Size j = 0; j < arguments_.underlying_.size(); ++j) {

                std::string resultSuffix2 = arguments_.underlying_.size() > 1 ? "_" + std::to_string(j + 1) : "";

                if (nthCpn > 0)
                    resultSuffix2 += "_nth(" + std::to_string(nthCpn) + ")";

                if (arguments_.fundingNotionalTypes_[i] == TRS::FundingData::NotionalType::Fixed) {

                    fundingLegNotionalFactor = 1.0;

                } else if (arguments_.fundingNotionalTypes_[i] == TRS::FundingData::NotionalType::PeriodReset) {

                    Real localNotionalFactor = 0.0, localFxFactor = 1.0; // local per underlying
                    if (currentIdx == 0 && arguments_.initialPrice_ != Null<Real>()) {
                        if (j == 0) {
                            localNotionalFactor =
                                arguments_.initialPrice_ *
                                (arguments_.underlying_.size() == 1 ? arguments_.underlyingMultiplier_[j] : 1.0);
                            localFxFactor = getFxConversionRate(arguments_.valuationSchedule_[currentIdx],
                                                                arguments_.initialPriceCurrency_,
                                                                arguments_.fundingCurrency_, false);
                        }
                    } else {
                        localNotionalFactor = arguments_.underlyingMultiplier_[j] *
                                              getUnderlyingFixing(j, arguments_.valuationSchedule_[currentIdx], false);
                        localFxFactor =
                            getFxConversionRate(arguments_.valuationSchedule_[currentIdx], arguments_.assetCurrency_[j],
                                                arguments_.fundingCurrency_, false);
                    }

                    fundingLegNotionalFactor += localNotionalFactor * localFxFactor;

                    results_.additionalResults["fundingLegNotional" + resultSuffix + resultSuffix2] =
                        localNotionalFactor;
                    results_.additionalResults["fundingLegFxRate" + resultSuffix + resultSuffix2] = localFxFactor;

                } else if (arguments_.fundingNotionalTypes_[i] == TRS::FundingData::NotionalType::DailyReset &&
                           (QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(cpn) ||
                            QuantLib::ext::dynamic_pointer_cast<IborCoupon>(cpn))) {

                    Real dcfTotal =
                        cpn->dayCounter().yearFraction(cpn->accrualStartDate(), std::min(cpn->accrualEndDate(), today));
                    for (QuantLib::Date d = cpn->accrualStartDate(); d < std::min(cpn->accrualEndDate(), today); ++d) {
                        Real dcfLocal = cpn->dayCounter().yearFraction(d, d + 1);
                        Date fixingDate = arguments_.underlyingIndex_[j]->fixingCalendar().adjust(d, Preceding);
                        Real localNotionalFactor = getUnderlyingFixing(j, fixingDate, false) *
                                                   arguments_.underlyingMultiplier_[j] * dcfLocal / dcfTotal;
                        Real localFxFactor = getFxConversionRate(fixingDate, arguments_.assetCurrency_[j],
                                                                 arguments_.fundingCurrency_, false);
                        fundingLegNotionalFactor += localNotionalFactor * localFxFactor;

                        results_.additionalResults["fundingLegNotional" + resultSuffix + resultSuffix2 + "_" +
                                                   ore::data::to_string(d)] = localNotionalFactor;
                        results_.additionalResults["fundingLegFxRate" + resultSuffix + resultSuffix2 + "_" +
                                                   ore::data::to_string(d)] = localFxFactor;
                    }
                } else if (arguments_.fundingNotionalTypes_[i] == TRS::FundingData::NotionalType::DailyReset &&
                           isOvernightCoupon) {
                    auto overnightCpn = QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(cpn);
                    auto valueDates = overnightCpn->valueDates();
                    auto fixingValues = overnightCpn->indexFixings();
                    auto dts = overnightCpn->dt();
                    double accruedInterest = 0;
                    double accruedSpreadInterest = 0;
                    double gearing = overnightCpn->gearing();
                    double spread = overnightCpn->spread();
                    for (size_t i = 0; i < valueDates.size() - 1; ++i) {
                        const Date& valueDate = valueDates[i];
                        double dt = dts[i];
                        double irFixing = fixingValues[i];
                        if (overnightCpn->includeSpread())
                            irFixing += overnightCpn->spread();
                        if (valueDate < today) {
                            Date fixingDate =
                                arguments_.underlyingIndex_[j]->fixingCalendar().adjust(valueDate, Preceding);
                            Real localNotional =
                                getUnderlyingFixing(j, fixingDate, false) * arguments_.underlyingMultiplier_[j];
                            Real localFxFactor = getFxConversionRate(fixingDate, arguments_.assetCurrency_[j],
                                                                     arguments_.fundingCurrency_, false);
                            results_.additionalResults["fundingLegNotional" + resultSuffix + resultSuffix2 + "_" +
                                                       ore::data::to_string(valueDate)] = localNotional;
                            results_.additionalResults["fundingLegFxRate" + resultSuffix + resultSuffix2 + "_" +
                                                       ore::data::to_string(valueDate)] = localFxFactor;
                            results_.additionalResults["fundingLegOISRate" + resultSuffix + resultSuffix2 + "_" +
                                                       ore::data::to_string(valueDate)] = irFixing;
                            results_.additionalResults["fundingLegDCF" + resultSuffix + resultSuffix2 + "_" +
                                                       ore::data::to_string(valueDate)] = dt;
                            localNotional *= localFxFactor;
                            accruedInterest = localNotional * irFixing * dt + accruedInterest * (1 + irFixing * dt);
                            if (!overnightCpn->includeSpread()) {
                                accruedSpreadInterest += localNotional * spread * dt;
                            }
                            results_.additionalResults["fundingLegAccruedInterest" + resultSuffix + resultSuffix2 +
                                                       "_" + ore::data::to_string(valueDate)] =
                                accruedInterest + accruedSpreadInterest;
                        }
                    }
                    fundingLegNotionalFactor = (gearing * accruedInterest + accruedSpreadInterest) / localFundingLegNpv;
                } else if (arguments_.fundingNotionalTypes_[i] == TRS::FundingData::NotionalType::DailyReset &&
                           QuantLib::ext::dynamic_pointer_cast<AverageONIndexedCoupon>(cpn) != nullptr) {
                    auto overnightCpn = QuantLib::ext::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(cpn);
                    auto valueDates = overnightCpn->valueDates();
                    auto fixingValues = overnightCpn->indexFixings();
                    auto dts = overnightCpn->dt();
                    double accruedInterest = 0;
                    double spread = overnightCpn->spread();
                    double gearing = overnightCpn->gearing();
                    for (size_t i = 0; i < valueDates.size() - 1; ++i) {
                        const Date& valueDate = valueDates[i];
                        double dt = dts[i];
                        double irFixing = fixingValues[i];

                        if (valueDate < today) {
                            Date fixingDate =
                                arguments_.underlyingIndex_[j]->fixingCalendar().adjust(valueDate, Preceding);
                            Real localNotional =
                                getUnderlyingFixing(j, fixingDate, false) * arguments_.underlyingMultiplier_[j];
                            Real localFxFactor = getFxConversionRate(fixingDate, arguments_.assetCurrency_[j],
                                                                     arguments_.fundingCurrency_, false);
                            results_.additionalResults["fundingLegNotional" + resultSuffix + resultSuffix2 + "_" +
                                                       ore::data::to_string(valueDate)] = localNotional;
                            results_.additionalResults["fundingLegFxRate" + resultSuffix + resultSuffix2 + "_" +
                                                       ore::data::to_string(valueDate)] = localFxFactor;
                            results_.additionalResults["fundingLegOISRate" + resultSuffix + resultSuffix2 + "_" +
                                                       ore::data::to_string(valueDate)] = irFixing;
                            results_.additionalResults["fundingLegDCF" + resultSuffix + resultSuffix2 + "_" +
                                                       ore::data::to_string(valueDate)] = dt;
                            localNotional *= localFxFactor;
                            accruedInterest += localNotional * (gearing * irFixing + spread) * dt;
                            results_.additionalResults["fundingLegAccruedInterest" + resultSuffix + resultSuffix2 +
                                                       "_" + ore::data::to_string(valueDate)] = accruedInterest;
                        }
                    }
                    fundingLegNotionalFactor = accruedInterest / localFundingLegNpv;
                } else if (arguments_.fundingNotionalTypes_[i] == TRS::FundingData::NotionalType::DailyReset) {
                    QL_FAIL("daily reset funding legs support fixed rate, ibor and overnight indexed coupons only");
                } else {
                    QL_FAIL("internal error: unknown notional type, contact dev");
                }
            } // loop over underlyings

            DLOG("fundingLegNpv for funding leg #"
                 << (i + 1) << " is " << fundingMultiplier * localFundingLegNpv << " * " << fundingLegNotionalFactor
                 << " = " << fundingMultiplier * localFundingLegNpv * fundingLegNotionalFactor << " "
                 << arguments_.fundingCurrency_ << " (notional type of leg is '" << arguments_.fundingNotionalTypes_[i]
                 << "')");

            localFundingLegNpv *= fundingLegNotionalFactor;

            results_.additionalResults["fundingLegNpv" + resultSuffix] = fundingMultiplier * localFundingLegNpv;

            // add funding leg cashflow to addtional results
            cfResults.emplace_back();
            cfResults.back().amount = fundingMultiplier * localFundingLegNpv;
            cfResults.back().payDate = today;
            cfResults.back().currency = arguments_.fundingCurrency_.code();
            cfResults.back().legNumber = 3 + i;
            cfResults.back().type = "AccruedFunding" + (nthCpn > 0 ? "_nth(" + std::to_string(nthCpn) + ")" : "");
            cfResults.back().accrualStartDate = std::min(fundingStartDate, today);
            cfResults.back().accrualEndDate = today;
            cfResults.back().notional = arguments_.fundingNotionalTypes_[i] == TRS::FundingData::NotionalType::Fixed
                                            ? fundingCouponNotional
                                            : fundingLegNotionalFactor;

            results_.additionalResults["fundingLegNotional" + resultSuffix] = cfResults.back().notional;

            fundingLegNpv += localFundingLegNpv;
            ++nthCpn;
        } // loop over funding leg coupons (indexed by cpnNo)
    }     // loop over funding legs (indexed by i)

    DLOG("total funding leg(s) npv is " << fundingMultiplier * fundingLegNpv);

    results_.additionalResults["fundingLegNpv"] = fundingMultiplier * fundingLegNpv;
    results_.additionalResults["fundingLegNpvCurrency"] = arguments_.fundingCurrency_.code();

    // additional cashflow leg valuation (take the plain amount of future cashflows as if paid today)

    Real additionalCashflowLegNpv = 0.0;
    for (auto const& cf : arguments_.additionalCashflowLeg_) {
        if (cf->date() > today) {
            Real tmp = cf->amount() * (arguments_.additionalCashflowLegPayer_ ? -1.0 : 1.0);
            additionalCashflowLegNpv += tmp;
            // add additional cashflows to additional results
            cfResults.emplace_back();
            cfResults.back().amount = tmp;
            cfResults.back().payDate = cf->date();
            cfResults.back().currency = arguments_.additionalCashflowCurrency_.code();
            cfResults.back().legNumber = 0;
            cfResults.back().type = "AdditionalCashFlow";
        }
    }
    DLOG("additionalCashflowLegNpv = " << additionalCashflowLegNpv << " " << arguments_.additionalCashflowCurrency_);
    results_.additionalResults["additionalCashflowLegNpv"] = additionalCashflowLegNpv;
    results_.additionalResults["additionalCashflowLegNpvCurrency"] = arguments_.additionalCashflowCurrency_.code();

    // set npv and current notional, set additional results

    Real fxAssetToPnlCcy = getFxConversionRate(today, arguments_.returnCurrency_, arguments_.fundingCurrency_, true);
    Real fxAdditionalCashflowLegToPnlCcy =
        getFxConversionRate(today, arguments_.additionalCashflowCurrency_, arguments_.fundingCurrency_, true);

    results_.additionalResults["fxConversionAssetLegNpvToPnlCurrency"] = fxAssetToPnlCcy;
    results_.additionalResults["fxConversionAdditionalCashflowLegNpvToPnlCurrency"] = fxAdditionalCashflowLegToPnlCcy;
    results_.additionalResults["pnlCurrency"] = arguments_.fundingCurrency_.code();

    results_.value = assetMultiplier * assetLegNpv * fxAssetToPnlCcy + fundingMultiplier * fundingLegNpv +
                     additionalCashflowLegNpv * fxAdditionalCashflowLegToPnlCcy;

    Real currentNotional = 0.0;
    for (Size j = 0; j < arguments_.underlying_.size(); ++j) {
        // this is using the underlyingStartValue and fxConversionFactor that was populated during the
        // valuation of the asset leg above in the last "nth current period" which contributed to this npv
        if (underlyingStartValue[j] == Null<Real>()) {
            currentNotional +=
                arguments_.underlyingMultiplier_[j] * getUnderlyingFixing(j, today, true) *
                getFxConversionRate(today, arguments_.initialPriceCurrency_, arguments_.returnCurrency_, true);
        } else {
            currentNotional += underlyingStartValue[j] * fxConversionFactor[j];
        }
    }

    for (Size j = 0; j < arguments_.underlying_.size(); ++j) {
        // the start fixing will refer to the last of the nth current return periods
        std::string resultSuffix = arguments_.underlying_.size() == 1 ? "" : "_" + std::to_string(j);
        Real startFixing = Null<Real>(), todaysFixing = Null<Real>();
        try {
            startFixing = getUnderlyingFixing(j, startDate, false);
        } catch (...) {
        }
        try {
            todaysFixing = getUnderlyingFixing(j, today, true);
        } catch (...) {
        }
        results_.additionalResults["startFixing" + resultSuffix] = startFixing;
        results_.additionalResults["todaysFixing" + resultSuffix] = todaysFixing;
    }

    for (auto const& d : arguments_.addFxIndices_) {
        Real startFixing = Null<Real>(), todaysFixing = Null<Real>();
        try {
            startFixing = d.second->fixing(d.second->fixingCalendar().adjust(startDate, Preceding));
        } catch (...) {
        }
        try {
            todaysFixing = d.second->fixing(d.second->fixingCalendar().adjust(today, Preceding), true);
        } catch (...) {
        }
        results_.additionalResults["startFxFixing(" + d.first + ")"] = startFixing;
        results_.additionalResults["todaysFxFixing(" + d.first + ")"] = todaysFixing;
    }

    results_.additionalResults["currentNotional"] = currentNotional * fxAssetToPnlCcy;
    results_.additionalResults["cashFlowResults"] = cfResults;

    // propagate underlying additional results to trswrapper

    for (Size i = 0; i < arguments_.underlying_.size(); ++i) {
        for (auto const& [key, value] : arguments_.underlying_[i]->instrument()->additionalResults()) {
            results_.additionalResults["und_ar_" + std::to_string(i + 1) + "_" + key] = value;
        }
    }

    DLOG("TRSWrapperAccrualEngine: all done, total npv = " << results_.value << " "
                                                           << arguments_.fundingCurrency_.code());
}

} // namespace data
} // namespace ore
