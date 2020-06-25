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

#include <functional>
#include <ql/patterns/singleton.hpp>
#include <string>

namespace ore {
namespace data {

class ReferenceDatum;

class AbstractReferenceDatumBuilder {
public:
    virtual ~AbstractReferenceDatumBuilder() {}
    virtual boost::shared_ptr<ReferenceDatum> build() const = 0;
};

//! Template TradeBuilder class
/*!
  \ingroup tradedata
*/
template <class T> class ReferenceDatumBuilder : public AbstractReferenceDatumBuilder {
public:
    virtual boost::shared_ptr<ReferenceDatum> build() const override { return boost::make_shared<T>(); }
};

class ReferenceDatumFactory : public QuantLib::Singleton<ReferenceDatumFactory> {

    friend class QuantLib::Singleton<ReferenceDatumFactory>;

public:
    typedef std::map<std::string, std::function<boost::shared_ptr<AbstractReferenceDatumBuilder>()>> map_type;

    boost::shared_ptr<ReferenceDatum> build(const std::string& refDatumType);

    void addBuilder(const std::string& refDatumType,
                    std::function<boost::shared_ptr<AbstractReferenceDatumBuilder>()> builder);

private:
    map_type map_;
};

template <class T> boost::shared_ptr<AbstractReferenceDatumBuilder> createReferenceDatumBuilder() {
    return boost::make_shared<T>();
}

template <class T> struct ReferenceDatumRegister {
public:
    ReferenceDatumRegister(const std::string& refDatumType) {
        ReferenceDatumFactory::instance().addBuilder(refDatumType, &createReferenceDatumBuilder<T>);
    }
};

} // namespace data
} // namespace ore
