/*
  Copyright (C) 2025 Quaternion Risk Management Ltd.
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

/*! \file qle/pricingengines/fdcallablebondevents.hpp */

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

class FdCallableBondEvents {
public:
    // represents call and put rights
    struct CallData {
        Real price;
        CallableBond::CallabilityData::PriceType priceType;
        bool includeAccrual;
    };

    FdCallableBondEvents(const Date& today, const DayCounter& dc, const Real N0);

    // The intended workflow is as follows:

    // 1 register events describing the callable bond features and cashflows
    void registerBondCashflow(const QuantLib::ext::shared_ptr<NumericLgmMultiionEngineBase::CashflowInfo>& c);
    void registerCall(const ConvertibleBond2::CallabilityData& c);
    void registerPut(const ConvertibleBond2::CallabilityData& c);

    // 2 get the times associated to the events, i.e. the mandatory times for the PDE grid
    const std::set<Real>& times() const;

    // 3 call finalise w.r.t. the desired time grid t_0, ..., t_n
    void finalise(const TimeGrid& grid);

    // 4 get event information per time index i for time t_i and event number j
    bool hasBondCashflow(const Size i) const;
    bool hasCall(const Size i) const;
    bool hasPut(const Size i) const;

    std::vector<QuantLib::ext::shared_ptr<NumericLgmMultiLegOptionEngineBase::CashflowInfo>>
    getBondCashflow(const Size i) const;
    std::vector<QuantLib::ext::shared_ptr<NumericLgmMultiLegOptionEngineBase::CashflowInfo>>
    getBondFinalRedemption(const Size i) const;
    const CallData& getCallData(const Size i) const;
    const CallData& getPutData(const Size i) const;

    Date getAssociatedDate(const Size i) const; // null if no date is associated

    const std::map<std::string, boost::any>& additionalResults() const { return additionalResults_; }

private:
    Date nextExerciseDate(const Date& d, const std::vector<CallableBond::CallabilityData>& data) const;

    Real time(const Date& d) const;

    void processBondCashflows();
    void processExerciseData(const std::vector<ConvertibleBond2::CallabilityData>& sourceData,
                             std::vector<bool>& targetFlags, std::vector<CallData>& targetData);

    Date today_;
    DayCounter dc_;
    Real N0_;

    std::set<Real> times_;
    TimeGrid grid_;
    bool finalised_ = false;

    Date lastRedemptionDate_;

    // the registered events (before finalise())
    std::vector<const QuantLib::ext::shared_ptr<NumericLgmMultiionEngineBase::CashflowInfo>> registeredBondCashflows_;
    std::vector<ConvertibleBond2::CallabilityData> registeredCallData_, registeredPutData_;

    // per time index i flags to indicate events
    std::vector<bool> hasBondCashflow_, hasCall_, hasPut_;

    // per time index the data associated to events
    std::vector<QuantLib::ext::shared_ptr<NumericLgmMultiLegOptionEngineBase::CashflowInfo>> bondCashflow_,
        bondFinalRedemption_;
    std::vector<CallData> callData_, putData_;

    std::vector<Date> associatedDate_;

    // additional results provided by the event processor
    std::map<std::string, boost::any> additionalResults_;
};

} // namespace QuantExt
