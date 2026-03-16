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

/*! \file engine/valuationcalculator.hpp
    \brief The counterparty cube calculator interface
    \ingroup simulation
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/simulation/simmarket.hpp>
#include <ored/utilities/dategrid.hpp>

namespace ore {
namespace analytics {
using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Size;

//! CounterpartyCalculator interface
class CounterpartyCalculator {
public:
    virtual ~CounterpartyCalculator() {}

    virtual void calculate(
        //! The counterparty name
        const std::string& name,
        //! Name index for writing to the cube
        Size nameIndex,
        //! The market
        const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
        //! The cube for data on name level
        QuantLib::ext::shared_ptr<NPVCube>& outputCube,
        //! The date
        const Date& date,
        //! Date index
        Size dateIndex,
        //! Sample
        Size sample,
        //! isCloseOut
        bool isCloseOut = false) = 0;

    virtual void calculateT0(
        //! The counterparty name
        const std::string& name,
        //! Name index for writing to the cube
        Size nameIndex,
        //! The market
        const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
        //! The cube
        QuantLib::ext::shared_ptr<NPVCube>& outputCube) = 0;
};

//! SurvivalProbabilityCalculator
/*! Calculate the survival probability of a counterparty
 *  If the SurvivalProbabilityCalculator() call throws, we log an exception and write 1 to the cube
 *
 */
class SurvivalProbabilityCalculator : public CounterpartyCalculator {
public:
    ~SurvivalProbabilityCalculator() {}
    //! base ccy and index to write to
    SurvivalProbabilityCalculator(const std::string& configuration, Size index = 0)
        : configuration_(configuration), index_(index) {}

    virtual void calculate(const std::string& name, Size nameIndex,
                           const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                           const Date& date, Size dateIndex, Size sample, bool isCloseOut = false) override;

    virtual void calculateT0(const std::string& name, Size nameIndex,
                             const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube) override;

private:
    Real survProb(const std::string& name,
                  const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
                  const Date& date = Date());

    std::string configuration_;
    Size index_;
};

} // namespace analytics
} // namespace ore
