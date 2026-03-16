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

#include <qle/models/assetmodelwrapper.hpp>

#include <ql/processes/blackscholesprocess.hpp>

using namespace QuantLib;

namespace QuantExt {

AssetModelWrapper::AssetModelWrapper() {}

AssetModelWrapper::AssetModelWrapper(const ProcessType processType,
                                     const std::vector<QuantLib::ext::shared_ptr<StochasticProcess>>& processes,
                                     const std::set<Date>& effectiveSimulationDates,
                                     const TimeGrid& discretisationTimeGrid,
				     const std::vector<AssetModelCalibrationResults>& calibration)
    : processType_(processType), processes_(processes), effectiveSimulationDates_(effectiveSimulationDates),
      discretisationTimeGrid_(discretisationTimeGrid), calibration_(calibration) {

    for (auto const& p : processes_) {
        switch (processType_) {
        case ProcessType::BlackScholes:
        case ProcessType::LocalVol:
            generalizedBlackScholesProcesses_.push_back(
                QuantLib::ext::dynamic_pointer_cast<GeneralizedBlackScholesProcess>(p));
            break;
        case ProcessType::Heston:
            hestonProcesses_.push_back(QuantLib::ext::dynamic_pointer_cast<HestonProcess>(p));
            break;
        case ProcessType::PiecewiseHeston:
            ptdHestonProcesses_.push_back(QuantLib::ext::dynamic_pointer_cast<PiecewiseTimeDependentHestonProcess>(p));
            break;
        case ProcessType::None:
            break;
        }
    }
}

const std::vector<QuantLib::ext::shared_ptr<StochasticProcess>>& AssetModelWrapper::processes() const {
    return processes_;
}

const std::set<Date>& AssetModelWrapper::effectiveSimulationDates() const { return effectiveSimulationDates_; }

const TimeGrid& AssetModelWrapper::discretisationTimeGrid() const { return discretisationTimeGrid_; }

const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>&
AssetModelWrapper::generalizedBlackScholesProcesses() const {
    QL_REQUIRE(processType_ == ProcessType::BlackScholes || processType_ == ProcessType::LocalVol,
               "AssetModelWrapper::generalizedBlackScholesProcesses(): process type ("
                   << static_cast<int>(processType_) << ") is not BlackScholes or LocalVol");
    return generalizedBlackScholesProcesses_;
}

const std::vector<QuantLib::ext::shared_ptr<HestonProcess>>& AssetModelWrapper::hestonProcesses() const {
    QL_REQUIRE(processType_ == ProcessType::Heston, "AssetModelWrapper::hestonProcesses(): process type ("
                                                        << static_cast<int>(processType_) << ") is not Heston");
    return hestonProcesses_;
}

const std::vector<QuantLib::ext::shared_ptr<PiecewiseTimeDependentHestonProcess>>&
AssetModelWrapper::ptdHestonProcesses() const {
    QL_REQUIRE(processType_ == ProcessType::PiecewiseHeston, "AssetModelWrapper::ptdHestonProcesses(): process type ("
                                                                 << static_cast<int>(processType_)
                                                                 << ") is not PiecewiseHeston");
    return ptdHestonProcesses_;
}

void AssetModelWrapper::update() { notifyObservers(); }

AssetModelWrapper::ProcessType AssetModelWrapper::processType() const { return processType_; }

} // namespace QuantExt
