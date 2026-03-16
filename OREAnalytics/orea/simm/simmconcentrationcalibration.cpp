/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
 All rights reserved.
*/
#include <orea/simm/simmconcentrationcalibration.hpp>
#include <orea/simm/simmconfiguration.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;

using std::map;
using std::set;
using std::string;

namespace ore {
namespace analytics {

// Ease syntax
using RiskType = CrifRecord::RiskType;
using RiskClass = SimmConfiguration::RiskClass;
using IRFXConcentrationThresholds = SimmCalibration::RiskClassData::IRFXConcentrationThresholds;

SimmConcentrationCalibration::SimmConcentrationCalibration(const QuantLib::ext::shared_ptr<SimmCalibration>& simmCalibration,
                                                           const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper)
    : simmBucketMapper_(simmBucketMapper) {

    for (const auto& [riskClass, rcData] : simmCalibration->riskClassData()) {

        const auto& concThresholds = rcData->concentrationThresholds();

        // IR and FX currency lists
        if (riskClass == RiskClass::InterestRate || riskClass == RiskClass::FX) {
            const auto& irFxThresholds = QuantLib::ext::dynamic_pointer_cast<IRFXConcentrationThresholds>(concThresholds);
            QL_REQUIRE(irFxThresholds, "Cannot cast ConcentrationThresholds to IRFXConcentrationThresholds");
            const auto& ccyLists = irFxThresholds->currencyLists();

            auto& categories = riskClass == RiskClass::InterestRate ? irCategories_ : fxCategories_;
            for (const auto& [ccyKey, ccyList] : ccyLists) {
                const string& bucket = std::get<0>(ccyKey);
                for (const string& ccy : ccyList) {
                    if (ccy == "Other") {
                        categories[bucket].clear();
                    } else {
                        categories[bucket].insert(ccy);
                    }
                }
            }
        }

        const auto& delta = concThresholds->delta();
        const auto& vega = concThresholds->vega();

        const auto& [deltaRiskType, vegaRiskType] = SimmConfiguration::riskClassToRiskType(riskClass);

        // Delta concentration thresholds
        if (delta.size() == 1) {
            flatThresholds_[deltaRiskType] = ore::data::parseReal(delta.begin()->second);
        } else if (delta.size() > 1) {
            for (const auto& [key, dTh] : delta) {
                const string& bucket = std::get<0>(key);
                bucketedThresholds_[deltaRiskType][bucket] = ore::data::parseReal(dTh);
            }
        }

        // Vega concentration thresholds
        if (vega.size() == 1) {
            flatThresholds_[vegaRiskType] = ore::data::parseReal(vega.begin()->second);
        } else if (vega.size() > 1) {
            for (const auto& [key, vTh] : vega) {
                const string& bucket = std::get<0>(key);
                bucketedThresholds_[vegaRiskType][bucket] = ore::data::parseReal(vTh);
            }
        }
    }
}

Real SimmConcentrationCalibration::threshold(const RiskType& riskType, const string& qualifier) const {
    return thresholdImpl(simmBucketMapper_, riskType, qualifier);
}

} // namespace analytics
} // namespace ore
