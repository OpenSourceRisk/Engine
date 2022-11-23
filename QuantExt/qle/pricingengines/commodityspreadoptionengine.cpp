/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <iostream>
#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/processes/ornsteinuhlenbeckprocess.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
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

CommoditySpreadOptionBaseEngine::CommoditySpreadOptionBaseEngine(
    const Handle<YieldTermStructure>& discountCurve, const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volLong,
    const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volShort, Real rho, Real beta)
    : discountCurve_(discountCurve), volTSLongAsset_(volLong), volTSShortAsset_(volShort), rho_(rho), beta_(beta) {
    QL_REQUIRE(rho >= -1.0 && rho <= 1.0, "-1.0 <= rho <= 1.0 required, found " << rho_);
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

    Time tte = discountCurve_->timeFromReference(exerciseDate);

    auto [obsTime1, F1, accrued1, sigma1] =
        timeAndAtmAndSigmaFromCashFlow(arguments_.longAssetFlow, *volTSLongAsset_, arguments_.longAssetFxIndex);

    auto [obsTime2, F2, accrued2, sigma2] =
        timeAndAtmAndSigmaFromCashFlow(arguments_.shortAssetFlow, *volTSShortAsset_, arguments_.shortAssetFxIndex);

    double sigma = 0;
    double stdDev = 0;
    double Y = 0;
    double Z = 0;
    double w1 = arguments_.longAssetFlow->gearing();
    double w2 = arguments_.shortAssetFlow->gearing();
    double effectiveStrike = arguments_.effectiveStrike - w1 * accrued1 + w2 * accrued2;

    if (exerciseDate <= today) {
        // if observation time is before expiry, continue the process with zero vol and zero drift from pricing date to
        // expiry
        double omega = arguments_.type == Option::Call ? 1 : -1;

        results_.value =
            df * arguments_.quantity * omega * std::max(w1 * F1 - w2 * F2 - arguments_.effectiveStrike, 0.0);

    } else if (effectiveStrike < 0 && !close_enough(effectiveStrike, 0.0)) {
        // Effective strike can be become negative if accrueds large enough
        if (arguments_.type == Option::Call) {
            results_.value = df * arguments_.quantity * std::max(w1 * F1 - w2 * F2 - effectiveStrike, 0.0);
        } else {
            results_.value = 0.0;
        }

    } else {
        sigma1 = sigma1 * std::min(1.0, std::sqrt(obsTime1 / tte));
        sigma2 = sigma2 * std::min(1.0, std::sqrt(obsTime2 / tte));

        // KirkFormula
        Y = (F2 * w2 + arguments_.effectiveStrike - w1 * accrued1 + w2 * accrued2);
        Z = w1 * F1 / Y;
        double sigmaTilde = sigma2 * F2 * w2 / Y;

        sigma = std::sqrt(std::pow(sigma1, 2.0) + std::pow(sigmaTilde, 2.0) - 2 * sigma1 * sigmaTilde * rho_);

        stdDev = sigma * sqrt(tte);

        results_.value = arguments_.quantity * Y * blackFormula(arguments_.type, 1, Z, stdDev, df);
    }

    // Calendar spread adjustment if observation period is before the exercise date
    mp["F1"] = F1;
    mp["accrued1"] = accrued1;
    mp["sigma1"] = sigma1;
    mp["obsTime1"] = obsTime1;
    mp["F2"] = F2;
    mp["accrued2"] = accrued2;
    mp["sigma2"] = sigma2;
    mp["obsTime2"] = obsTime2;
    mp["tte"] = tte;
    mp["sigma"] = sigma;
    mp["stdDev"] = stdDev;
    mp["Y"] = Y;
    mp["Z"] = Z;
    mp["npv"] = results_.value;
}

std::tuple<double, double, double, double>
CommoditySpreadOptionAnalyticalEngine::timeAndAtmAndSigmaFromCashFlow(const ext::shared_ptr<CommodityCashFlow>& flow,
                                                                      const ext::shared_ptr<BlackVolTermStructure>& vol,
                                                                      const ext::shared_ptr<FxIndex>& fxIndex) const {
    if (auto cf = ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(flow)) {
        auto tn = vol->timeFromReference(cf->pricingDate());
        double fxSpot = 1.0;
        if (fxIndex) {
            fxSpot = fxIndex->fixing(cf->pricingDate());
        }
        auto atm = cf->index()->fixing(cf->pricingDate()) * fxSpot;
        auto sigma = tn > 0 && !QuantLib::close_enough(tn, 0.0) ? vol->blackVol(tn, atm, true) : 0.0;
        return std::make_tuple(tn, atm, 0, sigma);
    } else if (auto avgCf = ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(flow)) {
        // accrued part
        double tn = 0.0;
        double accrueds = 0.0;
        double EA = 0;
        std::vector<Real> forwards;
        std::vector<Real> times;
        std::vector<Date> futureExpiries;
        std::map<Date, Real> futureVols;
        std::vector<double> spotVariances;
        double sigma = 0;
        size_t n = avgCf->indices().size();

        for (const auto& [pricingDate, index] : avgCf->indices()) {
            Date fixingDate = index->fixingCalendar().adjust(pricingDate, Preceding);
            if (pricingDate <= Settings::instance().evaluationDate()) {
                accrueds += index->fixing(fixingDate);
            } else {
                Real fxRate = (avgCf->fxIndex()) ? avgCf->fxIndex()->fixing(fixingDate) : 1.0;
                forwards.push_back(index->fixing(fixingDate) * fxRate);
                times.push_back(vol->timeFromReference(pricingDate));
                if (avgCf->useFuturePrice()) {
                    Date expiry = index->expiryDate();
                    futureExpiries.push_back(expiry);
                    if (futureVols.count(expiry) == 0) {
                        futureVols[expiry] = vol->blackVol(expiry, forwards.back());
                    }
                } else {
                    spotVariances.push_back(vol->blackVariance(times.back(), forwards.back()));
                }
                EA += forwards.back();
            }
        }
        accrueds /= static_cast<double>(n);
        EA /= static_cast<double>(n);

        double EA2 = 0.0;

        if (avgCf->useFuturePrice()) {
            // References future prices
            for (Size i = 0; i < forwards.size(); ++i) {
                Date e_i = futureExpiries[i];
                Volatility v_i = futureVols.at(e_i);
                EA2 += forwards[i] * forwards[i] * exp(v_i * v_i * times[i]);
                for (Size j = 0; j < i; ++j) {
                    Date e_j = futureExpiries[j];
                    Volatility v_j = futureVols.at(e_j);
                    EA2 += 2 * forwards[i] * forwards[j] * exp(rho(e_i, e_j, vol) * v_i * v_j * times[j]);
                }
            }
        } else {
            // References spot prices
            for (Size i = 0; i < forwards.size(); ++i) {
                EA2 += forwards[i] * forwards[i] * exp(spotVariances[i]);
                for (Size j = 0; j < i; ++j) {
                    EA2 += 2 * forwards[i] * forwards[j] * exp(spotVariances[j]);
                }
            }
        }
        EA2 /= std::pow(static_cast<double>(n), 2.0);

        // Calculate value
        if (!times.empty()) {
            tn = times.back();
            sigma = sqrt(log(EA2 / (EA * EA)) / tn);
        } else {
            tn = 0;
            sigma = 0;
        }
        return std::make_tuple(tn, EA, accrueds, sigma);
    } else {
        QL_FAIL("SpreadOptionEngine supports only CommodityIndexedCashFlow or CommodityIndexedAverageCashFlow");
    }
}
Real CommoditySpreadOptionAnalyticalEngine::rho(const Date& ed_1, const Date& ed_2,
                                                const ext::shared_ptr<BlackVolTermStructure>& vol) const {
    if (beta_ == 0.0 || ed_1 == ed_2) {
        return 1.0;
    } else {
        Time t_1 = vol->timeFromReference(ed_1);
        Time t_2 = vol->timeFromReference(ed_2);
        return exp(-beta_ * fabs(t_2 - t_1));
    }
}

} // namespace QuantExt
