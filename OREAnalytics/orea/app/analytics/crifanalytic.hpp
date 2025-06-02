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

/*! \file orea/app/analytics/crifanalytic.hpp
    \brief ORE CRIF Analytic
*/
#pragma once

#include <orea/app/analytic.hpp>
#include <orea/simm/crifmarket.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/additionalfieldgetter.hpp>

namespace ore {
namespace analytics {

std::pair<QuantLib::ext::shared_ptr<ore::analytics::SensitivityStream>,
          std::map<std::string, QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>>
computeSensitivities(QuantLib::ext::shared_ptr<ore::analytics::SensitivityAnalysis>& sensiAnalysis,
                     const QuantLib::ext::shared_ptr<InputParameters>& plusInputs, ore::analytics::Analytic* analytic,
                     const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio, const bool writeReports);

class CrifAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "CRIF";

    CrifAnalyticImpl(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs) : Analytic::Impl(inputs) {
        setLabel(LABEL);
    }
    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
};


class CrifAnalytic : public Analytic {
public:
    CrifAnalytic(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs,
                 const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager,
                 const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio = nullptr,
                 const std::string& baseCurrency = "");

    QuantLib::ext::shared_ptr<ore::analytics::Crif>& crif() { return crif_; }
    const std::string& baseCurrency() const { return baseCurrency_; }

    void setPortfolioNoSimmExemptions(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio) {
        portfolioNoSimmExemptions_ = portfolio;
    }
    const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolioNoSimmExemptions() const {
        return portfolioNoSimmExemptions_;
    }

    void setPortfolioSimmExemptions(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio) {
        portfolioSimmExemptions_ = portfolio;
    }
    const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolioSimmExemptions() const {
        return portfolioSimmExemptions_;
    }

    const set<CrifRecord::Regulation>& simmExemptionOverrides() const { return simmExemptionOverrides_; }

    //! Creates a CRIF from a sensitivity stream
    QuantLib::ext::shared_ptr<ore::analytics::Crif>
    computeCrif(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                const QuantLib::ext::shared_ptr<SensitivityStream>& sensiStream,
                const QuantLib::ext::shared_ptr<InputParameters>& inputs, 
                const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket,
                double usdSpot);
    
private:
    std::string baseCurrency_;
    QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolioNoSimmExemptions_;
    QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolioSimmExemptions_;
    set<CrifRecord::Regulation> simmExemptionOverrides_;
    QuantLib::ext::shared_ptr<ore::analytics::Crif> crif_;
};
  
} // namespace analytics
} // namespace ore
