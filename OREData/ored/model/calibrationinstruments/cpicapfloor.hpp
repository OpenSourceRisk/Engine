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

/*! \file ored/model/calibrationinstruments/cpicapfloor.hpp
    \brief class for holding details of a zero coupon CPI cap floor calibration instrument.
    \ingroup models
*/

#pragma once

#include <ored/marketdata/strike.hpp>
#include <ored/model/calibrationbasket.hpp>
#include <ored/model/calibrationinstrumentfactory.hpp>

#include <ql/instruments/capfloor.hpp>

#include <boost/variant.hpp>

namespace ore {
namespace data {

/*! Class for holding details of a CPI cap floor calibration instrument.
    \ingroup models
*/
class CpiCapFloor : public CalibrationInstrument {
public:
    //! Default constructor
    CpiCapFloor();

    //! Detailed constructor
    CpiCapFloor(QuantLib::CapFloor::Type type,
        const boost::variant<QuantLib::Date, QuantLib::Period> & maturity,
        const QuantLib::ext::shared_ptr<BaseStrike>& strike);

    //! \name Inspectors
    //@{
    QuantLib::CapFloor::Type type() const;
    const boost::variant<QuantLib::Date, QuantLib::Period>& maturity() const;
    const QuantLib::ext::shared_ptr<BaseStrike>& strike() const;
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    QuantLib::CapFloor::Type type_;
    boost::variant<QuantLib::Date, QuantLib::Period> maturity_;
    QuantLib::ext::shared_ptr<BaseStrike> strike_;
};

} // namespace data
} // namespace ore
