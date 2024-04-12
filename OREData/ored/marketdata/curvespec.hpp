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

/*! \file ored/marketdata/curvespec.hpp
    \brief Curve requirements specification
    \ingroup curves
*/

#pragma once

#include <ql/shared_ptr.hpp>

#include <ostream>
#include <string>

namespace ore {
namespace data {
using std::string;

//! Curve Specification
/*!
  Base class for curve descriptions
  \ingroup curves
 */
class CurveSpec {
public:
    //! Supported curve types
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

    //! Default constructor
    CurveSpec() {}

    //! Constructor that takes an underlying CurveConfig id
    CurveSpec(const std::string& curveConfigID) : curveConfigID_(curveConfigID) {}

    //! Default destructor
    virtual ~CurveSpec() {}

    //! \name Interface
    //@{
    virtual CurveType baseType() const = 0;
    virtual string subName() const = 0;

    //! returns the unique curve name
    string name() const { return baseName() + "/" + subName(); }

    /*! Returns the id of the CurveConfig associated with the CurveSpec. If there is no CurveConfig associated
        with the CurveSpec, it returns the default empty string.
    */
    const std::string& curveConfigID() const { return curveConfigID_; }

    string baseName() const;
    //@}
private:
    //! The id of the CurveConfig associated with the CurveSpec, if any.
    std::string curveConfigID_;
};

//! Stream operator for CurveSpec
std::ostream& operator<<(std::ostream& os, const CurveSpec& spec);
//! Stream operator for CurveType
std::ostream& operator<<(std::ostream& os, const CurveSpec::CurveType& t);

//! Relational operators for CurveSpecs
//@{
bool operator<(const CurveSpec& lhs, const CurveSpec& rhs);
bool operator==(const CurveSpec& lhs, const CurveSpec& rhs);
bool operator<(const QuantLib::ext::shared_ptr<CurveSpec>& lhs, const QuantLib::ext::shared_ptr<CurveSpec>& rhs);
bool operator==(const QuantLib::ext::shared_ptr<CurveSpec>& lhs, const QuantLib::ext::shared_ptr<CurveSpec>& rhs);
//@}

//! Yield curve description
/*!  \ingroup curves
 */
class YieldCurveSpec : public CurveSpec {
public:
    //! \name Constructors
    //@{
    //! Detailed constructor
    YieldCurveSpec(const string& ccy, const string& curveConfigID) : CurveSpec(curveConfigID), ccy_(ccy) {}
    //! Default constructor
    YieldCurveSpec() {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const override { return CurveType::Yield; }
    const string& ccy() const { return ccy_; }
    string subName() const override { return ccy_ + "/" + curveConfigID(); }
    //@}

protected:
    string ccy_;
};

//! Default curve description
/*!  \ingroup curves
 */
class DefaultCurveSpec : public CurveSpec {
public:
    //! \name Constructors
    //@{
    //! Detailed constructor
    DefaultCurveSpec(const string& ccy, const string& curveConfigID) : CurveSpec(curveConfigID), ccy_(ccy) {}
    //! Default constructor
    DefaultCurveSpec() {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const override { return CurveType::Default; }
    const string& ccy() const { return ccy_; }
    string subName() const override { return ccy_ + "/" + curveConfigID(); }
    //@}

private:
    string ccy_;
};

//! CDS Volatility curve description
/*! \ingroup curves
 */
class CDSVolatilityCurveSpec : public CurveSpec {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CDSVolatilityCurveSpec() {}
    //! Detailed constructor
    CDSVolatilityCurveSpec(const string& curveConfigID) : CurveSpec(curveConfigID) {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const override { return CurveType::CDSVolatility; }
    string subName() const override { return curveConfigID(); }
    //@}
private:
    string ccy_;
    string curveConfigID_;
};

//! Base Correlation surface description
/*! \ingroup curves
 */
class BaseCorrelationCurveSpec : public CurveSpec {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    BaseCorrelationCurveSpec() {}
    //! Detailed constructor
    BaseCorrelationCurveSpec(const string& curveConfigID) : CurveSpec(curveConfigID) {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const override { return CurveType::BaseCorrelation; }
    string subName() const override { return curveConfigID(); }
    //@}
};

//! Swaption Volatility curve description
/*! \ingroup curves
 */
class SwaptionVolatilityCurveSpec : public CurveSpec {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    SwaptionVolatilityCurveSpec() {}
    //! Detailed constructor
    SwaptionVolatilityCurveSpec(const string& key, const string& curveConfigID) : CurveSpec(curveConfigID), key_(key) {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const override { return CurveType::SwaptionVolatility; }
    const string& key() const { return key_; }
    string subName() const override { return key() + "/" + curveConfigID(); }
    //@}
private:
    string key_;
};

//! Yield volatility curve description
/*! \ingroup curves
 */
class YieldVolatilityCurveSpec : public CurveSpec {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    YieldVolatilityCurveSpec() {}
    //! Detailed constructor
    YieldVolatilityCurveSpec(const string& curveConfigID) : CurveSpec(curveConfigID) {}
    //@}
    //! \name Inspectors
    //@{
    CurveType baseType() const override { return CurveType::YieldVolatility; }
    string subName() const override { return curveConfigID(); }
    //@}
};

//! Cap/Floor Volatility curve description
class CapFloorVolatilityCurveSpec : public CurveSpec {
public:
    CapFloorVolatilityCurveSpec() {}
    // key is an index name (Ibor, OIS) or a currency
    CapFloorVolatilityCurveSpec(const string& key, const string& curveConfigID) : CurveSpec(curveConfigID), key_(key) {}

    //! \name CurveSpec interface
    //@{
    CurveType baseType() const override { return CurveType::CapFloorVolatility; }
    string subName() const override { return key() + "/" + curveConfigID(); }
    //@}

    //! \name Inspectors
    //@{
    const string& key() const { return key_; }
    //@}

private:
    string key_;
};

//! FX Spot description
/*! \ingroup curves
 */
class FXSpotSpec : public CurveSpec {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    FXSpotSpec() {}
    //! Detailed constructor
    FXSpotSpec(string unitCcy, string ccy) : unitCcy_(unitCcy), ccy_(ccy) {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const override { return CurveType::FX; }
    const string& unitCcy() const { return unitCcy_; }
    const string& ccy() const { return ccy_; }
    string subName() const override { return unitCcy() + "/" + ccy(); }
    //@}
private:
    string unitCcy_;
    string ccy_;
};

//! FX Volatility curve description
/*! \ingroup curves
 */
class FXVolatilityCurveSpec : public CurveSpec {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    FXVolatilityCurveSpec() {}
    //! Detailed constructor
    FXVolatilityCurveSpec(const string& unitCcy, const string& ccy, const string& curveConfigID)
        : CurveSpec(curveConfigID), unitCcy_(unitCcy), ccy_(ccy) {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const override { return CurveType::FXVolatility; }
    const string& unitCcy() const { return unitCcy_; }
    const string& ccy() const { return ccy_; }
    string subName() const override { return unitCcy() + "/" + ccy() + "/" + curveConfigID(); }
    //@}
private:
    string unitCcy_;
    string ccy_;
};

//! Inflation curve description
/*! \ingroup curves
 */
class InflationCurveSpec : public CurveSpec {
public:
    InflationCurveSpec() {}
    InflationCurveSpec(const string& index, const string& curveConfigID) : CurveSpec(curveConfigID), index_(index) {}

    CurveType baseType() const override { return CurveType::Inflation; }
    const string& index() const { return index_; }

    string subName() const override { return index() + "/" + curveConfigID(); }

private:
    string index_;
};

//! Inflation cap floor volatility description
/*! \ingroup curves
 */
class InflationCapFloorVolatilityCurveSpec : public CurveSpec {
public:
    InflationCapFloorVolatilityCurveSpec() {}
    InflationCapFloorVolatilityCurveSpec(const string& index, const string& curveConfigID)
        : CurveSpec(curveConfigID), index_(index) {}

    CurveType baseType() const override { return CurveType::InflationCapFloorVolatility; }
    const string& index() const { return index_; }

    string subName() const override { return index() + "/" + curveConfigID(); }

private:
    string index_;
};

//! Equity curve description
/*!  \ingroup curves
 */
class EquityCurveSpec : public CurveSpec {

public:
    //! \name Constructors
    //@{
    //! Detailed constructor
    EquityCurveSpec(const string& ccy, const string& curveConfigID) : CurveSpec(curveConfigID), ccy_(ccy) {}
    //! Default constructor
    EquityCurveSpec() {}

    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const override { return CurveType::Equity; }
    const string& ccy() const { return ccy_; }
    string subName() const override { return ccy_ + "/" + curveConfigID(); }
    //@}

private:
    string ccy_;
};

//! Equity Volatility curve description
/*! \ingroup curves
 */
class EquityVolatilityCurveSpec : public CurveSpec {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    EquityVolatilityCurveSpec() {}
    //! Detailed constructor
    EquityVolatilityCurveSpec(const string& ccy, const string& curveConfigID) : CurveSpec(curveConfigID), ccy_(ccy) {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const override { return CurveType::EquityVolatility; }
    const string& ccy() const { return ccy_; }
    string subName() const override { return ccy() + "/" + curveConfigID(); }
    //@}
private:
    string ccy_;
};

//! Security description
class SecuritySpec : public CurveSpec {
public:
    SecuritySpec(const string& securityID) : CurveSpec(securityID), securityID_(securityID) {}
    //! Default constructor
    SecuritySpec() {}
    CurveType baseType() const override { return CurveType::Security; }
    string subName() const override { return securityID_; }
    const string& securityID() const { return securityID_; }
    //@}

protected:
    string securityID_;
};

//! Commodity curve description
/*! \ingroup curves
 */
class CommodityCurveSpec : public CurveSpec {

public:
    //! \name Constructors
    //@{
    //! Default constructor
    CommodityCurveSpec() {}

    //! Detailed constructor
    CommodityCurveSpec(const std::string& currency, const std::string& curveConfigID)
        : CurveSpec(curveConfigID), currency_(currency) {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const override { return CurveType::Commodity; }
    const std::string& currency() const { return currency_; }
    std::string subName() const override { return currency_ + "/" + curveConfigID(); }
    //@}

private:
    std::string currency_;
};

//! Commodity volatility description
/*! \ingroup curves
 */
class CommodityVolatilityCurveSpec : public CurveSpec {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CommodityVolatilityCurveSpec() {}

    //! Detailed constructor
    CommodityVolatilityCurveSpec(const std::string& currency, const std::string& curveConfigID)
        : CurveSpec(curveConfigID), currency_(currency) {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const override { return CurveType::CommodityVolatility; }
    const std::string& currency() const { return currency_; }
    std::string subName() const override { return currency_ + "/" + curveConfigID(); }
    //@}

private:
    std::string currency_;
    std::string curveConfigId_;
};

//! Correlation curve description
/*! \ingroup curves
 */
class CorrelationCurveSpec : public CurveSpec {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CorrelationCurveSpec() {}
    //! Detailed constructor
    CorrelationCurveSpec(const string& curveConfigID) : CurveSpec(curveConfigID) {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const override { return CurveType::Correlation; }
    string subName() const override { return curveConfigID(); }
    //@}
private:
};

} // namespace data
} // namespace ore
