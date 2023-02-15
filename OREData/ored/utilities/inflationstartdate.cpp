/*
 Copyright (C) 2022 Quaternion Risk Management Ltd

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

#include <ored/utilities/inflationstartdate.hpp>

namespace ore {
namespace data {
using std::make_pair;
using QuantLib::Date;
using QuantLib::Period;
using QuantLib::Days;

std::pair<QuantLib::Date, QuantLib::Period> getStartAndLag(const QuantLib::Date& asof,
                                                           const InflationSwapConvention& conv) {

    using IPR = InflationSwapConvention::PublicationRoll;

    // If no roll schedule, just return (as of, convention's obs lag).
    if (conv.publicationRoll() == IPR::None) {
        return make_pair(asof, Period());
    }

    // If there is a publication roll, call getStart to retrieve the date.
    Date d = getInflationSwapStart(asof, conv);

    // Date in inflation period related to the inflation index value.
    Date dateInPeriod = d - Period(conv.index()->frequency());

    // Find period between dateInPeriod and asof. This will be the inflation curve's obsLag.
    QL_REQUIRE(dateInPeriod < asof, "InflationCurve: expected date in inflation period ("
                                        << io::iso_date(dateInPeriod) << ") to be before the as of date ("
                                        << io::iso_date(asof) << ").");
    Period curveObsLag = (asof - dateInPeriod) * Days;

    return make_pair(d, curveObsLag);
}

QuantLib::Date getInflationSwapStart(const QuantLib::Date& asof, const InflationSwapConvention& conv) {

    using IPR = InflationSwapConvention::PublicationRoll;

    // If no roll schedule, just return (as of, convention's obs lag).
    if (conv.publicationRoll() == IPR::None) {
        return asof;
    }

    // Get schedule and check not empty
    const Schedule& ps = conv.publicationSchedule();
    QL_REQUIRE(!ps.empty(), "InflationCurve: roll on publication is true for "
                                << conv.id() << " but the publication schedule is empty.");

    // Check the schedule dates cover the as of date.
    const vector<Date>& ds = ps.dates();
    QL_REQUIRE(ds.front() < asof, "InflationCurve: first date in the publication schedule ("
                                      << io::iso_date(ds.front()) << ") should be before the as of date ("
                                      << io::iso_date(asof) << ").");
    QL_REQUIRE(asof < ds.back(), "InflationCurve: last date in the publication schedule ("
                                     << io::iso_date(ds.back()) << ") should be after the as of date ("
                                     << io::iso_date(asof) << ").");

    // Find d such that d_- < asof <= d. If necessary, move to the next publication schedule date. We
    // know that there is another date because asof < ds.back() is checked above.
    auto it = lower_bound(ds.begin(), ds.end(), asof);
    Date d = *it;
    if (asof == d && conv.publicationRoll() == IPR::OnPublicationDate) {
        d = *next(it);
    }

    // Move d back availability lag and the 15th of that month is the helper's start date.
    // Note: the 15th of the month is specific to AU CPI. We may need to generalise later.
    d -= conv.index()->availabilityLag();

    return Date(15, d.month(), d.year());
}

} // namespace data
} // namespace ore
