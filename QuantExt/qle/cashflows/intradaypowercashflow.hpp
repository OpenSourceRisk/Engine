/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file qle/cashflows/intradaypowercashflow.hpp
 */

#pragma once
#include <iostream>
#include <ql/cashflow.hpp>
#include <ql/patterns/visitor.hpp>
#include <ql/time/schedule.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/indexes/intradaypowerindex.hpp>

namespace QuantExt {

enum class IntradayPowerQuantityMode {
    TotalEnergy,
    LoadShapeMultiplier,
};

IntradayPowerQuantityMode parseIntradayPowerQuantityMode(const std::string& s);

std::ostream& operator<<(std::ostream& os, IntradayPowerQuantityMode cqf);

class IntradayPowerCashFlow : public QuantLib::CashFlow {

public:
    //! Constructor taking an explicit \p paymentDate
    IntradayPowerCashFlow(QuantLib::Real quantity, const QuantLib::Date& startDate, const QuantLib::Date& endDate,
                          const QuantLib::Date& paymentDate, const ext::shared_ptr<IntradayPowerIndex>& index,
                          const ext::shared_ptr<IntradayPowerLoadTermStructure> loadCurve = nullptr,
                          const QuantLib::Calendar& pricingCalendar = QuantLib::Calendar(), QuantLib::Real spread = 0.0,
                          QuantLib::Real gearing = 1.0, bool includeStartDate = false, bool includeEndDate = true,
                          bool businessDays = true, IntradayPowerQuantityMode quantityMode = IntradayPowerQuantityMode::TotalEnergy,
                          const ext::shared_ptr<FxIndex>& fxIndex = nullptr,
                          std::optional<QuantLib::Natural> avgPricePrecision = std::nullopt);

    const QuantLib::Date& startDate() const { return startDate_; }
    const QuantLib::Date& endDate() const { return endDate_; }
    const QuantLib::Calendar& pricingCalendar() const { return pricingCalendar_; }
    const QuantLib::Real spread() const { return spread_; }
    const QuantLib::Real gearing() const { return gearing_; }
    bool includeStartDate() const { return includeStartDate_; }
    bool includeEndDate() const { return includeEndDate_; }
    bool businessDays() const { return businessDays_; }
    const ext::shared_ptr<IntradayPowerLoadTermStructure>& loadCurve() const { return loadCurve_; }
    const ext::shared_ptr<FxIndex>& fxIndex() const { return fxIndex_; }
    std::optional<QuantLib::Natural> avgPricePrecision() const { return avgPricePrecision_; }
    const std::map<QuantLib::Date, QuantLib::Real>& weights() const { return weights_; }

    const std::vector<std::pair<QuantLib::Date, ext::shared_ptr<IntradayPowerIndex>>>& indices() const {
        return indices_;
    }

    QuantLib::Real periodQuantity() const { return periodQuantity_; }
    QuantLib::Real fixing() const;

    QuantLib::Date lastPricingDate() const { return indices_.empty() ? QuantLib::Date() : indices_.back().first; }

    QuantLib::Date date() const override { return paymentDate_; }

    //! \name CashFlow interface

    QuantLib::Real amount() const override;

    void accept(QuantLib::AcyclicVisitor& v) override;

private:
    void init(const QuantLib::Real quantity, const ext::shared_ptr<IntradayPowerIndex>& index);

    void computePeriodQuantity(const QuantLib::Real quantity);
    void rolloutIndices(const ext::shared_ptr<IntradayPowerIndex>& index);
    void initWeights();

    void performCalculations() const override;
    QuantLib::Date startDate_;
    QuantLib::Date endDate_;
    QuantLib::Date paymentDate_;
    ext::shared_ptr<IntradayPowerLoadTermStructure> loadCurve_;
    QuantLib::Calendar pricingCalendar_;
    QuantLib::Real spread_;
    QuantLib::Real gearing_;
    
    bool includeStartDate_;
    bool includeEndDate_;
    bool businessDays_;
    IntradayPowerQuantityMode quantityMode_;
    ext::shared_ptr<FxIndex> fxIndex_;
    std::optional<QuantLib::Natural> avgPricePrecision_;

    std::vector<std::pair<QuantLib::Date, ext::shared_ptr<IntradayPowerIndex>>> indices_;

    QuantLib::Real periodQuantity_;
    std::map<QuantLib::Date, QuantLib::Real> weights_;

    mutable QuantLib::Real averagePricePerMWh_;
    mutable QuantLib::Real amount_;
};

//! Helper class building a sequence of commodity indexed average cashflows
class IntradayPowerLeg {

public:
    IntradayPowerLeg(const QuantLib::Schedule& schedule, const ext::shared_ptr<IntradayPowerIndex>& index,
                     const ext::shared_ptr<IntradayPowerLoadTermStructure> loadCurve = nullptr);
    IntradayPowerLeg& withQuantities(QuantLib::Real quantity);
    IntradayPowerLeg& withQuantities(const std::vector<QuantLib::Real>& quantities);
    IntradayPowerLeg& withPaymentLag(QuantLib::Natural paymentLag);
    IntradayPowerLeg& withPaymentCalendar(const QuantLib::Calendar& paymentCalendar);
    IntradayPowerLeg& withPaymentConvention(QuantLib::BusinessDayConvention paymentConvention);
    IntradayPowerLeg& withPricingCalendar(const QuantLib::Calendar& pricingCalendar);
    IntradayPowerLeg& withSpreads(QuantLib::Real spread);
    IntradayPowerLeg& withSpreads(const std::vector<QuantLib::Real>& spreads);
    IntradayPowerLeg& withGearings(QuantLib::Real gearing);
    IntradayPowerLeg& withGearings(const std::vector<QuantLib::Real>& gearings);
    IntradayPowerLeg& includeEndDate(bool flag = true);
    IntradayPowerLeg& includeStartDate(bool flag = false);
    IntradayPowerLeg& useBusinessDays(bool flag = true);
    IntradayPowerLeg& withPaymentDates(const std::vector<QuantLib::Date>& paymentDates);
    IntradayPowerLeg& withFxIndex(const ext::shared_ptr<FxIndex>& fxIndex);
    IntradayPowerLeg& withQuantityMode(IntradayPowerQuantityMode quantityMode);
    IntradayPowerLeg& withAvgPricePrecision(std::optional<QuantLib::Natural> precision = std::nullopt);
    operator Leg() const;

private:
    Schedule schedule_;
    ext::shared_ptr<IntradayPowerIndex> index_;
    ext::shared_ptr<IntradayPowerLoadTermStructure> loadCurve_;
    std::vector<QuantLib::Real> quantities_;
    QuantLib::Natural paymentLag_;
    QuantLib::Calendar paymentCalendar_;
    QuantLib::BusinessDayConvention paymentConvention_;
    QuantLib::Calendar pricingCalendar_;
    std::vector<QuantLib::Real> spreads_;
    std::vector<QuantLib::Real> gearings_;
    bool includeEndDate_ = true;
    bool includeStartDate_ = false;
    bool businessDays_ = true;
    IntradayPowerQuantityMode quantityMode_ = IntradayPowerQuantityMode::TotalEnergy;
    std::vector<QuantLib::Date> paymentDates_;
    ext::shared_ptr<FxIndex> fxIndex_;
    std::optional<QuantLib::Natural> avgPricePrecision_ = std::nullopt;
};

} // namespace QuantExt
