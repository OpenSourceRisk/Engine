/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
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
    virtual XMLNode* toXML(ore::data::XMLDocument& doc) override;

    //! Add underlying Bond names
    std::map<AssetClass, std::set<std::string>>
        underlyingIndices(const boost::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const;

    bool empty() { return bonds_.empty(); }
    void clear();

    boost::shared_ptr<QuantExt::BondBasket> build(const boost::shared_ptr<EngineFactory>& engineFactory,
                                                  const QuantLib::Currency& ccy,
                                                  const std::string& reinvestmentEndDate);
    //@}

    const std::vector<boost::shared_ptr<Bond>>& bonds() const { return bonds_; }

    const RequiredFixings& requiredFixings() const { return requiredFixings_; }
private:

    bool isFeeFlow(const ext::shared_ptr<QuantLib::CashFlow>& cf, const std::string& name);
    void setReinvestmentScalar();

    vector<boost::shared_ptr<Bond>> bonds_;
    std::map <string, boost::shared_ptr<QuantExt::FxIndex>> fxIndexMap_;
    RequiredFixings requiredFixings_;
    QuantLib::Date reinvestment_;
    std::map<std::string, std::vector<double> > reinvestmentScalar_;
    std::map<std::string, std::vector<std::string> > flowType_;

};
} // namespace data
} // namespace ore
