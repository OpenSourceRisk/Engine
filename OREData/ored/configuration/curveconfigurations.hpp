/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/configuration/curveconfigurations.hpp
    \brief Curve configuration repoistory
    \ingroup configuration
*/

#pragma once

#include <ored/configuration/basecorrelationcurveconfig.hpp>
#include <ored/configuration/capfloorvolcurveconfig.hpp>
#include <ored/configuration/cdsvolcurveconfig.hpp>
#include <ored/configuration/commoditycurveconfig.hpp>
#include <ored/configuration/commodityvolcurveconfig.hpp>
#include <ored/configuration/correlationcurveconfig.hpp>
#include <ored/configuration/defaultcurveconfig.hpp>
#include <ored/configuration/equitycurveconfig.hpp>
#include <ored/configuration/equityvolcurveconfig.hpp>
#include <ored/configuration/fxspotconfig.hpp>
#include <ored/configuration/fxvolcurveconfig.hpp>
#include <ored/configuration/inflationcapfloorvolcurveconfig.hpp>
#include <ored/configuration/inflationcurveconfig.hpp>
#include <ored/configuration/securityconfig.hpp>
#include <ored/configuration/swaptionvolcurveconfig.hpp>
#include <ored/configuration/yieldcurveconfig.hpp>
#include <ored/configuration/yieldvolcurveconfig.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/utilities/xmlutils.hpp>

#include <typeindex>
#include <typeinfo>

namespace ore {
namespace data {
using ore::data::XMLNode;
using ore::data::XMLSerializable;

//! Container class for all Curve Configurations
/*!
  \ingroup configuration
*/
class CurveConfigurations : public XMLSerializable {
public:
    //! Default constructor
    CurveConfigurations() {}

    //! \name Setters and Getters
    //@{
    const ReportConfig& reportConfigEqVols() const { return reportConfigEqVols_; }
    const ReportConfig& reportConfigFxVols() const { return reportConfigFxVols_; }

    bool hasYieldCurveConfig(const std::string& curveID) const;
    boost::shared_ptr<YieldCurveConfig>& yieldCurveConfig(const string& curveID) { return yieldCurveConfigs_[curveID]; }
    const boost::shared_ptr<YieldCurveConfig>& yieldCurveConfig(const string& curveID) const;

    bool hasFxVolCurveConfig(const std::string& curveID) const;
    boost::shared_ptr<FXVolatilityCurveConfig>& fxVolCurveConfig(const string& curveID) {
        return fxVolCurveConfigs_[curveID];
    }
    const boost::shared_ptr<FXVolatilityCurveConfig>& fxVolCurveConfig(const string& curveID) const;

    bool hasSwaptionVolCurveConfig(const std::string& curveID) const;
    boost::shared_ptr<SwaptionVolatilityCurveConfig>& swaptionVolCurveConfig(const string& curveID) {
        return swaptionVolCurveConfigs_[curveID];
    }
    const boost::shared_ptr<SwaptionVolatilityCurveConfig>& swaptionVolCurveConfig(const string& curveID) const;

    bool hasYieldVolCurveConfig(const std::string& curveID) const;
    boost::shared_ptr<YieldVolatilityCurveConfig>& yieldVolCurveConfig(const string& curveID) {
        return yieldVolCurveConfigs_[curveID];
    }
    const boost::shared_ptr<YieldVolatilityCurveConfig>& yieldVolCurveConfig(const string& curveID) const;

    bool hasCapFloorVolCurveConfig(const std::string& curveID) const;
    boost::shared_ptr<CapFloorVolatilityCurveConfig>& capFloorVolCurveConfig(const string& curveID) {
        return capFloorVolCurveConfigs_[curveID];
    }
    const boost::shared_ptr<CapFloorVolatilityCurveConfig>& capFloorVolCurveConfig(const string& curveID) const;

    bool hasDefaultCurveConfig(const std::string& curveID) const;
    boost::shared_ptr<DefaultCurveConfig>& defaultCurveConfig(const string& curveID) {
        return defaultCurveConfigs_[curveID];
    }
    const boost::shared_ptr<DefaultCurveConfig>& defaultCurveConfig(const string& curveID) const;

    bool hasCdsVolCurveConfig(const std::string& curveID) const;
    boost::shared_ptr<CDSVolatilityCurveConfig>& cdsVolCurveConfig(const string& curveID) {
        return cdsVolCurveConfigs_[curveID];
    }
    const boost::shared_ptr<CDSVolatilityCurveConfig>& cdsVolCurveConfig(const string& curveID) const;

    bool hasBaseCorrelationCurveConfig(const std::string& curveID) const;
    boost::shared_ptr<BaseCorrelationCurveConfig>& baseCorrelationCurveConfig(const string& curveID) {
        return baseCorrelationCurveConfigs_[curveID];
    }
    const boost::shared_ptr<BaseCorrelationCurveConfig>& baseCorrelationCurveConfig(const string& curveID) const;

    bool hasInflationCurveConfig(const std::string& curveID) const;
    boost::shared_ptr<InflationCurveConfig>& inflationCurveConfig(const string& curveID) {
        return inflationCurveConfigs_[curveID];
    };
    const boost::shared_ptr<InflationCurveConfig>& inflationCurveConfig(const string& curveID) const;

    bool hasInflationCapFloorVolCurveConfig(const std::string& curveID) const;
    boost::shared_ptr<InflationCapFloorVolatilityCurveConfig>& inflationCapFloorVolCurveConfig(const string& curveID) {
        return inflationCapFloorVolCurveConfigs_[curveID];
    };
    const boost::shared_ptr<InflationCapFloorVolatilityCurveConfig>&
    inflationCapFloorVolCurveConfig(const string& curveID) const;

    bool hasEquityCurveConfig(const std::string& curveID) const;
    boost::shared_ptr<EquityCurveConfig>& equityCurveConfig(const string& curveID) {
        return equityCurveConfigs_[curveID];
    };
    const boost::shared_ptr<EquityCurveConfig>& equityCurveConfig(const string& curveID) const;

    bool hasEquityVolCurveConfig(const std::string& curveID) const;
    boost::shared_ptr<EquityVolatilityCurveConfig>& equityVolCurveConfig(const string& curveID) {
        return equityVolCurveConfigs_[curveID];
    };
    const boost::shared_ptr<EquityVolatilityCurveConfig>& equityVolCurveConfig(const string& curveID) const;

    bool hasSecurityConfig(const std::string& curveID) const;
    boost::shared_ptr<SecurityConfig>& securityConfig(const string& curveID) { return securityConfigs_[curveID]; };
    const boost::shared_ptr<SecurityConfig>& securityConfig(const string& curveID) const;

    bool hasFxSpotConfig(const std::string& curveID) const;
    boost::shared_ptr<FXSpotConfig>& fxSpotConfig(const string& curveID) { return fxSpotConfigs_[curveID]; };
    const boost::shared_ptr<FXSpotConfig>& fxSpotConfig(const string& curveID) const;

    bool hasCommodityCurveConfig(const std::string& curveID) const;
    boost::shared_ptr<CommodityCurveConfig>& commodityCurveConfig(const std::string& curveID) {
        return commodityCurveConfigs_[curveID];
    };
    const boost::shared_ptr<CommodityCurveConfig>& commodityCurveConfig(const std::string& curveID) const;

    bool hasCommodityVolatilityConfig(const std::string& curveID) const;
    boost::shared_ptr<CommodityVolatilityConfig>& commodityVolatilityConfig(const std::string& curveID) {
        return commodityVolatilityConfigs_[curveID];
    };
    const boost::shared_ptr<CommodityVolatilityConfig>& commodityVolatilityConfig(const std::string& curveID) const;

    bool hasCorrelationCurveConfig(const std::string& curveID) const;
    boost::shared_ptr<CorrelationCurveConfig>& correlationCurveConfig(const std::string& curveID) {
        return correlationCurveConfigs_[curveID];
    };
    const boost::shared_ptr<CorrelationCurveConfig>& correlationCurveConfig(const std::string& curveID) const;

    boost::shared_ptr<CurveConfigurations>
    minimalCurveConfig(const boost::shared_ptr<TodaysMarketParameters> todaysMarketParams,
                       const std::set<std::string>& configurations = {""}) const;

    /*! Return the set of quotes that are required by the CurveConfig elements in CurveConfigurations.

        The set of quotes required by only those CurveConfig elements appearing
        in \p todaysMarketParams for the given configuration(s) is returned.
    */
    std::set<string> quotes(const boost::shared_ptr<TodaysMarketParameters> todaysMarketParams,
                            const std::set<std::string>& configurations = {""}) const;
    std::set<string> quotes() const;

    std::set<string> conventions(const boost::shared_ptr<TodaysMarketParameters> todaysMarketParams,
                                 const std::set<std::string>& configurations = {""}) const;
    std::set<string> conventions() const;

    /*! Return the Yields curves available */
    std::set<string> yieldCurveConfigIds();

    /*! Return all curve ids required by a given curve id of a given type */
    std::map<CurveSpec::CurveType, std::set<string>> requiredCurveIds(const CurveSpec::CurveType& type,
                                                                      const std::string& curveId) const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    ReportConfig reportConfigEqVols_;
    ReportConfig reportConfigFxVols_;

    std::map<std::string, boost::shared_ptr<YieldCurveConfig>> yieldCurveConfigs_;
    std::map<std::string, boost::shared_ptr<FXVolatilityCurveConfig>> fxVolCurveConfigs_;
    std::map<std::string, boost::shared_ptr<SwaptionVolatilityCurveConfig>> swaptionVolCurveConfigs_;
    std::map<std::string, boost::shared_ptr<YieldVolatilityCurveConfig>> yieldVolCurveConfigs_;
    std::map<std::string, boost::shared_ptr<CapFloorVolatilityCurveConfig>> capFloorVolCurveConfigs_;
    std::map<std::string, boost::shared_ptr<DefaultCurveConfig>> defaultCurveConfigs_;
    std::map<std::string, boost::shared_ptr<CDSVolatilityCurveConfig>> cdsVolCurveConfigs_;
    std::map<std::string, boost::shared_ptr<BaseCorrelationCurveConfig>> baseCorrelationCurveConfigs_;
    std::map<std::string, boost::shared_ptr<InflationCurveConfig>> inflationCurveConfigs_;
    std::map<std::string, boost::shared_ptr<InflationCapFloorVolatilityCurveConfig>> inflationCapFloorVolCurveConfigs_;
    std::map<std::string, boost::shared_ptr<EquityCurveConfig>> equityCurveConfigs_;
    std::map<std::string, boost::shared_ptr<EquityVolatilityCurveConfig>> equityVolCurveConfigs_;
    std::map<std::string, boost::shared_ptr<SecurityConfig>> securityConfigs_;
    std::map<std::string, boost::shared_ptr<FXSpotConfig>> fxSpotConfigs_;
    std::map<std::string, boost::shared_ptr<CommodityCurveConfig>> commodityCurveConfigs_;
    std::map<std::string, boost::shared_ptr<CommodityVolatilityConfig>> commodityVolatilityConfigs_;
    std::map<std::string, boost::shared_ptr<CorrelationCurveConfig>> correlationCurveConfigs_;

    // utility function for parsing a node of name "parentName" and storing the result in the map
    template <class T>
    void parseNode(XMLNode* node, const char* parentName, const char* childName, map<string, boost::shared_ptr<T>>& m);

    // utility function for getting a value from the map storing the configs, throwing if it is not present
    template <class T>
    const boost::shared_ptr<T>& get(const string& id, const map<string, boost::shared_ptr<T>>& m) const;

    // stores errors (parentName, msg) during parsing for keys (T, curveId), T = YieldCurveConfig etc.
    std::map<std::pair<std::type_index, std::string>, std::pair<std::string, std::string>> parseErrors_;
};

} // namespace data
} // namespace ore
