/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file models/blackscholesmodelwrapper.hpp
    \brief wrapper around a vector of BS processes
    \ingroup utilities
*/

#pragma once

#include <ql/processes/blackscholesprocess.hpp>
#include <ql/timegrid.hpp>

namespace oreplus {
namespace data {

using namespace QuantLib;

/* this class acts as an intermediate class between the black scholes model builder and the black scholes script model;
   the motivation to have a builder and this wrapper at all is to filter notifications from the vol surfaces and curves
   so that a recalculations only happens when relevant market data has changed */
class BlackScholesModelWrapper : public Observer, public Observable {
public:
    BlackScholesModelWrapper() {}
    BlackScholesModelWrapper(const std::vector<boost::shared_ptr<GeneralizedBlackScholesProcess>>& processes,
                             const std::set<Date>& effectiveSimulationDates, const TimeGrid& discretisationTimeGrid)
        : processes_(processes), effectiveSimulationDates_(effectiveSimulationDates),
          discretisationTimeGrid_(discretisationTimeGrid) {}

    const std::vector<boost::shared_ptr<GeneralizedBlackScholesProcess>>& processes() const { return processes_; }
    const std::set<Date>& effectiveSimulationDates() const { return effectiveSimulationDates_; }
    const TimeGrid& discretisationTimeGrid() const { return discretisationTimeGrid_; }

private:
    void update() override { notifyObservers(); }
    const std::vector<boost::shared_ptr<GeneralizedBlackScholesProcess>> processes_;
    const std::set<Date> effectiveSimulationDates_;
    const TimeGrid discretisationTimeGrid_;
};

} // namespace data
} // namespace oreplus
