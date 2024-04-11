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
    \brief Curve configuration repository
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
    const ReportConfig& reportConfigCommVols() const { return reportConfigCommVols_; }
    const ReportConfig& reportConfigIrCapFloorVols() const { return reportConfigIrCapFloorVols_; }
    const ReportConfig& reportConfigIrSwaptionVols() const { return reportConfigIrSwaptionVols_; }

    bool hasYieldCurveConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<YieldCurveConfig> yieldCurveConfig(const string& curveID) const;

    bool hasFxVolCurveConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<FXVolatilityCurveConfig> fxVolCurveConfig(const string& curveID) const;

    bool hasSwaptionVolCurveConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<SwaptionVolatilityCurveConfig> swaptionVolCurveConfig(const string& curveID) const;

    bool hasYieldVolCurveConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<YieldVolatilityCurveConfig> yieldVolCurveConfig(const string& curveID) const;

    bool hasCapFloorVolCurveConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<CapFloorVolatilityCurveConfig> capFloorVolCurveConfig(const string& curveID) const;

    bool hasDefaultCurveConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<DefaultCurveConfig> defaultCurveConfig(const string& curveID) const;

    bool hasCdsVolCurveConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<CDSVolatilityCurveConfig> cdsVolCurveConfig(const string& curveID) const;

    bool hasBaseCorrelationCurveConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<BaseCorrelationCurveConfig> baseCorrelationCurveConfig(const string& curveID) const;

    bool hasInflationCurveConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<InflationCurveConfig> inflationCurveConfig(const string& curveID) const;

    bool hasInflationCapFloorVolCurveConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<InflationCapFloorVolatilityCurveConfig>
    inflationCapFloorVolCurveConfig(const string& curveID) const;

    bool hasEquityCurveConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<EquityCurveConfig> equityCurveConfig(const string& curveID) const;

    bool hasEquityVolCurveConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<EquityVolatilityCurveConfig> equityVolCurveConfig(const string& curveID) const;

    bool hasSecurityConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<SecurityConfig> securityConfig(const string& curveID) const;

    bool hasFxSpotConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<FXSpotConfig> fxSpotConfig(const string& curveID) const;

    bool hasCommodityCurveConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<CommodityCurveConfig> commodityCurveConfig(const std::string& curveID) const;

    bool hasCommodityVolatilityConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<CommodityVolatilityConfig> commodityVolatilityConfig(const std::string& curveID) const;

    bool hasCorrelationCurveConfig(const std::string& curveID) const;
    QuantLib::ext::shared_ptr<CorrelationCurveConfig> correlationCurveConfig(const std::string& curveID) const;

    QuantLib::ext::shared_ptr<CurveConfigurations>
    minimalCurveConfig(const QuantLib::ext::shared_ptr<TodaysMarketParameters> todaysMarketParams,
                       const std::set<std::string>& configurations = {""}) const;

    /*! Return the set of quotes that are required by the CurveConfig elements in CurveConfigurations.

        The set of quotes required by only those CurveConfig elements appearing
        in \p todaysMarketParams for the given configuration(s) is returned.
    */
    std::set<string> quotes(const QuantLib::ext::shared_ptr<TodaysMarketParameters> todaysMarketParams,
                            const std::set<std::string>& configurations = {""}) const;
    std::set<string> quotes() const;

    std::set<string> conventions(const QuantLib::ext::shared_ptr<TodaysMarketParameters> todaysMarketParams,
                                 const std::set<std::string>& configurations = {""}) const;
    std::set<string> conventions() const;

    /*! Return the Yields curves available */
    std::set<string> yieldCurveConfigIds();

    /*! Return all curve ids required by a given curve id of a given type */
    std::map<CurveSpec::CurveType, std::set<string>> requiredCurveIds(const CurveSpec::CurveType& type,
                                                                      const std::string& curveId) const;
    //@}

    void add(const CurveSpec::CurveType& type, const string& curveId, const QuantLib::ext::shared_ptr<CurveConfig>& config);    
    bool has(const CurveSpec::CurveType& type, const string& curveId) const;
    const QuantLib::ext::shared_ptr<CurveConfig>& get(const CurveSpec::CurveType& type, const string& curveId) const;
    void parseAll();

    /*! add curve configs from given container that are not present in this container */
    void addAdditionalCurveConfigs(const CurveConfigurations& c);

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

 private:
    ReportConfig reportConfigEqVols_;
    ReportConfig reportConfigFxVols_;
    ReportConfig reportConfigCommVols_;
    ReportConfig reportConfigIrCapFloorVols_;
    ReportConfig reportConfigIrSwaptionVols_;

    mutable std::map<CurveSpec::CurveType, std::map<std::string, QuantLib::ext::shared_ptr<CurveConfig>>> configs_;
    mutable std::map<CurveSpec::CurveType, std::map<std::string, std::string>> unparsed_;

    // utility function for parsing a node of name "parentName" and storing the result in the map
    void parseNode(const CurveSpec::CurveType& type, const string& curveId) const;
    
    // utility function for getting a child curve config node
    void getNode(XMLNode* node, const char* parentName, const char* childName);

    // add to XML doc
    void addNodes(XMLDocument& doc, XMLNode* parent, const char* nodeName) const;
};

class CurveConfigurationsManager {
public:
    CurveConfigurationsManager() {}

    // add a curve config, if no id provided it gets added as a default
    void add(const QuantLib::ext::shared_ptr<CurveConfigurations>& config, std::string id = std::string());
    const QuantLib::ext::shared_ptr<CurveConfigurations>& get(std::string id = std::string()) const;
    const bool has(std::string id = std::string()) const;
    const std::map<std::string, QuantLib::ext::shared_ptr<CurveConfigurations>>& curveConfigurations() const;
    const bool empty() const;

private:
    std::map<std::string, QuantLib::ext::shared_ptr<CurveConfigurations>> configs_;
};

} // namespace data
} // namespace ore
