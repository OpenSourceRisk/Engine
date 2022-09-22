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
    \brief used to store multiple trade wrappers, so that the "state" of each trade is maintained
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/instrumentwrapper.hpp>
#include <ql/instrument.hpp>
#include <ql/time/date.hpp>
#include <ql/settings.hpp>
#include <ql/handle.hpp>
#include <ql/quote.hpp>
#include <ql/types.hpp>
#include <vector>

namespace ore {
namespace data {
using QuantLib::Real;
using QuantLib::Handle;
using QuantLib::Quote;
using QuantLib::Settings;
using QuantLib::Date;
using QuantLib::Size;
  
//! Composite Instrument Wrapper
/*!
  A Composite Instrument Wrapper will return the sum npv for all wrappers passed in.
 This is to allow for the pricing of trades through replication

  \ingroup tradedata
*/
class CompositeInstrumentWrapper : public ore::data::InstrumentWrapper {
public:
    CompositeInstrumentWrapper(const std::vector<boost::shared_ptr<InstrumentWrapper>>& wrappers,
                               const std::vector<Handle<Quote>>& fxRates, const Date& valuationDate = Date())
        : InstrumentWrapper(boost::shared_ptr<QuantLib::Instrument>()), wrappers_(wrappers), fxRates_(fxRates),
          valuationDate_(valuationDate) {
        QL_REQUIRE(wrappers.size() > 0, "no instrument wrappers provided");
        QL_REQUIRE(fxRates_.size() == wrappers_.size(), "unexpected number of fxRates provided");
        instrument_ = wrappers[0]->qlInstrument();
        multiplier_ = wrappers[0]->multiplier();

        isOption_ = false;

        for (auto w : wrappers_) {
            if (w->isOption()) {
                isOption_ = true;
                break;
            }
        }
    }

    //! Initialise with the given date grid
    void initialise(const std::vector<QuantLib::Date>& dates) override{};

    //! reset is called every time a new path is about to be priced.
    /*! For path dependent Wrappers, this is when internal state should be reset
     */
    void reset() override {
        for (auto w : wrappers_) {
            w->reset();
        }
    }

    //! Return the NPV of this instrument
    QuantLib::Real NPV() const override {
        Date today = Settings::instance().evaluationDate();
        if (valuationDate_ != Date()) {
            QL_REQUIRE(today == valuationDate_, "today must be the expected valuation date for this trade");
        }
        Real npv = 0;
        for (Size i = 0; i < wrappers_.size(); i++) {
            npv += wrappers_[i]->NPV() * fxRates_[i]->value();
        }
        return npv;
    }

    const std::map<std::string, boost::any>& additionalResults() const override {
        additionalResults_.clear();
        for (auto const& w : wrappers_) {
            additionalResults_.insert(w->additionalResults().begin(), w->additionalResults().end());
        }
        return additionalResults_;
    }

    //! call update on enclosed instrumentWrapper(s)
    void updateQlInstruments() override {
        // the instrument wrappers might contain nested lazy objects which we also want to be updated
        for (auto w : wrappers_) {
            w->updateQlInstruments();
        }
    }

    //! is it an Option?
    bool isOption() override { return isOption_; }

protected:
    bool isOption_;
    std::vector<boost::shared_ptr<InstrumentWrapper>> wrappers_;
    std::vector<QuantLib::Handle<Quote>> fxRates_;
    Date valuationDate_;
    mutable std::map<std::string, boost::any> additionalResults_;
};
} // namespace data
} // namespace oreplus
