/*
  Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <iomanip>
#include <iostream>
#include <ql/exercise.hpp>
#include <ql/math/matrixutilities/choleskydecomposition.hpp>
#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/math/randomnumbers/inversecumulativerng.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/processes/ornsteinuhlenbeckprocess.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/pricingengines/commodityswaptionengine.hpp>

using std::map;
using std::max;
using std::vector;

namespace {

using QuantExt::CommodityIndexedAverageCashFlow;
using QuantExt::CommodityIndexedCashFlow;
using QuantLib::CashFlow;
using QuantLib::close;
using QuantLib::Leg;

// Check that all cashflows in the 'leg' are of the type 'CommCashflow'
// Check also that the spread is 0.0 and the gearing is 1.0. These restrictions should be easy to remove
// but we should only spend time on this if it is needed.
template <typename CommCashflow> void checkCashflows(const Leg& leg) {
    for (const QuantLib::ext::shared_ptr<CashFlow>& cf : leg) {
        auto ccf = QuantLib::ext::dynamic_pointer_cast<CommCashflow>(cf);
        QL_REQUIRE(ccf, "checkCashflows: not all of the "
                            << "cashflows on the commodity floating leg are of the same type");
        QL_REQUIRE(close(ccf->spread(), 0.0), "checkCashflows: a non-zero spread on a commodity swap "
                                                  << "underlying a commodity swaption is not supported");
        QL_REQUIRE(close(ccf->gearing(), 1.0), "checkCashflows: a gearing different from 1.0 on a commodity swap "
                                                   << "underlying a commodity swaption is not supported");
    }
}

// If the first coupon in the leg references a commodity future price, return true. If it references a spot price
// return false. If the leg is not a commodity leg, throw.
bool referencesFuturePrice(const Leg& leg) {
    QuantLib::ext::shared_ptr<CashFlow> cf = leg.front();
    if (QuantLib::ext::shared_ptr<CommodityIndexedCashFlow> ccf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(cf)) {
        return ccf->useFuturePrice();
    } else if (QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow> ccf =
                   QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(cf)) {
        return ccf->useFuturePrice();
    } else {
        QL_FAIL("referencesFuturePrice: expected leg to be a commodity leg");
    }
}

} // namespace

namespace QuantExt {

CommoditySwaptionBaseEngine::CommoditySwaptionBaseEngine(const Handle<YieldTermStructure>& discountCurve,
                                                         const Handle<BlackVolTermStructure>& vol, Real beta)
    : discountCurve_(discountCurve), volStructure_(vol), beta_(beta) {
    QL_REQUIRE(beta_ >= 0.0, "beta >= 0 required, found " << beta_);
    registerWith(discountCurve_);
    registerWith(volStructure_);
}

Size CommoditySwaptionBaseEngine::fixedLegIndex() const {

    Size fixedLegIndex = Null<Size>();
    bool haveFloatingLeg = false;

    QL_REQUIRE(arguments_.legs.size() == 2, "Two legs expected but found " << arguments_.legs.size());

    // Check both legs and populate the index of the fixed leg.
    for (Size i = 0; i < 2; i++) {
        QuantLib::ext::shared_ptr<CashFlow> cf = arguments_.legs[i].front();
        if (QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow> flow =
                QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(cf)) {
            haveFloatingLeg = true;
            checkCashflows<CommodityIndexedAverageCashFlow>(arguments_.legs[i]);
        } else if (QuantLib::ext::shared_ptr<CommodityIndexedCashFlow> flow =
                       QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(cf)) {
            haveFloatingLeg = true;
            checkCashflows<CommodityIndexedCashFlow>(arguments_.legs[i]);
        } else {
            fixedLegIndex = i;
        }
    }

    QL_REQUIRE(haveFloatingLeg, "CommoditySwaptionBaseEngine: expected the swap to have a commodity floating leg");
    QL_REQUIRE(fixedLegIndex != Null<Size>(), "CommoditySwaptionBaseEngine: expected the swap to have a fixed leg");

    return fixedLegIndex;
}

Real CommoditySwaptionBaseEngine::fixedLegValue(Size fixedLegIndex) const {

    // Return the commodity fixed leg value at the swaption expiry time.
    // This is the quantity K^{*} in the ORE+ Product Catalogue.
    Real value = 0.0;

    for (const QuantLib::ext::shared_ptr<CashFlow>& cf : arguments_.legs[fixedLegIndex]) {
        value += cf->amount() * discountCurve_->discount(cf->date());
    }

    Date exercise = arguments_.exercise->dateAt(0);
    Real discountExercise = discountCurve_->discount(exercise);
    value /= discountExercise;

    return value;
}

Real CommoditySwaptionBaseEngine::strike(Size fixedLegIndex) const {

    // First calculation period fixed leg amount
    Real amount = arguments_.legs[fixedLegIndex].front()->amount();

    // Divide by first calculation period floating leg (full calculation period) quantity
    Size idxFloat = fixedLegIndex == 0 ? 1 : 0;
    QuantLib::ext::shared_ptr<CashFlow> cf = arguments_.legs[idxFloat].front();
    if (QuantLib::ext::shared_ptr<CommodityIndexedCashFlow> ccf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(cf)) {
        return amount / ccf->periodQuantity();
    } else if (QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow> ccf =
                   QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(cf)) {
        return amount / ccf->periodQuantity();
    } else {
        QL_FAIL("Expected a CommodityIndexedCashFlow or CommodityIndexedAverageCashFlow");
    }
}

Real CommoditySwaptionBaseEngine::rho(const Date& ed_1, const Date& ed_2) const {

    if (beta_ == 0.0 || ed_1 == ed_2) {
        return 1.0;
    } else {
        Time t_1 = volStructure_->timeFromReference(ed_1);
        Time t_2 = volStructure_->timeFromReference(ed_2);
        return exp(-beta_ * fabs(t_2 - t_1));
    }
}

bool CommoditySwaptionBaseEngine::averaging(Size floatLegIndex) const {

    // All cashflows in the floating leg have been checked for same type so just use first one here
    return static_cast<bool>(
        QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(arguments_.legs[floatLegIndex].front()));
}

void CommoditySwaptionEngine::calculate() const {

    // Get the index of the fixed and floating leg
    Size idxFixed = fixedLegIndex();
    Size idxFloat = idxFixed == 0 ? 1 : 0;

    // Fixed leg value is the strike
    Real kStar = fixedLegValue(idxFixed);

    // Max quantity is used as a normalisation factor
    Real normFactor = maxQuantity(idxFloat);

    // E[A(t_e)] from the ORE+ Product Catalogue
    Real EA = expA(idxFloat, normFactor);

    // Fixed leg strike price. This determines the strike at which we query the volatility surface in the
    // calculations. The implementation here just looks at the fixed price in the first period of the fixed leg. If
    // we have an underlying swap where the fixed price varies a lot over different calculation periods, this may
    // lead to a mispricing.
    Real strikePrice = strike(idxFixed);

    // E[A^2(t_e)] from the ORE+ Product Catalogue
    Real EAA = expASquared(idxFloat, strikePrice, normFactor);

    // Discount factor to the exercise date, P(0, t_e) from the ORE+ Product Catalogue
    Date exercise = arguments_.exercise->dateAt(0);
    Real discountExercise = discountCurve_->discount(exercise);

    // Time to swaption expiry, t_e from the ORE+ Product Catalogue
    Time t_e = volStructure_->timeFromReference(exercise);

    // sigma_X from the ORE+ Product Catalogue
    Real sigmaX = sqrt(log(EAA / (EA * EA)) / t_e);

    // We have used EA to get sigmaX so we can modify EA again now to remove the normalisation factor.
    EA *= normFactor;

    // If fixed leg payer flag value is -1 => payer swaption. In this case, we want \omega = 1 in black formula
    // so we need type = Call.
    Option::Type type = arguments_.payer[idxFixed] < 0 ? Option::Call : Option::Put;

    // Populate the value using Black-76 formula as documented in ORE+ Product Catalogue
    results_.value = blackFormula(type, kStar, EA, sigmaX * sqrt(t_e), discountExercise);

    // Populate some additional results
    results_.additionalResults["Sigma"] = sigmaX;
    results_.additionalResults["Forward"] = EA;
    results_.additionalResults["Strike"] = kStar;
    results_.additionalResults["StrikePrice"] = strikePrice;
    results_.additionalResults["Expiry"] = t_e;
}

Real CommoditySwaptionEngine::expA(Size floatLegIndex, Real normFactor) const {

    // This is the quantity E[A(t_e)] in the ORE+ Product Catalogue.
    Real value = 0.0;

    for (const QuantLib::ext::shared_ptr<CashFlow>& cf : arguments_.legs[floatLegIndex]) {
        value += cf->amount() * discountCurve_->discount(cf->date()) / normFactor;
    }

    Date exercise = arguments_.exercise->dateAt(0);
    Real discountExercise = discountCurve_->discount(exercise);
    value /= discountExercise;

    return value;
}

Real CommoditySwaptionEngine::expASquared(Size floatLegIndex, Real strike, Real normFactor) const {

    // This is the quantity E[A^2(t_e)] in the ORE+ Product Catalogue.
    Real value = 0.0;

    // Is the floating leg averaging.
    bool isAveraging = averaging(floatLegIndex);

    // Calculate E[A^2(t_e)]
    Size numPeriods = arguments_.legs[floatLegIndex].size();
    for (Size i = 0; i < numPeriods; i++) {
        for (Size j = 0; j <= i; j++) {
            Real factor = i == j ? 1.0 : 2.0;
            value += factor * crossTerms(arguments_.legs[floatLegIndex][i], arguments_.legs[floatLegIndex][j],
                                         isAveraging, strike, normFactor);
        }
    }

    // Divide through by P^2(0, t_e) to get quantity E[A^2(t_e)] in the ORE+ Product Catalogue.
    Date exercise = arguments_.exercise->dateAt(0);
    Real discountExercise = discountCurve_->discount(exercise);
    value /= (discountExercise * discountExercise);

    return value;
}

Real CommoditySwaptionEngine::crossTerms(const QuantLib::ext::shared_ptr<CashFlow>& cf_1,
                                         const QuantLib::ext::shared_ptr<CashFlow>& cf_2, bool isAveraging, Real strike,
                                         Real normFactor) const {

    // Time to swaption expiry
    Time t_e = volStructure_->timeFromReference(arguments_.exercise->dateAt(0));

    // Switch depending on whether the two cashflows are averaging or not
    if (isAveraging) {

        // Must have CommodityIndexedAverageCashFlow if averaging
        QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow> ccf_1 =
            QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(cf_1);
        QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow> ccf_2 =
            QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(cf_2);

        // Product of quantities
        Real result = ccf_1->periodQuantity() / normFactor;
        result *= ccf_2->periodQuantity() / normFactor;

        // Multiply by discount factor to payment date
        result *= discountCurve_->discount(ccf_1->date());
        result *= discountCurve_->discount(ccf_2->date());

        // Divide by number of observations
        result /= ccf_1->indices().size();
        result /= ccf_2->indices().size();

        // The cross terms due to the averaging in each cashflow's period. The calculation here differs
        // depending on whether cashflows reference a future price or spot.
        Real cross = 0.0;
        if (ccf_1->useFuturePrice()) {

            std::vector<Real> prices1(ccf_1->indices().size()), prices2(ccf_2->indices().size()),
                vol1(ccf_1->indices().size()), vol2(ccf_2->indices().size());
            std::vector<Date> expiries1(ccf_1->indices().size()), expiries2(ccf_2->indices().size());

            Size i = 0;
            for (const auto& [d, p] : ccf_1->indices()) {
                expiries1[i] = p->expiryDate();
                Real fxRate{1.};
                if (ccf_1->fxIndex())
                    fxRate = ccf_1->fxIndex()->fixing(expiries1[i]);
                prices1[i] = fxRate * p->fixing(expiries1[i]);
                vol1[i] = volStructure_->blackVol(expiries1[i], strike);
                ++i;
            }

            i = 0;
            for (const auto& [d, p] : ccf_2->indices()) {
                expiries2[i] = p->expiryDate();
                Real fxRate{1.};
                if (ccf_2->fxIndex())
                    fxRate = ccf_2->fxIndex()->fixing(expiries2[i]);
                prices2[i] = fxRate * p->fixing(expiries2[i]);
                vol2[i] = volStructure_->blackVol(expiries2[i], strike);
                ++i;
            }

            for (Size i = 0; i < prices1.size(); ++i) {
                for (Size j = 0; j < prices2.size(); ++j) {
                    cross += prices1[i] * prices2[j] * exp(rho(expiries1[i], expiries2[j]) * vol1[i] * vol2[j] * t_e);
                }
            }

        } else {

            std::vector<Real> prices1(ccf_1->indices().size()), prices2(ccf_2->indices().size());

            Size i = 0;
            for (const auto& [d, p] : ccf_1->indices()) {
                Real fxRate{1.};
                if (ccf_1->fxIndex())
                    fxRate = ccf_1->fxIndex()->fixing(d);
                prices1[i] = fxRate * p->fixing(d);
                ++i;
            }

            i = 0;
            for (const auto& [d, p] : ccf_2->indices()) {
                Real fxRate{1.};
                if (ccf_2->fxIndex())
                    fxRate = ccf_2->fxIndex()->fixing(d);
                prices2[i] = fxRate * p->fixing(d);
                ++i;
            }

            for (Size i = 0; i < prices1.size(); ++i) {
                for (Size j = 0; j < prices2.size(); ++j) {
                    cross += prices1[i] * prices2[j];
                }
            }

            cross *= exp(volStructure_->blackVariance(t_e, strike));
        }
        result *= cross;

        return result;

    } else {

        // Must have CommodityIndexedCashFlow if not averaging
        QuantLib::ext::shared_ptr<CommodityIndexedCashFlow> ccf_1 = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(cf_1);
        QuantLib::ext::shared_ptr<CommodityIndexedCashFlow> ccf_2 = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(cf_2);

        // Don't support non-zero spreads or gearing != 1 so amount gives forward * quantity for commodity cashflow
        // referencing spot price or future * quantity for commodity cashflow referencing future price
        Real result = ccf_1->amount() / normFactor;
        result *= ccf_2->amount() / normFactor;

        // Multiply by discount factor to payment date
        result *= discountCurve_->discount(ccf_1->date());
        result *= discountCurve_->discount(ccf_2->date());

        // Multiply by exp{v} where the quantity v depends on whether commodity is spot or future
        Real v = 0.0;
        if (ccf_1->useFuturePrice()) {

            Date e_1 = ccf_1->index()->expiryDate();
            v = volStructure_->blackVol(e_1, strike);

            Date e_2 = ccf_2->index()->expiryDate();
            if (e_1 == e_2) {
                v *= v;
            } else {
                v *= volStructure_->blackVol(e_2, strike);
                v *= rho(e_1, e_2);
            }

            v *= t_e;

        } else {
            v = volStructure_->blackVariance(t_e, strike);
        }
        result *= exp(v);

        return result;
    }
}

Real CommoditySwaptionEngine::maxQuantity(Size floatLegIndex) const {

    Real result = 0.0;

    if (averaging(floatLegIndex)) {
        for (const auto& cf : arguments_.legs[floatLegIndex]) {
            auto ccf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(cf);
            QL_REQUIRE(ccf, "maxQuantity: expected a CommodityIndexedAverageCashFlow");
            result = max(result, ccf->periodQuantity());
        }
    } else {
        for (const auto& cf : arguments_.legs[floatLegIndex]) {
            auto ccf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(cf);
            QL_REQUIRE(ccf, "maxQuantity: expected a CommodityIndexedCashFlow");
            result = max(result, ccf->periodQuantity());
        }
    }

    QL_REQUIRE(result > 0.0, "maxQuantity: quantities should be greater than 0.0");

    return result;
}

void CommoditySwaptionMonteCarloEngine::calculate() const {

    // Get the index of the fixed and floating leg
    Size idxFixed = fixedLegIndex();
    Size idxFloat = idxFixed == 0 ? 1 : 0;

    // Determines the strike at which we read values from the vol surface.
    Real strikePrice = strike(idxFixed);

    // Switch implementation depending on whether underlying swap references a spot or future price
    if (referencesFuturePrice(arguments_.legs[idxFloat])) {
        calculateFuture(idxFixed, idxFloat, strikePrice);
    } else {
        calculateSpot(idxFixed, idxFloat, strikePrice);
    }
}

void CommoditySwaptionMonteCarloEngine::calculateSpot(Size idxFixed, Size idxFloat, Real strike) const {

    // Fixed leg value
    Real valueFixedLeg = fixedLegValue(idxFixed);

    // If float leg payer flag is 1 (-1) => rec (pay) float and pay (rec) fixed => omega = 1 (-1) for payer (receiver).
    Real omega = arguments_.payer[idxFloat];

    // Time to swaption expiry
    Date exercise = arguments_.exercise->dateAt(0);
    Time t_e = volStructure_->timeFromReference(exercise);

    // Discount to exercise
    Real discountExercise = discountCurve_->discount(exercise);

    // Variance of spot process to expiry
    Real var = volStructure_->blackVariance(t_e, strike);
    Real stdDev = sqrt(var);

    // Sample spot is S_i(t_e) = F(0, t_e) exp(-var / 2) exp(stdDev z_i). factor is second two term here.
    Real factor = exp(-var / 2.0);

    // Populate these values below
    Real optionValue = 0.0;
    Real swapValue = 0.0;
    Real floatLegValue = 0.0;

    // Create the standard normal RNG
    LowDiscrepancy::rsg_type rsg = LowDiscrepancy::make_sequence_generator(1, seed_);

    // Get the value that when multiplied by the sample value gives the float leg value
    Real floatFactor = spotFloatLegFactor(idxFloat, discountExercise);

    // Value swap at swaption exercise date over a number, i = 1, ..., N of samples i.e. \hat{V}_i(t_e) from ORE+
    // Product Catalogue. Value of the swaption at time 0 is then P(0, t_e) \sum_{i=1}^{N} \hat{V}^{+}_i(t_e) / N
    for (Size i = 0; i < samples_; i++) {

        // Sample value is S_i(t_e) / F(0, t_e) = exp(-var / 2) exp(stdDev z_i).
        Real sample = factor * exp(stdDev * rsg.nextSequence().value[0]);

        // Calculate float leg value and swap value given sample
        Real sampleFloatLegValue = floatFactor * sample;
        Real sampleSwapValue = omega * (sampleFloatLegValue - valueFixedLeg);

        // Update values dividing by samples each time to guard against blow up.
        optionValue += max(sampleSwapValue, 0.0) / samples_;
        swapValue += sampleSwapValue / samples_;
        floatLegValue += sampleFloatLegValue / samples_;
    }

    // Populate the results remembering to multiply by P(0, t_e)
    results_.value = discountExercise * optionValue;
    results_.additionalResults["SwapNPV"] = discountExercise * swapValue;
    results_.additionalResults["FixedLegNPV"] = discountExercise * valueFixedLeg;
    results_.additionalResults["FloatingLegNPV"] = discountExercise * floatLegValue;
}

void CommoditySwaptionMonteCarloEngine::calculateFuture(Size idxFixed, Size idxFloat, Real strike) const {

    // Fixed leg value
    Real valueFixedLeg = fixedLegValue(idxFixed);

    // Unique expiry dates
    Matrix sqrtCorr;
    map<Date, Real> expiries = futureExpiries(idxFloat, sqrtCorr, strike);

    // Generate n _independent_ standard normal variables {Z_{i,1}, ..., Z_{i,n}} where n is the number of future
    // contracts that we are modelling and i = 1, ..., N is the number of samples. It is a speed-up to set n = 1 if
    // correlation between all the future contracts is 1.0 i.e. beta_ = 0.0. We don't do this and prefer code clarity.
    LowDiscrepancy::rsg_type rsg = LowDiscrepancy::make_sequence_generator(expiries.size(), seed_);

    // If float leg payer flag is 1 (-1) => rec (pay) float and pay (rec) fixed => omega = 1 (-1) for payer (receiver).
    Real omega = arguments_.payer[idxFloat];

    // Time to swaption expiry
    Date exercise = arguments_.exercise->dateAt(0);
    Time t_e = volStructure_->timeFromReference(exercise);

    // Discount to exercise
    Real discountExercise = discountCurve_->discount(exercise);

    // Precalculate exp{-var_j / 2} and stdDev_j mentioned in comment above
    vector<Real> expVar;
    vector<Real> stdDev;
    vector<Date> expiryDates;
    for (const auto& p : expiries) {
        Real var = p.second * p.second * t_e;
        expVar.push_back(exp(-var / 2.0));
        stdDev.push_back(sqrt(var));
        expiryDates.push_back(p.first);
    }

    // Populate these values below
    Real optionValue = 0.0;
    Real swapValue = 0.0;
    Real floatLegValue = 0.0;

    // Values that will be used to calculate floating leg value on each iteration. We precalculate as much as
    // possible here to avoid calculation on each iteration.
    Size numCfs = arguments_.legs[idxFloat].size();
    Matrix factors(numCfs, expiries.size(), 0.0);
    Array discounts(numCfs, 0.0);
    Array amounts(numCfs, 0.0);
    futureFloatLegFactors(idxFloat, discountExercise, expiryDates, factors, discounts, amounts);
    Array factor = (discounts * amounts) * factors;

    // Value swap at swaption exercise date over a number, i = 1, ..., N of samples i.e. \hat{V}_i(t_e) from ORE+
    // Product Catalogue. Value of the swaption at time 0 is then P(0, t_e) \sum_{i=1}^{N} \hat{V}^{+}_i(t_e) / N
    for (Size i = 0; i < samples_; i++) {

        // sequence is n independent standard normal random variables
        vector<Real> sequence = rsg.nextSequence().value;

        // w holds n correlated standard normal variables
        Array w = sqrtCorr * Array(sequence.begin(), sequence.end());

        // Update w to hold the sample value that we want i.e.
        //   F_{i,j}(t_e) / F_{i,j}(0) = exp{-var_j / 2} exp{stdDev_j w_{i,j}}
        // where j = 1,..., n indexes the futures (i.e. date keys in the map)
        // and i = 1,..., N indexes the number of Monte Carlo samples.
        for (Size i = 0; i < w.size(); i++) {
            w[i] = exp(stdDev[i] * w[i]) * expVar[i];
        }

        // Calculate float leg value on this iteration
        Real sampleFloatLegValue = DotProduct(factor, w);

        // Calculate the swap leg value on this iteration
        Real sampleSwapValue = omega * (sampleFloatLegValue - valueFixedLeg);

        // Update values dividing by samples each time to guard against blow up.
        optionValue += max(sampleSwapValue, 0.0) / samples_;
        swapValue += sampleSwapValue / samples_;
        floatLegValue += sampleFloatLegValue / samples_;
    }

    // Populate the results remembering to multiply by P(0, t_e)
    results_.value = discountExercise * optionValue;
    results_.additionalResults["SwapNPV"] = discountExercise * swapValue;
    results_.additionalResults["FixedLegNPV"] = discountExercise * valueFixedLeg;
    results_.additionalResults["FloatingLegNPV"] = discountExercise * floatLegValue;
}

Real CommoditySwaptionMonteCarloEngine::spotFloatLegFactor(Size idxFloat, Real discountExercise) const {

    // Different float leg valuation depending on whether leg is averaging or not.
    Real floatLegValue = 0.0;
    if (averaging(idxFloat)) {
        for (const auto& cf : arguments_.legs[idxFloat]) {
            auto ccf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(cf);
            QL_REQUIRE(ccf, "spotSwapValue: expected a CommodityIndexedAverageCashFlow");
            Real disc = discountCurve_->discount(ccf->date());
            floatLegValue += disc * ccf->amount();
        }
    } else {
        for (const auto& cf : arguments_.legs[idxFloat]) {
            auto ccf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(cf);
            QL_REQUIRE(ccf, "spotSwapValue: expected a CommodityIndexedCashFlow");
            Real disc = discountCurve_->discount(ccf->date());
            floatLegValue += disc * ccf->amount();
        }
    }

    // Divide floating leg value by discount to swaption expiry
    floatLegValue /= discountExercise;

    // Return the swap value at the expiry date t_e
    return floatLegValue;
}

void CommoditySwaptionMonteCarloEngine::futureFloatLegFactors(Size idxFloat, Real discountExercise,
                                                              const vector<Date>& expiries, Matrix& floatLegFactors,
                                                              Array& discounts, Array& amounts) const {

    Size numCfs = arguments_.legs[idxFloat].size();

    QL_REQUIRE(numCfs == floatLegFactors.rows(),
               "Expected number of rows in floatLegFactors to equal number of cashflows");
    QL_REQUIRE(numCfs == discounts.size(), "Expected size of discounts to equal number of cashflows");
    QL_REQUIRE(numCfs == amounts.size(), "Expected size of amounts to equal number of cashflows");
    QL_REQUIRE(expiries.size() == floatLegFactors.columns(),
               "Expected number of columns in floatLegFactors to equal number of expiries");

    // Different float leg valuation depending on whether leg is averaging or not.
    if (averaging(idxFloat)) {
        for (Size i = 0; i < numCfs; i++) {
            auto ccf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(arguments_.legs[idxFloat][i]);
            QL_REQUIRE(ccf, "futureFloatLegFactors: expected a CommodityIndexedAverageCashFlow");
            Size numObs = ccf->indices().size();
            for (const auto& p : ccf->indices()) {
                Date expiry = p.second->expiryDate();
                auto it = find(expiries.begin(), expiries.end(), expiry);
                QL_REQUIRE(it != expiries.end(), "futureFloatLegFactors: expected to find expiry in expiries vector");
                auto idx = distance(expiries.begin(), it);
                Real fxRate{1.};
                if (ccf->fxIndex())
                    fxRate = ccf->fxIndex()->fixing(expiry);
                floatLegFactors[i][idx] += fxRate * p.second->fixing(expiry) / numObs;
            }
            discounts[i] = discountCurve_->discount(ccf->date());
            amounts[i] = ccf->periodQuantity();
        }
    } else {
        for (Size i = 0; i < numCfs; i++) {
            auto ccf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(arguments_.legs[idxFloat][i]);
            QL_REQUIRE(ccf, "futureFloatLegFactors: expected a CommodityIndexedCashFlow");

            auto it = find(expiries.begin(), expiries.end(), ccf->index()->expiryDate());
            QL_REQUIRE(it != expiries.end(), "futureFloatLegFactors: expected to find expiry in expiries vector");
            auto idx = distance(expiries.begin(), it);
            floatLegFactors[i][idx] = 1.0;
            discounts[i] = discountCurve_->discount(ccf->date());
            amounts[i] = ccf->amount();
        }
    }

    // Divide discounts by discount to swaption expiry
    for (Size i = 0; i < discounts.size(); i++) {
        discounts[i] /= discountExercise;
    }
}

map<Date, Real> CommoditySwaptionMonteCarloEngine::futureExpiries(Size idxFloat, Matrix& outSqrtCorr,
                                                                  Real strike) const {

    // Will hold the result
    map<Date, Real> result;

    // Populate the unique future expiry dates referenced on the floating leg
    if (averaging(idxFloat)) {
        for (const auto& cf : arguments_.legs[idxFloat]) {
            auto ccf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(cf);
            QL_REQUIRE(ccf, "futureExpiries: expected a CommodityIndexedAverageCashFlow");
            QL_REQUIRE(ccf->useFuturePrice(), "futureExpiries: expected the cashflow to reference a future price");
            for (const auto& p : ccf->indices()) {
                result[p.second->expiryDate()] = 0.0;
            }
        }
    } else {
        for (const auto& cf : arguments_.legs[idxFloat]) {
            auto ccf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(cf);
            QL_REQUIRE(ccf, "futureExpiries: expected a CommodityIndexedCashFlow");
            QL_REQUIRE(ccf->useFuturePrice(), "futureExpiries: expected the cashflow to reference a future price");
            result[ccf->index()->expiryDate()] = 0.0;
        }
    }

    // Populate the map values i.e. the instantaneous volatility associated with the future contract whose expiry date
    // is the map key. Here, we make the simplifying assumption that the volatility can be read from the volatility
    // term structure at the future contract's expiry date. In most cases, if the volatility term structure is built
    // from options on futures, the option contract expiry will be a number of days before the future contract expiry
    // and we should really read off the term structure at that date. Also populate a temp vector containing the key
    // dates for use in the loop below where we populate the sqrt correlation matrix.
    vector<Date> expiryDates;
    for (auto& p : result) {
        p.second = volStructure_->blackVol(p.first, strike);
        expiryDates.push_back(p.first);
    }

    // Populate the outSqrtCorr matrix
    outSqrtCorr = Matrix(expiryDates.size(), expiryDates.size(), 1.0);
    for (Size i = 0; i < expiryDates.size(); i++) {
        for (Size j = 0; j < i; j++) {
            outSqrtCorr[i][j] = outSqrtCorr[j][i] = rho(expiryDates[i], expiryDates[j]);
        }
    }
    outSqrtCorr = pseudoSqrt(outSqrtCorr, SalvagingAlgorithm::None);

    return result;
}

} // namespace QuantExt
