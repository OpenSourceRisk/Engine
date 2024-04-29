/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file orea/app/analytics/analyticsfactory.hpp
    \brief Analytics Factory
    \ingroup analytics
*/

#pragma once

#include <map>
#include <ql/patterns/singleton.hpp>

#include <boost/make_shared.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <orea/app/inputparameters.hpp>

namespace ore {
namespace analytics {

class Analytic;

//! TradeBuilder base class
/*! All derived classes have to be stateless. It should not be necessary to derive classes
  other than TradeBuilder from this class anyway.

  \ingroup portfolio
*/
class AbstractAnalyticBuilder {
public:
    virtual ~AbstractAnalyticBuilder() {}
    virtual QuantLib::ext::shared_ptr<Analytic>
    build(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs) const = 0;
};

//! Template AnalyticBuilder class
/*!
  \ingroup analytics
*/
template <class T> class AnalyticBuilder : public AbstractAnalyticBuilder {
public:
    virtual QuantLib::ext::shared_ptr<Analytic>
    build(const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs) const override {
        return QuantLib::ext::make_shared<T>(inputs);
    }
};

//! AnalyticFactory
/*!
  \ingroup analytics
*/
class AnalyticFactory : public QuantLib::Singleton<AnalyticFactory, std::integral_constant<bool, true>> {
    std::map<std::string, std::pair<std::set<std::string>, QuantLib::ext::shared_ptr<AbstractAnalyticBuilder>>>
        builders_;
    mutable boost::shared_mutex mutex_;

public:
    std::map<std::string,
        std::pair<std::set<std::string>, QuantLib::ext::shared_ptr<AbstractAnalyticBuilder>>> getBuilders() const;
    std::pair < std::string,
        QuantLib::ext::shared_ptr<AbstractAnalyticBuilder>> getBuilder(const std::string& analyticType) const;
    void addBuilder(const std::string& className, const std::set<std::string>& subAnalytics,
                    const QuantLib::ext::shared_ptr<AbstractAnalyticBuilder>& builder,
                    const bool allowOverwrite = false);

    //! Build, throws for unknown className
    std::pair<std::string, QuantLib::ext::shared_ptr<Analytic>> build(const std::string& subAnalytic, 
        const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& inputs) const;
};

} // namespace analytics
} // namespace ore
