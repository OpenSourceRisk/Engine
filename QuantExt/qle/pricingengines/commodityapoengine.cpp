/*
 Copyright (C) 2019, 2022 Quaternion Risk Management Ltd
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

#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/processes/ornsteinuhlenbeckprocess.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/pricingengines/commodityapoengine.hpp>

using std::adjacent_difference;
using std::exp;
using std::make_pair;
using std::map;
using std::max;
using std::pair;
using std::set;
using std::vector;

namespace QuantExt {

namespace CommodityAveragePriceOptionMomementMatching {

QuantLib::Real MomentMatchingResults::firstMoment() { return forward; }
QuantLib::Real MomentMatchingResults::secondMoment() { return sigma * sigma * tn; }
QuantLib::Real MomentMatchingResults::stdDev() { return std::sqrt(secondMoment()); }
QuantLib::Time MomentMatchingResults::timeToExpriy() { return tn; }

MomentMatchingResults matchFirstTwoMomentsTurnbullWakeman(
    const ext::shared_ptr<CommodityIndexedAverageCashFlow>& flow,
    const ext::shared_ptr<QuantLib::BlackVolTermStructure>& vol,
    const std::function<double(const QuantLib::Date& expiry1, const QuantLib::Date& expiry2)>& rho,
    QuantLib::Real strike) {
    Date today = Settings::instance().evaluationDate();
    MomentMatchingResults res;

    res.tn = 0.0;
    res.accruals = 0.0;
    double EA = 0;
    std::vector<Date> futureExpiries;
    std::map<Date, Real> futureVols;
    std::vector<double> spotVariances;
    size_t n = flow->indices().size();
    double atmUnderlyingCcy = 0;

    for (const auto& [pricingDate, index] : flow->indices()) {
        Date fixingDate = index->fixingCalendar().adjust(pricingDate, Preceding);
        Real fxRate = (flow->fxIndex()) ? flow->fxIndex()->fixing(fixingDate) : 1.0;
        res.indexNames.push_back(index->name());
        res.pricingDates.push_back(fixingDate);
        res.indexExpiries.push_back(index->expiryDate());
        res.fixings.push_back(index->fixing(fixingDate) * fxRate);
        if (pricingDate <= today) {
            res.accruals += res.fixings.back();
        } else {
            atmUnderlyingCcy = index->fixing(fixingDate);
            res.forwards.push_back(res.fixings.back());
            res.times.push_back(vol->timeFromReference(pricingDate));
            // use ATM vol if no strike is given
            double K = strike == Null<Real>() ? atmUnderlyingCcy : strike;
            if (flow->useFuturePrice()) {
                Date expiry = index->expiryDate();
                futureExpiries.push_back(expiry);
                if (futureVols.count(expiry) == 0) {
                    futureVols[expiry] = vol->blackVol(expiry, K);
                }
            } else {
                spotVariances.push_back(
                    vol->blackVariance(res.times.back(), K));
                res.spotVols.push_back(std::sqrt(spotVariances.back() / res.times.back()));
            }
            EA += res.forwards.back();
        }
    }
    res.accruals /= static_cast<double>(n);
    EA /= static_cast<double>(n);

    res.forward = EA;

    double EA2 = 0.0;

    if (flow->useFuturePrice()) {
        // References future prices
        for (Size i = 0; i < res.forwards.size(); ++i) {
            Date e_i = futureExpiries[i];
            Volatility v_i = futureVols.at(e_i);
            res.futureVols.push_back(v_i);
            EA2 += res.forwards[i] * res.forwards[i] * exp(v_i * v_i * res.times[i]);
            for (Size j = 0; j < i; ++j) {
                Date e_j = futureExpiries[j];
                Volatility v_j = futureVols.at(e_j);
                EA2 += 2 * res.forwards[i] * res.forwards[j] * exp(rho(e_i, e_j) * v_i * v_j * res.times[j]);
            }
        }
    } else {
        // References spot prices
        for (Size i = 0; i < res.forwards.size(); ++i) {
            EA2 += res.forwards[i] * res.forwards[i] * exp(spotVariances[i]);
            for (Size j = 0; j < i; ++j) {
                EA2 += 2 * res.forwards[i] * res.forwards[j] * exp(spotVariances[j]);
            }
        }
    }
    EA2 /= std::pow(static_cast<double>(n), 2.0);
    res.EA2 = EA2;

    QL_REQUIRE(!std::isinf(EA2),
               "moment matching fails (EA2 = inf) - this is possibly due to too high input volatilities.");

    // Calculate value
    if (!res.times.empty()) {
        res.tn = res.times.back();
        double s = EA2 / (EA * EA);
        // if future vol = 0 for all dates, then EA2 = EA*EA, but
        // due to numerical precision EA2 can be actually less than EA*EA 
        if (s < 1.0 || QuantLib::close_enough(s, 1)) {
            res.sigma = 0.0;
        } else {
            res.sigma = std::sqrt(std::log(s) / res.tn);
        }
    } else {
        res.tn = 0;
        res.sigma = 0;
    }
    return res;
}
} // namespace CommodityAveragePriceOptionMomementMatching

CommodityAveragePriceOptionBaseEngine::CommodityAveragePriceOptionBaseEngine(
    const Handle<YieldTermStructure>& discountCurve, const QuantLib::Handle<QuantExt::BlackScholesModelWrapper>& model,
    QuantLib::Real beta)
    : discountCurve_(discountCurve), volStructure_(model->processes().front()->blackVolatility()), beta_(beta) {
    QL_REQUIRE(beta_ >= 0.0, "beta >= 0 required, found " << beta_);
    registerWith(model);
}

CommodityAveragePriceOptionBaseEngine::CommodityAveragePriceOptionBaseEngine(
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
    const QuantLib::Handle<QuantLib::BlackVolTermStructure>& vol, QuantLib::Real beta)
    : discountCurve_(discountCurve), volStructure_(vol), beta_(beta) {
    QL_REQUIRE(beta_ >= 0.0, "beta >= 0 required, found " << beta_);
    registerWith(discountCurve_);
    registerWith(volStructure_);
}

Real CommodityAveragePriceOptionBaseEngine::rho(const Date& ed_1, const Date& ed_2) const {
    if (beta_ == 0.0 || ed_1 == ed_2) {
        return 1.0;
    } else {
        Time t_1 = volStructure_->timeFromReference(ed_1);
        Time t_2 = volStructure_->timeFromReference(ed_2);
        return exp(-beta_ * fabs(t_2 - t_1));
    }
}

bool CommodityAveragePriceOptionBaseEngine::isModelDependent() const {

    // Discount factor to the APO payment date
    Real discount = discountCurve_->discount(arguments_.flow->date());

    // Valuation date
    Date today = Settings::instance().evaluationDate();

    // If all pricing dates are on or before today. This can happen when the APO payment date is a positive number
    // of days after the final APO pricing date and today is in between.
    if (today >= arguments_.flow->indices().rbegin()->first) {

        // Put call indicator
        Real omega = arguments_.type == Option::Call ? 1.0 : -1.0;

        // Populate the result value
        Real payoff = arguments_.flow->gearing() * max(omega * (arguments_.accrued - arguments_.effectiveStrike), 0.0);
        results_.value = arguments_.quantity * payoff * discount;

        return false;
    }

    // If a portion of the average price has already accrued, the effective strike of the APO will
    // have changed by the accrued amount. The strike could be non-positive.
    Real effectiveStrike = arguments_.effectiveStrike - arguments_.accrued;
    if (effectiveStrike <= 0.0) {

        // If the effective strike is <= 0, put payoff is 0.0 and call payoff is [A - K]
        if (arguments_.type == Option::Call) {
            results_.value = (arguments_.flow->amount() - arguments_.quantity * arguments_.strikePrice) * discount;
        } else {
            results_.value = 0.0;
        }

        return false;
    }

    // If we get to here, value is model dependent except the option was knocked out

    bool barrierTriggered = false;
    Real lastFixing = 0.0;

    for (const auto& kv : arguments_.flow->indices()) {
        // Break on the first pricing date that is greater than today
        if (today < kv.first) {
            break;
        }
        // Update accrued where pricing date is on or before today
        Real fxRate{1.};
        if(arguments_.fxIndex)
            fxRate=arguments_.fxIndex->fixing(kv.first);
        lastFixing = fxRate*kv.second->fixing(kv.first);
        if (arguments_.barrierStyle == Exercise::American)
            barrierTriggered = barrierTriggered || this->barrierTriggered(lastFixing, false);
    }

    if (arguments_.barrierStyle == Exercise::European)
        barrierTriggered = this->barrierTriggered(lastFixing, false);

    if(barrierTriggered && (arguments_.barrierType == Barrier::DownOut || arguments_.barrierType == Barrier::UpOut)) {
        results_.value = 0.0;
        return false;
    }

    return true;
}

bool CommodityAveragePriceOptionBaseEngine::barrierTriggered(const Real price, const bool logPrice) const {
    if (arguments_.barrierLevel == Null<Real>())
        return false;
    Real tmp = logPrice ? logBarrier_ : arguments_.barrierLevel;
    if (arguments_.barrierType == Barrier::DownIn || arguments_.barrierType == Barrier::DownOut)
        return price <= tmp;
    else if (arguments_.barrierType == Barrier::UpIn || arguments_.barrierType == Barrier::UpOut)
        return price >= tmp;
    return false;
}

bool CommodityAveragePriceOptionBaseEngine::alive(const bool barrierTriggered) const {
    if (arguments_.barrierLevel == Null<Real>())
        return true;
    return ((arguments_.barrierType == Barrier::DownIn || arguments_.barrierType == Barrier::UpIn) &&
            barrierTriggered) ||
           ((arguments_.barrierType == Barrier::DownOut || arguments_.barrierType == Barrier::UpOut) &&
            !barrierTriggered);
}

void CommodityAveragePriceOptionAnalyticalEngine::calculate() const {

    QL_REQUIRE(arguments_.barrierLevel == Null<Real>(),
               "CommodityAveragePriceOptionAnalyticalEngine does not support barrier feature. Use MC engine instead.");

    // Populate some additional results that don't change
    auto& mp = results_.additionalResults;
    Real discount = discountCurve_->discount(arguments_.flow->date());
    mp["gearing"] = arguments_.flow->gearing();
    mp["spread"] = arguments_.flow->spread();
    mp["strike"] = arguments_.strikePrice;
    mp["payment_date"] = arguments_.flow->date();
    mp["accrued"] = arguments_.accrued;
    mp["discount"] = discount;
    if(arguments_.fxIndex)
        mp["FXIndex"] = arguments_.fxIndex->name();

    // If not model dependent, return early.
    if (!isModelDependent()) {
        mp["effective_strike"] = arguments_.effectiveStrike;
        mp["npv"] = results_.value;
        return;
    }

    // We will read the volatility off the surface at the effective strike
    // We should only get this far when the effectiveStrike > 0 but will check anyway
    Real effectiveStrike = arguments_.effectiveStrike - arguments_.accrued;
    QL_REQUIRE(effectiveStrike > 0.0, "calculateSpot: expected effectiveStrike to be positive");

    // Valuation date
    
    auto matchedMoments = CommodityAveragePriceOptionMomementMatching::matchFirstTwoMomentsTurnbullWakeman(
        arguments_.flow, *volStructure_,
        std::bind(&CommodityAveragePriceOptionAnalyticalEngine::rho, this, std::placeholders::_1, std
                  ::placeholders::_2), effectiveStrike);
     
    if (arguments_.flow->useFuturePrice()) {
        mp["futureVols"] = matchedMoments.futureVols;
    } else {
        mp["spotVols"] = matchedMoments.spotVols;
    }

    // Populate results
    results_.value = arguments_.quantity * arguments_.flow->gearing() *
                         blackFormula(arguments_.type, effectiveStrike, matchedMoments.firstMoment(),
                                      matchedMoments.stdDev(), discount);

    // Add more additional results
    // Could be part of a strip so we add the value also.
    mp["effective_strike"] = effectiveStrike;
    mp["forward"] = matchedMoments.forward;
    mp["exp_A_2"] = matchedMoments.EA2;
    mp["tte"] = matchedMoments.timeToExpriy();
    mp["sigma"] = matchedMoments.sigma;
    mp["npv"] = results_.value;
    mp["times"] = matchedMoments.times;
    mp["forwards"] = matchedMoments.forwards;
    mp["beta"] = beta_;
}

void CommodityAveragePriceOptionMonteCarloEngine::calculate() const {
    if (isModelDependent()) {
        // Switch implementation depending on whether underlying swap references a spot or future price
        if (arguments_.flow->useFuturePrice()) {
            calculateFuture();
        } else {
            calculateSpot();
        }
    }
}

void CommodityAveragePriceOptionMonteCarloEngine::calculateSpot() const {

    // Discount factor to the APO payment date
    Real discount = discountCurve_->discount(arguments_.flow->date());

    // Put call indicator
    Real omega = arguments_.type == Option::Call ? 1.0 : -1.0;

    // Variable to hold the payoff
    Real payoff = 0.0;

    // Vector of timesteps from today = t_0 out to last pricing date t_n
    // i.e. {t_1 - t_0, t_2 - t_1,..., t_n - t_{n-1}}
    vector<Date> dates;
    vector<Real> dt = timegrid(dates);

    // On each Monte Carlo sample, we must generate the spot price process path over n time steps.
    LowDiscrepancy::rsg_type rsg = LowDiscrepancy::make_sequence_generator(dt.size(), seed_);

    // We will read the volatility off the surface at the effective strike
    // We will only call this method when the effectiveStrike > 0 but will check anyway
    Real effectiveStrike = arguments_.effectiveStrike - arguments_.accrued;
    QL_REQUIRE(effectiveStrike > 0.0, "calculateSpot: expected effectiveStrike to be positive");

    // Precalculate:
    // exp(-1/2 forward variance over dt): \exp \left(-\frac{1}{2} \int_{t_{i-1}}^{t_i} \sigma^2(u) du \right)
    // forward std dev over dt: \exp \left(\sqrt{\int_{t_{i-1}}^{t_i} \sigma^2(u) du} \right)
    // forward ratio: \exp \left(\int_{t_{i-1}}^{t_i} r(u) - r_f(u) \, du \right) = \frac{F(0, t_i)}{F(0, t_{i-1})}
    //                first period is multiplied by S(0)
    // factors array is exp(-1/2 forward variance over dt) x forward ratio
    Array expHalfFwdVar(dt.size(), 0.0);
    Array fwdStdDev(dt.size(), 0.0);
    Array fwdRatio(dt.size(), 0.0);
    Time t = 0.0;
    for (Size i = 0; i < dt.size(); i++) {
        t += dt[i];
        expHalfFwdVar[i] = volStructure_->blackForwardVariance(t - dt[i], t, effectiveStrike);
        fwdStdDev[i] = sqrt(expHalfFwdVar[i]);
        expHalfFwdVar[i] = exp(-expHalfFwdVar[i] / 2.0);
        Real fxRate{1.};
        if(arguments_.flow->fxIndex())
            fxRate = arguments_.flow->fxIndex()->fixing(dates[i+1]);
        fwdRatio[i] = fxRate * arguments_.flow->index()->fixing(dates[i + 1]);
        if (i > 0) {
            if(arguments_.flow->fxIndex())
                fxRate = arguments_.flow->fxIndex()->fixing(dates[i]);
            fwdRatio[i] /= (fxRate * arguments_.flow->index()->fixing(dates[i]));
        }
    }
    Array factors = expHalfFwdVar * fwdRatio;

    // Loop over each sample
    auto m = arguments_.flow->indices().size();
    for (Size k = 0; k < samples_; k++) {

        // Sequence is n independent standard normal random variables
        vector<Real> sequence = rsg.nextSequence().value;

        // Calculate payoff on this sample - price holds the evolved price
        Real price = 0.0;
        Real samplePayoff = 0.0;
        Array path(sequence.begin(), sequence.end());
        path = Exp(path * fwdStdDev) * factors;
        bool barrierTriggered = false;
        for (Size i = 0; i < dt.size(); i++) {

            // Update price
            price = i == 0 ? path[i] : price * path[i];

            // Update sum of the spot prices on the pricing dates after today
            samplePayoff += price;

            // check barrier
            if (arguments_.barrierStyle == Exercise::American)
                barrierTriggered = barrierTriggered || this->barrierTriggered(price, false);
        }
        // Average price on this sample
        samplePayoff /= m;

        // Finally, the payoff on this sample
        samplePayoff = max(omega * (samplePayoff - effectiveStrike), 0.0);

        // account for barrier
        if (arguments_.barrierStyle == Exercise::European)
            barrierTriggered = this->barrierTriggered(price, false);

        if (!alive(barrierTriggered))
            samplePayoff = 0.0;

        // Update the final average
        // Note: payoff * k / (k + 1) - left to right precedence => double division.
        payoff = k == 0 ? samplePayoff : payoff * k / (k + 1) + samplePayoff / (k + 1);
    }

    // Populate the result value
    results_.value = arguments_.quantity * arguments_.flow->gearing() * payoff * discount;
}

void CommodityAveragePriceOptionMonteCarloEngine::calculateFuture() const {

    // this method uses checkBarrier() on log prices, therefore we have to init the log barrier level
    if (arguments_.barrierLevel != Null<Real>())
        logBarrier_ = std::log(arguments_.barrierLevel);

    // Discount factor to the APO payment date
    Real discount = discountCurve_->discount(arguments_.flow->date());

    // Put call indicator
    Real omega = arguments_.type == Option::Call ? 1.0 : -1.0;

    // Variable to hold the payoff
    Real payoff = 0.0;

    // We will read the volatility off the surface the effective strike
    // We will only call this method when the effectiveStrike > 0 but will check anyway
    Real effectiveStrike = arguments_.effectiveStrike - arguments_.accrued;
    QL_REQUIRE(effectiveStrike > 0.0, "calculateFuture: expected effectiveStrike to be positive");

    // Unique future expiry dates i.e. contracts, their volatilities and the correlation matrix between them
    Matrix sqrtCorr;
    vector<Real> vols;
    vector<Real> prices;
    vector<Size> futureIndex;
    setupFuture(vols, sqrtCorr, prices, futureIndex, effectiveStrike);

    // Vector of timesteps from today = t_0 out to last pricing date t_n
    // i.e. {t_1 - t_0, t_2 - t_1,..., t_n - t_{n-1}}. Don't need the dates here.
    vector<Date> dates;
    vector<Real> dt = timegrid(dates);

    // On each Monte Carlo sample, we must generate the paths for N (size of vols) future contracts where
    // each path has n time steps. We will represent the paths with an N x n matrix. First step is to fill the
    // matrix with N x n _independent_ standard normal variables. Then correlate the N variables in each column
    // using the sqrtCorr matrix and then fill each entry F_{i, j} in the matrix with the value of the i-th
    // future price process at timestep j. Note, we will possibly simulate contracts past their expiries but not
    // use the price in the APO rate averaging.
    LowDiscrepancy::rsg_type rsg = LowDiscrepancy::make_sequence_generator(vols.size() * dt.size(), seed_);

    // Precalculate exp(-0.5 \sigma_i^2 \delta t_j) and std dev = sqrt(\delta t_j) \sigma_i
    Matrix drifts(vols.size(), dt.size(), 0.0);
    Matrix stdDev(vols.size(), dt.size(), 0.0);
    Array logPrices(vols.size());
    for (Size i = 0; i < drifts.rows(); i++) {
        logPrices[i] = std::log(prices[i]);
        for (Size j = 0; j < drifts.columns(); j++) {
            drifts[i][j] = -vols[i] * vols[i] * dt[j] / 2.0;
            stdDev[i][j] = vols[i] * sqrt(dt[j]);
        }
    }

    // Loop over each sample
    auto m = arguments_.flow->indices().size();
    Matrix paths(vols.size(), dt.size());
    for (Size k = 0; k < samples_; ++k) {

        // Sequence is N x n independent standard normal random variables
        const vector<Real>& sequence = rsg.nextSequence().value;

        // paths initially holds independent standard normal random variables
        std::copy(sequence.begin(), sequence.end(), paths.begin());

        // Correlate the random variables in each column
        paths = sqrtCorr * paths;

        // Fill the paths
        for (Size i = 0; i < paths.rows(); ++i) {
            for (Size j = 0; j < paths.columns(); ++j) {
                paths[i][j] = (j == 0 ? logPrices[i] : paths[i][j - 1]) + drifts[i][j] + stdDev[i][j] * paths[i][j];
            }
        }

        // Calculate the sum of the commodity future prices on the pricing dates after today
        Real samplePayoff = 0.0;
        bool barrierTriggered = false;
        Real price = 0.0;
        for (Size j = 0; j < dt.size(); ++j) {
            price = paths[futureIndex[j]][j];
            if (arguments_.barrierStyle == Exercise::American)
                barrierTriggered = barrierTriggered || this->barrierTriggered(price, true);
            samplePayoff += std::exp(price);
        }

        // Average price on this sample
        samplePayoff /= m;

        // Finally, the payoff on this sample
        samplePayoff = max(omega * (samplePayoff - effectiveStrike), 0.0);

        // account for barrier
        if (arguments_.barrierStyle == Exercise::European)
            barrierTriggered = this->barrierTriggered(price, true);

        if (!alive(barrierTriggered))
            samplePayoff = 0.0;

        // Update the final average
        // Note: payoff * k / (k + 1) - left to right precedence => double division.
        payoff = k == 0 ? samplePayoff : payoff * k / (k + 1) + samplePayoff / (k + 1);
    }

    // Populate the result value
    results_.value = arguments_.quantity * arguments_.flow->gearing() * payoff * discount;
}

void CommodityAveragePriceOptionMonteCarloEngine::setupFuture(vector<Real>& outVolatilities, Matrix& outSqrtCorr,
                                                              vector<Real>& prices, vector<Size>& futureIndex,
                                                              Real strike) const {

    // Ensure that vectors are empty
    outVolatilities.clear();
    prices.clear();
    futureIndex.clear();

    // Will hold the result
    map<Date, Real> result;

    // Note that here we make the simplifying assumption that the volatility can be read from the volatility
    // term structure at the future contract's expiry date. In most cases, if the volatility term structure is built
    // from options on futures, the option contract expiry will be a number of days before the future contract expiry
    // and we should really read off the term structure at that date. Also populate a temp vector containing the key
    // dates for use in the loop below where we populate the sqrt correlation matrix.

    // Initialise result with expiry date keys that are still live in the APO
    Date today = Settings::instance().evaluationDate();
    set<Date> expiryDates;
    for (const auto& p : arguments_.flow->indices()) {
        if (p.first > today) {
            Date expiry = p.second->expiryDate();
            // If expiry has not been encountered yet
            if (expiryDates.insert(expiry).second) {
                outVolatilities.push_back(volStructure_->blackVol(expiry, strike));
                Real fxRate{1.};
                if(arguments_.flow->fxIndex())
                    fxRate = arguments_.flow->fxIndex()->fixing(expiry);
                prices.push_back(fxRate*p.second->fixing(today));//check if today should not be p.first
            }
            futureIndex.push_back(expiryDates.size() - 1);
        }
    }

    // Populate the outSqrtCorr matrix
    vector<Date> vExpiryDates(expiryDates.begin(), expiryDates.end());
    outSqrtCorr = Matrix(vExpiryDates.size(), vExpiryDates.size(), 1.0);
    for (Size i = 0; i < vExpiryDates.size(); i++) {
        for (Size j = 0; j < i; j++) {
            outSqrtCorr[i][j] = outSqrtCorr[j][i] = rho(vExpiryDates[i], vExpiryDates[j]);
        }
    }
    outSqrtCorr = pseudoSqrt(outSqrtCorr, SalvagingAlgorithm::None);
}

vector<Real> CommodityAveragePriceOptionMonteCarloEngine::timegrid(vector<Date>& outDates) const {

    // Initialise result with expiry date keys that are still live in the APO
    // i.e. {t_1 - t_0, t_2 - t_0,..., t_n - t_0} where t_0 corresponds to today
    vector<Real> times;
    outDates.clear();
    Date today = Settings::instance().evaluationDate();
    outDates.push_back(today);
    for (const auto& p : arguments_.flow->indices()) {
        if (p.first > today) {
            outDates.push_back(p.first);
            times.push_back(volStructure_->timeFromReference(p.first));
        }
    }

    // Populate time deltas i.e. {t_1 - t_0, t_2 - t_1,..., t_n - t_{n-1}}
    vector<Real> dt(times.size());
    adjacent_difference(times.begin(), times.end(), dt.begin());

    return dt;
}

} // namespace QuantExt
