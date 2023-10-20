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

/*! \file qle/instruments/convertiblebond2.hpp */

#pragma once

#include <ql/instruments/bond.hpp>
#include <ql/pricingengine.hpp>
#include <ql/time/daycounter.hpp>

namespace QuantExt {

using QuantLib::Bond;
using QuantLib::Calendar;
using QuantLib::Date;
using QuantLib::GenericEngine;
using QuantLib::Leg;
using QuantLib::PricingEngine;
using QuantLib::Real;
using QuantLib::Size;

class ConvertibleBond2 : public QuantLib::Bond {
public:
    class arguments;
    class results;
    class engine;

    struct ExchangeableData {
        bool isExchangeable;
        bool isSecured;
    };

    struct CallabilityData {
        enum class ExerciseType { OnThisDate, FromThisDateOn };
        enum class PriceType { Clean, Dirty };
        Date exerciseDate;
        ExerciseType exerciseType;
        Real price;
        PriceType priceType;
        bool includeAccrual;
        bool isSoft;
        Real softTriggerRatio;
    };

    struct MakeWholeData {
        struct CrIncreaseData {
            Real cap;
            std::vector<Real> stockPrices;
            std::vector<Date> effectiveDates;
            std::vector<std::vector<Real>> crIncrease;
        };
        boost::optional<CrIncreaseData> crIncreaseData;
    };

    struct ConversionRatioData {
        Date fromDate;
        Real conversionRatio;
    };

    struct ConversionData {
        enum class ExerciseType { OnThisDate, FromThisDateOn };
        Date exerciseDate;
        ExerciseType exerciseType;
        enum class CocoType { None, Spot, StartOfPeriod };
        CocoType cocoType;
        Real cocoBarrier;
    };

    struct MandatoryConversionData {
        Date exerciseDate;
        Real pepsUpperBarrier;
        Real pepsLowerBarrier;
        Real pepsUpperConversionRatio;
        Real pepsLowerConversionRatio;
    };

    struct ConversionResetData {
        enum class ReferenceType { InitialCP, CurrentCP };
        ReferenceType referenceType;
        Date resetDate;
        Real threshold;
        Real gearing, floor, globalFloor;
    };

    struct DividendProtectionData {
        enum class AdjustmentStyle { CrUpOnly, CrUpDown, CrUpOnly2, CrUpDown2, PassThroughUpOnly, PassThroughUpDown };
        enum class DividendType { Absolute, Relative };
        Date startDate;
        Date protectionDate;
        AdjustmentStyle adjustmentStyle;
        DividendType dividendType;
        Real threshold;
    };

    /* callData, putData, conversionData and conversionReset data must be sorted w.r.t. their event dates */
    ConvertibleBond2(Size settlementDays, const Calendar& calendar, const Date& issueDate, const Leg& coupons,
                     const ExchangeableData& exchangeableData = ExchangeableData{false, false},
                     const std::vector<CallabilityData>& callData = {}, const MakeWholeData& makeWholeData = {},
                     const std::vector<CallabilityData>& putData = {},
                     const std::vector<ConversionRatioData>& conversionRatioData = {},
                     const std::vector<ConversionData>& conversionData = {},
                     const std::vector<MandatoryConversionData>& mandatoryConversionData = {},
                     const std::vector<ConversionResetData>& conversionResetData = {},
                     const std::vector<DividendProtectionData>& dividendProtectionData = {},
                     const bool detachable = false, const bool perpetual = false);

private:
    void setupArguments(PricingEngine::arguments*) const override;
    void fetchResults(const PricingEngine::results*) const override;

    // convertible data
    ExchangeableData exchangeableData_;
    std::vector<CallabilityData> callData_;
    MakeWholeData makeWholeData_;
    std::vector<CallabilityData> putData_;
    std::vector<ConversionData> conversionData_;
    std::vector<ConversionRatioData> conversionRatioData_;
    std::vector<MandatoryConversionData> mandatoryConversionData_;
    std::vector<ConversionResetData> conversionResetData_;
    std::vector<DividendProtectionData> dividendProtectionData_;
    bool detachable_;
    bool perpetual_;
};

class ConvertibleBond2::arguments : public QuantLib::Bond::arguments {
public:
    void validate() const override;
    Date startDate;
    std::vector<Real> notionals;
    ExchangeableData exchangeableData;
    std::vector<CallabilityData> callData;
    MakeWholeData makeWholeData;
    std::vector<CallabilityData> putData;
    std::vector<ConversionData> conversionData;
    std::vector<ConversionRatioData> conversionRatioData;
    std::vector<MandatoryConversionData> mandatoryConversionData;
    std::vector<ConversionResetData> conversionResetData;
    std::vector<DividendProtectionData> dividendProtectionData;
    bool detachable;
    bool perpetual;
};

class ConvertibleBond2::results : public QuantLib::Bond::results {
public:
    void reset() override;
};

class ConvertibleBond2::engine : public GenericEngine<ConvertibleBond2::arguments, ConvertibleBond2::results> {};

} // namespace QuantExt
