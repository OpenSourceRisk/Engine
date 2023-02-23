/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file orea/cube/sensitivitycube.hpp
    \brief holds a grid of NPVs for a list of trades under various scenarios
    \ingroup Cube
*/

#pragma once

#include <boost/bimap.hpp>
#include <boost/shared_ptr.hpp>
#include <orea/cube/npvsensicube.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <ql/errors.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>
#include <vector>

namespace ore {
namespace analytics {

//! SensitivityCube is a wrapper for an npvCube that gives easier access to the underlying cube elements
class SensitivityCube {

public:
    typedef std::pair<RiskFactorKey, RiskFactorKey> crossPair;
    typedef ShiftScenarioGenerator::ScenarioDescription ShiftScenarioDescription;

    struct FactorData {
        FactorData() : index(0), shiftSize(0.0) {}

        QuantLib::Size index;
        QuantLib::Real shiftSize;
        string factorDesc;

        bool operator<(const FactorData& fd) const { return index < fd.index; }
    };

    //! Constructor using a vector of scenario descriptions
    SensitivityCube(const boost::shared_ptr<NPVSensiCube>& cube,
                    const std::vector<ShiftScenarioDescription>& scenarioDescriptions,
                    const std::map<RiskFactorKey, QuantLib::Real>& shiftSizes,
                    const std::set<RiskFactorKey::KeyType>& twoSidedDeltas = {});

    //! Constructor using a vector of scenario description strings
    SensitivityCube(const boost::shared_ptr<NPVSensiCube>& cube,
                    const std::vector<std::string>& scenarioDescriptions,
                    const std::map<RiskFactorKey, QuantLib::Real>& shiftSizes,
                    const std::set<RiskFactorKey::KeyType>& twoSidedDeltas = {});

    //! \name Inspectors
    //@{
    const boost::shared_ptr<NPVSensiCube>& npvCube() const { return cube_; }
    const std::vector<ShiftScenarioDescription>& scenarioDescriptions() const { return scenarioDescriptions_; }
    //@}

    //! Check if the cube has scenario NPVs for trade with ID \p tradeId
    bool hasTrade(const std::string& tradeId) const;

    //! Return the map of up trade id's to index in cube
    const std::map<std::string, QuantLib::Size>& tradeIdx() const { return cube_->idsAndIndexes(); };

    /*! Return factor for given up/down scenario index or None if given index
      is not an up/down scenario (to be reviewed) */
    RiskFactorKey upDownFactor(const Size upDownIndex) const;

    //! Check if the cube has scenario NPVs for scenario with description \p scenarioDescription
    bool hasScenario(const ShiftScenarioDescription& scenarioDescription) const;

    //! Get the description for the risk factor key \p riskFactorKey
    //! Returns the result of ShiftScenarioGenerator::ScenarioDescription::factor1()
    std::string factorDescription(const RiskFactorKey& riskFactorKey) const;

    //! Returns the set of risk factor keys for which a delta and gamma can be calculated
    const std::set<RiskFactorKey>& factors() const;

    //! Return the map of up risk factors to it's factor data
    const boost::bimap<RiskFactorKey, SensitivityCube::FactorData>& upFactors() const { return upFactors_; };

    //! Return the map of down risk factors to it's factor data
    const std::map<RiskFactorKey, SensitivityCube::FactorData>& downFactors() const { return downFactors_; };

    //! Returns the set of pairs of risk factor keys for which a cross gamma is available
    const std::map<crossPair, std::tuple<SensitivityCube::FactorData, SensitivityCube::FactorData, QuantLib::Size>>&
    crossFactors() const;

    //! Returns the absolute shift size for given risk factor \p key
    QuantLib::Real shiftSize(const RiskFactorKey& riskFactorKey) const;

    //! Get the base NPV for trade with ID \p tradeId
    QuantLib::Real npv(const std::string& tradeId) const;

    //! Get the NPV with scenario description \p scenarioDescription for trade with ID \p tradeId
    QuantLib::Real npv(const std::string& tradeId, const ShiftScenarioDescription& scenarioDescription) const;

    //! Get the NPV for trade given the index of trade in the cube
    QuantLib::Real npv(QuantLib::Size id) const;

    //! Get the NPV for trade given the index of trade and scenario in the cube
    QuantLib::Real npv(QuantLib::Size id, QuantLib::Size scenarioIdx) const;

    //! Get the trade delta for trade with ID \p tradeId and for the given risk factor key \p riskFactorKey
    QuantLib::Real delta(const std::string& tradeId, const RiskFactorKey& riskFactorKey) const;

    //! Get the trade delta for trade given the index of trade and risk factors in the cube
    QuantLib::Real delta(QuantLib::Size id, QuantLib::Size scenarioIdx) const;

    //! Get the two sided trade delta for trade given the index of trade and risk factors in the cube
    QuantLib::Real delta(QuantLib::Size id, QuantLib::Size upIdx, QuantLib::Size downIdx) const;

    //! Get the trade gamma for trade with ID \p tradeId and for the given risk factor key \p riskFactorKey
    QuantLib::Real gamma(const std::string& tradeId, const RiskFactorKey& riskFactorKey) const;

    //! Get the trade gamma for trade given the index of trade and risk factors in the cube
    QuantLib::Real gamma(QuantLib::Size id, QuantLib::Size upScenarioIdx, QuantLib::Size downScenarioIdx) const;

    //! Get the trade cross gamma for trade with ID \p tradeId and for the given risk factor key pair \p
    //! riskFactorKeyPair
    QuantLib::Real crossGamma(const std::string& tradeId, const crossPair& riskFactorKeyPair) const;

    //! Get the relevant risk factors
    std::set<RiskFactorKey> relevantRiskFactors();

    //! Get the trade cross gamma for trade given the index of trade and risk factors in the cube
    QuantLib::Real crossGamma(QuantLib::Size id, QuantLib::Size upIdx_1, QuantLib::Size upIdx_2,
                              QuantLib::Size crossIdx) const;

    //! Check if a two sided delta has been configured for the given risk factor key type, \p keyType.
    bool twoSidedDelta(const RiskFactorKey::KeyType& keyType) const;

private:
    //! Initialise method used by the constructors
    void initialise();

    boost::shared_ptr<NPVSensiCube> cube_;
    std::vector<ShiftScenarioDescription> scenarioDescriptions_;
    std::map<RiskFactorKey, QuantLib::Real> shiftSizes_;

    // Duplication between map keys below and these sets but trade-off
    // Means that we can return by reference in public inspector methods
    std::set<RiskFactorKey> factors_;

    // Maps for faster lookup of cube entries. They are populated in the constructor
    // TODO: Review this i.e. could it be done better / using less memory
    std::map<std::string, QuantLib::Size> tradeIdx_;
    std::map<ShiftScenarioDescription, QuantLib::Size> scenarioIdx_;

    // Suppress warnings from Boost concept_check.hpp (VS 16.9.0, Boost 1.72.0).
    // warning C4834: discarding return value of function with 'nodiscard' attribute
#pragma warning( push )
#pragma warning( disable : 4834 )
    boost::bimap<RiskFactorKey, FactorData> upFactors_;
#pragma warning( pop )

    std::map<RiskFactorKey, FactorData> downFactors_;
    // map of crossPair to tuple of (data of first \p RiskFactorKey, data of second \p RiskFactorKey, index of
    // crossFactor)
    std::map<crossPair, std::tuple<FactorData, FactorData, QuantLib::Size>> crossFactors_;

    // Set of risk factor key types where we want a two-sided delta calculation.
    std::set<RiskFactorKey::KeyType> twoSidedDeltas_;
};

std::ostream& operator<<(std::ostream& out, const SensitivityCube::crossPair& cp);

} // namespace analytics
} // namespace ore
