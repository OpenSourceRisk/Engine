/*
 Copyright (C) 2021 Quaternion Risk Management Ltd.

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

#include <qle/instruments/convertiblebond2.hpp>

namespace QuantExt {

using namespace QuantLib;

ConvertibleBond2::ConvertibleBond2(Size settlementDays, const Calendar& calendar, const Date& issueDate,
                                   const Leg& coupons, const ExchangeableData& exchangeableData,
                                   const std::vector<CallabilityData>& callData, const MakeWholeData& makeWholeData,
                                   const std::vector<CallabilityData>& putData,
                                   const std::vector<ConversionRatioData>& conversionRatioData,
                                   const std::vector<ConversionData>& conversionData,
                                   const std::vector<MandatoryConversionData>& mandatoryConversionData,
                                   const std::vector<ConversionResetData>& conversionResetData,
                                   const std::vector<DividendProtectionData>& dividendProtectionData,
                                   const bool detachable, const bool perpetual)
    : Bond(settlementDays, calendar, issueDate, coupons), exchangeableData_(exchangeableData), callData_(callData),
      makeWholeData_(makeWholeData), putData_(putData), conversionData_(conversionData),
      conversionRatioData_(conversionRatioData), mandatoryConversionData_(mandatoryConversionData),
      conversionResetData_(conversionResetData), dividendProtectionData_(dividendProtectionData),
      detachable_(detachable), perpetual_(perpetual) {}

void ConvertibleBond2::setupArguments(PricingEngine::arguments* args) const {
    Bond::setupArguments(args);
    ConvertibleBond2::arguments* arguments = dynamic_cast<ConvertibleBond2::arguments*>(args);
    QL_REQUIRE(arguments != nullptr, "ConvertibleBond2::setupArguments(): wrong argument type");
    arguments->startDate = startDate();
    arguments->notionals = notionals();
    arguments->exchangeableData = exchangeableData_;
    arguments->callData = callData_;
    arguments->makeWholeData = makeWholeData_;
    arguments->putData = putData_;
    arguments->conversionRatioData = conversionRatioData_;
    arguments->conversionData = conversionData_;
    arguments->mandatoryConversionData = mandatoryConversionData_;
    arguments->conversionResetData = conversionResetData_;
    arguments->dividendProtectionData = dividendProtectionData_;
    arguments->detachable = detachable_;
    arguments->perpetual = perpetual_;
}

void ConvertibleBond2::fetchResults(const PricingEngine::results* r) const { Bond::fetchResults(r); }

void ConvertibleBond2::arguments::validate() const { Bond::arguments::validate(); }
void ConvertibleBond2::results::reset() { Bond::results::reset(); }

} // namespace QuantExt
