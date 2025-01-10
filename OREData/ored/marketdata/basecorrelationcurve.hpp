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

/*! \file ored/marketdata/basecorrelationcurve.hpp
    \brief Wrapper class for building base correlation structures
    \ingroup curves
*/

#pragma once

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>

#include <qle/termstructures/credit/basecorrelationstructure.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ored/marketdata/defaultcurve.hpp>
#include <string_view>
#include <qle/termstructures/creditcurve.hpp>

namespace ore {
namespace data {

class ReferenceDataManager;

//! Wrapper class for building Base Correlation structures
//! \ingroup curves
class BaseCorrelationCurve {
public:

    

    BaseCorrelationCurve() {}
    BaseCorrelationCurve(
        QuantLib::Date asof, BaseCorrelationCurveSpec spec, const Loader& loader,
        const CurveConfigurations& curveConfigs,
        const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
        const std::map<std::string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves = {},
        const std::map<std::string, QuantLib::ext::shared_ptr<DefaultCurve>>& creditCurves = {},
        const std::map<std::string, std::string>& creditNameMapping = {});

    //! \name Inspectors
    //@{
    const BaseCorrelationCurveSpec& spec() const { return spec_; }
    //! Base Correlation term structure
    const QuantLib::ext::shared_ptr<QuantExt::BaseCorrelationTermStructure>& baseCorrelationTermStructure() const {
        return baseCorrelation_;
    }
    //@}
private:
    struct AdjustForLossResults {
        double indexFactor;
        std::vector<double> adjDetachmentPoints;
        std::vector<double> remainingWeights;
        std::vector<std::string> remainingNames;
    };

    struct QuoteData {
            struct LessSetKey {
                bool operator()(const Real& lhs, const Real& rhs) const {
                return !close_enough(lhs, rhs) && lhs < rhs;
                }
            };

            struct LessDataKey {
                bool operator()(pair<Period, Real> k_1, pair<Period, Real> k_2) const {
                    if (k_1.first != k_2.first)
                        return k_1.first < k_2.first;
                    else
                        return !close_enough(k_1.second, k_2.second) && k_1.second < k_2.second;
                }
            }; 
            std::set<Period> terms;        
            set<Real, LessSetKey> dps;
            map<pair<Period, Real>, Handle<Quote>, LessDataKey> data;
    };

    QuoteData loadQuotes(const QuantLib::Date& asof, const BaseCorrelationCurveConfig& config, const Loader& loader) const;

    

    void buildFromCorrelations(const BaseCorrelationCurveConfig& config, const QuoteData& qdata) const;
    void buildFromUpfronts(const Date& asof, const BaseCorrelationCurveConfig& config, const QuoteData& qdata) const;

    std::string creditCurveNameMapping(const std::string& name) const {
        const auto& it = creditNameMapping_.find(name);
        if (it != creditNameMapping_.end()) {
            return it->second;
        }
        return name;
    }

    ext::shared_ptr<QuantExt::CreditCurve> getDefaultProbCurveAndRecovery(const std::string& name) const {
        const auto& it = creditCurves_.find(name);
        if (it != creditCurves_.end() && it->second->creditCurve() != nullptr) {
            const auto& creditCurve = it->second->creditCurve();
            return creditCurve;
        }
        return nullptr;
    }

    BaseCorrelationCurveSpec spec_;
    std::map<std::string, QuantLib::ext::shared_ptr<YieldCurve>> yieldCurves_;
    std::map<std::string, QuantLib::ext::shared_ptr<DefaultCurve>> creditCurves_;
    std::map<std::string, std::string> creditNameMapping_;
    QuantLib::ext::shared_ptr<ReferenceDataManager> referenceData_;
    mutable QuantLib::ext::shared_ptr<QuantExt::BaseCorrelationTermStructure> baseCorrelation_;
    /*! Use the reference data to adjust the detachment points, \p detachPoints, for existing losses if requested.
    */
    AdjustForLossResults adjustForLosses(const std::vector<QuantLib::Real>& detachPoints) const;
};
} // namespace data
} // namespace ore
