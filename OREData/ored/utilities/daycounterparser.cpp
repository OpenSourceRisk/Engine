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

/*! \file ored/utilities/calendarparser.hpp
    \brief calendar parser singleton class
    \ingroup utilities
*/

#include <ored/utilities/daycounterparser.hpp>

#include <qle/time/yearcounter.hpp>

#include <ql/time/daycounters/all.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

DayCounterParser::DayCounterParser() { reset(); }

QuantLib::DayCounter DayCounterParser::parseDayCounter(const std::string& name) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    auto it = dayCounters_.find(name);
    if (it != dayCounters_.end())
        return it->second;
    QL_FAIL("DayCounter \"" << name << "\" not recognized");
}

void DayCounterParser::reset() {

    boost::unique_lock<boost::shared_mutex> lock(mutex_);

    static std::map<std::string, DayCounter> ref = {{"A360", Actual360()},
                                               {"Actual/360", Actual360()},
                                               {"ACT/360", Actual360()},
                                               {"Act/360", Actual360()},
                                               {"A360 (Incl Last)", Actual360(true)},
                                               {"Actual/360 (Incl Last)", Actual360(true)},
                                               {"ACT/360 (Incl Last)", Actual360(true)},
                                               {"Act/360 (Incl Last)", Actual360(true)},
                                               {"A365", Actual365Fixed()},
                                               {"A365F", Actual365Fixed()},
                                               {"Actual/365 (Fixed)", Actual365Fixed()},
                                               {"Actual/365 (fixed)", Actual365Fixed()},
                                               {"ACT/365.FIXED", Actual365Fixed()},
                                               {"ACT/365", Actual365Fixed()},
                                               {"ACT/365L", Actual365Fixed()},
                                               {"Act/365", Actual365Fixed()},
                                               {"Act/365L", Actual365Fixed()},
                                               {"Act/365 (Canadian Bond)", Actual365Fixed(Actual365Fixed::Canadian)},
                                               {"T360", Thirty360(Thirty360::USA)},
                                               {"30/360", Thirty360(Thirty360::USA)},
                                               {"30/360 US", Thirty360(Thirty360::USA)},
                                               {"30/360 (US)", Thirty360(Thirty360::USA)},
                                               {"30U/360", Thirty360(Thirty360::USA)},
                                               {"30US/360", Thirty360(Thirty360::USA)},
                                               {"30/360 (Bond Basis)", Thirty360(Thirty360::BondBasis)},
                                               {"ACT/nACT", Thirty360(Thirty360::USA)},
                                               {"30E/360 (Eurobond Basis)", Thirty360(Thirty360::European)},
                                               {"30/360 AIBD (Euro)", Thirty360(Thirty360::European)},
                                               {"30E/360.ICMA", Thirty360(Thirty360::European)},
                                               {"30E/360 ICMA", Thirty360(Thirty360::European)},
                                               {"30E/360", Thirty360(Thirty360::European)},
                                               {"30E/360E", Thirty360(Thirty360::German)},
                                               {"30E/360.ISDA", Thirty360(Thirty360::German)},
                                               {"30E/360 ISDA", Thirty360(Thirty360::German)},
                                               {"30/360 German", Thirty360(Thirty360::German)},
                                               {"30/360 (German)", Thirty360(Thirty360::German)},
                                               {"30/360 Italian", Thirty360(Thirty360::Italian)},
                                               {"30/360 (Italian)", Thirty360(Thirty360::Italian)},
                                               {"ActActISDA", ActualActual(ActualActual::ISDA)},
                                               {"ACT/ACT.ISDA", ActualActual(ActualActual::ISDA)},
                                               {"Actual/Actual (ISDA)", ActualActual(ActualActual::ISDA)},
                                               {"ActualActual (ISDA)", ActualActual(ActualActual::ISDA)},
                                               {"ACT/ACT", ActualActual(ActualActual::ISDA)},
                                               {"ACT29", ActualActual(ActualActual::AFB)},
                                               {"ACT", ActualActual(ActualActual::ISDA)},
                                               {"ActActISMA", ActualActual(ActualActual::ISMA)},
                                               {"Actual/Actual (ISMA)", ActualActual(ActualActual::ISMA)},
                                               {"ActualActual (ISMA)", ActualActual(ActualActual::ISMA)},
                                               {"ACT/ACT.ISMA", ActualActual(ActualActual::ISMA)},
                                               {"ActActICMA", ActualActual(ActualActual::ISMA)},
                                               {"Actual/Actual (ICMA)", ActualActual(ActualActual::ISMA)},
                                               {"ActualActual (ICMA)", ActualActual(ActualActual::ISMA)},
                                               {"ACT/ACT.ICMA", ActualActual(ActualActual::ISMA)},
                                               {"ActActAFB", ActualActual(ActualActual::AFB)},
                                               {"ACT/ACT.AFB", ActualActual(ActualActual::AFB)},
                                               {"Actual/Actual (AFB)", ActualActual(ActualActual::AFB)},
                                               {"1/1", OneDayCounter()},
                                               {"BUS/252", Business252()},
                                               {"Business/252", Business252()},
                                               {"Actual/365 (No Leap)", Actual365Fixed(Actual365Fixed::NoLeap)},
                                               {"Act/365 (NL)", Actual365Fixed(Actual365Fixed::NoLeap)},
                                               {"NL/365", Actual365Fixed(Actual365Fixed::NoLeap)},
                                               {"Actual/365 (JGB)", Actual365Fixed(Actual365Fixed::NoLeap)},
                                               {"Simple", SimpleDayCounter()},
                                               {"Year", YearCounter()},
                                               {"A364", QuantLib::Actual364()},
                                               {"Actual/364", Actual364()},
                                               {"Act/364", Actual364()},
                                               {"ACT/364", Actual364()}};

    dayCounters_ = ref;

    // add ql day counter names

    for (auto const& c : ref) {
        dayCounters_[c.second.name()] = c.second;
    }
}

} // namespace data
} // namespace ore
