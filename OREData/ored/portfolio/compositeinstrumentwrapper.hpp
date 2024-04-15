/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/compositeinstrumentwrapper.hpp
    \brief used to store multiple trade wrappers
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/instrumentwrapper.hpp>

#include <ql/handle.hpp>
#include <ql/instrument.hpp>
#include <ql/quote.hpp>
#include <ql/settings.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>

#include <vector>

namespace ore {
namespace data {

using QuantLib::Date;
using QuantLib::Handle;
using QuantLib::Quote;
using QuantLib::Real;
using QuantLib::Settings;
using QuantLib::Size;

//! Composite Instrument Wrapper
/*! A Composite Instrument Wrapper will return the sum npv for all wrappers passed in.
    Notice that qlInstrument() will return a nullptr.
    \ingroup tradedata
*/
class CompositeInstrumentWrapper : public ore::data::InstrumentWrapper {
public:
    CompositeInstrumentWrapper(const std::vector<QuantLib::ext::shared_ptr<InstrumentWrapper>>& wrappers,
                               const std::vector<Handle<Quote>>& fxRates = {}, const Date& valuationDate = Date());

    void initialise(const std::vector<QuantLib::Date>& dates) override;
    void reset() override;
    QuantLib::Real NPV() const override;
    const std::map<std::string, boost::any>& additionalResults() const override;
    void updateQlInstruments() override;
    bool isOption() override;

protected:
    bool isOption_;
    std::vector<QuantLib::ext::shared_ptr<InstrumentWrapper>> wrappers_;
    std::vector<QuantLib::Handle<Quote>> fxRates_;
    Date valuationDate_;
    mutable std::map<std::string, boost::any> additionalResults_;
};

} // namespace data
} // namespace ore
