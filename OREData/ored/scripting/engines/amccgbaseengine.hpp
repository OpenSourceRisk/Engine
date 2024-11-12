/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file amccgbaseengine.hpp
    \brief AMC CG base engine
    \ingroup engines
*/

#pragma once

#include <ored/scripting/engines/amccgpricingengine.hpp>
#include <ored/scripting/models/modelcg.hpp>

#include <ql/instruments/swap.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/exercise.hpp>

namespace ore {
namespace data {

using QuantLib::Real;
using QuantLib::Size;
using QuantLib::Null;

class AmcCgBaseEngine : public AmcCgPricingEngine {
public:
    AmcCgBaseEngine(const QuantLib::ext::shared_ptr<ModelCG>& modelCg,
                    const std::vector<QuantLib::Date>& simulationDates,
                    const std::vector<QuantLib::Date>& stickyCloseOutDates,
                    const bool recalibrateOnStickyCloseOutDates = false,
                    const bool reevaluateExerciseInStickyRun = false);
    std::string npvName() const override;
    void buildComputationGraph() const override;
    void calculate() const;

protected:
    QuantLib::ext::shared_ptr<ModelCG> modelCg_;
    std::vector<QuantLib::Date> simulationDates_;
    std::vector<QuantLib::Date> stickyCloseOutDates_;
    bool recalibrateOnStickyCloseOutDates_;
    bool reevaluateExerciseInStickyRun_;

    // input data from the derived pricing engines, to be set in these engines
    mutable std::vector<QuantLib::Leg> leg_;
    mutable std::vector<std::string> currency_;
    mutable std::vector<bool> payer_;
    mutable QuantLib::ext::shared_ptr<QuantLib::Exercise> exercise_; // may be empty, if underlying is the actual trade
    mutable QuantLib::Settlement::Type optionSettlement_ = QuantLib::Settlement::Physical;
    mutable bool includeSettlementDateFlows_ = false;

    // set by engine
    std::string npvName_;

private:
    static constexpr Real tinyTime = 1E-10;

    // data structure storing info needed to generate the amount for a cashflow
    struct CashflowInfo {
        Size legNo = Null<Size>(), cfNo = Null<Size>();
        Real payTime = Null<Real>();
        Real exIntoCriterionTime = Null<Real>();
        std::string payCcy;
        bool payer = false;
        std::size_t flowNode;
    };

    // convert a date to a time w.r.t. the valuation date
    Real time(const Date& d) const;

    // create the info for a given flow
    CashflowInfo createCashflowInfo(QuantLib::ext::shared_ptr<QuantLib::CashFlow> flow, const std::string& payCcy,
                                    bool payer, Size legNo, Size cfNo) const;
};

} // namespace data
} // namespace ore
