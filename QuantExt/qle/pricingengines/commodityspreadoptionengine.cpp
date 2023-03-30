/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <iostream>
#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/processes/ornsteinuhlenbeckprocess.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/pricingengines/commodityapoengine.hpp>
#include <qle/pricingengines/commodityspreadoptionengine.hpp>

using std::adjacent_difference;
using std::exp;
using std::make_pair;
using std::map;
using std::max;
using std::pair;
using std::set;
using std::vector;

namespace QuantExt {

CommoditySpreadOptionAnalyticalEngine::CommoditySpreadOptionAnalyticalEngine(
    const Handle<YieldTermStructure>& discountCurve, const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volLong,
    const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volShort,
    const QuantLib::Handle<QuantExt::CorrelationTermStructure>& rho, Real beta)
    : discountCurve_(discountCurve), volTSLongAsset_(volLong), volTSShortAsset_(volShort), rho_(rho), beta_(beta) {
    QL_REQUIRE(beta_ >= 0.0, "beta >= 0 required, found " << beta_);
    registerWith(discountCurve_);
    registerWith(volTSLongAsset_);
    registerWith(volTSShortAsset_);
}

void CommoditySpreadOptionAnalyticalEngine::calculate() const {
    // Populate some additional results that don't change
    auto& mp = results_.additionalResults;
    QL_REQUIRE(arguments_.exercise->type() == QuantLib::Exercise::European, "Only European Spread Option supported");
    QL_REQUIRE(arguments_.longAssetFlow && arguments_.shortAssetFlow, "flows can not be null");

    Date today = Settings::instance().evaluationDate();

    Date exerciseDate = arguments_.exercise->lastDate();

    Date paymentDate = arguments_.paymentDate;
    if (paymentDate == Date())
        paymentDate = std::max(arguments_.longAssetFlow->date(), arguments_.shortAssetFlow->date());

    QL_REQUIRE(paymentDate >= exerciseDate, "Payment date needs to be on or after exercise date");

    double df = discountCurve_->discount(paymentDate);

    Time ttp = discountCurve_->timeFromReference(paymentDate);
    Time tte = discountCurve_->timeFromReference(exerciseDate);

    auto parameterFlow1 =
        derivePricingParameterFromFlow(arguments_.longAssetFlow, *volTSLongAsset_, arguments_.longAssetFxIndex);

    auto parameterFlow2 =
        derivePricingParameterFromFlow(arguments_.shortAssetFlow, *volTSShortAsset_, arguments_.shortAssetFxIndex);

    double F1 = parameterFlow1.atm;
    double F2 = parameterFlow2.atm;
    double sigma1 = parameterFlow1.sigma;
    double sigma2 = parameterFlow2.sigma;
    double obsTime1 = parameterFlow1.tn;
    double obsTime2 = parameterFlow2.tn;
    double accruals1 = parameterFlow1.accruals;
    double accruals2 = parameterFlow2.accruals;

    double sigma = 0;
    double stdDev = 0;
    double Y = 0;
    double Z = 0;
    double sigmaY = 0;
    double w1 = arguments_.longAssetFlow->gearing();
    double w2 = arguments_.shortAssetFlow->gearing();
    // Adjust strike for past fixings
    double effectiveStrike = arguments_.effectiveStrike - w1 * accruals1 + w2 * accruals2;
    Real correlation = QuantLib::Null<Real>();

    if (exerciseDate <= today && paymentDate <= today) {
        results_.value = 0;
    } else if (exerciseDate <= today && paymentDate > today) {
        // if observation time is before expiry, continue the process with zero vol and zero drift from pricing date to
        // expiry
        double omega = arguments_.type == Option::Call ? 1 : -1;

        results_.value = df * arguments_.quantity * omega * std::max(w1 * F1 - w2 * F2 - effectiveStrike, 0.0);

    } else if (effectiveStrike + F2 * w2 < 0) {
        // Effective strike can be become negative if accrueds large enough
        if (arguments_.type == Option::Call) {
            results_.value = df * arguments_.quantity * std::max(w1 * F1 - w2 * F2 - effectiveStrike, 0.0);
        } else {
            results_.value = 0.0;
        }

    } else {
        sigma1 = sigma1 * std::min(1.0, std::sqrt(obsTime1 / tte));
        sigma2 = sigma2 * std::min(1.0, std::sqrt(obsTime2 / tte));
        correlation = rho();
        // KirkFormula
        Y = (F2 * w2 + effectiveStrike);
        Z = w1 * F1 / Y;
        sigmaY = sigma2 * F2 * w2 / Y;

        sigma = std::sqrt(std::pow(sigma1, 2.0) + std::pow(sigmaY, 2.0) - 2 * sigma1 * sigmaY * correlation);

        stdDev = sigma * sqrt(tte);

        results_.value = arguments_.quantity * Y * blackFormula(arguments_.type, 1, Z, stdDev, df);
    }

    // Calendar spread adjustment if observation period is before the exercise date
    mp["F1"] = F1;
    mp["accruals1"] = accruals1;
    mp["sigma1"] = sigma1;
    mp["obsTime1"] = obsTime1;
    mp["F2"] = F2;
    mp["accruals2"] = accruals2;
    mp["sigma2"] = sigma2;
    mp["obsTime2"] = obsTime2;
    mp["tte"] = tte;
    mp["ttp"] = ttp;
    mp["df"] = df;
    mp["sigma"] = sigma;
    mp["stdDev"] = stdDev;
    mp["Y"] = Y;
    mp["Z"] = Z;
    mp["sigma_Y"] = sigmaY;
    mp["quantity"] = arguments_.quantity;
    mp["npv"] = results_.value;
    mp["exerciseDate"] = exerciseDate;
    mp["paymentDate"] = paymentDate;
    mp["w1"] = w1;
    mp["w2"] = w2;
    mp["rho"] = correlation;
}

CommoditySpreadOptionAnalyticalEngine::PricingParameter
CommoditySpreadOptionAnalyticalEngine::derivePricingParameterFromFlow(const ext::shared_ptr<CommodityCashFlow>& flow,
                                                                      const ext::shared_ptr<BlackVolTermStructure>& vol,
                                                                      const ext::shared_ptr<FxIndex>& fxIndex) const {
    PricingParameter res;
    if (auto cf = ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(flow)) {
        res.accruals = 0.0;
        res.tn = vol->timeFromReference(cf->pricingDate());
        double fxSpot = 1.0;
        if (fxIndex) {
            fxSpot = fxIndex->fixing(cf->pricingDate());
        }
        double atmUnderlyingCurrency = cf->index()->fixing(cf->pricingDate());
        res.atm = atmUnderlyingCurrency * fxSpot;
        res.sigma = res.tn > 0 && !QuantLib::close_enough(res.tn, 0.0)
                        ? vol->blackVol(res.tn, atmUnderlyingCurrency, true)
                        : 0.0;
    } else if (auto avgCf = ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(flow)) {
        auto parameter = CommodityAveragePriceOptionMomementMatching::matchFirstTwoMomentsTurnbullWakeman(
            avgCf, vol,
            std::bind(&CommoditySpreadOptionAnalyticalEngine::intraAssetCorrelation, this, std::placeholders::_1,
                      std::placeholders::_2, vol));
        res.tn = parameter.tn;
        res.atm = parameter.forward;
        res.accruals = parameter.accruals;
        res.sigma = parameter.sigma;
    } else {
        QL_FAIL("SpreadOptionEngine supports only CommodityIndexedCashFlow or CommodityIndexedAverageCashFlow");
    }
    return res;
}

Real CommoditySpreadOptionAnalyticalEngine::intraAssetCorrelation(
    const Date& ed_1, const Date& ed_2, const ext::shared_ptr<BlackVolTermStructure>& vol) const {
    if (beta_ == 0.0 || ed_1 == ed_2) {
        return 1.0;
    } else {
        Time t_1 = vol->timeFromReference(ed_1);
        Time t_2 = vol->timeFromReference(ed_2);
        return exp(-beta_ * fabs(t_2 - t_1));
    }
}

Real CommoditySpreadOptionAnalyticalEngine::rho() const {
    if (arguments_.longAssetFlow->index()->underlyingName() != arguments_.shortAssetFlow->index()->underlyingName()) {
        return rho_->correlation(arguments_.exercise->lastDate());
    } else {
        return intraAssetCorrelation(arguments_.shortAssetLastPricingDate, arguments_.longAssetLastPricingDate,
                                      *volTSLongAsset_);
    }
}

} // namespace QuantExt
