/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef ored_curvespec_i
#define ored_curvespec_i

%{
using ore::data::CurveSpec;
using ore::data::YieldCurveSpec;
using ore::data::DefaultCurveSpec;
using ore::data::CDSVolatilityCurveSpec;
using ore::data::BaseCorrelationCurveSpec;
using ore::data::SwaptionVolatilityCurveSpec;
using ore::data::YieldVolatilityCurveSpec;
using ore::data::CapFloorVolatilityCurveSpec;
using ore::data::FXSpotSpec;
using ore::data::FXVolatilityCurveSpec;
using ore::data::InflationCurveSpec;
using ore::data::InflationCapFloorVolatilityCurveSpec;
using ore::data::EquityCurveSpec;
using ore::data::EquityVolatilityCurveSpec;
using ore::data::SecuritySpec;
using ore::data::CommodityCurveSpec;
using ore::data::CommodityVolatilityCurveSpec;
using ore::data::CorrelationCurveSpec;
using ore::data::parseCurveSpec;
using ore::data::parseCurveConfigurationType;
%}

%shared_ptr(CurveSpec)
class CurveSpec {
	public:
		enum class CurveType {
		    FX,
            Yield,
            CapFloorVolatility,
            SwaptionVolatility,
            YieldVolatility,
            FXVolatility,
            Default,
            CDSVolatility,
            Inflation,
            InflationCapFloorVolatility,
            Equity,
            EquityVolatility,
            Security,
            BaseCorrelation,
            Commodity,
            CommodityVolatility,
            Correlation
       };
       CurveSpec();
       CurveSpec(const std::string& curveConfigID);
       
       std::string name() const;
       const std::string& curveConfigID() const;
       std::string baseName() const;

       std::string subName() const = 0;
       CurveSpec::CurveType baseType() const = 0;

};

%shared_ptr(YieldCurveSpec)
class YieldCurveSpec : public CurveSpec {
    public:
        YieldCurveSpec(const std::string& ccy, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& ccy() const;
        std::string subName() const;
};


%shared_ptr(DefaultCurveSpec)
class DefaultCurveSpec  : public CurveSpec {
    public:
        DefaultCurveSpec(const std::string& ccy, const std::string& curveConfigID);
       
        CurveSpec::CurveType baseType() const;
        const std::string& ccy() const;
        std::string subName() const;
};


%shared_ptr(CDSVolatilityCurveSpec)
class CDSVolatilityCurveSpec  : public CurveSpec {
    public:
        CDSVolatilityCurveSpec(const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        std::string subName() const;

};

%shared_ptr(BaseCorrelationCurveSpec)
class BaseCorrelationCurveSpec  : public CurveSpec {
    public:
        BaseCorrelationCurveSpec(const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        std::string subName() const;

};

%shared_ptr(SwaptionVolatilityCurveSpec)
class SwaptionVolatilityCurveSpec  : public CurveSpec {
    public:
        SwaptionVolatilityCurveSpec(const std::string& key, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& key() const;
        std::string subName() const;

};

%shared_ptr(YieldVolatilityCurveSpec)
class YieldVolatilityCurveSpec  : public CurveSpec {
    public:
        YieldVolatilityCurveSpec(const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        std::string subName() const;

};

%shared_ptr(CapFloorVolatilityCurveSpec)
class CapFloorVolatilityCurveSpec  : public CurveSpec {
    public:
        CapFloorVolatilityCurveSpec(const std::string& key, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& key() const;
        std::string subName() const;

};

%shared_ptr(FXSpotSpec)
class FXSpotSpec  : public CurveSpec {
    public:
        FXSpotSpec(std::string unitCcy, std::string ccy);

        CurveSpec::CurveType baseType() const;
        const std::string& unitCcy() const;
        const std::string& ccy() const;
        std::string subName() const;

};


%shared_ptr(FXVolatilityCurveSpec)
class FXVolatilityCurveSpec  : public CurveSpec {
    public:
        FXVolatilityCurveSpec();
        FXVolatilityCurveSpec(const std::string& unitCcy, const std::string& ccy, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& unitCcy() const;
        const std::string& ccy() const;
        std::string subName() const;

};


%shared_ptr(InflationCurveSpec)
class InflationCurveSpec  : public CurveSpec {
    public:
        InflationCurveSpec(const std::string& index, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& index() const;
        std::string subName() const;

};


%shared_ptr(InflationCapFloorVolatilityCurveSpec)
class InflationCapFloorVolatilityCurveSpec  : public CurveSpec {
    public:
        InflationCapFloorVolatilityCurveSpec(const std::string& index, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& index() const;
        std::string subName() const;

};


%shared_ptr(EquityCurveSpec)
class EquityCurveSpec  : public CurveSpec {
    public:
        EquityCurveSpec(const std::string& ccy, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& ccy() const;
        std::string subName() const;

};


%shared_ptr(EquityVolatilityCurveSpec)
class EquityVolatilityCurveSpec  : public CurveSpec {
    public:
        EquityVolatilityCurveSpec(const std::string& ccy, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& ccy() const;
        std::string subName() const;

};



%shared_ptr(SecuritySpec)
class SecuritySpec  : public CurveSpec {
    public:
        SecuritySpec(const std::string& securityID);

        CurveSpec::CurveType baseType() const;
        std::string subName() const;
        const std::string& securityID() const;

};


%shared_ptr(CommodityCurveSpec)
class CommodityCurveSpec  : public CurveSpec {
    public:
        CommodityCurveSpec(const std::string& currency, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        std::string subName() const;
        const std::string& currency() const;

};


%shared_ptr(CommodityVolatilityCurveSpec)
class CommodityVolatilityCurveSpec  : public CurveSpec {
    public:
        CommodityVolatilityCurveSpec(const std::string& currency, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        std::string subName() const;
        const std::string& currency() const;

};


%shared_ptr(CorrelationCurveSpec)
class CorrelationCurveSpec  : public CurveSpec {
    public:
        CorrelationCurveSpec(const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        std::string subName() const;

};

ext::shared_ptr<CurveSpec> parseCurveSpec(const std::string& s);
CurveSpec::CurveType parseCurveConfigurationType(const std::string&);

#endif