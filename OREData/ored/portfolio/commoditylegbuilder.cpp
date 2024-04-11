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

#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/conventionsbasedfutureexpiry.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/portfolio/builders/commodityswap.hpp>
#include <ored/portfolio/commoditylegbuilder.hpp>
#include <ored/portfolio/commoditylegdata.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/one.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/utilities/time.hpp>

using namespace ore::data;
using namespace QuantExt;
using namespace QuantLib;

using std::make_pair;
using std::string;

namespace {

// Utility method used below to update the non-averaging leg quantities if necessary.
// Note that the non-averaging leg may be representing an averaging leg by referring to a commodity price curve
// that gives the prices of averaging futures.
void updateQuantities(Leg& leg, bool isAveragingFuture, CommodityQuantityFrequency cqf, const Schedule& schedule,
                      bool excludePeriodStart, bool includePeriodEnd,
                      const QuantLib::ext::shared_ptr<CommodityFutureConvention>& conv,
                      const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& calc, Natural hoursPerDay, bool useBusinessDays,
                      const std::string& daylightSavingLocation, const string& commName, bool unrealisedQuantity,
                      const boost::optional<pair<Calendar, Real>>& offPeakPowerData) {

    QL_REQUIRE(leg.size() == schedule.size() - 1, "The number of schedule periods (" << (schedule.size() - 1) <<
        ") was expected to equal the number of leg cashflows (" << leg.size() << ") when updating quantities" <<
        " for commodity " << commName << ".");

    using CQF = CommodityQuantityFrequency;
    if (cqf == CQF::PerCalculationPeriod) {

        if (unrealisedQuantity) {
            if (!isAveragingFuture) {
                DLOG("The future " << commName << " is not averaging, unrealisedQuantity does not make sense" <<
                    " so the PerCalculationPeriod quantities have not been altered.");
            } else if (conv->contractFrequency() == Daily) {
                DLOG("The future " << commName << " is averaging but has a daily frequency " <<
                    " so the PerCalculationPeriod quantities have not been altered.");
            } else {
                // If unrealisedQuantity is true, find a cashflow where today in [start, end) and update.
                QL_REQUIRE(calc, "Updating commodity quantities due to unrealisedQuantity = true, expected a" <<
                    " valid future expiry calculator, commodity is " << commName << ".");
                Size numberCashflows = leg.size();
                for (Size i = 0; i < numberCashflows; ++i) {

                    auto ccf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(leg[i]);
                    QL_REQUIRE(ccf, io::ordinal(i + 1) << " quantity for commodity " << commName <<
                        ", expected a valid CommodityIndexedCashFlow.");
                    Date pricingDate = ccf->pricingDate();
                    Date aveEnd = calc->priorExpiry(true, pricingDate);
                    Date aveStart = calc->priorExpiry(false, aveEnd);
                    Date today = Settings::instance().evaluationDate();

                    if (aveStart <= today && today < aveEnd) {

                        QL_REQUIRE(conv, "Need a valid convention for " << commName <<
                            " while updating quantities due to unrealisedQuantity = true.");
                        auto pds = pricingDates(aveStart, aveEnd, conv->calendar(), true, true, useBusinessDays);

                        DLOG("UnrealisedQuantity is true so updating the quantity:");
                        DLOG("today: " << io::iso_date(today));
                        DLOG("pricing date: " << io::iso_date(pricingDate));
                        DLOG("period start: " << io::iso_date(aveStart));
                        DLOG("period end: " << io::iso_date(aveEnd));

                        Real unrealisedFraction = 0.0;
                        if (offPeakPowerData) {

                            Calendar peakCalendar = offPeakPowerData->first;
                            Real offPeakHours = offPeakPowerData->second;
                            Real total = 0;
                            Real unrealised = 0;
                            for (const auto& pd : pds) {
                                Real numHours = peakCalendar.isHoliday(pd) ? 24.0 : offPeakHours;
                                total += numHours;
                                if (pd > today)
                                    unrealised += numHours;
                            }
                            DLOG("total hours: " << total);
                            DLOG("unrealised hours: " << unrealised);
                            unrealisedFraction = unrealised / total;

                        } else {

                            auto unrealised = count_if(pds.begin(), pds.end(),
                                [&today](const Date& pd) { return pd > today; });
                            DLOG("total pricing dates: " << pds.size());
                            DLOG("unrealised pricing dates: " << unrealised);
                            unrealisedFraction = static_cast<Real>(unrealised) / pds.size();

                        }

                        if (unrealisedFraction > 0) {
                            Real oldQuantity = ccf->quantity();
                            Real newQuantity = oldQuantity / unrealisedFraction;
                            DLOG("old quantity: " << oldQuantity);
                            DLOG("new quantity: " << newQuantity);
                            ccf->setPeriodQuantity(newQuantity);
                        } else {
                            DLOG("UnrealisedQuantity is true but cannot update the quantity because" <<
                                " value of unrealised is 0.");
                        }

                        // There will only be one cashflow satisfying the condition
                        break;
                    }
                }
            }
        } else {
            DLOG("updateQuantities: quantity is PerCalculationPeriod and unrealisedQuantity is" <<
                " false so nothing to update.");
        }
        
    } else if (cqf == CQF::PerCalendarDay) {

        QL_REQUIRE(conv, "Need a valid convention for " << commName <<
            " while updating quantities (PerCalendarDay).");

        DLOG("updateQuantities: updating quantities based on PerCalendarDay.");

        Size numberCashflows = leg.size();
        for (Size i = 0; i < numberCashflows; ++i) {
            Date start = schedule.date(i);
            Date end = schedule.date(i + 1);
            bool excludeStart = i == 0 ? false : excludePeriodStart;
            bool includeEnd = i == numberCashflows - 1 ? true : includePeriodEnd;
            auto ccf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(leg[i]);
            QL_REQUIRE(ccf, "Updating " << io::ordinal(i + 1) << " quantity for commodity " << commName <<
                ", expected a valid CommodityIndexedCashFlow.");
            Real newQuantity = ccf->quantity() * ((end - start - 1.0) +
                (!excludeStart ? 1.0 : 0.0) + (includeEnd ? 1.0 : 0.0));
            ccf->setPeriodQuantity(newQuantity);
            DLOG("updateQuantities: updating quantity for pricing date " << ccf->pricingDate() << " from " <<
                ccf->quantity() << " to " << newQuantity);
        }

    } else {

        // Now, cqf is either PerHour, PerHourAndCalendarDay or PerPricingDay
        QL_REQUIRE(cqf == CQF::PerPricingDay || cqf == CQF::PerHour || cqf == CQF::PerHourAndCalendarDay,
                   "Did not cover commodity"
                       << " quantity frequency type " << cqf << " while updating quantities for " << commName << ".");

        // Store the original quantities and CommodityIndexedCashFlow pointers
        Size numberCashflows = leg.size();
        vector<Real> quantities(numberCashflows, 0.0);
        vector<QuantLib::ext::shared_ptr<CommodityIndexedCashFlow>> ccfs(numberCashflows);
        for (Size i = 0; i < numberCashflows; ++i) {
            auto ccf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(leg[i]);
            QL_REQUIRE(ccf, "Updating " << io::ordinal(i + 1) << " quantity for commodity " << commName <<
                ", expected a valid CommodityIndexedCashFlow.");
            ccfs[i] = ccf;
            quantities[i] = ccf->quantity();
        }

        if (!isAveragingFuture) {

            if (cqf == CQF::PerPricingDay) {
                DLOG("The future " << commName << " is not averaging so a commodity quantity frequency equal to" <<
                    " PerPricingDay does not make sense. Quantities have not been altered. Commodity is " <<
                    commName << ".");
            } else {
                DLOG("updateQuantities: the future " << commName << " is not averaging and quantity frequency is" <<
                    " PerHour so updating quantities with daily quantities.");
                if (offPeakPowerData) {
                    QL_REQUIRE(cqf == CQF::PerHour, "PerHourAndCalendarDay not allowed for "
                               "off-peak power contracts, expected PerHour");
                    Calendar peakCalendar = offPeakPowerData->first;
                    Real offPeakHours = offPeakPowerData->second;
                    for (Size i = 0; i < numberCashflows; ++i) {
                        Real numHours = peakCalendar.isHoliday(ccfs[i]->pricingDate()) ? 24.0 : offPeakHours;
                        ccfs[i]->setPeriodQuantity(quantities[i] * numHours);
                        DLOG("updateQuantities: updating quantity for pricing date " << ccfs[i]->pricingDate() <<
                            " from " << quantities[i] << " to " << (quantities[i] * numHours));
                    }
                } else {
                    QL_REQUIRE(hoursPerDay != Null<Natural>(),
                               "Need HoursPerDay when commodity quantity frequency"
                                   << " is PerHour or PerHourAndCalendarDay. Updating quantities failed, commodity is "
                                   << commName << ".");
                    for (Size i = 0; i < numberCashflows; ++i) {
                        Real newQuantity = 0.0;
                        if (cqf == CQF::PerHour) {
                            newQuantity = quantities[i] * hoursPerDay;
                        } else if (cqf == CQF::PerHourAndCalendarDay) {
                            Date start = schedule.date(i);
                            Date end = schedule.date(i + 1);
                            bool excludeStart = i == 0 ? false : excludePeriodStart;
                            bool includeEnd = i == numberCashflows - 1 ? true : includePeriodEnd;
                            int numberOfDays = ((end - start - 1) + (!excludeStart ? 1 : 0) + (includeEnd ? 1 : 0));
                            newQuantity =
                                quantities[i] * static_cast<Real>(hoursPerDay) * static_cast<Real>(numberOfDays) +
                                static_cast<Real>(daylightSavingCorrection(daylightSavingLocation, start, end));
                        }
                        ccfs[i]->setPeriodQuantity(newQuantity);
                        DLOG("updateQuantities: updating quantity for pricing date "
                             << ccfs[i]->pricingDate() << " from " << quantities[i] << " to "
                             << (quantities[i] * hoursPerDay));
                    }
                }
            }

        } else {

            QL_REQUIRE(conv, "Need a valid convention for " << commName <<
                " while updating quantities for averaging future (PerPricingDay/PerHour).");

            // Averaging future and cqf is either PerHour or PerPricingDay.
            // Need to calculate associated averaging period and the number of pricing dates in the period.
            if (conv->contractFrequency() == Daily) {
                DLOG("The future " << commName << " is averaging but has a daily frequency " <<
                    " so the quantities have not been altered.");
            } else {

                // Frequency must be monthly or greater. We loop over each period in the schedule, imply 
                // the associated averaging period using the future expiry calculator and determine the per 
                // calculation period quantities in each calculation period. We assume that the averaging period 
                // goes from expiry to expiry.
                QL_REQUIRE(calc, "Updating commodity quantities expected a valid future expiry calculator" <<
                    ", commodity is " << commName << ".");
                DLOG("The future " << commName << " is averaging and does not have a daily frequency.");
                for (Size i = 0; i < numberCashflows; ++i) {
                    Date pricingDate = ccfs[i]->pricingDate();
                    Date aveEnd = calc->priorExpiry(true, pricingDate);
                    Date aveStart = calc->priorExpiry(false, aveEnd);
                    auto pds = pricingDates(aveStart, aveEnd, conv->calendar(), true, true, useBusinessDays);
                    if (cqf == CQF::PerHour || cqf == CQF::PerHourAndCalendarDay) {
                        if (offPeakPowerData) {
                            QL_REQUIRE(cqf == CQF::PerHour, "PerHourAndCalendarDay not allowed for "
                                                            "off-peak power contracts, expected PerHour");
                            Calendar peakCalendar = offPeakPowerData->first;
                            Real offPeakHours = offPeakPowerData->second;
                            Real newQuantity = 0.0;
                            for (const auto& pd : pds) {
                                newQuantity += quantities[i] * (peakCalendar.isHoliday(pd) ? 24.0 : offPeakHours);
                            }
                            ccfs[i]->setPeriodQuantity(newQuantity);
                            DLOG("updateQuantities: updating quantity for pricing date " << ccfs[i]->pricingDate() <<
                                " from " << quantities[i] << " to " << newQuantity);
                        } else {
                            QL_REQUIRE(hoursPerDay != Null<Natural>(),
                                       "Need HoursPerDay when commodity"
                                           << " quantity frequency is PerHour or PerHourAndCalendarDay. Commodity is "
                                           << commName << ".");
                            Real newQuantity = 0.0;
                            if(cqf == CQF::PerHour) {
                                newQuantity = quantities[i] * hoursPerDay * pds.size();
                            } else if (cqf == CQF::PerHourAndCalendarDay) {
                                Date start = schedule.date(i);
                                Date end = schedule.date(i + 1);
                                bool excludeStart = i == 0 ? false : excludePeriodStart;
                                bool includeEnd = i == numberCashflows - 1 ? true : includePeriodEnd;
                                int numberOfDays =
                                    ((end - start - 1) + (!excludeStart ? 1 : 0) + (includeEnd ? 1 : 0));
                                newQuantity =
                                    quantities[i] *
                                    (static_cast<Real>(hoursPerDay) * static_cast<Real>(numberOfDays) +
                                     static_cast<Real>(daylightSavingCorrection(daylightSavingLocation, start, end)));
                            }
                            ccfs[i]->setPeriodQuantity(newQuantity);
                            DLOG("updateQuantities: updating quantity for pricing date " << ccfs[i]->pricingDate() <<
                                " from " << quantities[i] << " to " << newQuantity);
                        }
                    } else {
                        ccfs[i]->setPeriodQuantity(quantities[i] * pds.size());
                        DLOG("updateQuantities: updating quantity for pricing date " << ccfs[i]->pricingDate() <<
                            " from " << quantities[i] << " to " << (quantities[i] * pds.size()));
                    }
                }

            }
        }
    }
}
}

namespace ore {
namespace data {

Leg CommodityFixedLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                       RequiredFixings& requiredFixings, const string& configuration,
                                       const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {

    // Check that our leg data has commodity fixed leg data
    auto fixedLegData = QuantLib::ext::dynamic_pointer_cast<CommodityFixedLegData>(data.concreteLegData());
    QL_REQUIRE(fixedLegData, "Wrong LegType, expected CommodityFixed, got " << data.legType());

    // Build our schedule and get the quantities and prices
    Schedule schedule = makeSchedule(data.schedule(),openEndDateReplacement);
    vector<Real> prices = buildScheduledVector(fixedLegData->prices(), fixedLegData->priceDates(), schedule);
    vector<Real> quantities = buildScheduledVector(fixedLegData->quantities(), fixedLegData->quantityDates(), schedule);

    // Build fixed rate leg with 1/1 day counter, prices as rates and quantities as notionals so that we have price x
    // notional in each period as the amount. We don't make any payment date adjustments yet as they come later.
    OneDayCounter dc;
    Leg fixedRateLeg = FixedRateLeg(schedule)
                           .withNotionals(quantities)
                           .withCouponRates(prices, dc)
                           .withPaymentAdjustment(Unadjusted)
                           .withPaymentLag(0)
                           .withPaymentCalendar(NullCalendar());

    // Get explicit payment dates which in most cases should be empty
    vector<Date> paymentDates;
    if (!data.paymentDates().empty()) {
        paymentDates = parseVectorOfValues<Date>(data.paymentDates(), &parseDate);
        if (fixedLegData->commodityPayRelativeTo() == CommodityPayRelativeTo::FutureExpiryDate) {
            QL_REQUIRE(paymentDates.size() == fixedRateLeg.size(),
                       "Expected the number of payment dates derived from float leg with tag '"
                           << fixedLegData->tag() << "' (" << paymentDates.size()
                           << ") to equal the number of fixed price periods (" << fixedRateLeg.size()
                           << "). Are the leg schedules consistent? Should CommodityPayRelativeTo = FutureExpiryDate "
                              "be used?");
        } else {
            QL_REQUIRE(paymentDates.size() == fixedRateLeg.size(),
                       "Expected the number of explicit payment dates ("
                           << paymentDates.size() << ") to equal the number of fixed price periods ("
                           << fixedRateLeg.size() << ")");
        }
    }

    // Create the commodity fixed leg.
    Leg commodityFixedLeg;
    for (Size i = 0; i < fixedRateLeg.size(); i++) {

        auto cp = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(fixedRateLeg[i]);

        // Get payment date
        Date pmtDate;
        if (!paymentDates.empty()) {
            // Explicit payment dates were provided
            Calendar paymentCalendar =
                data.paymentCalendar().empty() ? NullCalendar() : parseCalendar(data.paymentCalendar());
            PaymentLag paymentLag = parsePaymentLag(data.paymentLag());
            Period paymentLagPeriod = boost::apply_visitor(PaymentLagPeriod(), paymentLag);
            BusinessDayConvention paymentConvention =
                data.paymentConvention().empty() ? Unadjusted : parseBusinessDayConvention(data.paymentConvention());
            pmtDate = paymentCalendar.advance(paymentDates[i], paymentLagPeriod, paymentConvention);
        } else {
            // Gather the payment conventions.
            BusinessDayConvention bdc =
                data.paymentConvention().empty() ? Following : parseBusinessDayConvention(data.paymentConvention());

            Calendar paymentCalendar =
                data.paymentCalendar().empty() ? schedule.calendar() : parseCalendar(data.paymentCalendar());

            PaymentLag payLag = parsePaymentLag(data.paymentLag());
            Period paymentLag = boost::apply_visitor(PaymentLagPeriod(), payLag);

            // Get unadjusted payment date based on pay relative to value.
            if (fixedLegData->commodityPayRelativeTo() == CommodityPayRelativeTo::CalculationPeriodEndDate) {
                pmtDate = cp->accrualEndDate();
            } else if (fixedLegData->commodityPayRelativeTo() == CommodityPayRelativeTo::CalculationPeriodStartDate) {
                pmtDate = cp->accrualStartDate();
            } else if (fixedLegData->commodityPayRelativeTo() == CommodityPayRelativeTo::TerminationDate) {
                pmtDate = fixedRateLeg.back()->date();
            } else if (fixedLegData->commodityPayRelativeTo() == CommodityPayRelativeTo::FutureExpiryDate) {
                QL_FAIL("Internal error: commodity fixed leg builder can not determine payment date relative to future "
                        "expiry date, this has to be handled in the instrument builder.");
            } else {
                QL_FAIL("Unexpected value " << fixedLegData->commodityPayRelativeTo() << " for CommodityPayRelativeTo");
            }

            // Adjust the payment date using the payment conventions
            pmtDate = paymentCalendar.advance(pmtDate, paymentLag, bdc);
        }

        // Create the fixed cashflow for this period
        commodityFixedLeg.push_back(QuantLib::ext::make_shared<SimpleCashFlow>(cp->amount(), pmtDate));
    }

    applyIndexing(commodityFixedLeg, data, engineFactory, requiredFixings, openEndDateReplacement, useXbsCurves);
    addToRequiredFixings(commodityFixedLeg, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));

    addToRequiredFixings(commodityFixedLeg, QuantLib::ext::make_shared<ore::data::FixingDateGetter>(requiredFixings));

    return commodityFixedLeg;
}

Leg CommodityFloatingLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                          RequiredFixings& requiredFixings, const string& configuration,
                                          const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {

    // allAveraging_ flag should be reset to false before each build. If we do not do this, the allAveraging_
    // flag may have been set from building a different leg previously
    allAveraging_ = false;

    auto floatingLegData = QuantLib::ext::dynamic_pointer_cast<CommodityFloatingLegData>(data.concreteLegData());
    QL_REQUIRE(floatingLegData, "Wrong LegType: expected CommodityFloating but got " << data.legType());

    // Commodity name and its conventions.
    // Default weekends only calendar used to create the "index". Attempt to populate with sth sensible here.
    string commName = floatingLegData->name();
    Calendar commCal = WeekendsOnly();
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<CommodityFutureConvention> commFutureConv;
    boost::optional<pair<Calendar, Real>> offPeakPowerData;
    bool balanceOfTheMonth = false;
    if (conventions->has(commName)) {
        QuantLib::ext::shared_ptr<Convention> commConv = conventions->get(commName);

        // If commodity forward convention
        if (auto c = QuantLib::ext::dynamic_pointer_cast<CommodityForwardConvention>(commConv)) {
            if (c->advanceCalendar() != NullCalendar()) {
                commCal = c->advanceCalendar();
            }
        }

        // If commodity future convention
        commFutureConv = QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(commConv);
        if (commFutureConv) {
            balanceOfTheMonth = commFutureConv->balanceOfTheMonth();
            commCal = commFutureConv->calendar();
            if (const auto& oppid = commFutureConv->offPeakPowerIndexData()) {
                offPeakPowerData = make_pair(oppid->peakCalendar(), oppid->offPeakHours());
            }
        }
    }

    // Get price type i.e. is the commodity floating leg referencing the spot price or a future settlement price.
    CommodityPriceType priceType = floatingLegData->priceType();

    // If referencing a future settlement price, we will need a valid FutureExpiryCalculator below.
    QuantLib::ext::shared_ptr<FutureExpiryCalculator> feCalc;
    if (priceType == CommodityPriceType::FutureSettlement) {

        // We should have a valid commodity future convention in this case but check anyway
        QL_REQUIRE(commFutureConv, "Expected to have a commodity future convention for commodity " << commName);

        feCalc = QuantLib::ext::make_shared<ConventionsBasedFutureExpiry>(*commFutureConv);

        // If the future contract is averaging but the trade is not averaging, we can't price the trade
        // TODO: Should we just issue a warning here, switch the trade to averaging and price it?
        if (commFutureConv->isAveraging()) {
            QL_REQUIRE(floatingLegData->isAveraged(),
                       "The future, " << commName << ", is averaging but the leg is not.");
            allAveraging_ = true;
        }
    }

    // Construct the commodity index.
    Handle<PriceTermStructure> priceCurve = engineFactory->market()->commodityPriceCurve(commName, configuration);
    auto index = parseCommodityIndex(commName, false, priceCurve, NullCalendar(),
                                     priceType == CommodityPriceType::FutureSettlement);

    // Get the commodity floating leg schedule and quantities
    Schedule schedule = makeSchedule(data.schedule());
    vector<Real> quantities =
        buildScheduledVector(floatingLegData->quantities(), floatingLegData->quantityDates(), schedule);

    // Get spreads and gearings which may be empty
    vector<Real> spreads = buildScheduledVector(floatingLegData->spreads(), floatingLegData->spreadDates(), schedule);
    vector<Real> gearings =
        buildScheduledVector(floatingLegData->gearings(), floatingLegData->gearingDates(), schedule);

    // Get explicit pricing dates which in most cases should be empty
    vector<Date> pricingDates;
    if (!floatingLegData->pricingDates().empty()) {
        pricingDates = parseVectorOfValues<Date>(floatingLegData->pricingDates(), &parseDate);
    }

    // Some common variables needed in building the commodity floating leg
    PaymentLag paymentLag = parsePaymentLag(data.paymentLag());
    BusinessDayConvention paymentConvention =
        data.paymentConvention().empty() ? Following : parseBusinessDayConvention(data.paymentConvention());
    Calendar paymentCalendar =
        data.paymentCalendar().empty() ? schedule.calendar() : parseCalendar(data.paymentCalendar());
    Calendar pricingCalendar;

    // Override missing pricing calendar with calendar from convention
    if (floatingLegData->pricingCalendar().empty() && floatingLegData->isAveraged() && balanceOfTheMonth &&
        commFutureConv) {
        pricingCalendar = commFutureConv->balanceOfTheMonthPricingCalendar();
    } else if (floatingLegData->pricingCalendar().empty()) {
        pricingCalendar = commCal;
    } else {
        pricingCalendar = parseCalendar(floatingLegData->pricingCalendar());
    }
         
    // Get explicit payment dates which in most cases should be empty
    vector<Date> paymentDates;

    
    Schedule paymentSchedule = makeSchedule(data.paymentSchedule(), openEndDateReplacement);
   
    if (!paymentSchedule.empty()) {
        paymentDates = paymentSchedule.dates();
    } else if (!data.paymentDates().empty()) {
        BusinessDayConvention paymentDatesConvention =
            data.paymentConvention().empty() ? Unadjusted : parseBusinessDayConvention(data.paymentConvention());
        Calendar paymentDatesCalendar =
            data.paymentCalendar().empty() ? NullCalendar() : parseCalendar(data.paymentCalendar());
        paymentDates = parseVectorOfValues<Date>(data.paymentDates(), &parseDate);
        for (Size i = 0; i < paymentDates.size(); i++)
            paymentDates[i] = paymentDatesCalendar.adjust(paymentDates[i], paymentDatesConvention);
    }

    // May need to poulate hours per day
    auto hoursPerDay = floatingLegData->hoursPerDay();
    if ((floatingLegData->commodityQuantityFrequency() == CommodityQuantityFrequency::PerHour ||
         floatingLegData->commodityQuantityFrequency() == CommodityQuantityFrequency::PerHourAndCalendarDay) &&
        hoursPerDay == Null<Natural>()) {
        QL_REQUIRE(commFutureConv,
                   "Commodity floating leg commodity frequency set to PerHour / PerHourAndCalendarDay but"
                       << " no HoursPerDay provided in floating leg data and no commodity future convention for "
                       << commName);
        hoursPerDay = commFutureConv->hoursPerDay();
        QL_REQUIRE(commFutureConv,
                   "Commodity floating leg commodity frequency set to PerHour / PerHourAndCalendarDay but"
                       << " no HoursPerDay provided in floating leg data and commodity future convention for "
                       << commName << " does not provide it.");
    }

    // populate daylight saving begin / end
    std::string daylightSavingLocation;
    if (floatingLegData->commodityQuantityFrequency() == CommodityQuantityFrequency::PerHourAndCalendarDay) {
        QL_REQUIRE(
            commFutureConv,
            "Commodity floating leg commodity frequency set to PerHourAndCalendarDay, need commodity convention for "
                << commName);
        daylightSavingLocation = commFutureConv->savingsTime();
    }
    QuantLib::ext::shared_ptr<FxIndex> fxIndex;
    // Build the leg. Different ctor depending on whether cashflow is averaging or not.
    Leg leg;

    bool isCashFlowAveraged =
        floatingLegData->isAveraged() && !allAveraging_ && floatingLegData->lastNDays() == Null<Natural>();

    // If daily expiry offset is given, check that referenced future contract has a daily frequency.
    auto dailyExpOffset = floatingLegData->dailyExpiryOffset();
    if (dailyExpOffset != Null<Natural>() && dailyExpOffset > 0) {
        QL_REQUIRE(commFutureConv, "A positive DailyExpiryOffset has been provided but no commodity"
                                       << " future convention given for " << commName);
        QL_REQUIRE(commFutureConv->contractFrequency() == Daily,
                   "A positive DailyExpiryOffset has been"
                       << " provided but the commodity contract frequency is not Daily ("
                       << commFutureConv->contractFrequency() << ")");
    }

    if (!floatingLegData->fxIndex().empty()) { // build the fxIndex for daily average conversion
        auto underlyingCcy = priceCurve->currency().code();
        auto npvCurrency = data.currency();
        if (underlyingCcy != npvCurrency) // only need an FX Index if currencies differ
            fxIndex = buildFxIndex(floatingLegData->fxIndex(), npvCurrency, underlyingCcy, engineFactory->market(),
                                   engineFactory->configuration(MarketContext::pricing));
    }

    if (isCashFlowAveraged) {
        CommodityIndexedAverageCashFlow::PaymentTiming paymentTiming =
            CommodityIndexedAverageCashFlow::PaymentTiming::InArrears;
        if (floatingLegData->commodityPayRelativeTo() == CommodityPayRelativeTo::CalculationPeriodStartDate) {
            paymentTiming = CommodityIndexedAverageCashFlow::PaymentTiming::InAdvance;
        } else if (floatingLegData->commodityPayRelativeTo() == CommodityPayRelativeTo::CalculationPeriodEndDate) {
            paymentTiming = CommodityIndexedAverageCashFlow::PaymentTiming::InArrears;
        } else if (floatingLegData->commodityPayRelativeTo() == CommodityPayRelativeTo::FutureExpiryDate) {
            QL_FAIL("CommodityLegBuilder: CommodityPayRelativeTo 'FutureExpiryDate' not allowed for average cashflow.");
        } else {
            QL_FAIL("CommodityLegBuilder: CommodityPayRelativeTo " << floatingLegData->commodityPayRelativeTo()
                                                                   << " not handled. This is an internal error.");
        }

        leg = CommodityIndexedAverageLeg(schedule, index)
                  .withQuantities(quantities)
                  .withPaymentLag(boost::apply_visitor(PaymentLagInteger(), paymentLag))
                  .withPaymentCalendar(paymentCalendar)
                  .withPaymentConvention(paymentConvention)
                  .withPricingCalendar(pricingCalendar)
                  .withSpreads(spreads)
                  .withGearings(gearings)
                  .paymentTiming(paymentTiming)
                  .useFuturePrice(priceType == CommodityPriceType::FutureSettlement)
                  .withDeliveryDateRoll(floatingLegData->deliveryRollDays())
                  .withFutureMonthOffset(floatingLegData->futureMonthOffset())
                  .withFutureExpiryCalculator(feCalc)
                  .payAtMaturity(floatingLegData->commodityPayRelativeTo() ==
                      CommodityPayRelativeTo::TerminationDate)
                  .includeEndDate(floatingLegData->includePeriodEnd())
                  .excludeStartDate(floatingLegData->excludePeriodStart())
                  .withQuantityFrequency(floatingLegData->commodityQuantityFrequency())
                  .withPaymentDates(paymentDates)
                  .useBusinessDays(floatingLegData->useBusinessDays())
                  .withHoursPerDay(hoursPerDay)
                  .withDailyExpiryOffset(dailyExpOffset)
                  .unrealisedQuantity(floatingLegData->unrealisedQuantity())
                  .withOffPeakPowerData(offPeakPowerData)
                  .withFxIndex(fxIndex);
    } else {
        CommodityIndexedCashFlow::PaymentTiming paymentTiming = CommodityIndexedCashFlow::PaymentTiming::InArrears;
        if (floatingLegData->commodityPayRelativeTo() == CommodityPayRelativeTo::CalculationPeriodStartDate) {
            paymentTiming = CommodityIndexedCashFlow::PaymentTiming::InAdvance;
        } else if (floatingLegData->commodityPayRelativeTo() == CommodityPayRelativeTo::CalculationPeriodEndDate) {
            paymentTiming = CommodityIndexedCashFlow::PaymentTiming::InArrears;
        } else if (floatingLegData->commodityPayRelativeTo() == CommodityPayRelativeTo::FutureExpiryDate) {
            paymentTiming = CommodityIndexedCashFlow::PaymentTiming::RelativeToExpiry;
        } else {
            QL_FAIL("CommodityLegBuilder: CommodityPayRelativeTo " << floatingLegData->commodityPayRelativeTo()
                                                                   << " not handled. This is an internal error.");
        }

        leg = CommodityIndexedLeg(schedule, index)
                  .withQuantities(quantities)
                  .withPaymentLag(boost::apply_visitor(PaymentLagInteger(), paymentLag))
                  .withPaymentCalendar(paymentCalendar)
                  .withPaymentConvention(paymentConvention)
                  .withPricingLag(floatingLegData->pricingLag())
                  .withPricingLagCalendar(pricingCalendar)
                  .withSpreads(spreads)
                  .withGearings(gearings)
                  .paymentTiming(paymentTiming)
                  .inArrears(floatingLegData->isInArrears())
                  .useFuturePrice(priceType == CommodityPriceType::FutureSettlement)
                  .useFutureExpiryDate(floatingLegData->pricingDateRule() == CommodityPricingDateRule::FutureExpiryDate)
                  .withFutureMonthOffset(floatingLegData->futureMonthOffset())
                  .withFutureExpiryCalculator(feCalc)
                  .payAtMaturity(floatingLegData->commodityPayRelativeTo() == CommodityPayRelativeTo::TerminationDate)
                  .withPricingDates(pricingDates)
                  .withPaymentDates(paymentDates)
                  .withDailyExpiryOffset(dailyExpOffset)
                  .withFxIndex(fxIndex)
                  .withIsAveraging(floatingLegData->isAveraged() && balanceOfTheMonth)
                  .withPricingCalendar(pricingCalendar)
                  .includeEndDate(floatingLegData->includePeriodEnd())
                  .excludeStartDate(floatingLegData->excludePeriodStart());

        // Possibly update the leg's quantities.
        updateQuantities(leg, allAveraging_, floatingLegData->commodityQuantityFrequency(), schedule,
                         floatingLegData->excludePeriodStart(), floatingLegData->includePeriodEnd(), commFutureConv,
                         feCalc, hoursPerDay, floatingLegData->useBusinessDays(), daylightSavingLocation, commName,
                         floatingLegData->unrealisedQuantity(), offPeakPowerData);

        // If lastNDays is set, amend each cashflow in the leg to an averaging cashflow over the lastNDays.
        auto lastNDays = floatingLegData->lastNDays();
        if (lastNDays != Null<Natural>() && lastNDays > 1) {
            if (commFutureConv) {
                if (lastNDays > 31) {
                    WLOG("LastNDays (" << lastNDays << ") should not be greater than 31. " <<
                        "Proceed as if it is not set.");
                } else if (commFutureConv->isAveraging()) {
                    WLOG("Commodity future convention for " << commName << " is averaging so LastNDays (" <<
                        lastNDays << ") is ignored. Proceed as if it is not set.");
                } else {
                    DLOG("Amending cashflows to account for LastNDays (" << lastNDays << ").");
                    const auto& cal = commFutureConv->calendar();
                    for (auto& cf : leg) {
                        auto ccf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(cf);
                        const Date& endDate = ccf->pricingDate();
                        Date startDate = cal.advance(endDate, -static_cast<Integer>(lastNDays) + 1, Days, Preceding);
                        TLOG("Creating cashflow averaging over period [" << io::iso_date(startDate) <<
                            "," << io::iso_date(endDate) << "]");
                        cf = QuantLib::ext::make_shared<CommodityIndexedAverageCashFlow>(ccf->periodQuantity(), startDate,
                            endDate, ccf->date(), ccf->index(), cal, ccf->spread(), ccf->gearing(),
                            ccf->useFuturePrice(), 0, 0, feCalc, true, false);
                    }
                }
            } else {
                WLOG("Need a commodity future convention for " << commName << " when LastNDays (" << lastNDays <<
                    ") is set and greater than 1. Proceed as if it is not set.");
            }
        }
    }

    if (fxIndex) {
        // fx daily indexing needed
        for (auto cf : leg) {
            auto cacf = QuantLib::ext::dynamic_pointer_cast<CommodityCashFlow>(cf);
            QL_REQUIRE(cacf, "Commodity Indexed averaged cashflow is required to compute daily converted average.");
            for (auto kv : cacf->indices()) {
                if (!fxIndex->fixingCalendar().isBusinessDay(
                        kv.first)) {
                    /* If fx index is not available for the commodity pricing day, this ensures to require the previous
                     * valid one which will be used in pricing from fxIndex()->fixing(...) */
                    Date adjustedFixingDate = fxIndex->fixingCalendar().adjust(kv.first, Preceding);
                    requiredFixings.addFixingDate(adjustedFixingDate, floatingLegData->fxIndex());
                } else
                    requiredFixings.addFixingDate(kv.first, floatingLegData->fxIndex());
            }
        }
    } else {
        // standard indexing approach
        applyIndexing(leg, data, engineFactory, requiredFixings, openEndDateReplacement, useXbsCurves);
    }

    addToRequiredFixings(leg, QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings));
    return leg;
}
} // namespace data
} // namespace ore
