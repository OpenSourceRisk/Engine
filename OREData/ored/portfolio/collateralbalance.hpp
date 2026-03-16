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

/*! \file ored/portfolio/collateralbalance.hpp
    \brief Holder class for collateral balances
    \ingroup tradedata
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ored/portfolio/nettingsetdetails.hpp>
#include <ql/utilities/null.hpp>
#include <ql/shared_ptr.hpp>

using ore::data::NettingSetDetails;

namespace ore {
namespace data {

class CollateralBalance : public ore::data::XMLSerializable {
public:
    /*!
      default constructor
    */
    CollateralBalance()
        : nettingSetDetails_(NettingSetDetails()), currency_(""), 
          im_(QuantLib::Null<QuantLib::Real>()), vm_(QuantLib::Null<QuantLib::Real>()) {}

    CollateralBalance(ore::data::XMLNode* node);

    CollateralBalance(const NettingSetDetails& nettingSetDetails, const std::string& currency,
                      const QuantLib::Real& im, const QuantLib::Real& vm = QuantLib::Null<QuantLib::Real>())
         : nettingSetDetails_(nettingSetDetails), currency_(currency), im_(im), vm_(vm) {}

    CollateralBalance(const std::string& nettingSetId, const std::string& currency,
                      const QuantLib::Real& im, const QuantLib::Real& vm = QuantLib::Null<QuantLib::Real>())
        : CollateralBalance(NettingSetDetails(nettingSetId), currency, im, vm) {}

    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    // Getters
    const std::string& nettingSetId() const {
        return nettingSetDetails_.nettingSetId();
    }
    const NettingSetDetails nettingSetDetails() const { return nettingSetDetails_; }
    const std::string& currency() const { return currency_; }
    const QuantLib::Real& initialMargin() const { return im_; }
    const QuantLib::Real& variationMargin() const { return vm_; }

    // Setters
    QuantLib::Real& initialMargin() { return im_; }
    QuantLib::Real& variationMargin() { return vm_; }

private:
    NettingSetDetails nettingSetDetails_;
    std::string currency_;
    QuantLib::Real im_, vm_;
    
};
//! Collateral Balances
/*!
  This class hold collateral balances
  
  \ingroup tradedata
*/
class CollateralBalances : public ore::data::XMLSerializable {
public:
    /*!
      default constructor
    */
    CollateralBalances() {}

    /*!
      clears the manager of all data
    */
    void reset();

    /*!
      checks if there are any collateral balance entries
    */
    const bool empty();

    /*!
      checks if object named nettingSetId exists in manager
    */
    bool has(const std::string& nettingSetId) const;

    /*!
      checks if object with the given nettingSetDetails exists in manager
    */
    bool has(const NettingSetDetails& nettingSetDetails) const;

    /*!
      adds a new collateral balance to manager
    */
    void add(const QuantLib::ext::shared_ptr<CollateralBalance>& cb, const bool overwrite = false);

    /*!
      extracts a collateral balance from manager
    */
    const QuantLib::ext::shared_ptr<CollateralBalance>& get(const std::string& nettingSetId) const;
    const QuantLib::ext::shared_ptr<CollateralBalance>& get(const NettingSetDetails& nettingSetDetails) const;
    
    /*!
        gets currentIM for DIM calculation
    */
    void currentIM(const std::string& baseCurrency, std::map<std::string, QuantLib::Real>& currentIM);
    
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    const std::map<NettingSetDetails, QuantLib::ext::shared_ptr<CollateralBalance>>& collateralBalances();

private:
    std::map<NettingSetDetails, QuantLib::ext::shared_ptr<CollateralBalance>> collateralBalances_;
};

} // namespace data
} // namespace ore
