/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#ifndef ored_curveconfig_i
#define ored_curveconfig_i

%include ored_curvespec.i
%include ored_xmlutils.i

%{
using ore::data::CurveConfig;
using ore::data::CurveSpec;
%}

%shared_ptr(CurveConfig)
class CurveConfig : public XMLSerializable {
public:
    CurveConfig(const std::string& curveID, const std::string& curveDescription, const std::vector<std::string>& quotes = std::vector<std::string>());
    CurveConfig();

    const std::string& curveID() const;
    const std::string& curveDescription() const;
    std::string& curveID();
    std::string& curveDescription();    
    std::set<std::string> requiredCurveIds(const CurveSpec::CurveType& curveType);
    std::map<CurveSpec::CurveType, std::set<std::string>> requiredCurveIds();
    void setRequiredCurveIds(const CurveSpec::CurveType& curveType, const std::set<std::string>& ids);
    void setRequiredCurveIds(const std::map<CurveSpec::CurveType, std::set<std::string>>& ids);
    const std::vector<std::string>& quotes();
};

#endif