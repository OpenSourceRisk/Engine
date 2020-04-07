/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
 */

/*! \file oredp/portfolio/underlying.hpp
    \brief underlying data model
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/schedule.hpp>

namespace ore {
namespace data {

//! Class to hold Underlyings
/*!
  \ingroup portfolio
*/

class Underlying : public ore::data::XMLSerializable {
public:
    //! Default constructor
    Underlying() : weight_(QuantLib::Null<QuantLib::Real>()) {};

    //! Constructor with type
    Underlying(const std::string& type, const std::string& name, const QuantLib::Real weight = QuantLib::Null<QuantLib::Real>()) :
        type_(type), name_(name), weight_(weight) {};

    //! \name Serialisation
    //@{
    virtual void fromXML(ore::data::XMLNode* node) override;
    virtual ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    const std::string& type() const { return type_; }
    virtual const std::string& name() const { return name_; }
    Real weight() const { return weight_; }
    //@}

protected:
    std::string type_, name_;
    Real weight_;
};

class EquityUnderlying : public Underlying {
public:
    //! Deault constructor
    EquityUnderlying() : Underlying() {}

    //! Constructor with equity name
    EquityUnderlying(const std::string& equityName) : Underlying("Equity", equityName) {};

    //! Constructor with identifer infomation
    EquityUnderlying(const std::string& type, const std::string& name, 
        const std::string& identifierType, const std::string& currency,        
        const std::string& exchange, const QuantLib::Real weight = QuantLib::Null<QuantLib::Real>()) :
        Underlying(type, name, weight), identifierType_(identifierType), 
        currency_(currency), exchange_(exchange) {
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
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

private:
    std::string equityName_, identifierType_, currency_, exchange_;
};

class CommodityUnderlying : public Underlying {
public:
    //! Default Constructor
    CommodityUnderlying() : Underlying() {}
    
    //! Constructor with identifer infomation
    CommodityUnderlying(const std::string& type, const std::string& name, const QuantLib::Real weight = QuantLib::Null<QuantLib::Real>(),
        const std::string& priceType = "", const QuantLib::Size futureMonthOffset = QuantLib::Null<QuantLib::Size>(),
        const QuantLib::Size deliveryRollDays = QuantLib::Null<QuantLib::Size>(), const std::string& deliveryRollCalendar = "")
        : Underlying(type, name, weight), priceType_(priceType), futureMonthOffset_(futureMonthOffset),
        deliveryRollDays_(deliveryRollDays), deliveryRollCalendar_(deliveryRollCalendar) {}

    const std::string& priceType() const { return priceType_; }                      
    QuantLib::Size futureMonthOffset() const { return futureMonthOffset_; }
    QuantLib::Size deliveryRollDays() const { return deliveryRollDays_; }
    const std::string& deliveryRollCalendar() const { return deliveryRollCalendar_; } 
    
    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

private:
    std::string priceType_;
    QuantLib::Size futureMonthOffset_;
    QuantLib::Size deliveryRollDays_;
    std::string deliveryRollCalendar_;
};

class FXUnderlying : public Underlying {
public:
    //! Default Constructor
    FXUnderlying() : Underlying() {};

    //! Constructor with identifer infomation
    FXUnderlying(const std::string& type, const std::string& name, const QuantLib::Real weight = QuantLib::Null<QuantLib::Real>()) :
        Underlying(type, name, weight) {};

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}
};

class UnderlyingBuilder : public XMLSerializable {
public:
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) override;

    const boost::shared_ptr<Underlying>& underlying() { return underlying_; };

private:
    boost::shared_ptr<Underlying> underlying_;
};

} // namespace data
} // namespace ore
