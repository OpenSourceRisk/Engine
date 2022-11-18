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
    const QuantLib::Handle<QuantLib::BlackVolTermStructure>& volShort, Real rho)
    : discountCurve_(discountCurve), volLong_(volLong), volShort_(volShort), rho_(rho) {
    QL_REQUIRE(rho >= -1.0 && rho <= 1.0, "-1.0 <= rho <= 1.0 required, found " << rho_);
    registerWith(discountCurve_);
    registerWith(volLong_);
    registerWith(volShort_);
}

void CommoditySpreadOptionAnalyticalEngine::calculate() const {
    // Populate some additional results that don't change
    QL_REQUIRE(arguments_.exercise->type() == QuantLib::Exercise::European, "Only European Spread Option supported");

    Date today = Settings::instance().evaluationDate();

    Date exerciseDate = arguments_.exercise->lastDate();

    Date paymentDate = std::max(arguments_.longFlow->date(), arguments_.shortFlow->date());

    QL_REQUIRE(paymentDate >= exerciseDate, "Payment date needs to be on or after exercise date");

    double df = discountCurve_->discount(paymentDate);

    Time tte = discountCurve_->timeFromReference(exerciseDate);

    auto& mp = results_.additionalResults;
    // ObsDate is either future contract expiry or forward date
    Date obsDate1 = arguments_.longFlow->pricingDate();
    Date obsDate2 = arguments_.shortFlow->pricingDate();

    Time obsTime1 = discountCurve_->timeFromReference(obsDate1);
    Time obsTime2 = discountCurve_->timeFromReference(obsDate2);

    // kirk formula
    double fxFixing1 = arguments_.fxIndex ? arguments_.fxIndex->fixing(obsDate1) : 1.0;
    double fxFixing2 = arguments_.fxIndex ? arguments_.fxIndex->fixing(obsDate2) : 1.0;

    double F1 = arguments_.longFlow->index()->fixing(obsDate1) * fxFixing1;
    double F2 = arguments_.shortFlow->index()->fixing(obsDate2) * fxFixing2;
    // if obsDate is in the past freeze vol at 0
    double sigma1 = obsDate1 > today ? volLong_->blackVol(obsDate1, F1) : 0.0;
    double sigma2 = obsDate2 > today ? volShort_->blackVol(obsDate2, F2) : 0.0;
    // Time adjustment if its a calendar spread
    sigma1 = sigma1 * std::min(1.0, std::sqrt(obsTime1 / tte));
    sigma2 = sigma2 * std::min(1.0, std::sqrt(obsTime2 / tte));

    
    double Y = (F2 + arguments_.strikePrice);
    double Z = F1 / Y;
    double sigmaTilde = sigma2 * F2 / Y;

    double sigma = std::sqrt(std::pow(sigma1, 2.0) + std::pow(sigmaTilde, 2.0) - 2 * sigma1 * sigmaTilde * rho_);

    double stdDev = sigma * sqrt(tte);

    results_.value = arguments_.quantity * Y * blackFormula(arguments_.type, 1, Z, stdDev, df);
    mp["obsDate1"] = obsDate1;
    mp["s_0_obsFuturePrice"] = arguments_.longFlow->useFuturePrice();
    mp["obsDate2"] = obsDate2;
    mp["s_0_obsFuturePrice"] = arguments_.shortFlow->useFuturePrice();
    mp["npv"] = results_.value;
}

} // namespace QuantExt
