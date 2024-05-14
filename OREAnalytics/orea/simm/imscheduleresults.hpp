/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file orea/simm/imscheduleresults.hpp
    \brief Class for holding IMSchedule results
*/

#pragma once

#include <map>

#include <ored/marketdata/market.hpp>
#include <orea/simm/simmconfiguration.hpp>
#include <ql/types.hpp>

namespace ore {
namespace analytics {

enum IMScheduleLabel { Credit2, Credit5, Credit100, Commodity, Equity, FX, Rates2, Rates5, Rates100, Other };

struct IMScheduleResult {

    IMScheduleResult(const QuantLib::Real& grossIM = 0.0, const QuantLib::Real& grossRC = 0.0,
                     const QuantLib::Real& netRC = 0.0, const QuantLib::Real& ngr = 0.0,
                     const QuantLib::Real& scheduleIM = 0.0)
        : grossIM(grossIM), grossRC(grossRC), netRC(netRC), NGR(ngr), scheduleIM(scheduleIM) {}

    QuantLib::Real grossIM;
    QuantLib::Real grossRC;
    QuantLib::Real netRC;
    QuantLib::Real NGR;
    QuantLib::Real scheduleIM;
};

/*! A container for Schedule IM results broken down by product class, risk class
    and margin type.
*/
class IMScheduleResults {
public:
    IMScheduleResults(const std::string& ccy = "") : ccy_(ccy){};

    /*! Add initial margin value \p im to the results container for the given combination of
        Schedule <em>product class</em>, <em>risk class</em> and <em>margin type</em>

        \remark If there is already a result in the container for that combination, it is
                overwritten. Can check this using the <code>has</code> method before adding.
    */
    void add(const CrifRecord::ProductClass& pc, const std::string& calculationCurrency,
             const QuantLib::Real grossIM, const QuantLib::Real grossRC, const QuantLib::Real netRC,
             const QuantLib::Real ngr, const QuantLib::Real scheduleIM);

    //! Convert Schedule IM amounts to a different currency
    // void convert(const QuantLib::ext::shared_ptr<ore::data::Market>& market, const std::string& currency);
    // void convert(QuantLib::Real fxSpot, const std::string& currency);

    /*! Get the initial margin value from the results container for the given combination of
        Schedule <em>product class</em>, <em>risk class</em> and <em>margin type</em>

        \warning returns <code>QuantLib::Null<QuantLib::Real></code> if there is no initial margin
                 value in the results for the given combination. Can avoid this by first checking
                 the results using the <code>has</code> method.
    */
    IMScheduleResult get(const CrifRecord::ProductClass& pc) const;

    /*! Check if there is an initial margin value in the results container for the given combination of
        Schedule <em>product class</em>, <em>risk class</em> and <em>margin type</em>
    */
    bool has(const CrifRecord::ProductClass& pc) const;

    //! Return true if the container is empty, otherwise false
    bool empty() const;

    //! Clear the results from the container
    void clear();

    //! Return the map containing the results
    // Not nice to do this i.e. exposing inner implementation but it
    // allows to iterate over results cleanly. Would be better to subclass
    // std::iterator in this class.
    const std::map<CrifRecord::ProductClass, IMScheduleResult>& data() const { return data_; }

    const std::string& currency() const { return ccy_; }

    std::string& currency() { return ccy_; }

private:
    std::map<CrifRecord::ProductClass, IMScheduleResult> data_;
    std::string ccy_;
};

} // namespace analytics
} // namespace ore
