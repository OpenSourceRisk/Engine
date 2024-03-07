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

/*! \file ored/portfolio/envelope.hpp
    \brief trade envelope data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/xmlutils.hpp>

#include <vector>

namespace ore {
namespace data {
using ore::data::XMLNode;
using ore::data::XMLSerializable;
using std::string;
using std::vector;

//! Serializable object holding a trade action
/*!
  \ingroup tradedata
*/
class TradeAction : public XMLSerializable {
public:
    //! Default constructor
    TradeAction() {}

    //! Constructor
    TradeAction(const string& type, const string& owner, const ScheduleData& schedule)
        : type_(type), owner_(owner), schedule_(schedule) {}

    //! \name Inspectors
    //@{
    const string& type() const { return type_; }
    const string& owner() const { return owner_; }
    const ScheduleData& schedule() const { return schedule_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    string type_;
    string owner_;
    ScheduleData schedule_;
};

//! Serializable object holding generic trade actions
class TradeActions : public XMLSerializable {
public:
    TradeActions(const vector<TradeAction>& actions = vector<TradeAction>()) : actions_(actions) {}

    void addAction(const TradeAction& action) { actions_.push_back(action); }

    const vector<TradeAction>& actions() const { return actions_; }

    //! Returns true of this set of actions is empty
    bool empty() const { return actions_.size() == 0; }

    //! Clear the trade actions
    void clear() { actions_.clear(); }

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;

private:
    vector<TradeAction> actions_;
};
} // namespace data
} // namespace ore
