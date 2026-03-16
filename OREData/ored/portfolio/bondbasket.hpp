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

/*! \file portfolio/bondbasket.hpp
    \brief credit bond basket data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <qle/instruments/bondbasket.hpp>

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/parsers.hpp>

#include <vector>

namespace ore {
namespace data {
using namespace QuantLib;
using ore::data::XMLSerializable;
using std::string;
using std::vector;

//! Serializable Bond-Basket Data
/*!
  \ingroup tradedata
*/

class BondBasket : public XMLSerializable {
public:
    //! Default constructor
    BondBasket() {}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    //! Add underlying Bond names
    std::map<AssetClass, std::set<std::string>>
        underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const;

    bool empty() { return bonds_.empty(); }
    void clear();

    QuantLib::ext::shared_ptr<QuantExt::BondBasket> build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                                  const QuantLib::Currency& ccy,
                                                  const std::string& reinvestmentEndDate);
    //@}

    const std::vector<QuantLib::ext::shared_ptr<Bond>>& bonds() const { return bonds_; }

    const RequiredFixings& requiredFixings() const { return requiredFixings_; }
private:

    bool isFeeFlow(const ext::shared_ptr<QuantLib::CashFlow>& cf, const std::string& name);
    void setReinvestmentScalar();

    vector<QuantLib::ext::shared_ptr<Bond>> bonds_;
    std::map <string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>> fxIndexMap_;
    RequiredFixings requiredFixings_;
    QuantLib::Date reinvestment_;
    std::map<std::string, std::vector<double> > reinvestmentScalar_;
    std::map<std::string, std::vector<std::string> > flowType_;

};
} // namespace data
} // namespace ore
