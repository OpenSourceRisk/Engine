/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/processes/ornsteinuhlenbeckprocess.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/pricingengines/commodityspreadoptionengine.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>

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
    const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volShort, Real rho, Real beta = 0.0)
    : discountCurve_(discountCurve), volTSLongAsset_(volLong), volTSShortAsset_(volShort), rho_(rho) {
    QL_REQUIRE(rho >= -1.0 && rho <= 1.0, "-1.0 <= rho <= 1.0 required, found " << rho_);
    registerWith(discountCurve_);
    registerWith(volTSLongAsset_);
    registerWith(volTSShortAsset_);
}

void CommoditySpreadOptionAnalyticalEngine::calculate() const {
    // Populate some additional results that don't change
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

    auto longAssetFlow = ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(arguments_.longAssetFlow);
    auto shortAssetFlow = ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(arguments_.shortAssetFlow);

    auto& mp = results_.additionalResults;
    // ObsDate is either future contract expiry or forward date
    Date obsDate1 = longAssetFlow->pricingDate();
    Date obsDate2 = shortAssetFlow->pricingDate();

    Time obsTime1 = discountCurve_->timeFromReference(obsDate1);
    Time obsTime2 = discountCurve_->timeFromReference(obsDate2);

    double fxFixing1 = arguments_.longAssetFxIndex ? arguments_.longAssetFxIndex->fixing(obsDate1) : 1.0;
    double fxFixing2 = arguments_.shortAssetFxIndex ? arguments_.shortAssetFxIndex->fixing(obsDate2) : 1.0;

    double F1 = longAssetFlow->index()->fixing(obsDate1) * fxFixing1;
    double F2 = shortAssetFlow->index()->fixing(obsDate2) * fxFixing2;

    double sigma1 = obsDate1 > today ? volTSLongAsset_->blackVol(obsDate1, F1) : 0.0;
    double sigma2 = obsDate2 > today ? volTSShortAsset_->blackVol(obsDate2, F2) : 0.0;
    // Calendar spread adjustment if observation period is before the exercise date
    sigma1 = sigma1 * std::min(1.0, std::sqrt(obsTime1 / tte));
    sigma2 = sigma2 * std::min(1.0, std::sqrt(obsTime2 / tte));

    // KirkFormula
    double Y = (F2 + arguments_.effectiveStrike);
    double Z = F1 / Y;
    double sigmaTilde = sigma2 * F2 / Y;

    double sigma = std::sqrt(std::pow(sigma1, 2.0) + std::pow(sigmaTilde, 2.0) - 2 * sigma1 * sigmaTilde * rho_);

    double stdDev = sigma * sqrt(tte);

    results_.value = arguments_.quantity * Y * blackFormula(arguments_.type, 1, Z, stdDev, df);
    mp["long_obsDate"] = obsDate1;
    mp["long_obsFuturePrice"] = longAssetFlow->useFuturePrice();
    mp["long_forward"] = F1;
    mp["long_vol"] = sigma1;
    mp["long_fxFixing"] = fxFixing1;

    mp["short_obsDate"] = obsDate2;
    mp["short_obsFuturePrice"] = shortAssetFlow->useFuturePrice();
    mp["short_forward"] = F2;
    mp["short_vol"] = sigma2;
    mp["short_fxFixing"] = fxFixing2;

    mp["Y"] = Y;
    mp["Z"] = Z;
    mp["sigma"] = sigma;
    mp["stdDev"] = stdDev;

    mp["npv"] = results_.value;
}

} // namespace QuantExt
