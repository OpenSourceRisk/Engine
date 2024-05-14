/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <qle/cashflows/mcgaussianformulabasedcouponpricer.hpp>

#include <ql/cashflows/cmscoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/math/randomnumbers/sobolrsg.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

#include <boost/make_shared.hpp>
using namespace QuantLib;
using namespace boost::accumulators;

namespace QuantExt {

MCGaussianFormulaBasedCouponPricer::MCGaussianFormulaBasedCouponPricer(
    const std::string& paymentCurrencyCode,
    const std::map<std::string, QuantLib::ext::shared_ptr<IborCouponPricer>>& iborPricers,
    const std::map<std::string, QuantLib::ext::shared_ptr<CmsCouponPricer>>& cmsPricers,
    const std::map<std::string, Handle<BlackVolTermStructure>>& fxVolatilities,
    const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlation,
    const Handle<YieldTermStructure>& couponDiscountCurve, const Size samples, const Size seed, const bool useSobol,
    SalvagingAlgorithm::Type salvaging)
    : FormulaBasedCouponPricer(paymentCurrencyCode, fxVolatilities, correlation), iborPricers_(iborPricers),
      cmsPricers_(cmsPricers), couponDiscountCurve_(couponDiscountCurve), samples_(samples), seed_(seed),
      useSobol_(useSobol), salvaging_(salvaging) {
    // registering with fx vol and correlations is done in the base class already
    for (auto const& p : iborPricers_)
        registerWith(p.second);
    for (auto const& p : cmsPricers_)
        registerWith(p.second);
    registerWith(couponDiscountCurve);
}

Real MCGaussianFormulaBasedCouponPricer::swapletRate() const {
    compute();
    return rateEstimate_;
}

Real MCGaussianFormulaBasedCouponPricer::swapletPrice() const {
    return coupon_->accrualPeriod() * discount_ * swapletRate();
}

Real MCGaussianFormulaBasedCouponPricer::capletPrice(Rate effectiveCap) const {
    QL_FAIL("MCGaussianFormulaBasedCouponPricer::capletPrice(): not provided");
}

Rate MCGaussianFormulaBasedCouponPricer::capletRate(Rate effectiveCap) const {
    QL_FAIL("MCGaussianFormulaBasedCouponPricer::capletRate(): not provided");
}

Real MCGaussianFormulaBasedCouponPricer::floorletPrice(Rate effectiveFloor) const {
    QL_FAIL("MCGaussianFormulaBasedCouponPricer::floorletPrice(): not provided");
}

Rate MCGaussianFormulaBasedCouponPricer::floorletRate(Rate effectiveFloor) const {
    QL_FAIL("MCGaussianFormulaBasedCouponPricer::floorletRate(): not provided");
}

namespace {
Real getCorrelation(
    const std ::string& key1, const std::string& key2,
    const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlation) {
    auto k1 = std::make_pair(key1, key2);
    auto k2 = std::make_pair(key2, key1);

    Real corr;
    if (correlation.find(k1) != correlation.end()) {
        corr = correlation.at(k1)->correlation(0.0, 1.0);
    } else if (correlation.find(k2) != correlation.end()) {
        corr = correlation.at(k2)->correlation(0.0, 1.0);
    } else {
        // missing correlation entry in the map
        QL_FAIL("No correlation between " << key1 << " and " << key2 << " is given!");
    }
    return corr;
}
} // namespace

void MCGaussianFormulaBasedCouponPricer::initialize(const FloatingRateCoupon& coupon) {
    coupon_ = dynamic_cast<const FormulaBasedCoupon*>(&coupon);
    QL_REQUIRE(coupon_, "MCGaussianFormulaBasedCouponPricer::initialize(): FormulaBasedCoupon excepted");
    QL_REQUIRE(!couponDiscountCurve_.empty(),
               "MCGaussianFormulaBasedCouponPricer::initialize(): coupon discount curve is empty");
    QL_REQUIRE(coupon_->paymentCurrency().code() == paymentCurrencyCode_,
               "MCGaussianFormulaBasedCouponPricer::initialize(): coupon payment currency ("
                   << coupon_->paymentCurrency().code() << ") does not match pricer's payment currency ("
                   << paymentCurrencyCode_ << ")");

    today_ = Settings::instance().evaluationDate();
    fixingDate_ = coupon_->fixingDate();
    Real fixingTime =
        couponDiscountCurve_->timeFromReference(fixingDate_); // date to time conversion via discount curve dc
    paymentDate_ = coupon_->date();
    index_ = coupon_->formulaBasedIndex();
    discount_ =
        paymentDate_ > couponDiscountCurve_->referenceDate() ? couponDiscountCurve_->discount(paymentDate_) : 1.0;

    // for past fixing we are done, also if there are actually no indices on which the formula depends

    if (fixingDate_ <= today_ || index_->indices().empty())
        return;

    // loop over source indices, compute mean and variance for the MC simulation

    n_ = index_->indices().size();
    volType_.resize(n_);
    volShift_.resize(n_);
    atmRate_.resize(n_);
    mean_ = Array(n_);
    Array vol(n_);

    covariance_ = Matrix(n_, n_, 0.0);

    for (Size i = 0; i < n_; ++i) {
        auto ibor = QuantLib::ext::dynamic_pointer_cast<IborIndex>(index_->indices()[i]);
        auto cms = QuantLib::ext::dynamic_pointer_cast<SwapIndex>(index_->indices()[i]);
        QuantLib::ext::shared_ptr<FloatingRateCoupon> c;
        if (ibor) {
            auto iborPricer = iborPricers_.find(ibor->name());
            QL_REQUIRE(iborPricer != iborPricers_.end(),
                       "MCGaussianFormulaBasedCouponPricer::initialize(): need ibor coupon pricer for key '"
                           << ibor->name() << "'");
            c = QuantLib::ext::make_shared<IborCoupon>(coupon_->date(), coupon_->nominal(), coupon_->accrualStartDate(),
                                               coupon_->accrualEndDate(), coupon_->fixingDays(), ibor, 1.0, 0.0,
                                               coupon_->referencePeriodStart(), coupon_->referencePeriodEnd(),
                                               coupon_->dayCounter(), coupon_->isInArrears());
            c->setPricer(iborPricer->second);
            atmRate_[i] = c->indexFixing();
            vol[i] = iborPricer->second->capletVolatility()->volatility(fixingDate_, atmRate_[i]);
            volType_[i] = iborPricer->second->capletVolatility()->volatilityType();
            if (volType_[i] == ShiftedLognormal) {
                volShift_[i] = iborPricer->second->capletVolatility()->displacement();
            } else {
                volShift_[i] = 0.0;
            }
        } else if (cms) {
            auto cmsPricer = cmsPricers_.find(cms->iborIndex()->name());
            QL_REQUIRE(cmsPricer != cmsPricers_.end(),
                       "MCGaussianFormulaBasedCouponPricer::initialize(): need cms coupon pricer for key '"
                           << cms->iborIndex()->name() << "'");
            c = QuantLib::ext::make_shared<CmsCoupon>(coupon_->date(), coupon_->nominal(), coupon_->accrualStartDate(),
                                              coupon_->accrualEndDate(), coupon_->fixingDays(), cms, 1.0, 0.0,
                                              coupon_->referencePeriodStart(), coupon_->referencePeriodEnd(),
                                              coupon_->dayCounter(), coupon_->isInArrears());
            c->setPricer(cmsPricer->second);
            atmRate_[i] = c->indexFixing();
            vol[i] = cmsPricer->second->swaptionVolatility()->volatility(fixingDate_, cms->tenor(), atmRate_[i]);
            volType_[i] = cmsPricer->second->swaptionVolatility()->volatilityType();
            if (volType_[i] == ShiftedLognormal) {
                volShift_[i] = cmsPricer->second->swaptionVolatility()->shift(fixingDate_, cms->tenor());
            } else {
                volShift_[i] = 0.0;
            }
        } else {
            QL_FAIL("MCGaussianFormulaBasedCouponPricer::initialize(): index not recognised, must be IBOR or CMS");
        }
        Real adjRate = c->adjustedFixing();
        if (volType_[i] == ShiftedLognormal) {
            mean_[i] =
                std::log((adjRate + volShift_[i]) / (atmRate_[i] + volShift_[i])) - 0.5 * vol[i] * vol[i] * fixingTime;
        } else {
            mean_[i] = adjRate;
        }

        // incorporate Quanto adjustment into mean, if applicable

        if (index_->indices()[i]->currency().code() != paymentCurrencyCode_) {
            Real quantoCorrelation = getCorrelation(index_->indices()[i]->name(), "FX", correlation_);
            auto ccy = index_->indices()[i]->currency().code();
            auto volIt = fxVolatilities_.find(ccy);
            QL_REQUIRE(volIt != fxVolatilities_.end(), "MCGaussianFormulaBasedCouponPricer::initialize(): need fx vol "
                                                           << ccy << " vs " << paymentCurrencyCode_);
            // TODO we rely on the fx vol structure to return the atm vol for strike = null, see warning in the header
            Real fxVol = volIt->second->blackVol(fixingDate_, Null<Real>());
            mean_[i] += vol[i] * fxVol * quantoCorrelation * fixingTime;
        }

    } // for indices

    // populate covariance matrix
    for (Size i = 0; i < n_; ++i) {
        for (Size j = 0; j < i; ++j) {
            Real corr = getCorrelation(index_->indices()[i]->name(), index_->indices()[j]->name(), correlation_);
            covariance_(i, j) = covariance_(j, i) = vol[i] * vol[j] * corr * fixingTime;
        }
        covariance_(i, i) = vol[i] * vol[i] * fixingTime;
    }

} // initialize

void MCGaussianFormulaBasedCouponPricer::compute() const {

    // is the rate determined?

    if (fixingDate_ <= today_) {
        rateEstimate_ = index_->fixing(fixingDate_);
        return;
    }

    // the actual MC simulation

    accumulator_set<double, stats<tag::mean>> acc;

    Matrix C = pseudoSqrt(covariance_, salvaging_);
    InverseCumulativeNormal icn;
    const CompiledFormula& formula = index_->formula();

    MersenneTwisterUniformRng mt_(seed_);
    SobolRsg sb_(n_, seed_); // default direction integers
    Array w(n_), z(n_);
    for (Size i = 0; i < samples_; ++i) {
        if (useSobol_) {
            auto seq = sb_.nextSequence().value;
            std::transform(seq.begin(), seq.end(), w.begin(), icn);
        } else {
            for (Size j = 0; j < n_; ++j) {
                w[j] = icn(mt_.nextReal());
            }
        }
        z = C * w + mean_;
        for (Size j = 0; j < n_; ++j) {
            if (volType_[j] == ShiftedLognormal) {
                z[j] = (atmRate_[j] + volShift_[j]) * std::exp(z[j]) - volShift_[j];
            }
        }
        acc(formula(z.begin(), z.end()));
    }

    rateEstimate_ = mean(acc);
} // compute

} // namespace QuantExt
