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

%shared_ptr(ore::data::CurveSpec)
namespace ore {
namespace data {
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

} // namespace data
} // namespace ore

%shared_ptr(ore::data::YieldCurveSpec)
namespace ore {
namespace data {
class YieldCurveSpec : public CurveSpec {
    public:
        YieldCurveSpec(const std::string& ccy, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& ccy() const;
        std::string subName() const;
};

} // namespace data
} // namespace ore


%shared_ptr(ore::data::DefaultCurveSpec)
namespace ore {
namespace data {
class DefaultCurveSpec  : public CurveSpec {
    public:
        DefaultCurveSpec(const std::string& ccy, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& ccy() const;
        std::string subName() const;
};

} // namespace data
} // namespace ore


%shared_ptr(ore::data::CDSVolatilityCurveSpec)
namespace ore {
namespace data {
class CDSVolatilityCurveSpec  : public CurveSpec {
    public:
        CDSVolatilityCurveSpec(const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        std::string subName() const;

};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::BaseCorrelationCurveSpec)
namespace ore {
namespace data {
class BaseCorrelationCurveSpec  : public CurveSpec {
    public:
        BaseCorrelationCurveSpec(const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        std::string subName() const;

};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::SwaptionVolatilityCurveSpec)
namespace ore {
namespace data {
class SwaptionVolatilityCurveSpec  : public CurveSpec {
    public:
        SwaptionVolatilityCurveSpec(const std::string& key, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& key() const;
        std::string subName() const;

};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::YieldVolatilityCurveSpec)
namespace ore {
namespace data {
class YieldVolatilityCurveSpec  : public CurveSpec {
    public:
        YieldVolatilityCurveSpec(const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        std::string subName() const;

};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::CapFloorVolatilityCurveSpec)
namespace ore {
namespace data {
class CapFloorVolatilityCurveSpec  : public CurveSpec {
    public:
        CapFloorVolatilityCurveSpec(const std::string& key, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& key() const;
        std::string subName() const;

};

} // namespace data
} // namespace ore

%shared_ptr(ore::data::FXSpotSpec)
namespace ore {
namespace data {
class FXSpotSpec  : public CurveSpec {
    public:
        FXSpotSpec(std::string unitCcy, std::string ccy);

        CurveSpec::CurveType baseType() const;
        const std::string& unitCcy() const;
        const std::string& ccy() const;
        std::string subName() const;

};

} // namespace data
} // namespace ore


%shared_ptr(ore::data::FXVolatilityCurveSpec)
namespace ore {
namespace data {
class FXVolatilityCurveSpec  : public CurveSpec {
    public:
        FXVolatilityCurveSpec();
        FXVolatilityCurveSpec(const std::string& unitCcy, const std::string& ccy, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& unitCcy() const;
        const std::string& ccy() const;
        std::string subName() const;

};

} // namespace data
} // namespace ore


%shared_ptr(ore::data::InflationCurveSpec)
namespace ore {
namespace data {
class InflationCurveSpec  : public CurveSpec {
    public:
        InflationCurveSpec(const std::string& index, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& index() const;
        std::string subName() const;

};

} // namespace data
} // namespace ore


%shared_ptr(ore::data::InflationCapFloorVolatilityCurveSpec)
namespace ore {
namespace data {
class InflationCapFloorVolatilityCurveSpec  : public CurveSpec {
    public:
        InflationCapFloorVolatilityCurveSpec(const std::string& index, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& index() const;
        std::string subName() const;

};

} // namespace data
} // namespace ore


%shared_ptr(ore::data::EquityCurveSpec)
namespace ore {
namespace data {
class EquityCurveSpec  : public CurveSpec {
    public:
        EquityCurveSpec(const std::string& ccy, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& ccy() const;
        std::string subName() const;

};

} // namespace data
} // namespace ore


%shared_ptr(ore::data::EquityVolatilityCurveSpec)
namespace ore {
namespace data {
class EquityVolatilityCurveSpec  : public CurveSpec {
    public:
        EquityVolatilityCurveSpec(const std::string& ccy, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        const std::string& ccy() const;
        std::string subName() const;

};

} // namespace data
} // namespace ore



%shared_ptr(ore::data::SecuritySpec)
namespace ore {
namespace data {
class SecuritySpec  : public CurveSpec {
    public:
        SecuritySpec(const std::string& securityID);

        CurveSpec::CurveType baseType() const;
        std::string subName() const;
        const std::string& securityID() const;

};

} // namespace data
} // namespace ore


%shared_ptr(ore::data::CommodityCurveSpec)
namespace ore {
namespace data {
class CommodityCurveSpec  : public CurveSpec {
    public:
        CommodityCurveSpec(const std::string& currency, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        std::string subName() const;
        const std::string& currency() const;

};

} // namespace data
} // namespace ore


%shared_ptr(ore::data::CommodityVolatilityCurveSpec)
namespace ore {
namespace data {
class CommodityVolatilityCurveSpec  : public CurveSpec {
    public:
        CommodityVolatilityCurveSpec(const std::string& currency, const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        std::string subName() const;
        const std::string& currency() const;

};

} // namespace data
} // namespace ore


%shared_ptr(ore::data::CorrelationCurveSpec)
namespace ore {
namespace data {
class CorrelationCurveSpec  : public CurveSpec {
    public:
        CorrelationCurveSpec(const std::string& curveConfigID);

        CurveSpec::CurveType baseType() const;
        std::string subName() const;

};

    ext::shared_ptr<CurveSpec> parseCurveSpec(const std::string& s);
    CurveSpec::CurveType parseCurveConfigurationType(const std::string&);

    } // namespace data
    } // namespace ore

#endif
