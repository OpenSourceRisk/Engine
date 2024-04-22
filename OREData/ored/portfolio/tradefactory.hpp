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

/*! \file ored/portfolio/tradefactory.hpp
    \brief Trade Factory
    \ingroup portfolio
*/

#pragma once

#include <map>
#include <ql/patterns/singleton.hpp>

#include <boost/make_shared.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>

namespace ore {
namespace data {

class Trade;

//! TradeBuilder base class
/*! All derived classes have to be stateless. It should not be necessary to derive classes
  other than TradeBuilder from this class anyway.

  \ingroup portfolio
*/
class AbstractTradeBuilder {
public:
    virtual ~AbstractTradeBuilder() {}
    virtual QuantLib::ext::shared_ptr<Trade> build() const = 0;
};

//! Template TradeBuilder class
/*!
  \ingroup tradedata
*/
template <class T> class TradeBuilder : public AbstractTradeBuilder {
public:
    virtual QuantLib::ext::shared_ptr<Trade> build() const override { return QuantLib::ext::make_shared<T>(); }
};

//! TradeFactory
/*!
  \ingroup tradedata
*/
class TradeFactory : public QuantLib::Singleton<TradeFactory, std::integral_constant<bool, true>> {
    std::map<std::string, QuantLib::ext::shared_ptr<AbstractTradeBuilder>> builders_;
    mutable boost::shared_mutex mutex_;

public:
    std::map<std::string, QuantLib::ext::shared_ptr<AbstractTradeBuilder>> getBuilders() const;
    QuantLib::ext::shared_ptr<AbstractTradeBuilder> getBuilder(const std::string& tradeType) const;
    void addBuilder(const std::string& tradeType, const QuantLib::ext::shared_ptr<AbstractTradeBuilder>& builder,
                    const bool allowOverwrite = false);

    //! Build, throws for unknown className
    QuantLib::ext::shared_ptr<Trade> build(const std::string& className) const;
};

} // namespace data
} // namespace ore
