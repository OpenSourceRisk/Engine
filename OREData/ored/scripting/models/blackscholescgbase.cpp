/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/scripting/models/blackscholescgbase.hpp>

#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/math/randomvariablelsmbasissystem.hpp>
#include <qle/math/randomvariable_ops.hpp>
#include <qle/ad/computationgraph.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/quotes/simplequote.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

BlackScholesCGBase::BlackScholesCGBase(const Size paths, const std::string& currency,
                                       const Handle<YieldTermStructure>& curve, const std::string& index,
                                       const std::string& indexCurrency, const Handle<BlackScholesModelWrapper>& model,
                                       const std::set<Date>& simulationDates,
                                       const IborFallbackConfig& iborFallbackConfig)
    : BlackScholesCGBase(paths, {currency}, {curve}, {}, {}, {}, {index}, {indexCurrency}, model, {}, simulationDates,
                         iborFallbackConfig) {}

BlackScholesCGBase::BlackScholesCGBase(
    const Size paths, const std::vector<std::string>& currencies, const std::vector<Handle<YieldTermStructure>>& curves,
    const std::vector<Handle<Quote>>& fxSpots,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
    const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
    const Handle<BlackScholesModelWrapper>& model,
    const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
    const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig)
    : ModelCGImpl(curves.at(0)->dayCounter(), paths, currencies, irIndices, infIndices, indices, indexCurrencies,
                  simulationDates, iborFallbackConfig),
      curves_(curves), fxSpots_(fxSpots), model_(model), correlations_(correlations) {

    // check inputs

    QL_REQUIRE(!model_.empty(), "model is empty");
    QL_REQUIRE(!curves_.empty(), "no curves given");
    QL_REQUIRE(currencies_.size() == curves_.size(), "number of currencies (" << currencies_.size()
                                                                              << ") does not match number of curves ("
                                                                              << curves_.size() << ")");
    QL_REQUIRE(currencies_.size() == fxSpots_.size() + 1,
               "number of currencies (" << currencies_.size() << ") does not match number of fx spots ("
                                        << fxSpots_.size() << ") + 1");

    QL_REQUIRE(indices_.size() == model_->processes().size(),
               "mismatch of processes size (" << model_->processes().size() << ") and number of indices ("
                                              << indices_.size() << ")");

    // register with observables

    for (auto const& o : fxSpots_)
        registerWith(o);
    for (auto const& o : correlations_)
        registerWith(o.second);

    registerWith(model_);

} // BlackScholesBase ctor

const Date& BlackScholesCGBase::referenceDate() const {
    calculate();
    return referenceDate_;
}

void BlackScholesCGBase::performCalculations() const {

    // needed for base class performCalculations()

    referenceDate_ = curves_.front()->referenceDate();

    // update cg version if necessary (eval date changed)

    ModelCGImpl::performCalculations();

    // if cg version has changed => update time grid related members and clear paths, so that they
    // are populated in derived classes

    if (cgVersion() != underlyingPathsCgVersion_) {

        // set up time grid

        effectiveSimulationDates_ = model_->effectiveSimulationDates();

        std::vector<Real> times;
        for (auto const& d : effectiveSimulationDates_) {
            times.push_back(curves_.front()->timeFromReference(d));
        }

        timeGrid_ = model_->discretisationTimeGrid();
        positionInTimeGrid_.resize(times.size());
        for (Size i = 0; i < positionInTimeGrid_.size(); ++i)
            positionInTimeGrid_[i] = timeGrid_.index(times[i]);

        underlyingPaths_.clear();
        underlyingPathsCgVersion_ = cgVersion();
    }
}

std::size_t BlackScholesCGBase::getIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date effFwd = fwd;
    if (indices_[indexNo].isComm()) {
        Date expiry = indices_[indexNo].comm(d)->expiryDate();
        // if a future is referenced we set the forward date effectively used below to the future's expiry date
        if (expiry != Date())
            effFwd = expiry;
        // if the future expiry is past the obsdate, we return the spot as of the obsdate, i.e. we
        // freeze the future value after its expiry, but keep it available for observation
        // TOOD should we throw an exception instead?
        effFwd = std::max(effFwd, d);
    }
    QL_REQUIRE(underlyingPaths_.find(d) != underlyingPaths_.end(), "did not find path for " << d);
    auto res = underlyingPaths_.at(d).at(indexNo);
    // compute forwarding factor
    if (effFwd != Null<Date>()) {
        auto p = model_->processes().at(indexNo);
        std::string idf = std::to_string(indexNo) + "_" + ore::data::to_string(effFwd);
        std::string idd = std::to_string(indexNo) + "_" + ore::data::to_string(d);
        std::size_t div_d = addModelParameter("__div_" + idd, [p, d]() { return p->dividendYield()->discount(d); });
        std::size_t div_f = addModelParameter("__div_" + idf, [p, effFwd]() { return p->dividendYield()->discount(effFwd); });
        std::size_t rfr_d = addModelParameter("__rfr_" + idd, [p, d]() { return p->riskFreeRate()->discount(d); });
        std::size_t rfr_f = addModelParameter("__rfr_" + idf, [p, effFwd]() { return p->riskFreeRate()->discount(effFwd); });
        res = cg_mult(*g_, res, cg_mult(*g_, div_f, cg_div(*g_, rfr_d, cg_mult(*g_, div_d, rfr_f))));
    }
    return res;
}

std::size_t BlackScholesCGBase::getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date effFixingDate = d;
    if (fwd != Null<Date>())
        effFixingDate = fwd;
    // ensure a valid fixing date
    effFixingDate = irIndices_.at(indexNo).second->fixingCalendar().adjust(effFixingDate);
    auto index = irIndices_.at(indexNo).second;
    std::string id = "__irFix_" + index->name() + "_" + ore::data::to_string(effFixingDate);
    return addModelParameter(id, [index, effFixingDate]() { return index->fixing(effFixingDate); });
}

std::size_t BlackScholesCGBase::getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date effFixingDate = d;
    if (fwd != Null<Date>())
        effFixingDate = fwd;
    auto index = infIndices_.at(indexNo).second;
    std::string id = "__infFix_" + index->name() + "_" + ore::data::to_string(effFixingDate);
    return addModelParameter(id, [index, effFixingDate]() { return index->fixing(effFixingDate); });
}

namespace {
struct comp {
    comp(const std::string& indexInput) : indexInput_(indexInput) {}
    template <typename T> bool operator()(const std::pair<IndexInfo, QuantLib::ext::shared_ptr<T>>& p) const {
        return p.first.name() == indexInput_;
    }
    const std::string indexInput_;
};
} // namespace

std::size_t BlackScholesCGBase::fwdCompAvg(const bool isAvg, const std::string& indexInput, const Date& obsdate,
                                           const Date& start, const Date& end, const Real spread, const Real gearing,
                                           const Integer lookback, const Natural rateCutoff, const Natural fixingDays,
                                           const bool includeSpread, const Real cap, const Real floor,
                                           const bool nakedOption, const bool localCapFloor) const {
    calculate();
    auto index = std::find_if(irIndices_.begin(), irIndices_.end(), comp(indexInput));
    QL_REQUIRE(index != irIndices_.end(),
               "BlackScholesCGBase::fwdCompAvg(): did not find ir index " << indexInput << " - this is unexpected.");
    auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index->second);
    QL_REQUIRE(on, "BlackScholesCGBase::fwdCompAvg(): expected on index for " << indexInput);
    // if we want to support cap / floors we need the OIS CF surface
    QL_REQUIRE(cap > 999998.0 && floor < -999998.0,
               "BlackScholesCGBase:fwdCompAvg(): cap (" << cap << ") / floor (" << floor << ") not supported");
    QuantLib::ext::shared_ptr<QuantLib::FloatingRateCoupon> coupon;
    QuantLib::ext::shared_ptr<QuantLib::FloatingRateCouponPricer> pricer;
    if (isAvg) {
        coupon = QuantLib::ext::make_shared<QuantExt::AverageONIndexedCoupon>(
            end, 1.0, start, end, on, gearing, spread, rateCutoff, on->dayCounter(), lookback * Days, fixingDays);
        pricer = QuantLib::ext::make_shared<AverageONIndexedCouponPricer>();
    } else {
        coupon = QuantLib::ext::make_shared<QuantExt::OvernightIndexedCoupon>(end, 1.0, start, end, on, gearing, spread, Date(),
                                                                      Date(), on->dayCounter(), false, includeSpread,
                                                                      lookback * Days, rateCutoff, fixingDays);
        pricer = QuantLib::ext::make_shared<OvernightIndexedCouponPricer>();
    }
    coupon->setPricer(pricer);
    std::string id = "__fwdCompAvg_" + std::to_string(g_->size());
    return addModelParameter(id, [coupon]() { return coupon->rate(); });
}

std::size_t BlackScholesCGBase::getDiscount(const Size idx, const Date& s, const Date& t) const {
    std::string ids = "__curve_" + std::to_string(idx) + "_" + ore::data::to_string(s);
    std::string idt = "__curve_" + std::to_string(idx) + "_" + ore::data::to_string(t);
    auto c = curves_.at(idx);
    std::size_t ns = addModelParameter(ids, [c, s] { return c->discount(s); });
    std::size_t nt = addModelParameter(idt, [c, t] { return c->discount(t); });
    return cg_div(*g_, nt, ns);
}

std::size_t BlackScholesCGBase::getNumeraire(const Date& s) const {
    std::string id = "__curve_0_" + ore::data::to_string(s);
    auto c = curves_.at(0);
    std::size_t ds = addModelParameter(id, [c, s] { return c->discount(s); });
    return cg_div(*g_, cg_const(*g_, 1.0), ds);
}

std::size_t BlackScholesCGBase::getFxSpot(const Size idx) const {
    std::string id = "__fxspot_" + std::to_string(idx);
    auto c = fxSpots_.at(idx);
    return addModelParameter(id, [c] { return c->value(); });
}

Real BlackScholesCGBase::getDirectFxSpotT0(const std::string& forCcy, const std::string& domCcy) const {
    auto c1 = std::find(currencies_.begin(), currencies_.end(), forCcy);
    auto c2 = std::find(currencies_.begin(), currencies_.end(), domCcy);
    QL_REQUIRE(c1 != currencies_.end(), "currency " << forCcy << " not handled");
    QL_REQUIRE(c2 != currencies_.end(), "currency " << domCcy << " not handled");
    Size cidx1 = std::distance(currencies_.begin(), c1);
    Size cidx2 = std::distance(currencies_.begin(), c2);
    Real fx = 1.0;
    if (cidx1 > 0)
        fx *= fxSpots_.at(cidx1 - 1)->value();
    if (cidx2 > 0)
        fx /= fxSpots_.at(cidx2 - 1)->value();
    return fx;
}

Real BlackScholesCGBase::getDirectDiscountT0(const Date& paydate, const std::string& currency) const {
    auto c = std::find(currencies_.begin(), currencies_.end(), currency);
    QL_REQUIRE(c != currencies_.end(), "currency " << currency << " not handled");
    Size cidx = std::distance(currencies_.begin(), c);
    return curves_.at(cidx)->discount(paydate);
}

std::size_t BlackScholesCGBase::npv(const std::size_t amount, const Date& obsdate, const std::size_t filter,
                                    const boost::optional<long>& memSlot, const std::size_t addRegressor1,
                                    const std::size_t addRegressor2) const {

    calculate();

    QL_REQUIRE(!memSlot, "BlackScholesCGBase::npv() with memSlot not yet supported!");

    // if obsdate is today, take a plain expectation

    if (obsdate == referenceDate()) {
        return cg_conditionalExpectation(*g_, amount, {}, cg_const(*g_, 1.0));
    }

    // build the state

    std::vector<std::size_t> state;

    if (!underlyingPaths_.empty()) {
        for (auto const& r : underlyingPaths_.at(obsdate))
            state.push_back(r);
    }

    if (addRegressor1 != ComputationGraph::nan)
        state.push_back(addRegressor1);
    if (addRegressor2 != ComputationGraph::nan)
        state.push_back(addRegressor2);

    // if the state is empty, return the plain expectation (no conditioning)

    if(state.empty()) {
        return cg_conditionalExpectation(*g_, amount, {}, cg_const(*g_, 1.0));
    }

    // if a memSlot is given and coefficients are stored, we use them
    // TODO ...

    // otherwise compute coefficients and store them if a memSlot is given
    // TODO ...

    // compute conditional expectation and return the result

    return cg_conditionalExpectation(*g_, amount, state, filter);
}

} // namespace data
} // namespace ore
