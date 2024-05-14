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

#include <ored/portfolio/builders/durationadjustedcms.hpp>
#include <ored/portfolio/durationadjustedcmslegbuilder.hpp>
#include <ored/portfolio/durationadjustedcmslegdata.hpp>
#include <ored/utilities/indexnametranslator.hpp>

#include <qle/cashflows/durationadjustedcmscoupon.hpp>

#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/legdata.hpp>

#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>

using namespace QuantExt;
using namespace QuantLib;

namespace ore {
namespace data {

Leg DurationAdjustedCmsLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                            RequiredFixings& requiredFixings, const string& configuration,
                                            const QuantLib::Date& openEndDateReplacement,
                                            const bool useXbsCurves) const {

    auto cmsData = QuantLib::ext::dynamic_pointer_cast<DurationAdjustedCmsLegData>(data.concreteLegData());
    QL_REQUIRE(cmsData, "Wrong LegType, expected CMS");

    string indexName = cmsData->swapIndex();
    auto index = *engineFactory->market()->swapIndex(indexName, configuration);
    Schedule schedule = makeSchedule(data.schedule(), openEndDateReplacement);
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(data.paymentConvention());

    vector<double> spreads =
        ore::data::buildScheduledVectorNormalised(cmsData->spreads(), cmsData->spreadDates(), schedule, 0.0);
    vector<double> gearings =
        ore::data::buildScheduledVectorNormalised(cmsData->gearings(), cmsData->gearingDates(), schedule, 1.0);
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);
    Size fixingDays = cmsData->fixingDays() == Null<Size>() ? index->fixingDays() : cmsData->fixingDays();

    applyAmortization(notionals, data, schedule, false);

    PaymentLag paymentLag = parsePaymentLag(data.paymentLag());
    DurationAdjustedCmsLeg leg = DurationAdjustedCmsLeg(schedule, index, cmsData->duration())
                                     .withNotionals(notionals)
                                     .withSpreads(spreads)
                                     .withGearings(gearings)
                                     .withPaymentDayCounter(dc)
                                     .withPaymentAdjustment(bdc)
                                     .withPaymentLag(boost::apply_visitor(PaymentLagInteger(), paymentLag))
                                     .withFixingDays(fixingDays)
                                     .inArrears(cmsData->isInArrears());

    if (!data.paymentCalendar().empty())
        leg.withPaymentCalendar(parseCalendar(data.paymentCalendar()));

    if (cmsData->caps().size() > 0)
        leg.withCaps(buildScheduledVector(cmsData->caps(), cmsData->capDates(), schedule));

    if (cmsData->floors().size() > 0)
        leg.withFloors(buildScheduledVector(cmsData->floors(), cmsData->floorDates(), schedule));

    // Get a coupon pricer for the leg
    auto builder = QuantLib::ext::dynamic_pointer_cast<DurationAdjustedCmsCouponPricerBuilder>(
        engineFactory->builder("DurationAdjustedCMS"));
    QL_REQUIRE(builder != nullptr, "No builder found for DurationAdjustedCmsLeg");
    auto couponPricer = builder->engine(IndexNameTranslator::instance().oreName(index->iborIndex()->name()));

    // Loop over the coupons in the leg and set pricer
    Leg result = leg;
    for (auto& c : result) {
        auto f = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(c);
        QL_REQUIRE(f != nullptr,
                   "DurationAdjustedCmsLegBuilder::buildLeg(): internal error, expected FloatingRateCoupon");
        f->setPricer(couponPricer);
    }

    // build naked option leg if required
    if (cmsData->nakedOption()) {
        result = StrippedCappedFlooredCouponLeg(result);
    }

    applyIndexing(result, data, engineFactory, requiredFixings, openEndDateReplacement, useXbsCurves);
    addToRequiredFixings(result, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));
    return result;
}
} // namespace data
} // namespace ore
