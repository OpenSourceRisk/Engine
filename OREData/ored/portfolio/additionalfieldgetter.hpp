/*
 Copyright (C) 2017 Quaternion Risk Management Ltd.
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

/*! \file ored/portfolio/additionalfieldgetter.hpp
    \brief Class that can return additional fields and basic information for a given trade ID.
 */

#pragma once

#include <ored/portfolio/portfolio.hpp>

#include <map>
#include <set>
#include <string>

namespace ore {
namespace data {

/*! Abstract class that defines an interface for getting additional fields
    for a given trade ID.
*/
class AdditionalFieldGetter {
public:
    virtual ~AdditionalFieldGetter() {}

    //! Returns the set of all possible additional fields
    virtual std::set<std::string> fieldNames() const = 0;

    /*! Returns the map of additional fields for the given trade ID \p tradeId

        \remark If the trade ID has no additional field value for a given additional
                field name, it is not included in the map.
    */
    virtual std::map<std::string, std::string> fields(const std::string& tradeId) const = 0;

    //! Returns the npv currency for a given trade ID \p tradeId
    virtual std::string npvCurrency(const std::string& tradeId) const = 0;
};

/*! Concrete implementation of AdditionalFieldGetter that gets the additional fields for
    each trade in a given portfolio.
*/
class PortfolioFieldGetter : public AdditionalFieldGetter {
public:
    /*! Constructor that takes a portfolio and optionally a set of \b base additional fields

        \remark The \b baseFieldNames are field names that will always be in the set returned by
                fieldNames regardless of whether they are found in any trade in the portfolio.

        \remark If \b addExtraFields is set to true (the default) this will scan all trades and
                add any addtionalField that appears
    */
    PortfolioFieldGetter(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                         const std::set<std::string>& baseFieldNames = {}, bool addExtraFields = true);

    void add(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio, bool addExtraFields = true);

    void removeFieldName(const std::string& fieldName) { fieldNames_.erase(fieldName); }

    //! Returns the set of additional fields considered relevant by this class
    std::set<std::string> fieldNames() const override;

    //! Returns the map of additional fields for the given trade ID \p tradeId
    std::map<std::string, std::string> fields(const std::string& tradeId) const override;

    //! Get additional field for the given trade if it exists
    std::string field(const std::string& tradeId, const std::string& fieldName) const;

    //! Returns the npv currency for a given trade ID \p tradeId
    std::string npvCurrency(const std::string& tradeId) const override;

private:
    //! The portfolio of trades
    QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio_;

    //! The relevant additional field names
    std::set<std::string> fieldNames_;
};

} // namespace data
} // namespace ore
