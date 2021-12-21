/*
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

/*! \file ored/portfolio/optionbarrierdata.hpp
    \brief option barrier data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/time/date.hpp>
#include <ql/instruments/barriertype.hpp>
#include <ql/experimental/barrieroption/doublebarriertype.hpp>
#include <boost/optional/optional.hpp>

namespace ore {
namespace data {
using QuantLib::Barrier;
using QuantLib::Date;
using QuantLib::DoubleBarrier;
using QuantLib::Real;

/*! Serializable object holding option barrier data with the type(s) and level(s).
    \ingroup tradedata
*/
class OptionBarrierData : public XMLSerializable {
public:
    //! Default constructor
    OptionBarrierData();
    //! Constructor when using single barrier
    OptionBarrierData(const Barrier::Type barrierType, const Real level, const string& windowStyle = "American",
                      const Real rebate = 0.0);
    //! Constructor when using double barrier
    OptionBarrierData(const DoubleBarrier::Type doubleBarrierType, const vector<Real>& levels,
                      const string& windowStyle = "American", const Real rebate = 0.0);

    //! \name Inspectors
    //@{
    boost::optional<Barrier::Type> barrierType() const { return barrierType_; }
    boost::optional<DoubleBarrier::Type> doubleBarrierType() const { return doubleBarrierType_; }
    const string& windowStyle() const { return windowStyle_; }
    const vector<Real>& levels() const { return levels_; }
    Real rebate() const { return rebate_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    boost::optional<Barrier::Type> barrierType_;
    boost::optional<DoubleBarrier::Type> doubleBarrierType_;

    string windowStyle_;
    vector<Real> levels_;
    Real rebate_; // Do we care about this here? Probably good for future uses
};

} // namespace data
} // namespace ore
