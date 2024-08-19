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

#include <orea/cube/npvsensicube.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>

#include <ql/errors.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>

#include <vector>

#include <boost/bimap.hpp>
#include <ql/shared_ptr.hpp>

namespace ore {
namespace analytics {

//! SensitivityCube is a wrapper for an npvCube that gives easier access to the underlying cube elements
class SensitivityCube {

public:
    typedef std::pair<RiskFactorKey, RiskFactorKey> crossPair;
    typedef ShiftScenarioGenerator::ScenarioDescription ShiftScenarioDescription;

    struct FactorData {
        QuantLib::Size index = 0;
        QuantLib::Real targetShiftSize = 0.0;
        QuantLib::Real actualShiftSize = 0.0;
        RiskFactorKey rfkey;
        string factorDesc;
        bool operator<(const FactorData& fd) const { return index < fd.index; }
    };

    //! Constructor using a vector of scenario descriptions
    SensitivityCube(const QuantLib::ext::shared_ptr<NPVSensiCube>& cube,
                    const std::vector<ShiftScenarioDescription>& scenarioDescriptions,
                    const std::map<RiskFactorKey, QuantLib::Real>& targetShiftSizes,
                    const std::map<RiskFactorKey, QuantLib::Real>& actualShiftSizes,
                    const std::map<RiskFactorKey, ShiftScheme>& shiftSchemes);

    //! Constructor using a vector of scenario description strings
    SensitivityCube(const QuantLib::ext::shared_ptr<NPVSensiCube>& cube, const std::vector<std::string>& scenarioDescriptions,
                    const std::map<RiskFactorKey, QuantLib::Real>& targetShiftSizes,
                    const std::map<RiskFactorKey, QuantLib::Real>& actualshiftSizes,
                    const std::map<RiskFactorKey, ShiftScheme>& shiftSchemes);

    //! \name Inspectors
    //@{
    const QuantLib::ext::shared_ptr<NPVSensiCube>& npvCube() const { return cube_; }
    const std::vector<ShiftScenarioDescription>& scenarioDescriptions() const { return scenarioDescriptions_; }
    //@}

    //! Check if the cube has scenario NPVs for trade with ID \p tradeId
    bool hasTrade(const std::string& tradeId) const;

    //! Return the map of up trade id's to index in cube
    const std::map<std::string, QuantLib::Size>& tradeIdx() const { return cube_->idsAndIndexes(); };

    /*! Return factor for given up or down scenario index or None if given index is not an up or down scenario (to be reviewed) */
    RiskFactorKey upDownFactor(const Size index) const;

    /*! Return factor for given cross scenario index or None if given index is not a cross scenario (to be reviewed) */
    crossPair crossFactor(const Size crossIndex) const;

    //! Check if the cube has scenario NPVs for scenario with description \p scenarioDescription
    bool hasScenario(const ShiftScenarioDescription& scenarioDescription) const;

    //! Get the description for the risk factor key \p riskFactorKey
    //! Returns the result of ShiftScenarioGenerator::ScenarioDescription::factor1()
    std::string factorDescription(const RiskFactorKey& riskFactorKey) const;

    //! Returns the set of risk factor keys for which a delta and gamma can be calculated
    const std::set<RiskFactorKey>& factors() const;

    //! Return the map of up risk factors to its factor data
    const std::map<RiskFactorKey, SensitivityCube::FactorData>& upFactors() const { return upFactors_; };

    //! Return the map of down risk factors to its factor data
    const std::map<RiskFactorKey, SensitivityCube::FactorData>& downFactors() const { return downFactors_; };

    //! Return the factor data for an up shift of a rf key, if that does not exist for a down shift of the same rf key
    SensitivityCube::FactorData upThenDownFactorData(const RiskFactorKey& rfkey);

    //! Returns the set of pairs of risk factor keys for which a cross gamma is available
    const std::map<crossPair, std::tuple<SensitivityCube::FactorData, SensitivityCube::FactorData, QuantLib::Size>>&
    crossFactors() const;

    //! Returns the absolute target shift size for given risk factor \p key
    QuantLib::Real targetShiftSize(const RiskFactorKey& riskFactorKey) const;

    //! Returns the absolute actual shift size for given risk factor \p key
    QuantLib::Real actualShiftSize(const RiskFactorKey& riskFactorKey) const;

    //! Returns the shift scheme for given risk factor \p key
    ShiftScheme shiftScheme(const RiskFactorKey& riskFactorKey) const;

    //! Get the base NPV for trade with ID \p tradeId
    QuantLib::Real npv(const std::string& tradeId) const;

    //! Get the NPV for trade given the index of trade in the cube
    QuantLib::Real npv(QuantLib::Size id) const;

    //! Get the trade delta for trade with index \p tradeIdx and for the given risk factor key \p riskFactorKey
    QuantLib::Real delta(const Size tradeIdx, const RiskFactorKey& riskFactorKey) const;

    //! Get the trade delta for trade with ID \p tradeId and for the given risk factor key \p riskFactorKey
    QuantLib::Real delta(const std::string& tradeId, const RiskFactorKey& riskFactorKey) const;

    //! Get the trade gamma for trade with index \p tradeIdx and for the given risk factor key \p riskFactorKey
    QuantLib::Real gamma(const Size tradeIdx, const RiskFactorKey& riskFactorKey) const;

    //! Get the trade gamma for trade with ID \p tradeId and for the given risk factor key \p riskFactorKey
    QuantLib::Real gamma(const std::string& tradeId, const RiskFactorKey& riskFactorKey) const;

    //! Get the trade cross gamma for trade with ID \p tradeId and for the given risk factor key pair \p
    //! riskFactorKeyPair
    QuantLib::Real crossGamma(const Size tradeIdx, const crossPair& riskFactorKeyPair) const;

    //! Get the trade cross gamma for trade with ID \p tradeId and for the given risk factor key pair \p
    //! riskFactorKeyPair
    QuantLib::Real crossGamma(const std::string& tradeId, const crossPair& riskFactorKeyPair) const;

    //! Get the trade cross gamma for trade given the index of trade and risk factors in the cube
    QuantLib::Real crossGamma(QuantLib::Size tradeIdx, QuantLib::Size upIdx_1, QuantLib::Size upIdx_2,
                              QuantLib::Size crossId, QuantLib::Real scaling1, QuantLib::Real scaling2) const;

    //! Get the relevant risk factors
    std::set<RiskFactorKey> relevantRiskFactors() const;

private:
    //! Initialise method used by the constructors
    void initialise();

    QuantLib::ext::shared_ptr<NPVSensiCube> cube_;
    std::vector<ShiftScenarioDescription> scenarioDescriptions_;
    std::map<RiskFactorKey, QuantLib::Real> targetShiftSizes_;
    std::map<RiskFactorKey, QuantLib::Real> actualShiftSizes_;
    std::map<RiskFactorKey, ShiftScheme> shiftSchemes_;

    // Duplication between map keys below and these sets but trade-off
    // Means that we can return by reference in public inspector methods
    std::set<RiskFactorKey> factors_;

    // Maps for faster lookup of cube entries. They are populated in the constructor
    // TODO: Review this i.e. could it be done better / using less memory
    std::map<std::string, QuantLib::Size> tradeIdx_;
    std::map<ShiftScenarioDescription, QuantLib::Size> scenarioIdx_;

    std::map<RiskFactorKey, FactorData> upFactors_, downFactors_;
    // map of crossPair to tuple of (data of first \p RiskFactorKey, data of second \p RiskFactorKey, index of
    // crossFactor)
    std::map<crossPair, std::tuple<FactorData, FactorData, QuantLib::Size>> crossFactors_;

    // map of up / down / cross factor index to risk factor key
    std::map<QuantLib::Size, RiskFactorKey> upIndexToKey_;
    std::map<QuantLib::Size, RiskFactorKey> downIndexToKey_;
    std::map<QuantLib::Size, crossPair> crossIndexToKey_;

};

std::ostream& operator<<(std::ostream& out, const SensitivityCube::crossPair& cp);

} // namespace analytics
} // namespace ore

