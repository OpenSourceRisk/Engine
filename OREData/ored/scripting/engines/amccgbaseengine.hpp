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
#include <ored/scripting/models/model.hpp>
#include <ored/scripting/models/modelcg.hpp>

#include <ql/instruments/swap.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/exercise.hpp>

namespace ore {
namespace data {

using QuantLib::Date;
using QuantLib::Null;
using QuantLib::Real;
using QuantLib::Size;

class AmcCgBaseEngine : public AmcCgPricingEngine {
public:
    // non-amc use
    AmcCgBaseEngine(const QuantLib::ext::shared_ptr<ModelCG>& modelCg, const Model::Params& mcParams,
                    const double indicatorSmoothingForValues, const double indicatorSmoothingForDerivatives,
                    const double sqrtSmoothingForDerivatives, const bool useCachedSensis,
                    const bool useExternalComputeFramework, const bool useDoublePrecisionForExternalCalculation,
                    const bool generateAdditionalResults);
    // amc use
    AmcCgBaseEngine(const QuantLib::ext::shared_ptr<ModelCG>& modelCg,
                    const std::vector<QuantLib::Date>& simulationDates = {},
                    const bool reevaluateExerciseInStickyCloseOutDateRun = false);

    void buildComputationGraph(const bool stickyCloseOutDateRun = false,
                               std::vector<TradeExposure>* tradeExposure = nullptr,
                               TradeExposureMetaInfo* tradeExposureMetaInfo = nullptr) const override;
    void calculate() const;

protected:
    // input to this class via ctor
    QuantLib::ext::shared_ptr<ModelCG> modelCg_;
    bool amcEnabled_;
    // ... non-amc
    Model::Params mcParams_;
    double indicatorSmoothingForValues_;
    double indicatorSmoothingForDerivatives_;
    double sqrtSmoothingForDerivatives_;
    bool useCachedSensis_ = false;
    bool useExternalComputeFramework_ = false;
    bool useDoublePrecisionForExternalCalculation_ = false;
    bool generateAdditionalResults_ = false;
    // ... amc
    std::vector<QuantLib::Date> simulationDates_;
    bool reevaluateExerciseInStickyCloseOutDateRun_;

    // cg utilities
    mutable std::vector<RandomVariableOpNodeRequirements> opNodeRequirements_;
    mutable std::vector<RandomVariableOp> ops_;
    mutable std::vector<RandomVariableGrad> grads_;

    // input data from the derived pricing engines, to be set in these engines
    mutable std::vector<QuantLib::Leg> leg_;
    mutable std::vector<std::string> currency_;
    mutable std::vector<bool> payer_;
    mutable QuantLib::ext::shared_ptr<QuantLib::Exercise> exercise_; // may be empty, if underlying is the actual trade
    mutable QuantLib::Settlement::Type optionSettlement_ = QuantLib::Settlement::Physical;
    mutable std::vector<QuantLib::Date> cashSettlementDates_;
    mutable bool exerciseIntoIncludeSameDayFlows_ = false;

    // set from global settings
    mutable bool includeTodaysCashflows_ = true;
    mutable bool includeReferenceDateEvents_ = false;

    // set by engine
    mutable std::set<std::string> relevantCurrencies_;
    mutable std::size_t npv_ = 0;
    mutable double npvValue_ = Null<double>();

    // cached exercise indicators to be used in sticky close-out date run
    mutable std::vector<std::size_t> cachedExerciseIndicators_;

    // remaining state
    mutable std::size_t cgVersion_ = 0;
    mutable bool haveBaseValues_ = false;
    mutable double baseNpv_ = Null<double>();
    mutable std::vector<std::pair<std::size_t, double>> baseModelParams_;
    mutable std::vector<double> sensis_;

private:
    // data structure storing info needed to generate the amount for a cashflow
    struct CashflowInfo {
        Size legNo = Null<Size>(), cfNo = Null<Size>();
        Date payDate = Null<Date>();
        Date exIntoCriterionDate = Null<Date>();
        std::string payCcy;
        std::set<std::string> addCcys; // from index, fx linked etc.
        bool payer = false;
        std::size_t flowNode;
    };

    // create the info for a given flow
    CashflowInfo createCashflowInfo(QuantLib::ext::shared_ptr<QuantLib::CashFlow> flow, const std::string& payCcy,
                                    bool payer, Size legNo, Size cfNo) const;

    // create a regression model (i.e. an npv - node in the graph)
    std::size_t createRegressionModel(const std::size_t amount, const Date& d,
                                      const std::vector<CashflowInfo>& cashflowInfo,
                                      const std::function<bool(std::size_t)>& cashflowRelevant,
                                      const std::size_t filter) const;
};

} // namespace data
} // namespace ore
