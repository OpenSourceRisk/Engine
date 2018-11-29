/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/parsers.hpp
    \brief Map text representations to QuantLib/QuantExt types
    \ingroup utilities
*/

#pragma once

#include <ql/compounding.hpp>
#include <ql/currency.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>
#include <ql/position.hpp>
#include <ql/time/businessdayconvention.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/date.hpp>
#include <ql/time/dategenerationrule.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>

#include <qle/methods/multipathgeneratorbase.hpp>

#include <boost/algorithm/string/trim.hpp>
#include <boost/tokenizer.hpp>


namespace ore {
namespace data {
using std::string;

//! Convert std::string to QuantLib::Date
/*!
  \ingroup utilities
*/
QuantLib::Date parseDate(const string& s);

//! Convert text to Real
/*!
  \ingroup utilities
*/
QuantLib::Real parseReal(const string& s);

//! Attempt to convert text to Real
/*! Attempts to convert text to Real
    \param[in]  s      The string we wish to convert to a Real
    \param[out] result The result of the conversion if it is valid.
                       Null<Real>() if conversion fails
    
    \return True if the conversion was successful, False if not
    
    \ingroup utilities
*/
bool tryParseReal(const string& s, QuantLib::Real& result);

//! Convert text to QuantLib::Integer
/*!
  \ingroup utilities
*/
QuantLib::Integer parseInteger(const string& s);

//! Convert text to bool
/*!
  \ingroup utilities
*/
bool parseBool(const string& s);

//! Convert text to QuantLib::Calendar
/*!

  For a joint calendar, the separate calendar names should be
  comma-delimited.
  \ingroup utilities
*/
QuantLib::Calendar parseCalendar(const string& s);

//! Convert text to QuantLib::Period
/*!
  \ingroup utilities
 */
QuantLib::Period parsePeriod(const string& s);

//! Convert text to QuantLib::BusinessDayConvention
/*!
  \ingroup utilities
 */
QuantLib::BusinessDayConvention parseBusinessDayConvention(const string& s);

//! Convert text to QuantLib::DayCounter
/*!
  \ingroup utilities
 */
QuantLib::DayCounter parseDayCounter(const string& s);

//! Convert text to QuantLib::Currency
/*!
  \ingroup utilities
 */
QuantLib::Currency parseCurrency(const string& s);

//! Convert text to QuantLib::DateGeneration::Rule
/*!
  \ingroup utilities
 */
QuantLib::DateGeneration::Rule parseDateGenerationRule(const string& s);

//! Convert text to QuantLib::Frequency
/*!
\ingroup utilities
*/
QuantLib::Frequency parseFrequency(const string& s);

//! Convert text to QuantLib::Compounding;
/*!
\ingroup utilities
*/
QuantLib::Compounding parseCompounding(const string& s);

//! Convert text to QuantLib::Position::Type
/*!
\ingroup utilities
*/
QuantLib::Position::Type parsePositionType(const string& s);

//! Convert text to QuantLib::Settlement::Type
/*!
\ingroup utilities
*/
QuantLib::Settlement::Type parseSettlementType(const string& s);

//! Convert text to QuantLib::Exercise::Type
/*!
\ingroup utilities
*/
QuantLib::Exercise::Type parseExerciseType(const string& s);

//! Convert text to QuantLib::Option::Type
/*!
\ingroup utilities
*/
QuantLib::Option::Type parseOptionType(const string& s);

//! Convert text to QuantLib::Period or QuantLib::Date
/*!
\ingroup utilities
*/
void parseDateOrPeriod(const string& s, QuantLib::Date& d, QuantLib::Period& p, bool& isDate);

//! Convert text to QuantLib::LsmBasisSystem::PolynomType
/*!
\ingroup utilities
*/
QuantLib::LsmBasisSystem::PolynomType parsePolynomType(const std::string& s);

//! Convert comma separated list of values to vector of values
/*!
\ingroup utilities
*/
template <class T> std::vector<T> parseListOfValues(string s, std::function<T(string)> parser) {
    boost::trim(s);
    std::vector<T> vec;
    boost::char_separator<char> sep(",");
    boost::tokenizer<boost::char_separator<char>> tokens(s, sep);
    for (auto r : tokens) {
        boost::trim(r);
        vec.push_back(parser(r));
    }
    return vec;
}

std::vector<string> parseListOfValues(string s);

enum class AmortizationType { None, FixedAmount, RelativeToInitialNotional, RelativeToPreviousNotional, Annuity };
AmortizationType parseAmortizationType(const std::string& s);

//! Convert string to sequence type
/*!
\ingroup utilities
*/
QuantExt::SequenceType parseSequenceType(const std::string& s);

} // namespace data
} // namespace ore
