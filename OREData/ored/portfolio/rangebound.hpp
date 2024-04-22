/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/rangebound.hpp
    \brief rangebound data model
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/schedule.hpp>

namespace ore {
namespace data {
using QuantLib::Null;
using QuantLib::Real;
using std::string;
using std::vector;

//! Serializable obejct holding range bound data
/*!
  \ingroup tradedata
*/
class RangeBound : public ore::data::XMLSerializable {
public:
    //! Default constructor
    RangeBound() : from_(Null<Real>()), to_(Null<Real>()), leverage_(Null<Real>()), strike_(Null<Real>()) {}
    //! Constructor
    RangeBound(const Real from, const Real to, const Real leverage, const Real strike, const Real strikeAdjustment)
        : from_(from), to_(to), leverage_(leverage), strike_(strike), strikeAdjustment_(strikeAdjustment) {}

    //! \name Inspectors
    //@{
    Real from() const { return from_; }
    Real to() const { return to_; }
    Real leverage() const { return leverage_; }
    Real strike() const { return strike_; }
    Real strikeAdjustment() const { return strikeAdjustment_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

private:
    Real from_, to_, leverage_, strike_, strikeAdjustment_;
};

bool operator==(const RangeBound& a, const RangeBound& b);
std::ostream& operator<<(std::ostream& out, const RangeBound& t);
std::ostream& operator<<(std::ostream& out, const std::vector<RangeBound>& t);

} // namespace data
} // namespace ore
