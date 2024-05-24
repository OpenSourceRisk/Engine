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

/*! \file ored/portfolio/legdatafactory.hpp
    \brief Leg data factory that can be used to build instances of leg data

    The idea here is taken from: https://stackoverflow.com/a/582456/1771882

    \ingroup portfolio
*/

#pragma once

#include <boost/make_shared.hpp>
#include <functional>
#include <map>
#include <ored/utilities/log.hpp>
#include <ql/patterns/singleton.hpp>

namespace ore {
namespace data {

// Forward declare LegAdditionalData because legdata.hpp will include this file.
class LegAdditionalData;

/*! Function that is used to build instances of LegAdditionalData

    The template parameter is simply a particular instance of a \c LegAdditionalData class that is default
    constructible. The function returns the default constructed LegAdditionalData object. A simple example is the
    function to build an instance of \c FixedLegData would be called via \c createLegData<FixedLegData>().

    \ingroup portfolio
*/
template <class T> QuantLib::ext::shared_ptr<LegAdditionalData> createLegData() { return QuantLib::ext::make_shared<T>(); }

/*! Leg data factory class

    This class is a repository of functions that can build instances of \c LegAdditionalData. The functions are keyed
    on the leg data type that they can build. An instance of this factory class can be asked to build a particular
    instance of the LegAdditionalData class via a call to <code>build(const std::string& legType)</code> with the
    correct \c legType name. For example, a call to <code>build("Fixed")</code> should return a \c FixedLegData
    instance if the fixed leg data building function has been added to the factory.

    It is up to each class derived from \c LegAdditionalData to register itself with the \c LegDataFactory via the
    \c LegDataRegister class below. All registration does is add a function that can build an instance of that class
    to the factory and store it against its leg type key.

    \ingroup portfolio
*/
class LegDataFactory : public QuantLib::Singleton<LegDataFactory, std::integral_constant<bool, true>> {

    friend class QuantLib::Singleton<LegDataFactory, std::integral_constant<bool, true>>;

public:
    /*! The container type used to store the leg data type key and the function that will be used to build a default
        instance of that leg data type.
    */
    typedef std::map<std::string, std::function<QuantLib::ext::shared_ptr<LegAdditionalData>()>> map_type;

    /*! A call to \c build should return an instance of \c LegAdditionalData corresponding to the required \p legType.
        For example, a call to <code>build("Fixed")</code> should return a \c FixedLegData instance.

        \warning If the \p legType has not been added to the factory then a call to this method for that \p legType
                 will return a \c nullptr
    */
    QuantLib::ext::shared_ptr<LegAdditionalData> build(const std::string& legType);

    /*! Add a builder function \p builder for a given \p legType
     */
    void addBuilder(const std::string& legType, std::function<QuantLib::ext::shared_ptr<LegAdditionalData>()> builder,
                    const bool allowOverwrite = false);

private:
    boost::shared_mutex mutex_;
    map_type map_;
};

} // namespace data
} // namespace ore
