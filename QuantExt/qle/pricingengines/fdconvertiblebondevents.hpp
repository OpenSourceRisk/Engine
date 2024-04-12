/*
  Copyright (C) 2021 Quaternion Risk Management Ltd.
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

/*! \file qle/pricingengines/fdconvertiblebondevents.hpp */

#pragma once

#include <qle/instruments/convertiblebond2.hpp>

#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/fxindex.hpp>

#include <ql/math/array.hpp>
#include <ql/math/matrix.hpp>
#include <ql/timegrid.hpp>

namespace QuantExt {

using QuantLib::DayCounter;
using QuantLib::TimeGrid;

class FdConvertibleBondEvents {
public:
    // represents call and put rights
    struct CallData {
        Real price;
        ConvertibleBond2::CallabilityData::PriceType priceType;
        bool includeAccrual;
        bool isSoft;
        Real softTriggerRatio;
        // make whole result of cr increase as a function of stock price and current cr
        std::function<Real(Real, Real)> mwCr;
    };

    // represents voluntary conversion with coco barrier
    struct ConversionData {
        Real cocoBarrier;
    };

    // represents mandatory conversion
    struct MandatoryConversionData {
        Real pepsUpperBarrier;
        Real pepsLowerBarrier;
        Real pepsUpperConversionRatio;
        Real pepsLowerConversionRatio;
    };

    /* represents
       1) conversion ratio resets or
       2) dividend protection with conversion ratio adjustments or
       3) conversion ratio change if an event of type 1) or 2) preceeds this change, because
       in this case we need to handle it differently from a simple determinisitic cr change */
    struct ConversionResetData {

        // conversion ratio reset
        bool resetActive = false;
        ConvertibleBond2::ConversionResetData::ReferenceType reference; // initial or current CP
        Real gearing;                                                   // > 0
        Real floor, globalFloor;                                        // zero if not applicable
        Real threshold;                                                 // > 0

        // dividend protection with conversion ratio adjustment
        bool divProtActive = false;
        ConvertibleBond2::DividendProtectionData::AdjustmentStyle
            adjustmentStyle;                                                 // CrUpOnly, CrUpDown, CrUpOnly2, CrUpDown2
        ConvertibleBond2::DividendProtectionData::DividendType dividendType; // Absolute, Relative
        Real accruedHistoricalDividends;
        Size lastDividendProtectionTimeIndex;
        Real divThreshold; // > 0

        // reset of cr to specific value
        bool resetToSpecificValue = false;
        Real newCr;
    };

    // represents dividend protection with div pass through
    struct DividendPassThroughData {
        ConvertibleBond2::DividendProtectionData::AdjustmentStyle
            adjustmentStyle;                                                 // PassThroughUpOnly, PassThroughUpDown
        ConvertibleBond2::DividendProtectionData::DividendType dividendType; // Absolute, Relative (remove, not needed?)
        Real accruedHistoricalDividends;
        Size lastDividendProtectionTimeIndex;
        Real divThreshold;
    };

    FdConvertibleBondEvents(const Date& today, const DayCounter& dc, const Real N0,
                            const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equity,
                            const QuantLib::ext::shared_ptr<FxIndex>& fxConversion);

    // The intended workflow is as follows:

    // 1 register events describing the convertible bond features and cashflows
    void registerBondCashflow(const QuantLib::ext::shared_ptr<CashFlow>& c);
    void registerCall(const ConvertibleBond2::CallabilityData& c);
    void registerMakeWhole(const ConvertibleBond2::MakeWholeData& c);
    void registerPut(const ConvertibleBond2::CallabilityData& c);
    void registerConversionRatio(const ConvertibleBond2::ConversionRatioData& c);
    void registerConversion(const ConvertibleBond2::ConversionData& c);
    void registerMandatoryConversion(const ConvertibleBond2::MandatoryConversionData& c);
    void registerConversionReset(const ConvertibleBond2::ConversionResetData& c);
    void registerDividendProtection(const ConvertibleBond2::DividendProtectionData& c);

    // 2 get the times associated to the events, i.e. the mandatory times for the PDE grid
    const std::set<Real>& times() const;

    // 3 call finalise w.r.t. the desired time grid t_0, ..., t_n
    void finalise(const TimeGrid& grid);

    // 4 get event information per time index i for time t_i and event number j
    bool hasBondCashflow(const Size i) const;
    bool hasCall(const Size i) const;
    bool hasPut(const Size i) const;
    bool hasConversion(const Size i) const;
    bool hasMandatoryConversion(const Size i) const;
    bool hasContingentConversion(const Size i) const;
    bool hasNoConversionPlane(const Size i) const; // true => barrier check is done on next date in the past where false
    bool hasConversionReset(const Size i) const;   // due to conv reset or div prot with cr adj
    bool hasDividendPassThrough(const Size i) const;

    Real getBondCashflow(const Size i) const;
    Real getBondFinalRedemption(const Size i) const;
    const CallData& getCallData(const Size i) const;
    const CallData& getPutData(const Size i) const;
    const ConversionData& getConversionData(const Size i) const; // conv reset or div prot with cr adj
    const MandatoryConversionData& getMandatoryConversionData(const Size i) const;
    const ConversionResetData& getConversionResetData(const Size i) const;
    const DividendPassThroughData& getDividendPassThroughData(const Size i) const;

    bool hasStochasticConversionRatio(const Size i) const; // populated for all i
    Real getInitialConversionRatio() const;                // initial conv ratio, even if in the past
    Real getCurrentConversionRatio(const Size i) const;    // populated for all i
    Real getCurrentFxConversion(const Size i) const;       // populated for all i
    Date getAssociatedDate(const Size i) const;            // null if no date is associated

    const std::map<std::string, boost::any>& additionalResults() const { return additionalResults_; }

private:
    Date nextExerciseDate(const Date& d, const std::vector<ConvertibleBond2::CallabilityData>& data) const;
    Date nextConversionDate(const Date& d) const;

    Real time(const Date& d) const;

    void processBondCashflows();
    void processExerciseData(const std::vector<ConvertibleBond2::CallabilityData>& sourceData,
                             std::vector<bool>& targetFlags, std::vector<CallData>& targetData);
    void processMakeWholeData();
    void processConversionAndDivProtData();
    void processMandatoryConversionData();

    Date today_;
    DayCounter dc_;
    Real N0_;
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equity_;
    QuantLib::ext::shared_ptr<FxIndex> fxConversion_;

    std::set<Real> times_;
    TimeGrid grid_;
    bool finalised_ = false;

    Date lastRedemptionDate_;

    // the registered events (before finalise())
    std::vector<QuantLib::ext::shared_ptr<CashFlow>> registeredBondCashflows_;
    std::vector<ConvertibleBond2::CallabilityData> registeredCallData_, registeredPutData_;
    std::vector<ConvertibleBond2::ConversionRatioData> registeredConversionRatioData_;
    std::vector<ConvertibleBond2::ConversionData> registeredConversionData_;
    std::vector<ConvertibleBond2::MandatoryConversionData> registeredMandatoryConversionData_;
    std::vector<ConvertibleBond2::ConversionResetData> registeredConversionResetData_;
    std::vector<ConvertibleBond2::DividendProtectionData> registeredDividendProtectionData_;
    ConvertibleBond2::MakeWholeData registeredMakeWholeData_;

    // per time index i flags to indicate events
    std::vector<bool> hasBondCashflow_, hasCall_, hasPut_;
    std::vector<bool> hasConversion_, hasMandatoryConversion_, hasContingentConversion_, hasConversionInfoSet_;
    std::vector<bool> hasNoConversionPlane_, hasConversionReset_;
    std::vector<bool> hasDividendPassThrough_;

    // per time index the data associated to events
    std::vector<Real> bondCashflow_, bondFinalRedemption_;
    std::vector<CallData> callData_, putData_;
    std::vector<ConversionData> conversionData_;
    std::vector<MandatoryConversionData> mandatoryConversionData_;
    std::vector<ConversionResetData> conversionResetData_;
    std::vector<DividendPassThroughData> dividendPassThroughData_;

    std::vector<bool> stochasticConversionRatio_; // filled for all i
    Real initialConversionRatio_;
    std::vector<Real> currentConversionRatio_; // filled for all i
    std::vector<Real> currentFxConversion_;    // filled for all i
    std::vector<Date> associatedDate_;

    // additional results provided by the event processor
    std::map<std::string, boost::any> additionalResults_;

    // containers to store interpolation data for mw cr increases
    QuantLib::Array mw_cr_inc_x_, mw_cr_inc_y_;
    QuantLib::Matrix mw_cr_inc_z_;
};

} // namespace QuantExt
