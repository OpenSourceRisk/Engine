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

/*! \file portfolio/referencedatafactory.hpp
    \brief Reference data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <map>
#include <functional>
#include <ql/patterns/singleton.hpp>
#include <string>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_types.hpp>

namespace ore {
namespace data {

class ReferenceDatum;

class AbstractReferenceDatumBuilder {
public:
    virtual ~AbstractReferenceDatumBuilder() {}
    virtual QuantLib::ext::shared_ptr<ReferenceDatum> build() const = 0;
};

//! Template TradeBuilder class
/*!
  \ingroup tradedata
*/
template <class T> class ReferenceDatumBuilder : public AbstractReferenceDatumBuilder {
public:
    virtual QuantLib::ext::shared_ptr<ReferenceDatum> build() const override { return QuantLib::ext::make_shared<T>(); }
};

class ReferenceDatumFactory : public QuantLib::Singleton<ReferenceDatumFactory, std::integral_constant<bool, true>> {

    friend class QuantLib::Singleton<ReferenceDatumFactory, std::integral_constant<bool, true>>;

public:
    typedef std::map<std::string, std::function<QuantLib::ext::shared_ptr<AbstractReferenceDatumBuilder>()>> map_type;

    QuantLib::ext::shared_ptr<ReferenceDatum> build(const std::string& refDatumType);

    void addBuilder(const std::string& refDatumType,
                    std::function<QuantLib::ext::shared_ptr<AbstractReferenceDatumBuilder>()> builder,
                    const bool allowOverwrite = false);

private:
    boost::shared_mutex mutex_;
    map_type map_;
};

template <class T> QuantLib::ext::shared_ptr<AbstractReferenceDatumBuilder> createReferenceDatumBuilder() {
    return QuantLib::ext::make_shared<T>();
}

} // namespace data
} // namespace ore
