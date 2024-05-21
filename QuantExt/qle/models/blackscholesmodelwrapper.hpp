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

/*! \file models/blackscholesmodelwrapper.hpp
    \brief wrapper around a vector of BS processes
    \ingroup utilities
*/

#pragma once

#include <ql/processes/blackscholesprocess.hpp>
#include <ql/timegrid.hpp>

namespace QuantExt {

using namespace QuantLib;

/* this class acts as an intermediate class between the black scholes model builder and the black scholes script model;
   the motivation to have a builder and this wrapper at all is to filter notifications from the vol surfaces and curves
   so that a recalculations only happens when relevant market data has changed */
class BlackScholesModelWrapper : public Observer, public Observable {
public:
    BlackScholesModelWrapper() {}
    BlackScholesModelWrapper(const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
                             const std::set<Date>& effectiveSimulationDates, const TimeGrid& discretisationTimeGrid)
        : processes_(processes), effectiveSimulationDates_(effectiveSimulationDates),
          discretisationTimeGrid_(discretisationTimeGrid) {}

    const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>>& processes() const { return processes_; }
    const std::set<Date>& effectiveSimulationDates() const { return effectiveSimulationDates_; }
    const TimeGrid& discretisationTimeGrid() const { return discretisationTimeGrid_; }

private:
    void update() override { notifyObservers(); }
    const std::vector<QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>> processes_;
    const std::set<Date> effectiveSimulationDates_;
    const TimeGrid discretisationTimeGrid_;
};

} // namespace QuantExt
