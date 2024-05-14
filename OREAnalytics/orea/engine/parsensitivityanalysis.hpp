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

/*! \file orea/engine/parsensitivityanalysis.hpp
    \brief Perfrom sensitivity analysis for a given portfolio
    \ingroup simulation
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/engine/parsensitivityinstrumentbuilder.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/sensitivityscenariogenerator.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/report/report.hpp>

#include <ql/instruments/inflationcapfloor.hpp>
#include <ql/math/matrixutilities/sparsematrix.hpp>

#include <boost/numeric/ublas/vector.hpp>

#include <map>
#include <set>
#include <tuple>

namespace ore {
namespace analytics {
using namespace std;
using namespace QuantLib;
using namespace ore::data;

//! Par Sensitivity Analysis
/*!
  This class adds par sensitivity conversion to the base class functionality
  \ingroup simulation
*/
class ParSensitivityAnalysis {
public:
    typedef std::map<std::pair<ore::analytics::RiskFactorKey, ore::analytics::RiskFactorKey>, Real> ParContainer;

    //! Constructor
    ParSensitivityAnalysis(const QuantLib::Date& asof,
                           const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams,
                           const ore::analytics::SensitivityScenarioData& sensitivityData,
                           const string& marketConfiguration = Market::defaultConfiguration,
                           const bool continueOnError = false,
                           const std::set<ore::analytics::RiskFactorKey::KeyType>& typesDisabled = {});

    virtual ~ParSensitivityAnalysis() {}

    //! Compute par instrument sensitivities
    void computeParInstrumentSensitivities(const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket);

    //! Return computed par sensitivities. Empty if they have not been computed yet.
    const ParContainer& parSensitivities() const { return parSensi_; }

    //! align pillars in scenario simulation market parameters with those of the par instruments
    void alignPillars();

    //! Returns true if risk factor type is applicable for par conversion
    static bool isParType(ore::analytics::RiskFactorKey::KeyType type);

    //! get / set the relevant scenarios (if empty, these are ignored)
    const std::set<ore::analytics::RiskFactorKey>& relevantRiskFactors() const { return relevantRiskFactors_; }
    std::set<ore::analytics::RiskFactorKey>& relevantRiskFactors() { return relevantRiskFactors_; }

    //! Return the zero rate and par rate absolute shift size for each risk factor key
    std::map<ore::analytics::RiskFactorKey, std::pair<QuantLib::Real, QuantLib::Real>> shiftSizes() const {
        return shiftSizes_;
    }

    /*! Disable par conversion for the given set of risk factor key types. May be called multiple times in order to
        add key types that should not be considered for par conversion.
    */
    void disable(const std::set<ore::analytics::RiskFactorKey::KeyType>& types);

    //! Return the set of key types disabled for this instance of ParSensitivityAnalysis.
    const std::set<ore::analytics::RiskFactorKey::KeyType>& typesDisabled() const { return typesDisabled_; }

    const ParSensitivityInstrumentBuilder::Instruments& parInstruments() const { return instruments_; }

private:
    //! Augment relevant risk factors
    void augmentRelevantRiskFactors();

    //! Populate `shiftSizes_` for \p key given the implied fair par rate \p parRate
    void populateShiftSizes(const ore::analytics::RiskFactorKey& key, QuantLib::Real parRate,
                            const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket);

    //! As of date for the calculation of the par sensitivities
    QuantLib::Date asof_;
    //! Simulation market parameters
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketParams_;
    //! Sensitivity data
    ore::analytics::SensitivityScenarioData sensitivityData_;
    //! sensitivity of par rates w.r.t. raw rate shifts (including optionlet/cap volatility)
    ParContainer parSensi_;
    ParSensitivityInstrumentBuilder::Instruments instruments_;

    std::string marketConfiguration_;
    bool continueOnError_;
    std::set<ore::analytics::RiskFactorKey> relevantRiskFactors_;

    static std::set<ore::analytics::RiskFactorKey::KeyType> parTypes_;

    //! Set of risk factor types disabled for this instance of ParSensitivityAnalysis.
    std::set<ore::analytics::RiskFactorKey::KeyType> typesDisabled_;

    /*! Store the zero rate and par rate absolute shift size for each risk factor key

        The first element in the pair is the zero or raw rate absolute shift size. The second element in the pair is
        the corresponding par rate absolute shift size.

        In the case of the zero rate shift being configured as `Absolute` in `sensitivityData_`, then the par rate
        absolute shift size is just equal to the configured zero rate shift size. In the case of the zero rate
        shift being configured as `Relative` in `sensitivityData_`, we take the fair implied par rate and multiply it
        by the configured relative zero rate shift size to give the par rate absolute shift size.
    */
    std::map<ore::analytics::RiskFactorKey, std::pair<QuantLib::Real, QuantLib::Real>> shiftSizes_;
};

//! ParSensitivityConverter class
/*!
  1) Build Jacobi matrix containing sensitivities of par rates (first index) w.r.t. zero shifts (second index)
  2) Convert zero rate and optionlet vol sensitivities into par rate/vol sensitivities

  Let: p_ij denote curve i's par instrument j (curve may be a discount or an index curve)
           z_kl denote curve k's zero shift l (curve may be a discount or an index curve)
           i,k run from 0 to I
           j,l run from 0 to J

      Organisation of the matrix:
             z_00 z_01 z_02 ... z_0J ... z_10 z_11 ... z_1J ... z_I0 z_I1 ... z_IJ
      p_00
      p_01
      p_02
      ...
      p_0J
      p_10
      p_11
      ...
      p_1J
      ...
      p_I0
      p_I1
      ...
      p_IJ

      The curve numbering starts with discount curves (by ccy), followed by index curves (by index name).
      The number J of par instruments respectively zero shifts can differ between discount and index curves.
      The number of zero shifts matches the number of par instruments.
      The Jacobi matrix is therefore quadratic by construction.
 */
class ParSensitivityConverter {
public:
    /*! Constructor where \p parSensitivities is the par rate sensitivities w.r.t. zero shifts \p shiftSizes gives
        the absolute zero and par shift sizes for each risk factor key.
    */
    ParSensitivityConverter(
        const ParSensitivityAnalysis::ParContainer& parSensitivities,
        const std::map<ore::analytics::RiskFactorKey, std::pair<QuantLib::Real, QuantLib::Real>>& shiftSizes);

    //! Inspectors
    //@{
    //! Return the set of raw, i.e. zero, risk factor keys
    //! The ordering in this set defines the order of the columns in the Jacobi matrix
    const std::set<ore::analytics::RiskFactorKey>& rawKeys() { return rawKeys_; }
    //! Return the set of par risk factor keys
    //! The ordering in this set defines the order of the rows in the Jacobi matrix
    const std::set<ore::analytics::RiskFactorKey>& parKeys() { return parKeys_; }
    //@}

    //! Takes an array of zero sensitivities and returns an array of par sensitivities
    /*! \param  zeroSensitivities array of zero sensitivities ordered according to rawKeys()

        \return array of par sensitivities ordered according to parKeys()

        \todo should we use a compressed_vector for the input / output parameter or both ?
    */
    boost::numeric::ublas::vector<Real>
    convertSensitivity(const boost::numeric::ublas::vector<Real>& zeroSensitivities);

    //! Write the inverse of the transposed Jacobian to the \p reportOut
    void writeConversionMatrix(ore::data::Report& reportOut) const;

    ParSensitivityAnalysis::ParContainer inverseJacobian() const { ParSensitivityAnalysis::ParContainer results;
        Size parIdx = 0;
        for (const auto& parKey : parKeys_) {
            Size rawIdx = 0;
            for (const auto& rawKey : rawKeys_) {
                results[{rawKey, parKey}] = jacobi_transp_inv_(parIdx, rawIdx);
                rawIdx++;
            }
            parIdx++;
        }
        return results;
    }

private:
    std::set<ore::analytics::RiskFactorKey> rawKeys_;
    std::set<ore::analytics::RiskFactorKey> parKeys_;
    // transposed inverse Jacobian, i.e. the matrix we use for the zero-par conversion effectively
    QuantLib::SparseMatrix jacobi_transp_inv_;
    //! Vector of absolute zero shift sizes
    boost::numeric::ublas::vector<QuantLib::Real> zeroShifts_;
    //! Vector of absolute par shift sizes
    boost::numeric::ublas::vector<QuantLib::Real> parShifts_;
};

//! Write par instrument sensitivity report
void writeParConversionMatrix(const ore::analytics::ParSensitivityAnalysis::ParContainer& parSensitivities,
                              ore::data::Report& reportOut);

} // namespace analytics
} // namespace ore
