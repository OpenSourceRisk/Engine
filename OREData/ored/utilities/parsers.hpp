/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

#include <ored/configuration/commoditycurveconfig.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/portfolio/types.hpp>
#include <ored/utilities/log.hpp>

#include <qle/cashflows/commoditycashflow.hpp>
#include <qle/currencies/configurablecurrency.hpp>
#include <qle/indexes/bondindex.hpp>
#include <qle/instruments/cdsoption.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/pricingengines/mcmultilegbaseengine.hpp>
#include <qle/termstructures/sabrparametricvolatility.hpp>

#include <ql/cashflows/cpicoupon.hpp>
#include <ql/compounding.hpp>
#include <ql/currency.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/doublebarriertype.hpp>
#include <ql/experimental/fx/deltavolquote.hpp>
#include <ql/instruments/averagetype.hpp>
#include <ql/instruments/barriertype.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/instruments/capfloor.hpp>
#include <ql/instruments/inflationcapfloor.hpp>
#include <ql/instruments/overnightindexfuture.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/methods/finitedifferences/solvers/fdmbackwardsolver.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>
#include <ql/position.hpp>
#include <ql/termstructures/bootstraphelper.hpp>
#include <ql/time/businessdayconvention.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/date.hpp>
#include <ql/time/dategenerationrule.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>

#include <boost/algorithm/string/trim.hpp>
#include <boost/tokenizer.hpp>
#include <boost/variant.hpp>

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

//! Convert text to Real, empty string to Null<Real>()
/*!
  \ingroup utilities
*/
Real parseRealOrNull(const string& s);

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

//! return true if s represents a period of the form [0-9][D|W|M|Y] (i.e. 1Y6M would return false)
bool isOnePeriod(const string& s);

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

//! Convert text to QuantExt::ConfigurableCurrency::Type (Major, Minor, Metal, Crypto)
/*!
  \ingroup utilities
 */
QuantExt::ConfigurableCurrency::Type parseCurrencyType(const string& s);

//! Convert text to QuantLib::Currency for minor currencies e.g GBp -> GBPCurrency()
/*!
  \ingroup utilities
 */
QuantLib::Currency parseMinorCurrency(const string& s);

//! Convert text to QuantLib::Currency
/*!
  \ingroup utilities
 */
QuantLib::Currency parseCurrencyWithMinors(const string& s);

//! Convert text to std::pair<QuantLib::Currency, QuantLib::Currency>
/*!
  \ingroup utilities
 */
std::pair<QuantLib::Currency, QuantLib::Currency> parseCurrencyPair(const string& s, const string& delimiters);

//! check for vaid currency code, including minors and pseudo currencies
/*!
  \ingroup utilities
 */
bool checkCurrency(const string& code);

//! check for pseudo currency = precious metal or crypto currency */
/*!
  \ingroup utilities
 */
bool isPseudoCurrency(const string& code);

//! check for precious metal */
/*!
  \ingroup utilities
 */
bool isPreciousMetal(const string& code);

//! check for crypto currency */
/*!
  \ingroup utilities
 */
bool isCryptoCurrency(const string& code);

//! Convert a value from a minor ccy to major
/*! .i.e 100 GBp to 1 GBP
    \ingroup utilities
*/
QuantLib::Real convertMinorToMajorCurrency(const std::string& s, QuantLib::Real value);

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

//! Convert text to QuantLib::Protection::Side
/*!
\ingroup utilities
*/
QuantLib::Protection::Side parseProtectionSide(const string& s);

//! Convert text to QuantLib::Settlement::Type
/*!
\ingroup utilities
*/
QuantLib::Settlement::Type parseSettlementType(const string& s);

//! Convert text to QuantLib::Settlement::Method
/*!
\ingroup utilities
*/
QuantLib::Settlement::Method parseSettlementMethod(const string& s);

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

//! Convert text to QuantLib::Bond::Price::Type
/*!
\ingroup utilities
*/
QuantLib::Bond::Price::Type parseBondPriceType(const string& s);

//! Convert text to QuantLib::Period or QuantLib::Date
/*!
\ingroup utilities
*/
boost::variant<QuantLib::Date, QuantLib::Period> parseDateOrPeriod(const string& s);

//! Convert text to QuantLib::Period or QuantLib::Date (deprecated version)
/*!
\ingroup utilities
*/
void parseDateOrPeriod(const string& s, QuantLib::Date& d, QuantLib::Period& p, bool& isDate);

//! Convert text to QuantLib::LsmBasisSystem::PolynomialType
/*!
\ingroup utilities
*/
QuantLib::LsmBasisSystem::PolynomialType parsePolynomType(const std::string& s);

//! Write QuantLib::LsmBasisSystem::PolynomialType to stream
/*!
\ingroup utilities
*/
std::ostream& operator<<(std::ostream& os, QuantLib::LsmBasisSystem::PolynomialType a);

//! Convert text to QuantLib::SobolBrownianGenerator::Ordering
/*!
\ingroup utilities
*/
QuantLib::SobolBrownianGenerator::Ordering parseSobolBrownianGeneratorOrdering(const std::string& s);

//! Convert text to QuantLib::SobolRsg::DirectionIntegers
/*!
\ingroup utilities
*/
QuantLib::SobolRsg::DirectionIntegers parseSobolRsgDirectionIntegers(const std::string& s);

/*! Convert text to QuantLib::Weekday
    \ingroup utilities
*/
QuantLib::Weekday parseWeekday(const std::string& s);

/*! Convert text to QuantLib::Month
    \ingroup utilities
*/
QuantLib::Month parseMonth(const std::string& s);


//! Convert text to PaymentLag
/*!
\ingroup utilities
*/
PaymentLag parsePaymentLag(const string& s);

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

template <class T> std::vector<T> parseVectorOfValues(std::vector<std::string> str, std::function<T(string)> parser) {
    std::vector<T> vec;
    for (auto s : str) {
        vec.push_back(parser(s));
    }
    return vec;
}

std::vector<string> parseListOfValues(string s, const char escape = '\\', const char delim = ',',
                                      const char quote = '\"');

enum class AmortizationType {
    None,
    FixedAmount,
    RelativeToInitialNotional,
    RelativeToPreviousNotional,
    Annuity,
    LinearToMaturity
};
AmortizationType parseAmortizationType(const std::string& s);

//! Convert string to sequence type
/*!
\ingroup utilities
*/
QuantExt::SequenceType parseSequenceType(const std::string& s);

//! Convert string to observation interpolation
/*!
\ingroup utilities
*/
QuantLib::CPI::InterpolationType parseObservationInterpolation(const std::string& s);

//! Convert string to fdm scheme desc
/*!
\ingroup utilities
*/
QuantLib::FdmSchemeDesc parseFdmSchemeDesc(const std::string& s);

enum class AssetClass { EQ, FX, COM, IR, INF, CR, BOND, BOND_INDEX };

//! Convert text to ore::data::AssetClass
/*!
\ingroup utilities
*/
AssetClass parseAssetClass(const std::string& s);

//! Write ore::data::AssetClass to stream
/*!
\ingroup utilities
*/
std::ostream& operator<<(std::ostream& os, AssetClass a);

//! Convert text to QuantLib::DeltaVolQuote::AtmType
/*!
\ingroup utilities
*/
QuantLib::DeltaVolQuote::AtmType parseAtmType(const std::string& s);

//! Convert text to QuantLib::DeltaVolQuote::DeltaType
/*!
\ingroup utilities
*/
QuantLib::DeltaVolQuote::DeltaType parseDeltaType(const std::string& s);

//! Convert text to QuantLib::Rounding
/*!
\ingroup utilities
*/
QuantLib::Rounding::Type parseRoundingType(const std::string& s);
  
//! Convert std::string to QuantLib::BarrierType
/*!
  \ingroup utilities
*/
QuantLib::Barrier::Type parseBarrierType(const string& s);

//! Convert std::string to QuantLib::DoubleBarrierType
/*!
  \ingroup utilities
*/
QuantLib::DoubleBarrier::Type parseDoubleBarrierType(const string& s);

/*! Attempt to parse string \p str to \p obj of type \c T using \p parser
    \param[in]  str    The string we wish to parse.
    \param[out] obj    The resulting object if the parsing was successful.
    \param[in]  parser The function to use to attempt to parse \p str. This function may throw.

    \return \c true if the parsing was successful and \c false if not.

    \ingroup utilities
*/
template <class T> bool tryParse(const std::string& str, T& obj, std::function<T(const std::string&)> parser) {
    DLOG("tryParse: attempting to parse " << str);
    try {
        obj = parser(str);
    } catch (...) {
        TLOG("String " << str << " could not be parsed");
        return false;
    }
    return true;
}

inline bool tryParseCurrency(const std::string& str, Currency& obj) {
    DLOG("tryParse: attempting to parse currency from " << str);
    try {
        obj = parseCurrency(str);
    } catch (...) {
        TLOG("String " << str << " could not be parsed");
        return false;
    }
    return true;
}

//! Enumeration for holding various extrapolation settings
enum class Extrapolation { None, UseInterpolator, Flat };

//! Parse Extrapolation from string
Extrapolation parseExtrapolation(const std::string& s);

//! Write Extrapolation, \p extrap, to stream.
std::ostream& operator<<(std::ostream& os, Extrapolation extrap);

/*! Convert text to QuantLib::VolatilityType
    \ingroup utilities
*/
QuantLib::VolatilityType parseVolatilityQuoteType(const std::string& s);

/*! Convert text to QuantLib::CapFloor::Type
    \ingroup utilities
*/
QuantLib::CapFloor::Type parseCapFloorType(const std::string& s);

/*! Convert text to QuantLib::YoYInflationCapFloor::Type
    \ingroup utilities
*/
QuantLib::YoYInflationCapFloor::Type parseYoYInflationCapFloorType(const std::string& s);

/*! Convert text to QuantExt::CrossAssetModelTypes::AssetType
    \ingroup utilities
*/
QuantExt::CrossAssetModel::AssetType parseCamAssetType(const std::string& s);

/*! Convert boost::any to pair<string,string>, including the valueType and the value
    \ingroup utilities
*/
std::pair<string, string> parseBoostAny(const boost::any& anyType, Size precision = 8);

//! Convert text to QuantLib::RateAveraging::Type
QuantLib::RateAveraging::Type parseOvernightIndexFutureNettingType(const std::string& s);

//! Write QuantLib::RateAveraging::Type to stream
std::ostream& operator<<(std::ostream& os, QuantLib::RateAveraging::Type t);

//! Convert text to FutureConvention::DateGeneration
FutureConvention::DateGenerationRule parseFutureDateGenerationRule(const std::string& s);

//! Write QuantLib::RateAveraging::Type to stream
std::ostream& operator<<(std::ostream& os, FutureConvention::DateGenerationRule t);

//! Convert text to InflationSwapConvention::PublicationRoll
InflationSwapConvention::PublicationRoll parseInflationSwapPublicationRoll(const std::string& s);

//! Write InflationSwapConvention::PublicationRoll to stream
std::ostream& operator<<(std::ostream& os, InflationSwapConvention::PublicationRoll pr);

//! Write QuantLib::SobolBrownianGenerator::Ordering to stream
std::ostream& operator<<(std::ostream& os, SobolBrownianGenerator::Ordering t);
    
//! Write QuantLib::SobolRsg::DirectionIntegers to stream
std::ostream& operator<<(std::ostream& os, SobolRsg::DirectionIntegers t);
    
//! Enum to string used in ScenarioGeneratorData's toXML
std::ostream& operator<<(std::ostream& os, QuantExt::CrossAssetModel::Discretization type);

//! Convert text to CommodityFutureConvention::AveragingData::CalculationPeriod
CommodityFutureConvention::AveragingData::CalculationPeriod parseAveragingDataPeriod(const std::string& s);

//! Write CommodityFutureConvention::AveragingData::CalculationPeriod to stream
std::ostream& operator<<(std::ostream& os, CommodityFutureConvention::AveragingData::CalculationPeriod cp);

//! Convert text to PriceSegment::Type
PriceSegment::Type parsePriceSegmentType(const std::string& s);

//! Write PriceSegment::Type to stream
std::ostream& operator<<(std::ostream& os, PriceSegment::Type pst);

//! Convert text to QuantExt::CommodityQuantityFrequency
QuantExt::CommodityQuantityFrequency parseCommodityQuantityFrequency(const std::string& s);

//! Write QuantExt::CommodityQuantityFrequency to stream
std::ostream& operator<<(std::ostream& os, QuantExt::CommodityQuantityFrequency cqf);

/*! Convert text to QuantExt::CdsOption::StrikeType
    \ingroup utilities
*/
QuantExt::CdsOption::StrikeType parseCdsOptionStrikeType(const std::string& s);

/*! Convert text to QuantLib::Average::Type
    \ingroup utilities
*/
QuantLib::Average::Type parseAverageType(const std::string& s);

/*! Convert text to QuantExt::BondData::PriceQuoteMethod
\ingroup utilities
 */
QuantExt::BondIndex::PriceQuoteMethod parsePriceQuoteMethod(const std::string& s);

//! Write PriceQuoteMethod to stream
std::ostream& operator<<(std::ostream& os, QuantExt::BondIndex::PriceQuoteMethod);

//! Helper function to get the two tokens in a correlation name Index2:Index1
std::vector<std::string> getCorrelationTokens(const std::string& name);

//! Convert FX pair to market standard dominance
/*!
 Convert FX pair to market standard dominance, e.g. "USD" & "GBP" -> "GBPUSD", "USD" & "JPY" -> "USDJPY"
  \ingroup utilities
*/
string fxDominance(const string& s1, const string& s2);

//! Convert FX index name to market standard dominance
string normaliseFxIndex(const std::string& indexName);

enum class MomentType { Variance, Volatility };

//! Convert text to ore::data::MomentType
/*!
\ingroup utilities
*/
MomentType parseMomentType(const std::string& s);

//! Enumeration CreditPortfolioSensitivityDecomposition
enum class CreditPortfolioSensitivityDecomposition { Underlying, NotionalWeighted, LossWeighted, DeltaWeighted };

//! Convert text to CreditPortfolioSensitivitiyDecomposition
CreditPortfolioSensitivityDecomposition parseCreditPortfolioSensitivityDecomposition(const std::string& s);

//! Output operator for CreditPortfolioSensitivityDecomposition
std::ostream& operator<<(std::ostream& os, const CreditPortfolioSensitivityDecomposition d);

//! Convert text to QuantLib::Pillar::Choice
/*!
\ingroup utilities
*/
QuantLib::Pillar::Choice parsePillarChoice(const std::string& s);

//! Convert text to QuantExt::McMultiLegBaseEngine::RegressorModel
/*!
\ingroup utilities
*/
QuantExt::McMultiLegBaseEngine::RegressorModel parseRegressorModel(const std::string& s);

enum MporCashFlowMode { Unspecified, NonePay, BothPay, WePay, TheyPay };

//! Convert text to MporCashFlowMode
/*!
\ingroup utilities
*/
MporCashFlowMode parseMporCashFlowMode(const std::string& s);

//! Write MporCashFlowMode to stream
/*!
\ingroup utilities
*/
std::ostream& operator<<(std::ostream& os, MporCashFlowMode t);

//! Parse SabrParametricVolatility::ModelVariant
/*!
\ingroup utilities
*/
QuantExt::SabrParametricVolatility::ModelVariant parseSabrParametricVolatilityModelVariant(const std::string& s);

//! Write SabrParametricVolatility::ModelVariant
/*!
\ingroup utilities
*/
std::ostream& operator<<(std::ostream& out, QuantExt::SabrParametricVolatility::ModelVariant m);

//! Write QuantLib::Exercise::Type
/*!
\ingroup utilities
*/
std::ostream& operator<<(std::ostream& os, QuantLib::Exercise::Type type);

} // namespace data
} // namespace ore
