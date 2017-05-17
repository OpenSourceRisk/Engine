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

#include <boost/shared_ptr.hpp>
#include <ostream>
#include <string>

using std::string;

namespace ore {
namespace data {

//! Curve Specification
/*!
  Base class for curve descriptions
  \ingroup curves
 */
class CurveSpec {
public:
    //! Supported curve types
    enum class CurveType { Yield, CapFloorVolatility, SwaptionVolatility, FX, FXVolatility, Default };
    //! Default destructor
    virtual ~CurveSpec() {}

    //! \name Interface
    //@{
    virtual CurveType baseType() const = 0;
    virtual string subName() const = 0;

    //! returns the unique curve name
    string name() const { return baseName() + "/" + subName(); }

    string baseName() const {
        switch (baseType()) {
        case CurveType::Yield:
            return "Yield";
        case CurveType::CapFloorVolatility:
            return "CapFloorVolatility";
        case CurveType::SwaptionVolatility:
            return "SwaptionVolatility";
        case CurveType::FX:
            return "FX";
        case CurveType::FXVolatility:
            return "FXVolatility";
        case CurveType::Default:
            return "Default";
        default:
            return "N/A";
        }
    }
    //@}
};

//! Stream operator
std::ostream& operator<<(std::ostream& os, const CurveSpec& spec);

//! Relational operators for CurveSpecs
//@{
bool operator<(const CurveSpec& lhs, const CurveSpec& rhs);
bool operator==(const CurveSpec& lhs, const CurveSpec& rhs);
bool operator<(const boost::shared_ptr<CurveSpec>& lhs, const boost::shared_ptr<CurveSpec>& rhs);
bool operator==(const boost::shared_ptr<CurveSpec>& lhs, const boost::shared_ptr<CurveSpec>& rhs);
//@}

//! Yield curve description
/*!  \ingroup curves
 */
class YieldCurveSpec : public CurveSpec {
public:
    //! \name Constructors
    //@{
    //! Detailed constructor
    YieldCurveSpec(const string& ccy, const string& curveConfigID) : ccy_(ccy), curveConfigID_(curveConfigID) {}
    //! Default constructor
    YieldCurveSpec() {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const { return CurveType::Yield; }
    const string& ccy() const { return ccy_; }
    const string& curveConfigID() const { return curveConfigID_; }
    string subName() const { return ccy_ + "/" + curveConfigID_; }
    //@}

protected:
    string ccy_;
    string curveConfigID_;
};

//! Default curve description
/*!  \ingroup curves
 */
class DefaultCurveSpec : public CurveSpec {
public:
    //! \name Constructors
    //@{
    //! Detailed constructor
    DefaultCurveSpec(const string& ccy, const string& curveConfigID) : ccy_(ccy), curveConfigID_(curveConfigID) {}
    //! Default constructor
    DefaultCurveSpec() {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const { return CurveType::Default; }
    const string& ccy() const { return ccy_; }
    const string& curveConfigID() const { return curveConfigID_; }
    string subName() const { return ccy_ + "/" + curveConfigID_; }
    //@}

private:
    string ccy_;
    string curveConfigID_;
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
    SwaptionVolatilityCurveSpec(const string& ccy, const string& curveConfigID)
        : ccy_(ccy), curveConfigID_(curveConfigID) {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const { return CurveType::SwaptionVolatility; }
    const string& ccy() const { return ccy_; }
    const string& curveConfigID() const { return curveConfigID_; }
    string subName() const { return ccy() + "/" + curveConfigID(); }
    //@}
private:
    string ccy_;
    string curveConfigID_;
};

//! Cap/Floor Volatility curve description
class CapFloorVolatilityCurveSpec : public CurveSpec {
public:
    CapFloorVolatilityCurveSpec() {}
    CapFloorVolatilityCurveSpec(const string& ccy, const string& curveConfigID)
        : ccy_(ccy), curveConfigID_(curveConfigID) {}

    //! \name CurveSpec interface
    //@{
    CurveType baseType() const { return CurveType::CapFloorVolatility; }
    string subName() const { return ccy() + "/" + curveConfigID(); }
    //@}

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const string& curveConfigID() const { return curveConfigID_; }
    //@}

private:
    string ccy_;
    string curveConfigID_;
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
    CurveType baseType() const { return CurveType::FX; }
    const string& unitCcy() const { return unitCcy_; }
    const string& ccy() const { return ccy_; }
    string subName() const { return unitCcy() + "/" + ccy(); }
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
        : unitCcy_(unitCcy), ccy_(ccy), curveConfigID_(curveConfigID) {}
    //@}

    //! \name Inspectors
    //@{
    CurveType baseType() const { return CurveType::FXVolatility; }
    const string& unitCcy() const { return unitCcy_; }
    const string& ccy() const { return ccy_; }
    const string& curveConfigID() const { return curveConfigID_; }
    string subName() const { return unitCcy() + "/" + ccy() + "/" + curveConfigID(); }
    //@}
private:
    string unitCcy_;
    string ccy_;
    string curveConfigID_;
};
} // namespace data
} // namespace ore
