/*
 Copyright (C) 2017, 2023 Quaternion Risk Management Ltd
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

/*! \file portfolio/forwardrateagreement.hpp
    \brief ForwardRateAgreement data model and serialization
    \ingroup tradedata
*/


#pragma once

#include <ored/portfolio/swap.hpp>

namespace ore {
namespace data {

//! Serializable ForwardRateAgreement
/*!
  \ingroup tradedata
*/
class ForwardRateAgreement : public Swap {
public:
    ForwardRateAgreement() : ore::data::Swap("ForwardRateAgreement") {}
    ForwardRateAgreement(Envelope& env, string longShort, string currency, string startDate, string endDate,
                         string index, double strike, double amount)
        : ore::data::Swap(env, "ForwardRateAgreement"), longShort_(longShort), currency_(currency), startDate_(startDate),
          endDate_(endDate), index_(index), strike_(strike), amount_(amount) {}

    void build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) override;

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    const string& index() const { return index_; }

private:
    string longShort_;
    string currency_;
    string startDate_;
    string endDate_;
    string index_;
    double strike_;
    double amount_;
};
} // namespace data
} // namespace ore

