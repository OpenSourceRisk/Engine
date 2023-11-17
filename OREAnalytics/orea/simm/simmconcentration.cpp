/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <orea/simm/simmconcentration.hpp>
#include <orea/simm/simmbucketmapper.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>

using ore::data::checkCurrency;
using QuantLib::Real;
using std::map;
using std::set;
using std::string;

namespace ore {
namespace analytics {

// Ease syntax
using RiskType = SimmConfiguration::RiskType;
using RiskClass = SimmConfiguration::RiskClass;
using IRFXConcentrationThresholds = SimmCalibration::RiskClassData::IRFXConcentrationThresholds;

//SimmConcentrationBase::SimmConcentrationBase(const boost::shared_ptr<SimmBucketMapper>& simmBucketMapper,
//                                             const boost::shared_ptr<SimmCalibration>& simmCalibration)
//    : simmBucketMapper_(simmBucketMapper) {
//
//    if (simmCalibration) {
//        for (const auto& [riskClass, rcData] : simmCalibration->riskClassData()) {
//
//            if (riskClass == RiskClass::InterestRate || riskClass == RiskClass::FX) {
//                const auto& irFxThresholds = boost::dynamic_pointer_cast<IRFXConcentrationThresholds>(rcData);
//                QL_REQUIRE(irFxThresholds, "Cannot cast ConcentrationThresholds to IRFXConcentrationThresholds");
//                const auto& ccyLists = irFxThresholds->currencyLists();
//                
//                auto& categories = riskClass == RiskClass::InterestRate ? irCategories_ : fxCategories_;
//                for (const auto& ccyAmount : ccyLists) {
//                    string ccy = ccyAmount->value();
//                    if (ccy == "Other") {
//                        categories[ccyAmount->bucket()].clear();
//                    } else {
//                        categories[ccyAmount->bucket()].insert(ccy);
//                    }
//                }
//            }
//
//            const auto& delta = rcData->concentrationThresholds()->delta();
//            const auto& vega = rcData->concentrationThresholds()->vega();
//            
//            RiskType deltaRiskType, vegaRiskType;
//            switch (riskClass) {
//            case (RiskClass::InterestRate):
//                deltaRiskType = RiskType::IRCurve;
//                vegaRiskType = RiskType::IRVol;
//                break;
//            case (RiskClass::CreditQualifying):
//                deltaRiskType = RiskType::CreditQ;
//                vegaRiskType = RiskType::CreditVol;
//                break;
//            case (RiskClass::CreditNonQualifying):
//                deltaRiskType = RiskType::CreditNonQ;
//                vegaRiskType = RiskType::CreditVolNonQ;
//                break;
//            case (RiskClass::Equity):
//                deltaRiskType = RiskType::Equity;
//                vegaRiskType = RiskType::EquityVol;
//                break;
//            case (RiskClass::Commodity):
//                deltaRiskType = RiskType::Commodity;
//                vegaRiskType = RiskType::CommodityVol;
//                break;
//            case (RiskClass::FX):
//                deltaRiskType = RiskType::FX;
//                vegaRiskType = RiskType::FXVol;
//                break;
//            default:
//                QL_FAIL("Unexpected risk class");
//            }
//
//            // Delta concentration thresholds
//            if (delta.size() == 1) {
//                flatThresholds_[deltaRiskType] = ore::data::parseReal(delta.front()->value());
//            } else if (delta.size() > 1) {
//                for (const auto& dTh : delta) {
//                    bucketedThresholds_[deltaRiskType][dTh->bucket()] = ore::data::parseReal(dTh->value());
//                }
//            }
//
//            // Vega concentration thresholds
//            if (vega.size() == 1) {
//                flatThresholds_[vegaRiskType] = ore::data::parseReal(vega.front()->value());
//            } else if (vega.size() > 1) {
//                for (const auto& vTh : vega) {
//                    bucketedThresholds_[vegaRiskType][vTh->bucket()] = ore::data::parseReal(vTh->value());
//                }
//            }
//        }
//    }
//}

Real SimmConcentrationBase::thresholdImpl(const boost::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                          const RiskType& riskType, const string& qualifier) const {

    // Deal with some specific cases first
    if (riskType == RiskType::IRCurve || riskType == RiskType::IRVol || riskType == RiskType::FX) {

        QL_REQUIRE(qualifier.size() == 3, "Expect the qualifier, " << qualifier << ", to be a valid currency code");
        QL_REQUIRE(checkCurrency(qualifier),
                   "The qualifier, " << qualifier << ", is not a supported currency code");
        auto cat = riskType == RiskType::FX ? category(qualifier, fxCategories_) : category(qualifier, irCategories_);

        return bucketedThresholds_.at(riskType).at(cat) * units_;
    }

    if (riskType == RiskType::FXVol) {
        return fxVolThreshold(qualifier) * units_;
    }

    // Check if the risk type's threshold is not bucket dependent and if so return the threshold
    if (flatThresholds_.count(riskType) > 0) {
        return flatThresholds_.at(riskType) * units_;
    }

    // Check if the risk type's threshold is bucket dependent and if so, find the bucket and return the threshold
    if (bucketedThresholds_.count(riskType) > 0) {
        string bucket = simmBucketMapper->bucket(riskType, qualifier);
        bucket = bucket == "residual" ? "Residual" : bucket;
        auto const& m = bucketedThresholds_.at(riskType);
        auto const& t = m.find(bucket);
        QL_REQUIRE(t != m.end(), "SimmConcentrationBase::thresholdImpl(): bucket '"
                                     << bucket << "' not found in bucketedThresholds for qualifier '" << qualifier
                                     << "' and risk type '" << riskType << "'");
        return t->second * units_;
    }

    // If we get to here, no threshold
    return QL_MAX_REAL;
}

string SimmConcentrationBase::category(const string& qualifier, const map<string, set<string>>& categories) const {
    string result;
    for (const auto& kv : categories) {
        if (kv.second.empty()) {
            result = kv.first;
        } else {
            if (kv.second.count(qualifier) > 0) {
                // Found qualifier in category so return it
                return kv.first;
            }
        }
    }

    // If we get here, result should hold category with empty set
    return result;
}

Real SimmConcentrationBase::fxVolThreshold(const string& fxPair) const {

    QL_REQUIRE(fxPair.size() == 6, "Expected '" << fxPair << "' to be a currency pair so it should be of length 6.");

    string ccy_1 = fxPair.substr(0, 3);
    QL_REQUIRE(checkCurrency(ccy_1),
               "First currency in pair " << fxPair << " (" << ccy_1 << ") is not a supported currency code");
    string ccy_2 = fxPair.substr(3);
    QL_REQUIRE(checkCurrency(ccy_2),
               "Second currency in pair " << fxPair << " (" << ccy_2 << ") is not a supported currency code");

    // Find category of currency 1 and currency 2
    string category_1 = category(ccy_1, fxCategories_);
    string category_2 = category(ccy_2, fxCategories_);

    if (category_1 == "1" && category_2 == "1") {
        // Both currencies in FX category 1
        return bucketedThresholds_.at(RiskType::FXVol).at("1");
    } else if ((category_1 == "1" && category_2 == "2") || (category_1 == "2" && category_2 == "1")) {
        // One currency in FX category 1 and the other in FX category 2
        return bucketedThresholds_.at(RiskType::FXVol).at("2");
    } else if ((category_1 == "1" && category_2 == "3") || (category_1 == "3" && category_2 == "1")) {
        // One currency in FX category 1 and the other in FX category 3
        return bucketedThresholds_.at(RiskType::FXVol).at("3");
    } else if (category_1 == "2" && category_2 == "2") {
        // Both currencies in FX category 2
        return bucketedThresholds_.at(RiskType::FXVol).at("4");
    } else if ((category_1 == "2" && category_2 == "3") || (category_1 == "3" && category_2 == "2")) {
        // One currency in FX category 2 and the other in FX category 3
        return bucketedThresholds_.at(RiskType::FXVol).at("5");
    } else {
        // Both currencies in FX category 3
        return bucketedThresholds_.at(RiskType::FXVol).at("6");
    }
}

} // namespace analytics
} // namespace ore
