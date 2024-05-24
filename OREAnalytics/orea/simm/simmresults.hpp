/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file orea/simm/simmresults.hpp
    \brief Class for holding SIMM results
*/

#pragma once

#include <map>
#include <tuple>

#include <orea/simm/simmconfiguration.hpp>
#include <ored/marketdata/market.hpp>
#include <ql/types.hpp>

namespace ore {
namespace analytics {

/*! A container for SIMM results broken down by product class, risk class
    and margin type.
*/
class SimmResults {
public:
    typedef CrifRecord::ProductClass ProductClass;
    typedef SimmConfiguration::RiskClass RiskClass;
    typedef SimmConfiguration::MarginType MarginType;
    typedef std::tuple<ProductClass, RiskClass, MarginType, std::string> Key;

    SimmResults(const std::string& resultCcy = "", const std::string& calcCcy = "")
        : resultCcy_(resultCcy), calcCcy_(calcCcy){};

    /*! Add initial margin value \p im to the results container for the given combination of
        SIMM <em>product class</em>, <em>risk class</em> and <em>margin type</em>

        \remark If there is already a result in the container for that combination, it is
                overwritten if overwrite=true. Otherwise, the amounts are added together.
                Can check this using the <code>has</code> method before adding.
    */
    void add(const CrifRecord::ProductClass& pc, const SimmConfiguration::RiskClass& rc,
             const SimmConfiguration::MarginType& mt, const std::string& b, QuantLib::Real im,
             const std::string& resultCurrency, const std::string& calculationCurrency, const bool overwrite);

    void add(const Key& key, QuantLib::Real im, const std::string& resultCurrency,
             const std::string& calculationCurrency, const bool overwrite);

    //! Convert SIMM amounts to a different currency
    void convert(const QuantLib::ext::shared_ptr<ore::data::Market>& market, const std::string& currency);
    void convert(QuantLib::Real fxSpot, const std::string& currency);

    /*! Get the initial margin value from the results container for the given combination of
        SIMM <em>product class</em>, <em>risk class</em> and <em>margin type</em>

        \warning returns <code>QuantLib::Null<QuantLib::Real></code> if there is no initial margin
                 value in the results for the given combination. Can avoid this by first checking
                 the results using the <code>has</code> method.
    */
    QuantLib::Real get(const CrifRecord::ProductClass& pc, const SimmConfiguration::RiskClass& rc,
                       const SimmConfiguration::MarginType& mt, const std::string b) const;

    /*! Check if there is an initial margin value in the results container for the given combination of
        SIMM <em>product class</em>, <em>risk class</em> and <em>margin type</em>
    */
    bool has(const CrifRecord::ProductClass& pc, const SimmConfiguration::RiskClass& rc,
             const SimmConfiguration::MarginType& mt, const std::string b) const;

    //! Return true if the container is empty, otherwise false
    bool empty() const;

    //! Clear the results from the container
    void clear();

    //! Return the map containing the results
    // Not nice to do this i.e. exposing inner implementation but it
    // allows to iterate over results cleanly. Would be better to subclass
    // std::iterator in this class.
    const std::map<Key, QuantLib::Real>& data() const { return data_; }
    std::map<Key, QuantLib::Real>& data() { return data_; }

    std::string& resultCurrency() { return resultCcy_; }
    const std::string& resultCurrency() const { return resultCcy_; }

    std::string& calculationCurrency() { return calcCcy_; }
    const std::string& calculationCurrency() const { return calcCcy_; }

private:
    std::map<Key, QuantLib::Real> data_;
    std::string resultCcy_;
    std::string calcCcy_;
};

//! Enable writing of Key
std::ostream& operator<<(std::ostream& out, const SimmResults::Key& resultsKey);

} // namespace analytics
} // namespace ore
