/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file models/blackscholesmodelwrapper.hpp
    \brief wrapper around a vector of BS processes
    \ingroup utilities
*/

#pragma once

#include <ql/processes/blackscholesprocess.hpp>
#include <ql/processes/hestonprocess.hpp>
#include <ql/timegrid.hpp>

#include <qle/processes/ptdhestonprocess.hpp>

namespace QuantExt {

using namespace QuantLib;

struct AssetModelCalibrationResults {
    struct InstrumentResults {
        Period expiry;
        Real moneyness;
        Real strike;
        Real marketValue;
        Real modelValue;
        Real marketVol;
        Real modelVol;
    };
    std::string indexName;
    std::vector<std::pair<std::string,Real>> constantParameters;
    std::vector<std::pair<std::string,std::string>> piecewiseParameters;
    Real rmse;
    std::vector<InstrumentResults> data;

    void clear() {
        indexName = "";
        constantParameters.clear();
        piecewiseParameters.clear();
        rmse = 0.0;
        data.clear();
    }
};

/* This class acts as an intermediate class between the model builder and the script model. The motivation to have a
   builder and this wrapper at all is to filter notifications from the vol surfaces and curves so that a recalculations
   only happens when relevant market data has changed */
class AssetModelWrapper : public Observer, public Observable {
public:
  enum class ProcessType { None, BlackScholes, LocalVol, Heston, PiecewiseHeston };
    AssetModelWrapper();
    AssetModelWrapper(const ProcessType processType,
                      const std::vector<QuantLib::ext::shared_ptr<StochasticProcess>>& processes,
                      const std::set<Date>& effectiveSimulationDates, const TimeGrid& discretisationTimeGrid,
		      const std::vector<AssetModelCalibrationResults>& calibrationResults = std::vector<AssetModelCalibrationResults>());

    const std::vector<QuantLib::ext::shared_ptr<StochasticProcess>>& processes() const;
    const std::set<Date>& effectiveSimulationDates() const;
    const TimeGrid& discretisationTimeGrid() const;

    const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>&
    generalizedBlackScholesProcesses() const;
    const std::vector<QuantLib::ext::shared_ptr<HestonProcess>>& hestonProcesses() const;
    const std::vector<QuantLib::ext::shared_ptr<PiecewiseTimeDependentHestonProcess>>& ptdHestonProcesses() const;

    ProcessType processType() const;

    const std::vector<AssetModelCalibrationResults>& calibration() { return calibration_; }

private:
    void update() override;
    ProcessType processType_ = ProcessType::None;
    std::vector<QuantLib::ext::shared_ptr<StochasticProcess>> processes_;
    std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>> generalizedBlackScholesProcesses_;
    std::vector<QuantLib::ext::shared_ptr<HestonProcess>> hestonProcesses_;
    std::vector<QuantLib::ext::shared_ptr<PiecewiseTimeDependentHestonProcess>> ptdHestonProcesses_;
    std::set<Date> effectiveSimulationDates_;
    TimeGrid discretisationTimeGrid_;
    std::vector<AssetModelCalibrationResults> calibration_;
};

} // namespace QuantExt
