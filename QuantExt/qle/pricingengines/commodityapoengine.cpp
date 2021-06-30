/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
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

CommodityAveragePriceOptionBaseEngine::CommodityAveragePriceOptionBaseEngine(
    const Handle<YieldTermStructure>& discountCurve, const Handle<BlackVolTermStructure>& vol, Real beta)
    : discountCurve_(discountCurve), volStructure_(vol), beta_(beta) {

    QL_REQUIRE(beta_ >= 0.0, "beta >= 0 required, found " << beta_);
    registerWith(discountCurve_);
    registerWith(volStructure_);
}

pair<Real, Size> CommodityAveragePriceOptionBaseEngine::calculateAccrued() const {

    auto result = make_pair(0.0, 0);

    Date today = Settings::instance().evaluationDate();

    // If all of the pricing dates are greater than today => no accrued
    if (today < arguments_.flow->indices().begin()->first) {
        return result;
    }

    // Calculate accrued
    for (const auto& kv : arguments_.flow->indices()) {

        // Break on the first pricing date that is greater than today
        if (today < kv.first) {
            break;
        }

        // Update accrued where pricing date is on or before today
        result.first += kv.second->fixing(kv.first);
        result.second++;
    }

    // We should have pricing dates in the period but check.
    auto n = arguments_.flow->indices().size();
    QL_REQUIRE(n > 0, "APO coupon accrued calculation has a degenerate coupon.");
    result.first /= n;

    return result;
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

bool CommodityAveragePriceOptionBaseEngine::isModelDependent(const pair<Real, Size>& accrued) const {

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
        Real payoff = arguments_.flow->gearing() * max(omega * (accrued.first - arguments_.effectiveStrike), 0.0);
        results_.value = arguments_.quantity * payoff * discount;

        return false;
    }

    // If a portion of the average price has already accrued, the effective strike of the APO will
    // have changed by the accrued amount. The strike could be non-positive.
    Real effectiveStrike = arguments_.effectiveStrike - accrued.first;
    if (effectiveStrike <= 0.0) {

        // If the effective strike is <= 0, put payoff is 0.0 and call payoff is [A - K]
        if (arguments_.type == Option::Call) {
            results_.value = (arguments_.flow->amount() - arguments_.quantity * arguments_.strikePrice) * discount;
        } else {
            results_.value = 0.0;
        }

        return false;
    }

    // If we get to here, value is model dependent
    return true;
}

void CommodityAveragePriceOptionAnalyticalEngine::calculate() const {

    // Calculate the accrued portion
    pair<Real, Size> accrued = calculateAccrued();

    // Populate some additional results that don't change
    auto& mp = results_.additionalResults;
    Real discount = discountCurve_->discount(arguments_.flow->date());
    mp["gearing"] = arguments_.flow->gearing();
    mp["spread"] = arguments_.flow->spread();
    mp["strike"] = arguments_.strikePrice;
    mp["payment_date"] = arguments_.flow->date();
    mp["accrued"] = accrued.first;
    mp["discount"] = discount;

    // If not model dependent, return early.
    if (!isModelDependent(accrued)) {
        mp["effective_strike"] = arguments_.effectiveStrike;
        mp["npv"] = results_.value;
        return;
    }

    // We will read the volatility off the surface at the effective strike
    // We should only get this far when the effectiveStrike > 0 but will check anyway
    Real effectiveStrike = arguments_.effectiveStrike - accrued.first;
    QL_REQUIRE(effectiveStrike > 0.0, "calculateSpot: expected effectiveStrike to be positive");

    // Valuation date
    Date today = Settings::instance().evaluationDate();

    // Expected value of the non-accrued portion of the average commodity prices
    // In general, m will equal n below if there is no accrued. If accrued, m > n.
    Real EA = 0.0;
    vector<Real> forwards;
    vector<Time> times;
    vector<Date> futureExpiries;
    map<Date, Real> futureVols;
    vector<Real> spotVars;
    vector<Real> futureVolsVec, spotVolsVec; // additional results only
    auto m = arguments_.flow->indices().size();
    for (const auto& p : arguments_.flow->indices()) {
        if (p.first > today) {
            forwards.push_back(p.second->fixing(p.first));
            times.push_back(volStructure_->timeFromReference(p.first));
            if (arguments_.flow->useFuturePrice()) {
                Date expiry = p.second->expiryDate();
                futureExpiries.push_back(expiry);
                if (futureVols.count(expiry) == 0) {
                    futureVols[expiry] = volStructure_->blackVol(expiry, effectiveStrike);
                }
            } else {
                spotVars.push_back(volStructure_->blackVariance(times.back(), effectiveStrike));
                spotVolsVec.push_back(std::sqrt(spotVars.back() / times.back()));
            }
            EA += forwards.back();
        }
    }
    EA /= m;

    // Expected value of A^2. Different calculation depending on whether APO references future prices or spot price.
    Real EA2 = 0.0;
    Size n = forwards.size();
    if (arguments_.flow->useFuturePrice()) {
        // References future prices
        for (Size i = 0; i < n; ++i) {
            Date e_i = futureExpiries[i];
            Volatility v_i = futureVols.at(e_i);
            EA2 += forwards[i] * forwards[i] * exp(v_i * v_i * times[i]);
            futureVolsVec.push_back(v_i);
            for (Size j = 0; j < i; ++j) {
                Date e_j = futureExpiries[j];
                Volatility v_j = futureVols.at(e_j);
                EA2 += 2 * forwards[i] * forwards[j] * exp(rho(e_i, e_j) * v_i * v_j * times[j]);
            }
        }
        mp["futureVols"] = futureVolsVec;
    } else {
        // References spot prices
        for (Size i = 0; i < n; ++i) {
            EA2 += forwards[i] * forwards[i] * exp(spotVars[i]);
            for (Size j = 0; j < i; ++j) {
                EA2 += 2 * forwards[i] * forwards[j] * exp(spotVars[j]);
            }
        }
        mp["spotVols"] = spotVolsVec;
    }
    EA2 /= m * m;

    // Calculate value
    Real tn = times.back();
    Real sigma = sqrt(log(EA2 / (EA * EA)) / tn);

    // Populate results
    results_.value = arguments_.quantity * arguments_.flow->gearing() *
                     blackFormula(arguments_.type, effectiveStrike, EA, sigma * sqrt(tn), discount);

    // Add more additional results
    // Could be part of a strip so we add the value also.
    mp["effective_strike"] = effectiveStrike;
    mp["forward"] = EA;
    mp["exp_A_2"] = EA2;
    mp["tte"] = tn;
    mp["sigma"] = sigma;
    mp["npv"] = results_.value;
    mp["times"] = times;
    mp["forwards"] = forwards;
    mp["beta"] = beta_;
}

void CommodityAveragePriceOptionMonteCarloEngine::calculate() const {

    // Calculate the accrued portion
    pair<Real, Size> accrued = calculateAccrued();

    if (isModelDependent(accrued)) {
        // Switch implementation depending on whether underlying swap references a spot or future price
        if (arguments_.flow->useFuturePrice()) {
            calculateFuture(accrued);
        } else {
            calculateSpot(accrued);
        }
    }
}

void CommodityAveragePriceOptionMonteCarloEngine::calculateSpot(const pair<Real, Size>& accrued) const {

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
    Real effectiveStrike = arguments_.effectiveStrike - accrued.first;
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
        fwdRatio[i] = arguments_.flow->index()->fixing(dates[i + 1]);
        if (i > 0) {
            fwdRatio[i] /= arguments_.flow->index()->fixing(dates[i]);
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
        for (Size i = 0; i < dt.size(); i++) {

            // Update price
            price = i == 0 ? path[i] : price * path[i];

            // Update sum of the spot prices on the pricing dates after today
            samplePayoff += price;
        }

        // Average price on this sample
        samplePayoff /= m;

        // Finally, the payoff on this sample
        samplePayoff = max(omega * (samplePayoff - effectiveStrike), 0.0);

        // Update the final average
        // Note: payoff * k / (k + 1) - left to right precedence => double division.
        payoff = k == 0 ? samplePayoff : payoff * k / (k + 1) + samplePayoff / (k + 1);
    }

    // Populate the result value
    results_.value = arguments_.quantity * arguments_.flow->gearing() * payoff * discount;
}

void CommodityAveragePriceOptionMonteCarloEngine::calculateFuture(const pair<Real, Size>& accrued) const {

    // Discount factor to the APO payment date
    Real discount = discountCurve_->discount(arguments_.flow->date());

    // Put call indicator
    Real omega = arguments_.type == Option::Call ? 1.0 : -1.0;

    // Variable to hold the payoff
    Real payoff = 0.0;

    // We will read the volatility off the surface the effective strike
    // We will only call this method when the effectiveStrike > 0 but will check anyway
    Real effectiveStrike = arguments_.effectiveStrike - accrued.first;
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
    Matrix factors(vols.size(), dt.size(), 0.0);
    Matrix stdDev(vols.size(), dt.size(), 0.0);
    for (Size j = 0; j < factors.columns(); j++) {
        for (Size i = 0; i < factors.rows(); i++) {
            factors[i][j] = exp(-vols[i] * vols[i] * dt[j] / 2.0);
            stdDev[i][j] = vols[i] * sqrt(dt[j]);
        }
    }

    // Loop over each sample
    auto m = arguments_.flow->indices().size();
    for (Size k = 0; k < samples_; k++) {

        // Sequence is N x n independent standard normal random variables
        vector<Real> sequence = rsg.nextSequence().value;

        // paths initially holds independent standard normal random variables
        Matrix paths(vols.size(), dt.size(), sequence.begin(), sequence.end());

        // Correlate the random variables in each column
        paths = sqrtCorr * paths;

        // Fill the paths
        for (Size i = 0; i < paths.rows(); i++) {
            for (Size j = 0; j < paths.columns(); j++) {
                // This is \exp(-\sigma_i^2 \delta t_j / 2) * \exp(\sigma_i \sqrt(\delta t_j) \delta W_{i,j})
                paths[i][j] = factors[i][j] * exp(stdDev[i][j] * paths[i][j]);
                if (j == 0) {
                    // If on first timestep, multiply by F_i(0)
                    paths[i][j] *= prices[i];
                } else {
                    // If on timestep greater than t_0, multiply by F_i(t_{j-1}) i.e. value of contract at
                    // previous time
                    paths[i][j] *= paths[i][j - 1];
                }
            }
        }

        // Calculate the sum of the commodity future prices on the pricing dates after today
        Real samplePayoff = 0.0;
        for (Size j = 0; j < dt.size(); j++) {
            samplePayoff += paths[futureIndex[j]][j];
        }

        // Average price on this sample
        samplePayoff /= m;

        // Finally, the payoff on this sample
        samplePayoff = max(omega * (samplePayoff - effectiveStrike), 0.0);

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
                prices.push_back(p.second->fixing(today));
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
