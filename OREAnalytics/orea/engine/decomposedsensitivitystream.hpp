/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file orea/engine/Decomposedsensitivitystream.hpp
    \brief Class that wraps a sensitivity stream and decomposes equity/commodity and default risk records
 */

#pragma once

#include <orea/engine/sensitivitystream.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/configuration/curveconfigurations.hpp>

#include <fstream>
#include <set>
#include <string>

namespace ore {
namespace analytics {

//! Class that wraps a sensitivity stream and decompose default, equity and commodity risk records given weights
class DecomposedSensitivityStream : public SensitivityStream {
public:
    /*! Constructor providing the weights for the credit index decomposition and the ids and reference data used for 
    */
    DecomposedSensitivityStream(const boost::shared_ptr<SensitivityStream>& ss, std::map<std::string, std::map<std::string, double>> defaultRiskDecompositionWeights = {},
                                const std::set<std::string>& eqComDecompositionTradeIds = {}, const std::map<std::string, double>& currencyHedgedIndexQuantities = {},
                                const boost::shared_ptr<ore::data::ReferenceDataManager>& refDataManager = nullptr, const boost::shared_ptr<ore::data::CurveConfigurations>& curveConfigs = nullptr);
    //! Returns the next SensitivityRecord in the stream after filtering
    SensitivityRecord next() override;
    //! Resets the stream so that SensitivityRecord objects can be streamed again
    void reset() override;

private:
    struct EqComIndexDecompositionResults {
        std::map<std::string, double> equityDelta;
        std::map<std::string, double> fxRisk;
        std::string indexCurrency;
    };

    //! Decompose the record and add it to the internal storage;
    std::vector<SensitivityRecord> decompose(const SensitivityRecord& record) const;
    std::vector<SensitivityRecord> decomposeSurvivalProbability(const SensitivityRecord& record) const;
    std::vector<SensitivityRecord> decomposeEquityRisk(const SensitivityRecord& record) const ;
    std::vector<SensitivityRecord> decomposeCommodityRisk(const SensitivityRecord& record) const;

    EqComIndexDecompositionResults decomposeEqComIndexRisk(double delta,
                                                           const boost::shared_ptr<ore::data::IndexReferenceDatum>& ird,
                                                           ore::data::CurveSpec::CurveType curveType) const;

    std::vector<SensitivityRecord>
    createDecompositionRecords(const SensitivityRecord& sr, const EqComIndexDecompositionResults& decompResults) const;


    std::vector<SensitivityRecord> decomposedRecords_;
    std::vector<SensitivityRecord>::iterator itCurrent_;

    //! The underlying sensitivity stream that has been wrapped
    boost::shared_ptr<SensitivityStream> ss_;
    //! map of trade ids to the basket consituents with their resp. weights
    std::map<std::string, std::map<std::string, double>> defaultRiskDecompositionWeights_;
    //! list of trade id, for which a equity index decomposition should be applied
    std::set<std::string> eqComDecompositionTradeIds_;
    //! list of trade id, for which a commodity index decomposition should be applied
    std::map<std::string, double> currencyHedgedIndexQuantities_;
    //! refDataManager holding the equity and commodity index decomposition weights
    boost::shared_ptr<ore::data::ReferenceDataManager> refDataManager_;
    boost::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;

    bool decompose_;
};

} // namespace analytics
} // namespace ore
