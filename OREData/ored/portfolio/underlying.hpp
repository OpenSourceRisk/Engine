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

/*! \file ored/portfolio/underlying.hpp
    \brief underlying data model
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/schedule.hpp>
#include <ql/cashflows/cpicoupon.hpp>

namespace ore {
namespace data {

//! Class to hold Underlyings
/*!
  \ingroup portfolio
*/

class Underlying : public ore::data::XMLSerializable {
public:
    //! Default Constructor
    Underlying() : nodeName_("Underlying"), basicUnderlyingNodeName_("Name"){};

    //! Constructor with type, name, weight
    Underlying(const std::string& type, const std::string& name,
               const QuantLib::Real weight = QuantLib::Null<QuantLib::Real>());

    //! \name Serialisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    //! \name Setters
    void setType(const string& type) { type_ = type; }
    void setName(const string& name) { name_ = name; }
    void setWeight(const QuantLib::Real weight) { weight_ = weight; }
    void setNodeName(const string& nodeName) { nodeName_ = nodeName; }
    void setBasicUnderlyingNodeName(const string& basicUnderlyingNodeName) {
        basicUnderlyingNodeName_ = basicUnderlyingNodeName;
    }

    //! \name Inspectors
    //@{
    const std::string& type() const { return type_; }
    virtual const std::string& name() const { return name_; }
    Real weight() const { return weight_; }
    //@}

protected:
    std::string type_, name_;
    Real weight_ = QuantLib::Null<QuantLib::Real>();
    std::string nodeName_, basicUnderlyingNodeName_;
    bool isBasic_ = false;
};

class BasicUnderlying : public Underlying {
public:
    //! Default Constructor
    BasicUnderlying() : Underlying() {
        setType("Basic");
        isBasic_ = true;
    };

    //! Constructor with identifier
    explicit BasicUnderlying(const std::string& name) : Underlying("Basic", name) { isBasic_ = true; }

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}
};

class EquityUnderlying : public Underlying {
public:
    //! Default constructor
    EquityUnderlying() : Underlying() { setType("Equity"); }

    //! Constructor with equity name
    explicit EquityUnderlying(const std::string& equityName) : Underlying("Equity", equityName) { isBasic_ = true; };

    //! Constructor with identifier information
    EquityUnderlying(const std::string& name, const std::string& identifierType, const std::string& currency,
                     const std::string& exchange, const QuantLib::Real weight)
        : Underlying("Equity", name, weight), identifierType_(identifierType), currency_(currency),
          exchange_(exchange) {
        setEquityName();
    };

    const std::string& name() const override { return equityName_.empty() ? name_ : equityName_; };
    const std::string& identifierType() const { return identifierType_; }
    const std::string& currency() const { return currency_; }
    const std::string& exchange() const { return exchange_; };

    //! set name of equity
    void setEquityName();

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    std::string equityName_, identifierType_, currency_, exchange_;
};

class CommodityUnderlying : public Underlying {
public:
    //! Default Constructor
    CommodityUnderlying() : Underlying() { setType("Commodity"); }

    //! Constructor with identifier information
    CommodityUnderlying(const std::string& name, const QuantLib::Real weight, const std::string& priceType,
                        const QuantLib::Size futureMonthOffset, const QuantLib::Size deliveryRollDays,
                        const std::string& deliveryRollCalendar)
        : Underlying("Commodity", name, weight), priceType_(priceType), futureMonthOffset_(futureMonthOffset),
          deliveryRollDays_(deliveryRollDays), deliveryRollCalendar_(deliveryRollCalendar) {}

    const std::string& priceType() const { return priceType_; }
    QuantLib::Size futureMonthOffset() const { return futureMonthOffset_; }
    QuantLib::Size deliveryRollDays() const { return deliveryRollDays_; }
    const std::string& deliveryRollCalendar() const { return deliveryRollCalendar_; }
    const std::string& futureContractMonth() const { return futureContractMonth_; }
    const std::string& futureExpiryDate() const { return futureExpiryDate_;  }
    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    std::string priceType_;
    QuantLib::Size futureMonthOffset_ = QuantLib::Null<QuantLib::Size>();
    QuantLib::Size deliveryRollDays_ = QuantLib::Null<QuantLib::Size>();
    std::string deliveryRollCalendar_;
    std::string futureContractMonth_;
    std::string futureExpiryDate_;
};

class FXUnderlying : public Underlying {
public:
    //! Default Constructor
    explicit FXUnderlying() : Underlying() { setType("FX"); };

    //! Constructor with identifier information
    FXUnderlying(const std::string& type, const std::string& name, const QuantLib::Real weight)
        : Underlying(type, name, weight){};

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}
};

class InterestRateUnderlying : public Underlying {
public:
    //! Default Constructor
    explicit InterestRateUnderlying() : Underlying() { setType("InterestRate"); };

    //! Constructor with identifier information
    InterestRateUnderlying(const std::string& type, const std::string& name, const QuantLib::Real weight)
        : Underlying(type, name, weight){};

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}
};

class InflationUnderlying : public Underlying {
public:
    //! Default Constructor
    explicit InflationUnderlying() : Underlying() { setType("Inflation"); };

    //! Constructor with identifier information
    InflationUnderlying(const std::string& type, const std::string& name, const QuantLib::Real weight,
                        const QuantLib::CPI::InterpolationType& interpolation = QuantLib::CPI::InterpolationType::Flat)
        : Underlying(type, name, weight), interpolation_(interpolation){};
    const QuantLib::CPI::InterpolationType& interpolation() const { return interpolation_; }

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}
private:
    QuantLib::CPI::InterpolationType interpolation_;
};

class CreditUnderlying : public Underlying {
public:
    //! Default Constructor
    explicit CreditUnderlying() : Underlying() { setType("Credit"); };

    //! Constructor with identifier information
    CreditUnderlying(const std::string& type, const std::string& name, const QuantLib::Real weight)
        : Underlying(type, name, weight){};

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}
};

class BondUnderlying : public Underlying {
public:
    //! Default constructor
    BondUnderlying() : Underlying() { setType("Bond"); }

    //! Constructor with full bond name (e.g. ISIN:DE00001142867)
    explicit BondUnderlying(const std::string& name) : Underlying("Bond", name) { isBasic_ = true; };

    //! Constructor with identifer infomation (e.g. identifier = DE00001142867, identifierType = ISIN)
    BondUnderlying(const std::string& identifier, const std::string& identifierType, const QuantLib::Real weight)
        : Underlying("Bond", identifier, weight), identifierType_(identifierType){};

    const std::string& name() const override { return bondName_.empty() ? name_ : bondName_; };
    const std::string& identifierType() const { return identifierType_; }
    double bidAskAdjustment() const { return bidAskAdjustment_; }

    //! set name of bond
    void setBondName();

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    std::string bondName_, identifierType_;
    double bidAskAdjustment_ = 0.0;
};

class UnderlyingBuilder : public XMLSerializable {
public:
    explicit UnderlyingBuilder(const std::string& nodeName = "Underlying",
                               const std::string& basicUnderlyingNodeName = "Name")
        : nodeName_(nodeName), basicUnderlyingNodeName_(basicUnderlyingNodeName) {}
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    const QuantLib::ext::shared_ptr<Underlying>& underlying() { return underlying_; };

private:
    const std::string nodeName_, basicUnderlyingNodeName_;
    QuantLib::ext::shared_ptr<Underlying> underlying_;
};

} // namespace data
} // namespace ore
