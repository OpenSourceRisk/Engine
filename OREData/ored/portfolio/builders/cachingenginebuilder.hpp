/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/builders/cachingenginebuilder.hpp
  \brief Abstract template engine builder class
  \ingroup builders
*/

#pragma once

#include <ored/portfolio/enginefactory.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <qle/cashflows/cpicouponpricer.hpp>

namespace ore {
namespace data {

//! Abstract template EngineBuilder class that can cache engines and coupon pricers
/*! Subclasses must implement two protected methods:
 *  - keyImpl() returns a key that is used to cache
 *  - engineImpl() return a new pricing engine or coupon pricer.
 *  When the engine() method is called the CachingEngineBuilder first
 *  looks in it's cache to see if it has an engine or coupon pricer for this key already
 *  if so it is returned, otherwise a new engine or coupon pricer is created, stored and
 *  returned.
 *
 *  The first template argument is the cache key type (e.g. a std::string)
 *  The second template argument is PricingEngine or FloatingRateCouponPricer
 *  The remaining variable arguments are to be passed to engine() and
 *  engineImpl(), these are the specific parameters required to build
 *  an engine or coupon pricer for this trade type.
    \ingroup builders
 */
template <class T, class U, typename... Args> class CachingEngineBuilder : public EngineBuilder {
public:
    /*! Constructor that takes a model and engine name
        @param model the model name
        @param engine the engine name
        @param tradeTypes a set of trade types
     */
    CachingEngineBuilder(const string& model, const string& engine, const set<string>& tradeTypes)
        : EngineBuilder(model, engine, tradeTypes) {}

    //! Return a PricingEngine or a FloatingRateCouponPricer
    QuantLib::ext::shared_ptr<U> engine(Args... params) {
        T key = keyImpl(params...);
        if (engines_.find(key) == engines_.end()) {
            // build first (in case it throws)
            QuantLib::ext::shared_ptr<U> engine = engineImpl(params...);
            // then add to map
            engines_[key] = engine;
        }
        return engines_[key];
    }

    void reset() override { engines_.clear(); }

protected:
    virtual T keyImpl(Args...) = 0;
    virtual QuantLib::ext::shared_ptr<U> engineImpl(Args...) = 0;

    map<T, QuantLib::ext::shared_ptr<U>> engines_;
};

template <class T, typename... Args>
using CachingPricingEngineBuilder = CachingEngineBuilder<T, PricingEngine, Args...>;

template <class T, typename... Args>
using CachingCouponPricerBuilder = CachingEngineBuilder<T, FloatingRateCouponPricer, Args...>;

template <class T, typename... Args>
using CachingInflationCouponPricerBuilder = CachingEngineBuilder<T, InflationCouponPricer, Args...>;

template <class T, typename... Args>
using CachingInflationCashFlowPricerBuilder = CachingEngineBuilder<T, QuantExt::InflationCashFlowPricer, Args...>;

} // namespace data
} // namespace ore
