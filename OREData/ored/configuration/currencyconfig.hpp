/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/calendaradjustmentconfig.hpp
    \brief Interface for calendar modifications, additional holidays and business days
    \ingroup utilities
*/

#pragma once

#include <map>
#include <ored/utilities/xmlutils.hpp>
#include <qle/currencies/configurablecurrency.hpp>
#include <ql/patterns/singleton.hpp>
#include <ql/currency.hpp>
#include <ql/utilities/dataparsers.hpp>
#include <set>
#include <string>
#include <vector>

namespace ore {
namespace data {
using QuantExt::ConfigurableCurrency;
using QuantLib::Currency;
using std::map;
using std::set;
using std::string;
using std::vector;

//! Currency configuration
/*!
  Allows reading a list of currency specifications from XML rather than hard coding each in QuantExt/QuantLib
*/
class CurrencyConfig : public XMLSerializable {
public:
    // default constructor
    CurrencyConfig();

    //! check out any configured currencies
    const vector<ConfigurableCurrency>& getCurrencies() const { return currencies_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

private:
    vector<ConfigurableCurrency> currencies_;
};

} // namespace data
} // namespace ore
