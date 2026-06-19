/*
    Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file portfolio/powerloadprofiledata.hpp
    \brief Power load profile data class
    \ingroup portfolio
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/time/date.hpp>
#include <qle/termstructures/intradaypowerloadtermstructure.hpp>

#include <map>
#include <memory>

namespace ore {
namespace data {

//! Base class for power load profile data
class PowerLoadData : public XMLSerializable {
public:
    virtual ~PowerLoadData() = default;

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override = 0;
    virtual XMLNode* toXML(XMLDocument& doc) const override = 0;
    //@}

    //! Load term structure
    virtual QuantLib::ext::shared_ptr<QuantExt::IntradayPowerLoadTermStructure> loadTermStructure() const {
        return nullptr;
    }
};

//! Explicit dates implementation of power load profile data
class ExplicitData : public PowerLoadData {
public:
    ExplicitData() = default;

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    QuantLib::ext::shared_ptr<QuantExt::IntradayPowerLoadTermStructure> loadTermStructure() const override {
        return loadTermStructure_;
    }

private:
    QuantLib::ext::shared_ptr<QuantExt::IntradayPowerLoadTermStructureExplicit> loadTermStructure_;
};

//! Business day rule implementation of power load profile data
class BusinessDayRuleData : public PowerLoadData {
public:
    BusinessDayRuleData() = default;

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    QuantLib::ext::shared_ptr<QuantExt::IntradayPowerLoadTermStructure> loadTermStructure() const override {
        return loadTermStructure_;
    }

private:
    QuantLib::ext::shared_ptr<QuantExt::IntradayPowerLoadTermStructureBusinessDayRule> loadTermStructure_;
};

//! Serializable object holding power load profile data
class PowerLoadProfileData : public PowerLoadData {
public:
    PowerLoadProfileData() = default;

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! Load term structure (delegates to concrete implementation)
    QuantLib::ext::shared_ptr<QuantExt::IntradayPowerLoadTermStructure> loadTermStructure() const override;

private:
    QuantLib::ext::shared_ptr<PowerLoadData> concreteData_;
};

} // namespace data
} // namespace ore