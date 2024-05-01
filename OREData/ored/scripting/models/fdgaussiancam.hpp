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

/*! \file ored/scripting/models/fdgaussiancam.hpp
    \brief fd gaussian cross asset model for single underlying ir model
    \ingroup utilities
*/

#pragma once

#include <ored/scripting/models/amcmodel.hpp>
#include <ored/scripting/models/modelimpl.hpp>

#include <ored/model/crossassetmodelbuilder.hpp>

#include <qle/processes/crossassetstateprocess.hpp>
#include <qle/models/lgmbackwardsolver.hpp>

#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>

namespace ore {
namespace data {

class FdGaussianCam : public ModelImpl {
public:
    /* see GaussianCam, this is the FD variant for a single underlying IR model */
    FdGaussianCam(const Handle<CrossAssetModel>& cam, const std::string& currency,
                  const Handle<YieldTermStructure>& curve,
                  const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
                  const std::set<Date>& simulationDates, const Size stateGridPoints = 50,
                  const Size timeStepsPerYear = 24, const Real mesherEpsilon = 1E-4,
                  const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig());

    // Model interface implementation
    Type type() const override { return Type::FD; }
    const Date& referenceDate() const override;
    RandomVariable npv(const RandomVariable& amount, const Date& obsdate, const Filter& filter,
                       const boost::optional<long>& memSlot, const RandomVariable& addRegressor1,
                       const RandomVariable& addRegressor2) const override;
    RandomVariable fwdCompAvg(const bool isAvg, const std::string& index, const Date& obsdate, const Date& start,
                              const Date& end, const Real spread, const Real gearing, const Integer lookback,
                              const Natural rateCutoff, const Natural fixingDays, const bool includeSpread,
                              const Real cap, const Real floor, const bool nakedOption,
                              const bool localCapFloor) const override;
    Real extractT0Result(const RandomVariable& result) const override;
    // override to set time on result
    RandomVariable pay(const RandomVariable& amount, const Date& obsdate, const Date& paydate,
                       const std::string& currency) const override;
    // override to clear cache
    void releaseMemory() override;

private:
    // ModelImpl interface implementation
    void performCalculations() const override;
    RandomVariable getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                        const RandomVariable& barrier, const bool above) const override;
    RandomVariable getIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd = Null<Date>()) const override;
    RandomVariable getDiscount(const Size idx, const Date& s, const Date& t) const override;
    RandomVariable getNumeraire(const Date& s) const override;
    Real getFxSpot(const Size idx) const override;

    // input parameters
    Handle<CrossAssetModel> cam_;
    std::string currency_;
    Handle<YieldTermStructure> curve_;
    std::set<Date> simulationDates_;
    Size stateGridPoints_;
    Size timeStepsPerYear_;
    Real mesherEpsilon_;
    IborFallbackConfig iborFallbackConfig_;

    // computed values
    mutable Date referenceDate_;                        // the model reference date
    mutable std::set<Date> effectiveSimulationDates_;   // the dates effectively simulated (including today)
    mutable std::unique_ptr<LgmBackwardSolver> solver_; // the lgm solver

    // internal cache for ir index fixings
    mutable std::map<std::tuple<Size, Date, Date>, RandomVariable> irIndexValueCache_;
};

} // namespace data
} // namespace ore
