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
#include <ored/portfolio/bondposition.hpp>
#include <ored/portfolio/compositeinstrumentwrapper.hpp>
#include <qle/indexes/compositeindex.hpp>

#include <ored/utilities/to_string.hpp>

#include <qle/instruments/cashflowresults.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/currencies/exchangeratemanager.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/zerofixedcoupon.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;
using std::map;
using std::string;

TRSWrapper::TRSWrapper(
    const std::vector<QuantLib::ext::shared_ptr<ore::data::Trade>>& underlying,
    const std::vector<QuantLib::ext::shared_ptr<Index>>& underlyingIndex, const std::vector<Real> underlyingMultiplier,
    const bool includeUnderlyingCashflowsInReturn, const Real initialPrice, const Real portfolioInitialPrice, const std::string portfolioId, const Currency& initialPriceCurrency,
    const std::vector<Currency>& assetCurrency, const Currency& returnCurrency,
    const std::vector<Date>& valuationSchedule, const std::vector<Date>& paymentSchedule,
    const std::vector<Leg>& fundingLegs, const std::vector<TRS::FundingData::NotionalType>& fundingNotionalTypes,
    const Currency& fundingCurrency, const Size fundingResetGracePeriod, const bool paysAsset, const bool paysFunding,
    const Leg& additionalCashflowLeg, const bool additionalCashflowLegPayer, const Currency& additionalCashflowCurrency,
    const std::vector<QuantLib::ext::shared_ptr<FxIndex>>& fxIndexAsset, const QuantLib::ext::shared_ptr<FxIndex>& fxIndexReturn,
    const QuantLib::ext::shared_ptr<FxIndex>& fxIndexAdditionalCashflows,
    const std::map<std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& addFxIndices,
    const QuantLib::ext::optional<TRS::FXConversion>& fxConversion, Real indexQuantity, bool pricePerIndexUnit)

    : underlying_(underlying), underlyingIndex_(underlyingIndex), underlyingMultiplier_(underlyingMultiplier),
      includeUnderlyingCashflowsInReturn_(includeUnderlyingCashflowsInReturn), initialPrice_(initialPrice),
      portfolioInitialPrice_(portfolioInitialPrice), portfolioId_(portfolioId),
      initialPriceCurrency_(initialPriceCurrency), assetCurrency_(assetCurrency), returnCurrency_(returnCurrency),
      valuationSchedule_(valuationSchedule), paymentSchedule_(paymentSchedule), fundingLegs_(fundingLegs),
      fundingNotionalTypes_(fundingNotionalTypes), fundingCurrency_(fundingCurrency),
      fundingResetGracePeriod_(fundingResetGracePeriod), paysAsset_(paysAsset), paysFunding_(paysFunding),
      additionalCashflowLeg_(additionalCashflowLeg), additionalCashflowLegPayer_(additionalCashflowLegPayer),
      additionalCashflowCurrency_(additionalCashflowCurrency), fxIndexAsset_(fxIndexAsset),
      fxIndexReturn_(fxIndexReturn), fxIndexAdditionalCashflows_(fxIndexAdditionalCashflows),
      addFxIndices_(addFxIndices), fxConversion_(fxConversion), indexQuantity_(indexQuantity),
      pricePerIndexUnit_(pricePerIndexUnit) {

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
        if (auto compositeWrapper = QuantLib::ext::dynamic_pointer_cast<CompositeInstrumentWrapper>(
                underlying_[i]->instrument())) {
            for (const auto& w : compositeWrapper->wrappers())
                registerWith(w->qlInstrument());
        } else {
            registerWith(underlying_[i]->instrument()->qlInstrument());
        }
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

    // If we have a portfolio ID and the price per index unit flag is set, we set the basket level index.
    isBespokeIndex_ = !portfolioId_.empty() && pricePerIndexUnit_;
    if (isBespokeIndex_) {
        basketIndex_ = ext::make_shared<QuantExt::GenericIndex>("GENERIC-" + portfolioId_);
    }
}

bool TRSWrapper::isExpired() const {
    ext::optional<bool> includeToday = Settings::instance().includeTodaysCashFlows();
    Date refDate = Settings::instance().evaluationDate();
    return detail::simple_event(lastDate_).hasOccurred(refDate, includeToday);
}

void TRSWrapper::setupArguments(PricingEngine::arguments* args) const {
    TRSWrapper::arguments* a = dynamic_cast<TRSWrapper::arguments*>(args);
    QL_REQUIRE(a != nullptr, "wrong argument type in TRSWrapper");
    a->underlying_ = underlying_;
    a->underlyingIndex_ = underlyingIndex_;
    a->underlyingMultiplier_ = underlyingMultiplier_;
    a->includeUnderlyingCashflowsInReturn_ = includeUnderlyingCashflowsInReturn_;
    a->initialPrice_ = initialPrice_;
    a->portfolioInitialPrice_ = portfolioInitialPrice_;
    a->portfolioId_ = portfolioId_;
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
    a->fxConversion_ = fxConversion_;
    a->indexQuantity_ = indexQuantity_;
    a->pricePerIndexUnit_ = pricePerIndexUnit_;
    a->basketIndex_ = basketIndex_;
    a->isBespokeIndex_ = isBespokeIndex_;
}

void TRSWrapper::arguments::validate() const {
    QL_REQUIRE(!initialPriceCurrency_.empty(), "empty initial price currency");
    for (auto const& a : assetCurrency_)
        QL_REQUIRE(!a.empty(), "empty asset currency");
    QL_REQUIRE(!returnCurrency_.empty(), "empty return currency");
    QL_REQUIRE(!fundingCurrency_.empty(), "empty funding currency");
}

void TRSWrapper::fetchResults(const PricingEngine::results* r) const { Instrument::fetchResults(r); }

void TRSWrapper::calculate() const {
    Instrument::calculate();
    setCalculated(true);
}

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
        if (arguments_.initialPrice_ == Null<Real>() && !arguments_.portfolioId_.empty()) {
            arguments_.initialPrice_ = arguments_.portfolioInitialPrice_;
        }      
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
                        Real factor = arguments_.underlyingMultiplier_.size() == 1 ?
                            arguments_.underlyingMultiplier_[i] : arguments_.indexQuantity_;
                        Real s0 = arguments_.initialPrice_ * factor;
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
                Date fxDate = arguments_.fxConversion_ != TRS::FXConversion::End
                                  ? v0
                                  : (endDate == Null<Date>() ? today : endDate);
                Real s0 = 0.0, fx0 = 1.0;
                if (nth == 0 && arguments_.initialPrice_ != Null<Real>() &&
                    v0 == arguments_.valuationSchedule_.front()) {
                    if (i == 0) {
                        DLOG("initial price is given as " << arguments_.initialPrice_ << " "
                                                          << arguments_.initialPriceCurrency_);
                        Real factor = arguments_.underlying_.size() == 1 ?
                            arguments_.underlyingMultiplier_[i] : arguments_.indexQuantity_;
                        s0 = arguments_.initialPrice_ * factor;
                        fx0 = getFxConversionRate(fxDate, arguments_.initialPriceCurrency_, arguments_.returnCurrency_,
                                                  false);
                        usingInitialPrice = true;
                    }
                } else {
                    std::map<std::string, QuantLib::ext::any> s0AdditionalData;
                    s0 = getUnderlyingFixing(i, v0, false, s0AdditionalData) * arguments_.underlyingMultiplier_[i];
                    for (const auto& [key, value] : s0AdditionalData) {
                        results_.additionalResults["s0_" + key] = value;
                    }
                    fx0 = getFxConversionRate(fxDate, arguments_.assetCurrency_[i], arguments_.returnCurrency_, false);
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

TRSWrapperAccrualEngine::TRSWrapperAccrualEngine(
    const Handle<YieldTermStructure>& additionalCashflowCurrencyDiscountCurve)
    : additionalCashflowCurrencyDiscountCurve_(additionalCashflowCurrencyDiscountCurve) {}

Real TRSWrapperAccrualEngine::getFxConversionRate(const Date& date, const Currency& source, const Currency& target,
                                                  const bool enforceProjection) const {

    if (source == target)
        return 1.0;

    return fxLegFactor(source, date, enforceProjection) / fxLegFactor(target, date, enforceProjection);
}

Real TRSWrapperAccrualEngine::fxLegFactor(const Currency& ccy, const Date& date, const bool enforceProjection) const {

    if (ccy == arguments_.fundingCurrency_)
        return 1.0;

    Real result = 1.0;
    bool found = false;
    // if several asset fx indices match, the last one wins (as in the original implementation)
    for (Size i = 0; i < arguments_.fxIndexAsset_.size(); ++i) {
        if (arguments_.fxIndexAsset_[i] == nullptr)
            continue;
        if (ccy == arguments_.fxIndexAsset_[i]->sourceCurrency() ||
            ccy == arguments_.fxIndexAsset_[i]->targetCurrency()) {
            result = getFxIndexFixing(arguments_.fxIndexAsset_[i], ccy, date, enforceProjection);
            found = true;
        }
    }
    if (!found) {
        if (arguments_.fxIndexReturn_ != nullptr && (ccy == arguments_.fxIndexReturn_->sourceCurrency() ||
                                                     ccy == arguments_.fxIndexReturn_->targetCurrency())) {
            result = getFxIndexFixing(arguments_.fxIndexReturn_, ccy, date, enforceProjection);
        } else if (arguments_.fxIndexAdditionalCashflows_ != nullptr &&
                   (ccy == arguments_.fxIndexAdditionalCashflows_->sourceCurrency() ||
                    ccy == arguments_.fxIndexAdditionalCashflows_->targetCurrency())) {
            result = getFxIndexFixing(arguments_.fxIndexAdditionalCashflows_, ccy, date, enforceProjection);
        } else {
            QL_FAIL("TRSWrapperAccrualEngine: could not convert " << ccy.code() << " to funding currency "
                                                                  << arguments_.fundingCurrency_
                                                                  << ", are all required FXTerms set up?");
        }
    }

    return result;
}

Real TRSWrapperAccrualEngine::getUnderlyingFixing(const Size i, const Date& date, const bool enforceProjection) const {
    std::map<std::string, QuantLib::ext::any> unused;
    return getUnderlyingFixing(i, date, enforceProjection, unused);
}

Real TRSWrapperAccrualEngine::getUnderlyingFixing(const Size i, const Date& date, const bool enforceProjection,
                                                std::map<std::string, QuantLib::ext::any>& fixingAdditionalData) const {
    Date today = Settings::instance().evaluationDate();
    QL_REQUIRE(date <= today, "TRSWrapperAccrualEngine: internal error, getUnderlyingFixing("
                                  << date << ") for future date requested (today=" << today << ")");
    if (enforceProjection) {
        auto tmp = getUnderlyingNPV(i, fixingAdditionalData);
        return QuantLib::close_enough(tmp, 0.0) ? 0.0 : tmp / arguments_.underlyingMultiplier_[i];
    }
    Date adjustedDate = arguments_.underlyingIndex_[i]->fixingCalendar().adjust(date, Preceding);
    try {
        auto tmp = arguments_.underlyingIndex_[i]->fixing(adjustedDate);
        return tmp;
    } catch (const std::exception&) {
        if (adjustedDate == today) {
            auto tmp = getUnderlyingNPV(i, fixingAdditionalData);
            return QuantLib::close_enough(tmp, 0.0) ? 0.0 : tmp / arguments_.underlyingMultiplier_[i];
        }
        else
            throw;
    }
}

Real TRSWrapperAccrualEngine::getUnderlyingNPV(const Size i) const {
    std::map<std::string, QuantLib::ext::any> unused;
    return getUnderlyingNPV(i, unused);
}

Real TRSWrapperAccrualEngine::getUnderlyingNPV(const Size i, std::map<std::string, QuantLib::ext::any>& fixingAdditionalData) const {
    if (QuantLib::ext::dynamic_pointer_cast<BondIndex>(arguments_.underlyingIndex_[i]) != nullptr ||
        QuantLib::ext::dynamic_pointer_cast<BondFuturesIndex>(arguments_.underlyingIndex_[i]) != nullptr) {
        Date today = Settings::instance().evaluationDate();
        return arguments_.underlyingIndex_[i]->fixing(today, true) * arguments_.underlyingMultiplier_[i]
            * arguments_.indexQuantity_;
    } else {
        if(auto bondPositionWrapper = QuantLib::ext::dynamic_pointer_cast<BondPositionInstrumentWrapper>(arguments_.underlying_[i]->instrument())){
            auto bondDetails = bondPositionWrapper->NPVBreakDown();
            for(Size k = 0; k < bondDetails.size(); k++){
                fixingAdditionalData["underlying["+ std::to_string(k)+ "]_weight"] = std::get<0>(bondDetails[k]);
                fixingAdditionalData["underlying["+ std::to_string(k)+ "]_bidAskSpread"] = std::get<1>(bondDetails[k]);
                fixingAdditionalData["underlying["+ std::to_string(k)+ "]_fxConversion"] = std::get<2>(bondDetails[k]);
                fixingAdditionalData["underlying["+ std::to_string(k)+ "]_npv"] = std::get<3>(bondDetails[k]);
            }
            return bondPositionWrapper->NPV() * arguments_.indexQuantity_;
        }
        return arguments_.underlying_[i]->instrument()->NPV() * arguments_.indexQuantity_;
    }
}

std::string TRSWrapperAccrualEngine::underlyingSuffix(Size i, Size nth) const {
    std::string suffix = arguments_.underlying_.size() > 1 ? "_" + std::to_string(i + 1) : "";
    if (nth > 0)
        suffix += "_nth(" + std::to_string(nth) + ")";
    return suffix;
}

void TRSWrapperAccrualEngine::calculate() const {

    if (arguments_.isBespokeIndex_) {
        calculateForIndex();
        return;
    }

    Date today = Settings::instance().evaluationDate();

    DLOG("TRSWrapperAccrualEngine: today = " << today << ", paysAsset = " << std::boolalpha << arguments_.paysAsset_
                                             << ", paysFunding = " << std::boolalpha << arguments_.paysFunding_);

    results_.additionalResults["returnCurrency"] = arguments_.returnCurrency_.code();
    results_.additionalResults["fundingCurrency"] = arguments_.fundingCurrency_.code();
    results_.additionalResults["returnLegInitialPrice"] = arguments_.initialPrice_;
    results_.additionalResults["returnLegInitialPriceCurrency"] = arguments_.initialPriceCurrency_.code();

    // vector holding cashflow results, we store these as an additional result
    std::vector<CashFlowResults> cfResults;

    // these are populated during the asset leg valuation (in the last "nth current period" that contributed to the
    // npv) and used afterwards for the current notional and start fixing additional results
    std::vector<Real> underlyingStartValue(arguments_.underlying_.size(), 0.0);
    std::vector<Real> fxConversionFactor(arguments_.underlying_.size(), 1.0);
    Date startDate = Null<Date>();

    // asset leg, funding leg(s) and additional cashflow leg valuation (accrual method); the returned leg values are
    // already multiplied by the respective payer sign
    Real assetLegVal = assetLegValue(cfResults, underlyingStartValue, fxConversionFactor, startDate);
    Real fundingLegVal = fundingLegValue(cfResults);
    Real acfLegVal = additionalCashflowLegValue(cfResults, 0);

    // set npv and additional results

    Real fxAssetToPnlCcy = getFxConversionRate(today, arguments_.returnCurrency_, arguments_.fundingCurrency_, true);
    Real fxAcfToPnlCcy =
        getFxConversionRate(today, arguments_.additionalCashflowCurrency_, arguments_.fundingCurrency_, true);

    results_.additionalResults["fxConversionAssetLegNpvToPnlCurrency"] = fxAssetToPnlCcy;
    results_.additionalResults["fxConversionAdditionalCashflowLegNpvToPnlCurrency"] = fxAcfToPnlCcy;
    results_.additionalResults["pnlCurrency"] = arguments_.fundingCurrency_.code();

    results_.value = assetLegVal * fxAssetToPnlCcy + fundingLegVal + acfLegVal * fxAcfToPnlCcy;

    finalizeResults(cfResults, underlyingStartValue, fxConversionFactor, startDate, fxAssetToPnlCcy);

    // propagate underlying additional results to trswrapper
    propagateUnderlyingAdditionalResults();

    DLOG("TRSWrapperAccrualEngine: all done, total npv = " << results_.value << " "
                                                           << arguments_.fundingCurrency_.code());
}

Real TRSWrapperAccrualEngine::assetLegValue(std::vector<CashFlowResults>& cfResults,
                                            std::vector<Real>& underlyingStartValue,
                                            std::vector<Real>& fxConversionFactor, Date& startDate) const {

    Date today = Settings::instance().evaluationDate();

    Real assetMultiplier = (arguments_.paysAsset_ ? -1.0 : 1.0);

    // asset leg valuation (accrual method)

    Real assetLegNpv = 0.0;
    Size nthCurrentPeriod = 0;

    Date endDate = Null<Date>();
    bool usingInitialPrice;

    while (computeStartValue(underlyingStartValue, fxConversionFactor, startDate, endDate, usingInitialPrice,
                             nthCurrentPeriod)) {

        // vector holding cashflow results, we store these as an additional result
        for (Size i = 0; i < arguments_.underlying_.size(); ++i) {

            std::string resultSuffix = underlyingSuffix(i, nthCurrentPeriod);

            results_.additionalResults["underlyingCurrency" + resultSuffix] = arguments_.assetCurrency_[i].code();

            if (underlyingStartValue[i] != Null<Real>()) {
                Real s1, fx1;
                std::map<std::string, QuantLib::ext::any> s1AdditionalData;
                if (endDate == Null<Date>()) {
                    s1 = getUnderlyingNPV(i, s1AdditionalData);
                    fx1 = getFxConversionRate(today, arguments_.assetCurrency_[i], arguments_.returnCurrency_, true);
                } else {
                    s1 = getUnderlyingFixing(i, endDate, false, s1AdditionalData) * arguments_.underlyingMultiplier_[i];
                    fx1 = getFxConversionRate(endDate, arguments_.assetCurrency_[i], arguments_.returnCurrency_, false);
                }
                for (const auto& [key, value] : s1AdditionalData) {
                    results_.additionalResults["s1_" + key] = value;
                }
                assetLegNpv += fx1 * s1 - underlyingStartValue[i] * fxConversionFactor[i];
                DLOG("end value (underlying " << std::to_string(i + 1) << "): s1=" << s1 << " fx1=" << fx1 << " => "
                                              << fx1 * s1 << " on "
                                              << io::iso_date(endDate == Null<Date>() ? today : endDate));

                // add details  return leg valuation to additional results
                // We want S0 or S0_i_nth(>0)
                if (nthCurrentPeriod == 0 && i == 0) {
                    results_.additionalResults["s0"] = underlyingStartValue[i];
                    results_.additionalResults["fx0"] = fxConversionFactor[i];
                } else if (nthCurrentPeriod > 0) {
                    results_.additionalResults["s0" + resultSuffix] = underlyingStartValue[i];
                    results_.additionalResults["fx0" + resultSuffix] = fxConversionFactor[i];
                }

                results_.additionalResults["s1" + resultSuffix] = s1;
                results_.additionalResults["fx1" + resultSuffix] = fx1;
                results_.additionalResults["underlyingMultiplier" + resultSuffix] = arguments_.underlyingMultiplier_[i];

                // add return cashflow to additional results
                auto& cf = cfResults.emplace_back();
                cf.amount = fx1 * s1;
                if (arguments_.underlying_.size() == 1 || !usingInitialPrice) {
                    cf.amount -= underlyingStartValue[i] * fxConversionFactor[i];
                }
                cf.amount *= assetMultiplier;
                cf.payDate = today;
                cf.currency = arguments_.returnCurrency_.code();
                cf.legNumber = 0;
                cf.type = "AccruedReturn" + resultSuffix;
                cf.accrualStartDate = startDate;
                cf.accrualEndDate = endDate == Null<Date>() ? today : endDate;
                cf.fixingValue = s1 / arguments_.underlyingMultiplier_[i];
                cf.notional = underlyingStartValue[i] * fxConversionFactor[i];

                // if initial price is used and there is more than one underlying, add cf for initialPrice
                if (arguments_.underlying_.size() > 1 && usingInitialPrice && i == 0) {
                    auto& cf = cfResults.emplace_back();
                    cf.amount = -underlyingStartValue[i] * fxConversionFactor[i];
                    cf.payDate = today;
                    cf.currency = arguments_.returnCurrency_.code();
                    cf.legNumber = 0;
                    cf.type = "AccruedReturn" + resultSuffix;
                    cf.accrualStartDate = startDate;
                    cf.accrualEndDate = endDate == Null<Date>() ? today : endDate;
                    cf.notional = underlyingStartValue[i] * fxConversionFactor[i];
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
                                auto& cf = cfResults.emplace_back();
                                cf.amount = assetMultiplier * (tmp * fx1);
                                cf.payDate = c->date();
                                cf.currency = arguments_.returnCurrency_.code();
                                cf.legNumber = 1;
                                cf.type = "UnderlyingCashFlow" + resultSuffix;
                                cf.notional = underlyingStartValue[i] * fxConversionFactor[i];
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
                        auto& cf = cfResults.emplace_back();
                        cf.amount = assetMultiplier * (dividends * fx1);
                        cf.payDate = today;
                        cf.currency = arguments_.returnCurrency_.code();
                        cf.legNumber = 2;
                        cf.type = "UnderlyingDividends" + resultSuffix;
                        cf.notional = underlyingStartValue[i] * fxConversionFactor[i];
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
    results_.additionalResults["assetLegNpvCurrency"] = arguments_.returnCurrency_.code();
    DLOG("asset leg npv = " << assetMultiplier * assetLegNpv << " " << arguments_.returnCurrency_.code());

    return assetMultiplier * assetLegNpv;
}

Real TRSWrapperAccrualEngine::fundingLegValue(std::vector<CashFlowResults>& cfResults) const {

    Date today = Settings::instance().evaluationDate();

    Real fundingMultiplier = (arguments_.paysFunding_ ? -1.0 : 1.0);

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

            using FNT = TRS::FundingData::NotionalType;
            const auto& ntlType = arguments_.fundingNotionalTypes_[i];

            if (ntlType == FNT::Fixed) {
                fundingLegNotionalFactor = 1.0;
            } else if (ntlType == FNT::PeriodReset) {
                fundingLegNotionalFactor = fundingLegPeriodResetNotionalFactor(currentIdx, resultSuffix, nthCpn);
            } else if (ntlType == FNT::DailyReset) {
                fundingLegNotionalFactor = fundingLegDailyResetNotionalFactor(cpn, localFundingLegNpv, resultSuffix, nthCpn);
            } else {
                QL_FAIL("internal error: unknown notional type, contact dev");
            }

            DLOG("fundingLegNpv for funding leg #"
                 << (i + 1) << " is " << fundingMultiplier * localFundingLegNpv << " * " << fundingLegNotionalFactor
                 << " = " << fundingMultiplier * localFundingLegNpv * fundingLegNotionalFactor << " "
                 << arguments_.fundingCurrency_ << " (notional type of leg is '" << ntlType << "')");

            localFundingLegNpv *= fundingLegNotionalFactor;

            results_.additionalResults["fundingLegNpv" + resultSuffix] = fundingMultiplier * localFundingLegNpv;

            // add funding leg cashflow to addtional results
            auto& cf = cfResults.emplace_back();
            cf.amount = fundingMultiplier * localFundingLegNpv;
            cf.payDate = today;
            cf.currency = arguments_.fundingCurrency_.code();
            cf.legNumber = 3 + i;
            cf.type = "AccruedFunding" + (nthCpn > 0 ? "_nth(" + std::to_string(nthCpn) + ")" : "");
            cf.accrualStartDate = std::min(fundingStartDate, today);
            cf.accrualEndDate = today;
            cf.notional = ntlType == TRS::FundingData::NotionalType::Fixed ? fundingCouponNotional : fundingLegNotionalFactor;

            results_.additionalResults["fundingLegNotional" + resultSuffix] = cf.notional;

            fundingLegNpv += localFundingLegNpv;
            ++nthCpn;
        } // loop over funding leg coupons (indexed by cpnNo)
    }     // loop over funding legs (indexed by i)

    DLOG("total funding leg(s) npv is " << fundingMultiplier * fundingLegNpv);

    results_.additionalResults["fundingLegNpv"] = fundingMultiplier * fundingLegNpv;
    results_.additionalResults["fundingLegNpvCurrency"] = arguments_.fundingCurrency_.code();

    return fundingMultiplier * fundingLegNpv;
}

Real TRSWrapperAccrualEngine::fundingLegPeriodResetNotionalFactor(Size currentIdx, const std::string& resultSuffix,
                                                                  Size nthCpn) const {

    Real fundingLegNotionalFactor = 0.0;
    for (Size j = 0; j < arguments_.underlying_.size(); ++j) {

        std::string resultSuffix2 = underlyingSuffix(j, nthCpn);

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

        results_.additionalResults["fundingLegNotional" + resultSuffix + resultSuffix2] = localNotionalFactor;
        results_.additionalResults["fundingLegFxRate" + resultSuffix + resultSuffix2] = localFxFactor;
    }

    return fundingLegNotionalFactor;
}

Real TRSWrapperAccrualEngine::fundingLegDailyResetNotionalFactor(const ext::shared_ptr<Coupon>& cpn,
                                                                 Real localFundingLegNpv,
                                                                 const std::string& resultSuffix,
                                                                 Size nthCpn) const {

    Date today = Settings::instance().evaluationDate();

    Real fundingLegNotionalFactor = 0.0;
    for (Size j = 0; j < arguments_.underlying_.size(); ++j) {

        std::string resultSuffix2 = underlyingSuffix(j, nthCpn);

        if (QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(cpn) || QuantLib::ext::dynamic_pointer_cast<IborCoupon>(cpn)) {

            Real dcfTotal =
                cpn->dayCounter().yearFraction(cpn->accrualStartDate(), std::min(cpn->accrualEndDate(), today));
            for (QuantLib::Date d = cpn->accrualStartDate(); d < std::min(cpn->accrualEndDate(), today); ++d) {
                string cpnSuffix = resultSuffix + resultSuffix2 + "_" + ore::data::to_string(d);
                Real dcfLocal = cpn->dayCounter().yearFraction(d, d + 1);
                Date fixingDate = arguments_.underlyingIndex_[j]->fixingCalendar().adjust(d, Preceding);
                Real localNotionalFactor = getUnderlyingFixing(j, fixingDate, false) *
                                           arguments_.underlyingMultiplier_[j] * dcfLocal / dcfTotal;
                Real localFxFactor = getFxConversionRate(fixingDate, arguments_.assetCurrency_[j],
                                                         arguments_.fundingCurrency_, false);
                fundingLegNotionalFactor += localNotionalFactor * localFxFactor;

                results_.additionalResults["fundingLegNotional" + cpnSuffix] = localNotionalFactor;
                results_.additionalResults["fundingLegFxRate" + cpnSuffix] = localFxFactor;
            }
        } else if (QuantLib::ext::dynamic_pointer_cast<OvernightIndexedCouponBase>(cpn)) {
            auto overnightCpn = QuantLib::ext::dynamic_pointer_cast<OvernightIndexedCouponBase>(cpn);
            const auto& intDates = overnightCpn->interestDates();
            const auto& fixingValues = overnightCpn->indexFixings();
            const auto& dts = overnightCpn->dt();
            double accruedInterest = 0;
            double accruedSpreadInterest = 0;
            double gearing = overnightCpn->gearing();
            double spread = overnightCpn->spread();
            for (size_t i = 0; i < intDates.size() - 1; ++i) {
                const Date& intStartDate = intDates[i];
                const Date& intEndDate = intDates[i + 1];
                string cpnSuffix = resultSuffix + resultSuffix2 + "_" + ore::data::to_string(intStartDate);
                double irFixing = fixingValues[i];
                if (overnightCpn->includeSpread())
                    irFixing += overnightCpn->spread();
                if (intStartDate < today) {
                    double dt = intEndDate > today ?
                        overnightCpn->dayCounter().yearFraction(intStartDate, today) : dts[i];
                    Date fixingDate =
                        arguments_.underlyingIndex_[j]->fixingCalendar().adjust(intStartDate, Preceding);
                    Real localNotional =
                        getUnderlyingFixing(j, fixingDate, false) * arguments_.underlyingMultiplier_[j];
                    Real localFxFactor = getFxConversionRate(fixingDate, arguments_.assetCurrency_[j],
                                                             arguments_.fundingCurrency_, false);
                    results_.additionalResults["fundingLegNotional" + cpnSuffix] = localNotional;
                    results_.additionalResults["fundingLegFxRate" + cpnSuffix] = localFxFactor;
                    results_.additionalResults["fundingLegOISRate" + cpnSuffix] = irFixing;
                    results_.additionalResults["fundingLegDCF" + cpnSuffix] = dt;
                    localNotional *= localFxFactor;
                    if (overnightCpn->rateType() != OvernightIndexedCouponBase::Type::Averaging) {
                        accruedInterest = localNotional * irFixing * dt + accruedInterest * (1 + irFixing * dt);
                        if (!overnightCpn->includeSpread()) {
                            accruedSpreadInterest += localNotional * spread * dt;
                        }
                    } else {
                        accruedInterest += localNotional * (gearing * irFixing + spread) * dt;
                    }

                    results_.additionalResults["fundingLegAccruedInterest" + cpnSuffix] = accruedInterest + accruedSpreadInterest;
                }
            }

            if (overnightCpn->rateType() != OvernightIndexedCouponBase::Type::Averaging)
                fundingLegNotionalFactor = (gearing * accruedInterest + accruedSpreadInterest);
            else
                fundingLegNotionalFactor = accruedInterest;
            fundingLegNotionalFactor /= localFundingLegNpv;

        } else if (QuantLib::ext::dynamic_pointer_cast<QuantExt::ZeroFixedCoupon>(cpn) != nullptr) {
            auto zeroCpn = QuantLib::ext::dynamic_pointer_cast<QuantExt::ZeroFixedCoupon>(cpn);
            double rate = zeroCpn->rate();
            DayCounter dc = zeroCpn->dayCounter();
            Compounding comp = zeroCpn->compounding();
            Date endDate = std::min(cpn->accrualEndDate(), today);
            double accruedFunding = 0.0;
            double prevPriceFx = 0.0;
            for (QuantLib::Date d = cpn->accrualStartDate(); d < endDate; ++d) {
                string cpnSuffix = resultSuffix + resultSuffix2 + "_" + ore::data::to_string(d);
                Date fixingDate =
                    arguments_.underlyingIndex_[j]->fixingCalendar().adjust(d, Preceding);
                Real localNotional =
                    getUnderlyingFixing(j, fixingDate, false) * arguments_.underlyingMultiplier_[j];
                Real localFxFactor = getFxConversionRate(fixingDate, arguments_.assetCurrency_[j],
                                                         arguments_.fundingCurrency_, false);
                Real priceFx = localNotional * localFxFactor;
                Real deltaPriceFx = priceFx - prevPriceFx;
                double tau = dc.yearFraction(d, endDate);
                double compFactor = (comp == QuantLib::Compounded) ? std::pow(1.0 + rate, tau) : (1.0 + rate * tau);
                accruedFunding += deltaPriceFx * compFactor;
                results_.additionalResults["fundingLegNotional" + cpnSuffix] = localNotional;
                results_.additionalResults["fundingLegFxRate" + cpnSuffix] = localFxFactor;
                results_.additionalResults["fundingLegDeltaNotionalFx" + cpnSuffix] = deltaPriceFx;
                results_.additionalResults["fundingLegCompoundFactor" + cpnSuffix] = compFactor;
                prevPriceFx = priceFx;
            }
            // When subtractNotional=true the coupon's accruedAmount (=localFundingLegNpv) subtracts
            // the notional. It would be Sum delta * (compFactor - 1), which yields to a telescopic sum
            // and only the last priceFx remains.
            if (zeroCpn->subtractNotional())
                accruedFunding -= prevPriceFx;
            fundingLegNotionalFactor += accruedFunding / localFundingLegNpv;
        } else {
            QL_FAIL("daily reset funding legs support fixed rate, ibor, overnight indexed and zero coupon fixed coupons only");
        }
    } // loop over underlyings

    return fundingLegNotionalFactor;
}

void TRSWrapperAccrualEngine::finalizeResults(const std::vector<CashFlowResults>& cfResults,
                                              const std::vector<Real>& underlyingStartValue,
                                              const std::vector<Real>& fxConversionFactor, const Date& startDate,
                                              Real fxAssetToPnlCcy) const {

    Date today = Settings::instance().evaluationDate();

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
        std::string resultSuffix = underlyingSuffix(j, 0);
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
}

Real TRSWrapperAccrualEngine::additionalCashflowLegValue(std::vector<CashFlowResults>& cfResults,
                                                         Size legNumber) const {

    Date today = Settings::instance().evaluationDate();

    // additional cashflow leg valuation (take the plain amount of future cashflows as if paid today)

    Real additionalCashflowLegNpv = 0.0;
    for (auto const& cf : arguments_.additionalCashflowLeg_) {
        if (cf->date() > today) {
            QL_REQUIRE(!additionalCashflowCurrencyDiscountCurve_.empty(),
                       "TRSWrapperAccrualEngine: additionalCashflowCurrencyDiscountCurve is empty, but "
                       "additional cashflows are present.");
            Real tmp = cf->amount() * (arguments_.additionalCashflowLegPayer_ ? -1.0 : 1.0);
            Real discountFactor = additionalCashflowCurrencyDiscountCurve_->discount(cf->date());
            additionalCashflowLegNpv += tmp * discountFactor;
            // add additional cashflows to additional results
            auto& cfR = cfResults.emplace_back();
            cfR.amount = tmp;
            cfR.discountFactor = discountFactor;
            cfR.payDate = cf->date();
            cfR.currency = arguments_.additionalCashflowCurrency_.code();
            cfR.legNumber = legNumber;
            cfR.type = "AdditionalCashFlow";
        }
    }
    DLOG("additionalCashflowLegNpv = " << additionalCashflowLegNpv << " " << arguments_.additionalCashflowCurrency_);
    results_.additionalResults["additionalCashflowLegNpv"] = additionalCashflowLegNpv;
    results_.additionalResults["additionalCashflowLegNpvCurrency"] = arguments_.additionalCashflowCurrency_.code();

    return additionalCashflowLegNpv;
}

void TRSWrapperAccrualEngine::propagateUnderlyingAdditionalResults() const {
    for (Size i = 0; i < arguments_.underlying_.size(); ++i) {
        for (auto const& [key, value] : arguments_.underlying_[i]->instrument()->additionalResults()) {
            results_.additionalResults["und_ar_" + std::to_string(i + 1) + "_" + key] = value;
        }
    }
}

bool TRSWrapperAccrualEngine::computeStartValueForIndex(Real& s0, Real& fx0, Date& startDate, Date& endDate,
    Size nth, Date& pmtDate) const {

    // For brevity below.
    auto& a = arguments_;
    const auto& pmtSched = a.paymentSchedule_;
    const auto& valSched = a.valuationSchedule_;

    Date today = Settings::instance().evaluationDate();

    // itPmt is first payment date > today.
    auto itPmt = std::upper_bound(pmtSched.begin(), pmtSched.end(), today);
    Size payIdx = std::distance(pmtSched.begin(), itPmt) + nth;
    // valuation dates associated with the payIdx-th payment date.
    Date v0 = payIdx < valSched.size() ? valSched[payIdx] : Date::maxDate();
    Date v1 = payIdx < valSched.size() - 1 ? valSched[payIdx + 1] : Date::maxDate();

    // Check whether there is a "nth" current valuation period, nth > 0.
    if (nth > 0 && (payIdx >= pmtSched.size() || v0 > today))
        return false;

    // Starting state.
    s0 = 0.0;
    fx0 = 1.0;
    startDate = Null<Date>();
    endDate = Null<Date>();
    pmtDate = Null<Date>();

    // If beyond the last payment date.
    if (payIdx >= pmtSched.size()) {
        // we are beyond the last date in the payment schedule, return false.
        DLOG("skip because eval date (" << today << ") is >= last date in payment schedule ("
            << pmtSched.back() << ") in " << io::ordinal(nth) << " current period");
        return false;
    }

    // Set the payment date for this valuation period.
    pmtDate = pmtSched[payIdx];

    // If v0, the start valuation date, is after today.
    if (v0 > today) {
        // Internal consistency check: make sure that v0 is the initial date of the valuation schedule.
        // This requirement, implicitly requires `nth == 0` also from how payIdx is calculated above.
        QL_REQUIRE(payIdx == 0, "TRSWrapper: internal error, expected valuation date " << v0 << " for pay date = " <<
            pmtSched[payIdx] << " to be the first valuation date, since it is > today (" << today << ")");

        // If no initial price is given, return false.
        if (a.initialPrice_ == Null<Real>()) {
            DLOG("skip because eval date (" << today << ") is before start valuation date ("
                << v0 << ") and no initial price is given");
            return false;
        }

        // Otherwise, we have an initial price, so we return this price, possibly converted with todays FX rate to 
        // return ccy. This allows for a reasonable asset leg npv estimation, which would otherwise be zero and jump to 
        // its actual value on the day after v0.
        s0 = a.initialPrice_ * a.indexQuantity_;
        fx0 = getFxConversionRate(today, a.initialPriceCurrency_, a.returnCurrency_, false);
        DLOG("start value s0 = " << s0 << ", from fixed initial price, fx0 = " << fx0
            << " => " << fx0 * s0 << " as of today, " << today << ", for valuation start " << v0);
        startDate = v0;
        if (v1 <= today)
            endDate = v1;
        return true;
    }

    // If we get to here, start valuation date v0 is <= today

    // Set the start and end dates.
    startDate = v0;
    if (v1 <= today)
        endDate = v1;

    // Date to use for FX conversion.
    Date fxDate = a.fxConversion_ != TRS::FXConversion::End ? v0 : (endDate == Null<Date>() ? today : endDate);

    // If v0 is the first valuation date and an initial price is given, we use it.
    if (nth == 0 && a.initialPrice_ != Null<Real>() && v0 == valSched.front()) {
        s0 = a.initialPrice_ * a.indexQuantity_;
        fx0 = getFxConversionRate(fxDate, a.initialPriceCurrency_, a.returnCurrency_, false);
        DLOG("start value s0 = " << s0 << ", from fixed initial price, fx0 = " << fx0
            << " => " << fx0 * s0 << " as of today, " << today << ", for valuation start " << v0);
        return true;
    }

    // Here, v0 <= today and we have no initial price (or it cannot be used), so we use the basket index fixing.
    s0 = basketValue(v0, fxDate, false);
    fx0 = getFxConversionRate(fxDate, a.initialPriceCurrency_, a.returnCurrency_, false);
    return true;
}

void TRSWrapperAccrualEngine::calculateForIndex() const {

    // Reset the legNumber_
    legNumber_ = 0;

    // For brevity below, shorten some names etc.
    auto& a = arguments_;
    auto& addRes = results_.additionalResults;

    Date today = Settings::instance().evaluationDate();
    DLOG("TRSWrapperAccrualEngine::calculateForIndex: today = " << today << ", paysAsset = " << std::boolalpha <<
        a.paysAsset_ << ", paysFunding = " << std::boolalpha << a.paysFunding_);

    addRes["returnCurrency"] = a.returnCurrency_.code();
    addRes["fundingCurrency"] = a.fundingCurrency_.code();
    addRes["returnLegInitialPriceCurrency"] = a.initialPriceCurrency_.code();

    // Set the initial price and add to additional results.
    if (a.initialPrice_ == Null<Real>() && a.portfolioInitialPrice_ != Null<Real>())
        a.initialPrice_ = a.portfolioInitialPrice_;
    if (a.initialPrice_ != Null<Real>())
        addRes["returnLegInitialPrice"] = a.initialPrice_;
    else
        addRes["returnLegInitialPrice"] = "NA";

    // Add to this vector when valuing asset leg and funding leg(s).
    vector<CashFlowResults> cfResults;

    // Accrual valuation of asset leg.
    ext::optional<pair<Real, Real>> s0Fx0;
    Real assetLegValue = assetLegValueForIndex(cfResults, s0Fx0);

    // Accrual valuation of funding leg(s).
    Real fundingLegValue = fundingLegValueForIndex(cfResults);

    // Accrual valuation of additional cashflow (acf for short below) leg.
    Real acfLegValue = additionalCashflowLegValueForIndex(cfResults);

    // Set npv and current notional and update additional results
    Real fxAssetToPnlCcy = getFxConversionRate(today, a.returnCurrency_, a.fundingCurrency_, true);
    Real fxAcfToPnlCcy = getFxConversionRate(today, a.additionalCashflowCurrency_, a.fundingCurrency_, true);
    addRes["fxConversionAssetLegNpvToPnlCurrency"] = fxAssetToPnlCcy;
    addRes["fxConversionAdditionalCashflowLegNpvToPnlCurrency"] = fxAcfToPnlCcy;
    addRes["pnlCurrency"] = a.fundingCurrency_.code();
    results_.value = assetLegValue * fxAssetToPnlCcy + fundingLegValue + acfLegValue * fxAcfToPnlCcy;

    // Get the current notional.
    Real currentNotional = 0.0;
    if (!s0Fx0) {
        currentNotional = basketValue(today, today, true);
        currentNotional *= getFxConversionRate(today, a.initialPriceCurrency_, a.returnCurrency_, true);
    } else {
        currentNotional = s0Fx0->first * s0Fx0->second;
    }
    addRes["currentNotional"] = currentNotional * fxAssetToPnlCcy;
    addRes["cashFlowResults"] = cfResults;

    // Propagate underlying additional results to this engine's additional results.
    propagateUnderlyingAdditionalResults();

    DLOG("TRSWrapperAccrualEngine::calculateForIndex: finished, total npv (" <<
        a.fundingCurrency_.code() << ") = " << results_.value);
}

Real TRSWrapperAccrualEngine::assetLegValueForIndex(vector<CashFlowResults>& cfResults,
    ext::optional<pair<Real, Real>>& outS0Fx0) const {

    auto& a = arguments_;
    auto& addRes = results_.additionalResults;
    Date today = Settings::instance().evaluationDate();
    Real multiplier = a.paysAsset_ ? -1.0 : 1.0;

    // We may have multiple live current periods due to payments lags etc.
    // This keeps track of which one of those we are in.
    Size nth = 0;

    // Accrual valuation of asset leg.
    Real legValue = 0;
    Real s0 = 0;
    Real fx0 = 1;
    Date startDate = Null<Date>();
    Date endDate = Null<Date>();
    Date pmtDate = Null<Date>();

    while (computeStartValueForIndex(s0, fx0, startDate, endDate, nth, pmtDate)) {

        string resultSuffix = nth > 0 ? "_nth(" + std::to_string(nth) + ")" : "";

        // Add what was computed in computeStartValue to additional results.
        addRes["s0" + resultSuffix] = s0;
        addRes["fx0" + resultSuffix] = fx0;

        // `endDate` will either be null => calculate the basket value as of today or 
        // it will be a date <= today => try to determine the basket value via a fixing.
        bool enforceProjection = endDate == Null<Date>();
        Date fixDate = enforceProjection ? today : endDate;
        Real s1 = basketValue(fixDate, fixDate, enforceProjection);
        Real fx1 = getFxConversionRate(fixDate, a.initialPriceCurrency_, a.returnCurrency_, enforceProjection);
        addRes["s1" + resultSuffix] = s1;
        addRes["fx1" + resultSuffix] = fx1;

        // Update asset leg value.
        Real amount = fx1 * s1 - fx0 * s0;
        legValue += amount;

        // Add a cashflow for this return.
        auto& cf = cfResults.emplace_back();
        cf.amount = amount * multiplier;
        cf.payDate = today;
        cf.currency = a.returnCurrency_.code();
        cf.legNumber = legNumber_;
        cf.type = "AccruedReturn" + resultSuffix;
        cf.accrualStartDate = startDate;
        cf.accrualEndDate = fixDate;
        cf.fixingValue = s1 / a.indexQuantity_;
        cf.notional = fx0 * s0;

        // Update nth.
        ++nth;

        // Set so that we have the last computed s0 and fx0.
        outS0Fx0 = {s0, fx0};
    }

    legValue *= multiplier;
    addRes["assetLegNpv"] = legValue;
    addRes["assetLegNpvCurrency"] = a.returnCurrency_.code();
    DLOG("Asset leg npv (" << a.returnCurrency_.code() << ") = " << legValue);
    legNumber_++;

    return legValue;
}

Real TRSWrapperAccrualEngine::fundingLegValueForIndex(vector<CashFlowResults>& cfResults) const {

    using FNT = TRS::FundingData::NotionalType;
    auto& a = arguments_;
    auto& addRes = results_.additionalResults;
    const auto& valSched = a.valuationSchedule_;

    Date today = Settings::instance().evaluationDate();
    Real multiplier = a.paysFunding_ ? -1.0 : 1.0;

    Real legsValue = 0.0;
    for (Size i = 0; i < a.fundingLegs_.size(); ++i) {

        const auto& leg = a.fundingLegs_[i];
        const auto& ntlType = a.fundingNotionalTypes_[i];
        string legSuffix = "_" + std::to_string(i + 1);
        Real legValue = 0.0;
        Real fundingNtl = 0.0;

        for (Size cpnNo = 0; cpnNo < leg.size(); ++cpnNo) {

            // Can we skip this coupon.
            auto cpn = ext::dynamic_pointer_cast<Coupon>(leg[cpnNo]);
            if (!cpn || cpn->date() <= today || cpn->accrualStartDate() >= today)
                continue;

            // Look up latest valuation date <= this funding coupon's start date. Fall back to the first valuation 
            // date, if first valuation date is > this funding coupon's start date.
            const Date& startDate = cpn->accrualStartDate();
            auto itVal = std::upper_bound(valSched.begin(), valSched.end(), startDate + a.fundingResetGracePeriod_);
            Size valIdx = std::distance(valSched.begin(), itVal);
            if (valIdx > 0)
                --valIdx;

            if (valSched[valIdx] > today) {
                DLOG("coupon " << (cpnNo + 1) << " on funding leg " << (i + 1) << " is skipped because the last "
                    "associated relevant valuation date (" << valSched[valIdx] << ") is > today (" << today << ")");
                continue;
            }

            // Suffix values used in additional results to distinguish between multiple funding legs and coupons.
            // Keep it simple and use the leg number and coupon number, e.g. "_1_2" for funding leg #1, coupon #2.
            string cpnSuffix = legSuffix + "_" + std::to_string(cpnNo + 1);

            // Process the different notional types and coupons to calculate the accrual value of the current coupon.
            Real cpnValue = 0.0;
            if (ntlType == FNT::Fixed) {
                cpnValue = fixedNtlCpnVal(cpn, today, cpnSuffix, fundingNtl);
            } else if (ntlType == FNT::PeriodReset) {
                cpnValue = periodResetCpnVal(cpn, today, cpnSuffix, valIdx, fundingNtl);
            } else if (ntlType == FNT::DailyReset) {
                if (auto specificCpn = ext::dynamic_pointer_cast<FixedRateCoupon>(cpn)) {
                    cpnValue = dailyResetCpnVal(specificCpn, today, cpnSuffix, fundingNtl);
                } else if (auto specificCpn = ext::dynamic_pointer_cast<IborCoupon>(cpn)) {
                    cpnValue = dailyResetCpnVal(specificCpn, today, cpnSuffix, fundingNtl);
                } else if (auto specificCpn = ext::dynamic_pointer_cast<OvernightIndexedCouponBase>(cpn)) {
                    cpnValue = dailyResetCpnVal(specificCpn, today, cpnSuffix, fundingNtl);
                } else {
                    // I have intentionally left out ZeroFixedCoupon here, because I don't understand the existing 
                    // code for it in the presence of `subtractNotional`.
                    QL_FAIL("daily reset funding legs for TRS on bespoke indices support fixed rate, ibor, "
                        "overnight indexed only.");
                }
            } else {
                QL_FAIL("internal error: unexpected notional type, " << ntlType << ", while processing funding legs "
                    "for TRS on bespoke indices.");
            }

            // Add funding leg cashflow to cashflow results
            auto& cf = cfResults.emplace_back();
            cf.amount = multiplier * cpnValue;
            cf.payDate = cpn->date();
            cf.currency = a.fundingCurrency_.code();
            cf.legNumber = legNumber_;
            cf.type = "AccruedFunding_" + std::to_string(cpnNo + 1);
            cf.accrualStartDate = startDate;
            cf.accrualEndDate = today;
            cf.notional = fundingNtl;

            legValue += cpnValue;
        } // loop over funding leg coupons (indexed by cpnNo)

        addRes["fundingLegNotional" + legSuffix] = fundingNtl;
        addRes["fundingLegNpv" + legSuffix] = multiplier * legValue;
        legsValue += legValue;
        legNumber_++;

    } // loop over funding legs (indexed by i)

    legsValue *= multiplier;
    DLOG("Total funding leg(s) value (" << a.fundingCurrency_.code() << ") = " << legsValue);
    addRes["fundingLegNpv"] = legsValue;
    addRes["fundingLegNpvCurrency"] = a.fundingCurrency_.code();

    return legsValue;
}

Real TRSWrapperAccrualEngine::additionalCashflowLegValueForIndex(vector<CashFlowResults>& cfResults) const {
    Real legValue = additionalCashflowLegValue(cfResults, legNumber_);
    legNumber_++;
    return legValue;
}

Real TRSWrapperAccrualEngine::basketValue(const Date& fixingDate, const Date& fxDate, bool enforceProjection) const {

    auto& a = arguments_;
    Date today = Settings::instance().evaluationDate();
    QL_REQUIRE(fixingDate <= today, "TRSWrapperAccrualEngine: basket value not available for " <<
        arguments_.basketIndex_->name() << " on fixing date " << fixingDate <<
        " which is strictly greater than today " << today);

    ext::optional<Real> basketFixing;
    if (!enforceProjection) {
        try {
            basketFixing = a.basketIndex_->fixing(fixingDate);
        } catch (const std::exception&) {
            basketFixing = ext::nullopt;
        }
    }

    Real result = 0;
    if (basketFixing) {
        result = *basketFixing * a.indexQuantity_;
    } else {
        QL_REQUIRE(enforceProjection || fixingDate == today, "TRSWrapperAccrualEngine: no fixing available for " <<
            a.basketIndex_->name() << " on fixing date " << fixingDate << ", strictly less than today " << today);

        Real tmp;
        for (Size i = 0; i < a.underlying_.size(); ++i) {
            tmp = getUnderlyingNPV(i);
            tmp *= getFxConversionRate(fxDate, a.assetCurrency_[i], a.initialPriceCurrency_, enforceProjection);
            result += tmp;
        }
    }

    return result;
}

Real TRSWrapperAccrualEngine::fixedNtlCpnVal(const ext::shared_ptr<Coupon>& cpn, const Date& today,
    const string& cpnSuffix, Real& outNtl) const {

    auto& addRes = results_.additionalResults;

    Real result = cpn->accruedAmount(today);
    outNtl = cpn->nominal();
    addRes["fundingLegNotional" + cpnSuffix] = outNtl;
    Time accruedDcf = cpn->accruedPeriod(today);
    addRes["fundingLegDCF" + cpnSuffix] = accruedDcf;
    if (!close(accruedDcf, 0.0) && !close(outNtl, 0.0))
        addRes["fundingCouponRate" + cpnSuffix] = result / (outNtl * accruedDcf);

    return result;
}

Real TRSWrapperAccrualEngine::periodResetCpnVal(const ext::shared_ptr<Coupon>& cpn, const Date& today,
    const string& cpnSuffix, Size valIdx, Real& outNtl) const {

    auto& a = arguments_;
    auto& addRes = results_.additionalResults;
    const auto& valSched = a.valuationSchedule_;

    Real effNtl = valIdx == 0 && a.initialPrice_ != Null<Real>() ? a.initialPrice_ * a.indexQuantity_
        : basketValue(valSched[valIdx], valSched[valIdx], false);
    Real fx = getFxConversionRate(valSched[valIdx], a.initialPriceCurrency_, a.fundingCurrency_, false);
    addRes["fundingLegNotional" + cpnSuffix] = effNtl;
    addRes["fundingLegFxRate" + cpnSuffix] = fx;
    Time accruedDcf = cpn->accruedPeriod(today);
    addRes["fundingLegDCF" + cpnSuffix] = accruedDcf;
    outNtl = effNtl * fx;
    Real accruedPerUnitNtl = cpn->accruedAmount(today);
    if (!close(accruedDcf, 0.0) && !close(outNtl, 0.0))
        addRes["fundingCouponRate" + cpnSuffix] = accruedPerUnitNtl / accruedDcf;

    return accruedPerUnitNtl * outNtl;
}

Real TRSWrapperAccrualEngine::dailyResetCpnVal(const ext::shared_ptr<FixedRateCoupon>& cpn, const Date& today,
    const string& cpnSuffix, Real& outNtl) const {

    auto& a = arguments_;
    auto& addRes = results_.additionalResults;

    // Effective notional on each day is the basket value on that day. For past dates, it will be obtained via an index 
    // fixing for the basket level mulitplied by the index quantity giving the basket value in initial price currency 
    // units. This needs to be converted to funding currency units and the rate applied to calculate the accrual for 
    // that date in funding currency units. The sum of all daily accruals is the total accrual for the coupon.
    Real result = 0;
    const auto& dc = cpn->dayCounter();
    Rate fixedRate = cpn->rate();
    addRes["fundingCouponRate" + cpnSuffix] = fixedRate;
    Real fundingNtl = 0;

    // We step on week days only. Can't see a situation where we are getting basket fixings on weekends.
    WeekendsOnly stepCal;
    Date stopDate = std::min(cpn->accrualEndDate(), today);
    pair<Real, Date> lastFixing;
    for (Date d = cpn->accrualStartDate(), dNext; d < stopDate;  d = dNext) {
        dNext = stepCal.advance(d, 1, Days);
        Real dt = dc.yearFraction(d, std::min(dNext, stopDate));
        lastFixing = lastAvailableFixing(d, lastFixing.second);
        Real effNtl = lastFixing.first * a.indexQuantity_;
        Real fx = getFxConversionRate(lastFixing.second, a.initialPriceCurrency_, a.fundingCurrency_, false);
        string extSuffix = cpnSuffix + "_" + ore::data::to_string(d);
        addRes["fundingLegNotional" + extSuffix] = effNtl;
        addRes["fundingLegFxRate" + extSuffix] = fx;
        addRes["fundingLegDCF" + extSuffix] = dt;
        fundingNtl = effNtl * fx;
        result += fundingNtl * fixedRate * dt;
    }

    outNtl = fundingNtl;
    return result;
}

Real TRSWrapperAccrualEngine::dailyResetCpnVal(const ext::shared_ptr<IborCoupon>& cpn, const Date& today,
    const string& cpnSuffix, Real& outNtl) const {

    // Note, this method is very like the fixed rate function above but I am not sure it is exactly what will be 
    // expected for Ibor coupons with daily reset. It may be expected that you step on the Ibor index fixing dates and 
    // use the fixing on each of those dates instead of using the single Ibor coupon fixing for the coupon. Leave it 
    // as a separate method here in case we need to amend it later.

    auto& a = arguments_;
    auto& addRes = results_.additionalResults;

    // Effective notional on each day is the basket value on that day. For past dates, it will be obtained via an index 
    // fixing for the basket level mulitplied by the index quantity giving the basket value in initial price currency 
    // units. This needs to be converted to funding currency units and the rate applied to calculate the accrual for 
    // that date in funding currency units. The sum of all daily accruals is the total accrual for the coupon.
    Real result = 0;
    const auto& dc = cpn->dayCounter();
    Rate fltRate = cpn->rate();
    addRes["fundingCouponRate" + cpnSuffix] = fltRate;
    Real fundingNtl = 0;

    // We step on week days only. Can't see a situation where we are getting basket fixings on weekends.
    WeekendsOnly stepCal;
    Date stopDate = std::min(cpn->accrualEndDate(), today);
    pair<Real, Date> lastFixing;
    for (Date d = cpn->accrualStartDate(), dNext; d < stopDate; d = dNext) {
        dNext = stepCal.advance(d, 1, Days);
        Real dt = dc.yearFraction(d, std::min(dNext, stopDate));
        lastFixing = lastAvailableFixing(d, lastFixing.second);
        Real effNtl = lastFixing.first * a.indexQuantity_;
        Real fx = getFxConversionRate(lastFixing.second, a.initialPriceCurrency_, a.fundingCurrency_, false);
        string extSuffix = cpnSuffix + "_" + ore::data::to_string(d);
        addRes["fundingLegNotional" + extSuffix] = effNtl;
        addRes["fundingLegFxRate" + extSuffix] = fx;
        addRes["fundingLegDCF" + extSuffix] = dt;
        fundingNtl = effNtl * fx;
        result += fundingNtl * fltRate * dt;
    }

    outNtl = fundingNtl;
    return result;
}

Real TRSWrapperAccrualEngine::dailyResetCpnVal(const ext::shared_ptr<OvernightIndexedCouponBase>& cpn,
    const Date& today, const string& cpnSuffix, Real& outNtl) const {

    auto& a = arguments_;
    auto& addRes = results_.additionalResults;

    // OIS coupon relevant values.
    const auto& intDates = cpn->interestDates();
    const auto& fixingValues = cpn->indexFixings();
    const auto& dts = cpn->dt();
    const auto& dc = cpn->dayCounter();
    double accInt = 0;
    double accSpreadInt = 0;
    double gearing = cpn->gearing();
    double spread = cpn->spread();
    bool incSpread = cpn->includeSpread();

    // Effective notional on each day is the basket value on that day. For past dates, it will be obtained via an index 
    // fixing for the basket level mulitplied by the index quantity giving the basket value in initial price currency 
    // units. This needs to be converted to funding currency units and the rate applied to calculate the accrual for 
    // that date in funding currency units. The sum of all daily accruals is the total accrual for the coupon.
    Real fundingNtl = 0;
    pair<Real, Date> lastBasketFixing;
    for (Size i = 0; i < intDates.size() - 1; ++i) {
        const Date& intStart = intDates[i];

        // Break early if no more overnight periods to process.
        if (intStart >= today)
            break;

        // Get the applicable notional and fx for the single overnight period.
        lastBasketFixing = lastAvailableFixing(intStart, lastBasketFixing.second);
        Real effNtl = lastBasketFixing.first * a.indexQuantity_;
        Real fx = getFxConversionRate(lastBasketFixing.second, a.initialPriceCurrency_, a.fundingCurrency_, false);
        string extSuffix = cpnSuffix + "_" + ore::data::to_string(intStart);
        addRes["fundingLegNotional" + extSuffix] = effNtl;
        addRes["fundingLegFxRate" + extSuffix] = fx;

        // Get the applicable overnight rate and day count fraction.
        const Date& intEnd = intDates[i + 1];
        double onFixing = fixingValues[i];
        if (incSpread)
            onFixing += spread;
        Real dt = intEnd > today ? dc.yearFraction(intStart, today) : dts[i];
        addRes["fundingLegOISRate" + extSuffix] = onFixing;
        addRes["fundingLegDCF" + extSuffix] = dt;

        // Calculate and store the accrual amount for this one overnight period.
        using OICBT = OvernightIndexedCouponBase::Type;
        fundingNtl = effNtl * fx;
        if (cpn->rateType() != OICBT::Averaging) {
            accInt = fundingNtl * onFixing * dt + accInt * (1 + onFixing * dt);
            if (!incSpread)
                accSpreadInt += fundingNtl * spread * dt;
        } else {
            accInt += fundingNtl * (gearing * onFixing + spread) * dt;
        }
        addRes["fundingLegAccruedInterest" + extSuffix] = accInt + accSpreadInt;
    }

    outNtl = fundingNtl;
    return accInt + accSpreadInt;
}

pair<Real, Date> TRSWrapperAccrualEngine::lastAvailableFixing(const Date& fixingDate,
    const Date& earliestDate, Natural gracePeriod) const {

    const auto& basketIndex = arguments_.basketIndex_;
    WeekendsOnly stepCal;

    // If no earliestDate provided, go back gracePeriod week days to get the earliest date to look for a fixing.
    Date earliest = earliestDate;
    if (earliest == Date())
        earliest = stepCal.advance(fixingDate, -static_cast<Integer>(gracePeriod), Days);

    // Look for the last available fixing on or before fixingDate, but not before earliest.
    for (Date effFixingDate = fixingDate; effFixingDate >= earliest;
        effFixingDate = stepCal.advance(effFixingDate, -1, Days)) {
        try {
            Real fixing = basketIndex->fixing(effFixingDate);
            return { fixing, effFixingDate };
        } catch (const std::exception&) {
            // no fixing available on this date; try previous weekday
        }
    }

    // If we get here, no fixing was found in the grace period so fail.
    QL_FAIL("TRSWrapperAccrualEngine::lastAvailableFixing: no fixing found for basket index " << basketIndex->name()
        << " in the grace period (" << gracePeriod << " days) ending on " << fixingDate);
}

} // namespace data
} // namespace ore
