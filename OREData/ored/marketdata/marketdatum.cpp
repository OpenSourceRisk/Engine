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

#include <ored/marketdata/marketdatum.hpp>
#include <ored/utilities/parsers.hpp>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/lexical_cast.hpp>

using boost::bad_lexical_cast;
using boost::lexical_cast;

namespace ore {
namespace data {

std::ostream& operator<<(std::ostream& out, const MarketDatum::QuoteType& type) {
    switch (type) {
    case MarketDatum::QuoteType::BASIS_SPREAD:
        return out << "BASIS_SPREAD";
    case MarketDatum::QuoteType::CREDIT_SPREAD:
        return out << "CREDIT_SPREAD";
    case MarketDatum::QuoteType::CONV_CREDIT_SPREAD:
        return out << "CONV_CREDIT_SPREAD";
    case MarketDatum::QuoteType::YIELD_SPREAD:
        return out << "YIELD_SPREAD";
    case MarketDatum::QuoteType::RATE:
        return out << "RATE";
    case MarketDatum::QuoteType::RATIO:
        return out << "RATIO";
    case MarketDatum::QuoteType::PRICE:
        return out << "PRICE";
    case MarketDatum::QuoteType::RATE_LNVOL:
        return out << "RATE_LNVOL";
    case MarketDatum::QuoteType::RATE_NVOL:
        return out << "RATE_NVOL";
    case MarketDatum::QuoteType::RATE_SLNVOL:
        return out << "RATE_SLNVOL";
    case MarketDatum::QuoteType::BASE_CORRELATION:
        return out << "BASE_CORRELATION";
    case MarketDatum::QuoteType::SHIFT:
        return out << "SHIFT";
    case MarketDatum::QuoteType::NONE:
        return out << "NULL";
    default:
        return out << "?";
    }
}

EquityOptionQuote::EquityOptionQuote(Real value, Date asofDate, const string& name, QuoteType quoteType,
                                     string equityName, string ccy, string expiry,
                                     const boost::shared_ptr<BaseStrike>& strike, bool isCall)
    : MarketDatum(value, asofDate, name, quoteType, InstrumentType::EQUITY_OPTION), eqName_(equityName), ccy_(ccy),
      expiry_(expiry), strike_(strike), isCall_(isCall) {

    // we will call a parser on the expiry string, to ensure it is a correctly-formatted date or tenor
    Date tmpDate;
    Period tmpPeriod;
    bool tmpBool;
    parseDateOrPeriod(expiry, tmpDate, tmpPeriod, tmpBool);
}

IndexCDSOptionQuote::IndexCDSOptionQuote(QuantLib::Real value, const Date& asof, const string& name,
                                         const string& indexName, const boost::shared_ptr<Expiry>& expiry,
                                         const string& indexTerm, const boost::shared_ptr<BaseStrike>& strike)
    : MarketDatum(value, asof, name, QuoteType::RATE_LNVOL, InstrumentType::INDEX_CDS_OPTION), indexName_(indexName),
      expiry_(expiry), indexTerm_(indexTerm), strike_(strike) {}

namespace {
Natural yearFromExpiryString(const std::string& expiry) {
    QL_REQUIRE(expiry.length() == 7, "The expiry string must be of "
                                     "the form YYYY-MM");
    string strExpiryYear = expiry.substr(0, 4);
    Natural expiryYear;
    try {
        expiryYear = lexical_cast<Natural>(strExpiryYear);
    } catch (const bad_lexical_cast&) {
        QL_FAIL("Could not convert year string, " << strExpiryYear << ", to number.");
    }
    return expiryYear;
}

Month monthFromExpiryString(const std::string& expiry) {
    QL_REQUIRE(expiry.length() == 7, "The expiry string must be of "
                                     "the form YYYY-MM");
    string strExpiryMonth = expiry.substr(5);
    Natural expiryMonth;
    try {
        expiryMonth = lexical_cast<Natural>(strExpiryMonth);
    } catch (const bad_lexical_cast&) {
        QL_FAIL("Could not convert month string, " << strExpiryMonth << ", to number.");
    }
    return static_cast<Month>(expiryMonth);
}
} // namespace

Natural MMFutureQuote::expiryYear() const { return yearFromExpiryString(expiry_); }

Month MMFutureQuote::expiryMonth() const { return monthFromExpiryString(expiry_); }

Natural OIFutureQuote::expiryYear() const { return yearFromExpiryString(expiry_); }

Month OIFutureQuote::expiryMonth() const { return monthFromExpiryString(expiry_); }

QuantLib::Size SeasonalityQuote::applyMonth() const {
    QL_REQUIRE(month_.length() == 3, "The month string must be of "
                                     "the form MMM");
    std::vector<std::string> allMonths = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    QuantLib::Size applyMonth;
    auto it = std::find(allMonths.begin(), allMonths.end(), month_);
    if (it != allMonths.end()) {
        applyMonth = std::distance(allMonths.begin(), it) + 1;
    } else {
        QL_FAIL("Unknown month string: " << month_);
    }
    return applyMonth;
}

CommodityOptionQuote::CommodityOptionQuote(Real value, const Date& asof, const string& name, QuoteType quoteType,
                                           const string& commodityName, const string& quoteCurrency,
                                           const boost::shared_ptr<Expiry>& expiry,
                                           const boost::shared_ptr<BaseStrike>& strike,
                                           Option::Type optionType)
    : MarketDatum(value, asof, name, quoteType, InstrumentType::COMMODITY_OPTION), commodityName_(commodityName),
      quoteCurrency_(quoteCurrency), expiry_(expiry), strike_(strike), optionType_(optionType) {}

CorrelationQuote::CorrelationQuote(Real value, const Date& asof, const string& name, QuoteType quoteType,
                                   const string& index1, const string& index2, const string& expiry,
                                   const string& strike)
    : MarketDatum(value, asof, name, quoteType, InstrumentType::CORRELATION), index1_(index1), index2_(index2),
      expiry_(expiry), strike_(strike) {

    // If strike is not ATM, it must parse to Real
    if (strike != "ATM") {
        Real result;
        QL_REQUIRE(tryParseReal(strike_, result),
                   "Commodity option quote strike (" << strike_ << ") must be either ATM or an actual strike price");
    }

    // Call parser to check that the expiry_ resolves to a period or a date
    Date outDate;
    Period outPeriod;
    bool outBool;
    parseDateOrPeriod(expiry_, outDate, outPeriod, outBool);
}

template <class Archive> void MarketDatum::serialize(Archive& ar, const unsigned int version) {
    Real value;
    // save / load the value of the quote, do not try to serialize the quote as such
    if (Archive::is_saving::value) {
        value = quote_->value();
        ar& value;
    } else {
        ar& value;
        quote_ = Handle<Quote>(boost::make_shared<SimpleQuote>(value));
    }
    ar& asofDate_;
    ar& name_;
    ar& instrumentType_;
    ar& quoteType_;
}

template <class Archive> void MoneyMarketQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& ccy_;
    ar& fwdStart_;
    ar& term_;
}

template <class Archive> void FRAQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& ccy_;
    ar& fwdStart_;
    ar& term_;
}

template <class Archive> void ImmFraQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& ccy_;
    ar& imm1_;
    ar& imm2_;
}

template <class Archive> void SwapQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& ccy_;
    ar& fwdStart_;
    ar& term_;
    ar& tenor_;
}

template <class Archive> void ZeroQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& ccy_;
    ar& date_;
    ar& dayCounter_;
    ar& tenor_;
    ar& tenorBased_;
}

template <class Archive> void DiscountQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& ccy_;
    ar& date_;
}

template <class Archive> void MMFutureQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& ccy_;
    ar& expiry_;
    ar& contract_;
    ar& tenor_;
}

template <class Archive> void OIFutureQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& ccy_;
    ar& expiry_;
    ar& contract_;
    ar& tenor_;
}

template <class Archive> void BasisSwapQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& flatTerm_;
    ar& term_;
    ar& ccy_;
    ar& maturity_;
}

template <class Archive> void BMASwapQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& term_;
    ar& ccy_;
    ar& maturity_;
}

template <class Archive> void CrossCcyBasisSwapQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& flatCcy_;
    ar& flatTerm_;
    ar& ccy_;
    ar& term_;
    ar& maturity_;
}

template <class Archive> void CrossCcyFixFloatSwapQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& floatCurrency_;
    ar& floatTenor_;
    ar& fixedCurrency_;
    ar& fixedTenor_;
    ar& maturity_;
}

template <class Archive> void CdsQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& underlyingName_;
    ar& seniority_;
    ar& ccy_;
    ar& term_;
}

template <class Archive> void HazardRateQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& underlyingName_;
    ar& seniority_;
    ar& ccy_;
    ar& term_;
    ar& docClause_;
}

template <class Archive> void RecoveryRateQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& underlyingName_;
    ar& seniority_;
    ar& ccy_;
    ar& docClause_;
}

template <class Archive> void SwaptionQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& ccy_;
    ar& expiry_;
    ar& term_;
    ar& dimension_;
    ar& strike_;
}

template <class Archive> void SwaptionShiftQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& ccy_;
    ar& term_;
}

template <class Archive> void BondOptionQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& qualifier_;
    ar& expiry_;
    ar& term_;
}

template <class Archive> void BondOptionShiftQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& qualifier_;
    ar& term_;
}

template <class Archive> void CapFloorQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& ccy_;
    ar& term_;
    ar& underlying_;
    ar& atm_;
    ar& relative_;
    ar& strike_;
}

template <class Archive> void CapFloorShiftQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& ccy_;
    ar& indexTenor_;
}

template <class Archive> void FXSpotQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& unitCcy_;
    ar& ccy_;
}

template <class Archive> void FXForwardQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& unitCcy_;
    ar& ccy_;
    ar& term_;
    ar& conversionFactor_;
}

template <class Archive> void FXOptionQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& unitCcy_;
    ar& ccy_;
    ar& expiry_;
    ar& strike_;
}

template <class Archive> void ZcInflationSwapQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& index_;
    ar& term_;
}

template <class Archive> void InflationCapFloorQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& index_;
    ar& term_;
    ar& isCap_;
    ar& strike_;
}

template <class Archive> void ZcInflationCapFloorQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<InflationCapFloorQuote>(*this);
}

template <class Archive> void YoYInflationSwapQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& index_;
    ar& term_;
}

template <class Archive> void YyInflationCapFloorQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<InflationCapFloorQuote>(*this);
}

template <class Archive> void SeasonalityQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& index_;
    ar& type_;
    ar& month_;
}

template <class Archive> void EquitySpotQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& eqName_;
    ar& ccy_;
}

template <class Archive> void EquityForwardQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& eqName_;
    ar& ccy_;
    ar& expiry_;
}

template <class Archive> void EquityDividendYieldQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& eqName_;
    ar& ccy_;
    ar& tenor_;
}

template <class Archive> void EquityOptionQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& eqName_;
    ar& ccy_;
    ar& expiry_;
    ar& strike_;
    ar& isCall_;
}

template <class Archive> void SecuritySpreadQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& securityID_;
}

template <class Archive> void BaseCorrelationQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& cdsIndexName_;
    ar& term_;
    ar& detachmentPoint_;
}

template <class Archive> void IndexCDSOptionQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& indexName_;
    ar& expiry_;
    ar& indexTerm_;
    ar& strike_;
}

template <class Archive> void CommoditySpotQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& commodityName_;
    ar& quoteCurrency_;
}

template <class Archive> void CommodityForwardQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& commodityName_;
    ar& quoteCurrency_;
    ar& expiryDate_;
    ar& tenor_;
    ar& startTenor_;
    ar& tenorBased_;
}

template <class Archive> void CommodityOptionQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& commodityName_;
    ar& quoteCurrency_;
    ar& expiry_;
    ar& strike_;
}

template <class Archive> void CorrelationQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& index1_;
    ar& index2_;
    ar& expiry_;
    ar& strike_;
}

template <class Archive> void CPRQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& securityID_;
}

template <class Archive> void BondPriceQuote::serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<MarketDatum>(*this);
    ar& securityID_;
}

template void MarketDatum::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void MarketDatum::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void MoneyMarketQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void MoneyMarketQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void FRAQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void FRAQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void ImmFraQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void ImmFraQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void SwapQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void SwapQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void ZeroQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void ZeroQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void DiscountQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void DiscountQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void MMFutureQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void MMFutureQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void OIFutureQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void OIFutureQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void BasisSwapQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void BasisSwapQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void BMASwapQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void BMASwapQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void CrossCcyBasisSwapQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void CrossCcyBasisSwapQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void CrossCcyFixFloatSwapQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void CrossCcyFixFloatSwapQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void CdsQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void CdsQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void HazardRateQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void HazardRateQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void RecoveryRateQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void RecoveryRateQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void SwaptionQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void SwaptionQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void SwaptionShiftQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void SwaptionShiftQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void BondOptionQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void BondOptionQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void BondOptionShiftQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void BondOptionShiftQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void CapFloorQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void CapFloorQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void CapFloorShiftQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void CapFloorShiftQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void FXSpotQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void FXSpotQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void FXForwardQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void FXForwardQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void FXOptionQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void FXOptionQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void ZcInflationSwapQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void ZcInflationSwapQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void InflationCapFloorQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void InflationCapFloorQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void ZcInflationCapFloorQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void ZcInflationCapFloorQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void YoYInflationSwapQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void YoYInflationSwapQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void YyInflationCapFloorQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void YyInflationCapFloorQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void SeasonalityQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void SeasonalityQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void EquitySpotQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void EquitySpotQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void EquityForwardQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void EquityForwardQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void EquityDividendYieldQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void EquityDividendYieldQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void EquityOptionQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void EquityOptionQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void SecuritySpreadQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void SecuritySpreadQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void BaseCorrelationQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void BaseCorrelationQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void IndexCDSOptionQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void IndexCDSOptionQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void CommoditySpotQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void CommoditySpotQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void CommodityForwardQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void CommodityForwardQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void CommodityOptionQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void CommodityOptionQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void CorrelationQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void CorrelationQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void CPRQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void CPRQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);
template void BondPriceQuote::serialize(boost::archive::binary_oarchive& ar, const unsigned int version);
template void BondPriceQuote::serialize(boost::archive::binary_iarchive& ar, const unsigned int version);

} // namespace data
} // namespace ore

BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::MoneyMarketQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::FRAQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::ImmFraQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::SwapQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::ZeroQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::DiscountQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::MMFutureQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::OIFutureQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::BasisSwapQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::BMASwapQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::CrossCcyBasisSwapQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::CrossCcyFixFloatSwapQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::CdsQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::HazardRateQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::RecoveryRateQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::SwaptionQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::SwaptionShiftQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::BondOptionQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::BondOptionShiftQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::CapFloorQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::CapFloorShiftQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::FXSpotQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::FXForwardQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::FXOptionQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::ZcInflationSwapQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::InflationCapFloorQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::ZcInflationCapFloorQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::YoYInflationSwapQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::YyInflationCapFloorQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::SeasonalityQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::EquitySpotQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::EquityForwardQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::EquityDividendYieldQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::EquityOptionQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::SecuritySpreadQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::BaseCorrelationQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::IndexCDSOptionQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::CommoditySpotQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::CommodityForwardQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::CommodityOptionQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::CorrelationQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::CPRQuote);
BOOST_CLASS_EXPORT_IMPLEMENT(ore::data::BondPriceQuote);
