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

#include <boost/make_shared.hpp>
#include <ored/portfolio/trade.hpp>

namespace ore {
namespace data {

//! TradeBuilder base class
/*!
  \ingroup portfolio
*/
class AbstractTradeBuilder {
public:
    virtual ~AbstractTradeBuilder() {}
    virtual boost::shared_ptr<Trade> build() const = 0;
};

//! Template TradeBuilder class
/*!
  \ingroup tradedata
*/
template <class T> class TradeBuilder : public AbstractTradeBuilder {
public:
    virtual boost::shared_ptr<Trade> build() const override { return boost::make_shared<T>(); }
};

//! TradeFactory (todo)
/*!
  \ingroup tradedata
*/
class TradeFactory {
public:
    //! Construct a factory with the default builders
    TradeFactory();

    //! Add a new custom builder
    void addBuilder(const string& className, const boost::shared_ptr<AbstractTradeBuilder>&);

    //! Build, if className is unknown, an empty pointer is returned
    boost::shared_ptr<Trade> build(const string& className) const;

private:
    map<string, boost::shared_ptr<AbstractTradeBuilder>> builders_;
};
} // namespace data
} // namespace ore
