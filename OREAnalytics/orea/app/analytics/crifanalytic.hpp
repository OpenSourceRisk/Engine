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
#include <orea/simm/crifrecord.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/additionalfieldgetter.hpp>
#include <ored/report/inmemoryreport.hpp>

namespace ore {
namespace analytics {

class InputParameters;
class Crif;

class CrifAnalyticBase {
public:
    virtual ~CrifAnalyticBase() = default;

    virtual QuantLib::ext::shared_ptr<ore::analytics::Crif>& crif() = 0;
    virtual const std::string& baseCurrency() const = 0;

    virtual void setPortfolioNoSimmExemptions(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio) = 0;
    virtual const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolioNoSimmExemptions() const = 0;

    virtual void setPortfolioSimmExemptions(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio) = 0;
    virtual const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolioSimmExemptions() const = 0;

    virtual const set<CrifRecord::Regulation>& simmExemptionOverrides() const = 0;

    virtual QuantLib::ext::shared_ptr<Crif>
    computeCrif(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                const QuantLib::ext::shared_ptr<SensitivityStream>& sensiStream,
                const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                bool isSimmOverrideExceptionPortfolio,
                const std::set<std::string>& removedTrades,
                const std::set<std::string>& modifiedTrades,
                const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket,
                const QuantLib::ext::shared_ptr<PortfolioFieldGetter>& fieldGetter,
                double usdSpot) = 0;
};

class CrifAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "CRIF";
    static constexpr const char* sensitivityLookUpKey = "SENSITIVITY";

    CrifAnalyticImpl(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs) : Analytic::Impl(inputs) {
        setLabel(LABEL);
    }
    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
    void buildDependencies() override;

protected:
    virtual void handlePreSimmExemptionsReports(CrifAnalyticBase& crifAnalytic,
                                                const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                                const std::string& marketConfig,
                                                const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& npvWithoutReport);
    virtual bool continueWithEmptyPortfolio(const CrifAnalyticBase& crifAnalytic) const;
    virtual void handlePostSimmExemptionsReports(CrifAnalyticBase& crifAnalytic,
                                                 const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                                 const std::string& marketConfig,
                                                 const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& npvWithReport,
                                                 const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& cfWithReport);
    virtual QuantLib::ext::shared_ptr<ore::data::Portfolio>
    buildSimmExemptionOverridePortfolio(CrifAnalyticBase& crifAnalytic,
                                        const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                        std::set<std::string>& removedTrades,
                                        std::set<std::string>& modifiedTrades);
    virtual QuantLib::ext::shared_ptr<SensitivityStream>
    extractParSensitivityStream(const QuantLib::ext::shared_ptr<Analytic>& sensiAnalytic,
                                const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio) const;
    virtual QuantLib::ext::shared_ptr<SensitivityStream>
    computeExtraSensitivityStream(CrifAnalyticBase& crifAnalytic,
                                  const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                  const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                  const QuantLib::ext::shared_ptr<Analytic>& sensiAnalytic,
                                  const QuantLib::ext::shared_ptr<ore::data::Portfolio>& simmOverridesPortfolio);
    virtual void handleMainSensitivityReports(CrifAnalyticBase& crifAnalytic,
                                              const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                              const QuantLib::ext::shared_ptr<Analytic>& sensiAnalytic);
    virtual QuantLib::ext::shared_ptr<PortfolioFieldGetter>
    buildPortfolioFieldGetter(CrifAnalyticBase& crifAnalytic,
                              const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                              const QuantLib::ext::shared_ptr<ore::data::Portfolio>& simmOverridesPortfolio);
    virtual void extendCrif(CrifAnalyticBase& crifAnalytic,
                            const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                            const QuantLib::ext::shared_ptr<Crif>& crif,
                            const QuantLib::ext::shared_ptr<ore::data::Portfolio>& simmOverridesPortfolio,
                            const QuantLib::ext::shared_ptr<SensitivityStream>& ssSimmOverrides,
                            const std::set<std::string>& removedTrades,
                            const std::set<std::string>& modifiedTrades,
                            const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket,
                            const QuantLib::ext::shared_ptr<PortfolioFieldGetter>& fieldGetter,
                            double usdSpot);
    virtual void writeCrifReport(CrifAnalyticBase& crifAnalytic,
                                 const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                 const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& crifReport,
                                 const QuantLib::ext::shared_ptr<Crif>& crif,
                                 const QuantLib::ext::shared_ptr<PortfolioFieldGetter>& fieldGetter);

    bool applySimmExemptions_ = true;
};


class CrifAnalytic : public Analytic, public CrifAnalyticBase {
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
    QuantLib::ext::shared_ptr<Crif>
    computeCrif(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                const QuantLib::ext::shared_ptr<SensitivityStream>& sensiStream,
                const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                bool isSimmOverrideExceptionPortfolio,
                const std::set<std::string>& removedTrades,
                const std::set<std::string>& modifiedTrades,
                const QuantLib::ext::shared_ptr<CrifMarket>& crifMarket,
                const QuantLib::ext::shared_ptr<PortfolioFieldGetter>& fieldGetter,
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
