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

/*! \file ored/model/calibrationinstrumentfactory.hpp
    \brief factory for making calibration instruments.

    The idea here is taken from: https://stackoverflow.com/a/582456/1771882 and is used in 
    \c ore::data::LegDataFactory also.

    \ingroup models
*/

#pragma once

#include <boost/make_shared.hpp>
#include <functional>
#include <map>
#include <ored/model/calibrationbasket.hpp>
#include <ored/utilities/log.hpp>
#include <ql/patterns/singleton.hpp>

namespace ore {
namespace data {

/*! Function that is used to build instances of CalibrationInstrument

    The template parameter is simply a particular instance of a \c CalibrationInstrument class that is default
    constructible. The function returns the default constructed CalibrationInstrument object. A simple example is the
    function to build an instance of \c CpiCapFloor would be called via \c createLegData<CpiCapFloor>().

    \ingroup models
*/
template <class T>
QuantLib::ext::shared_ptr<CalibrationInstrument> createCalibrationInstrument() { return QuantLib::ext::make_shared<T>(); }

/*! Calibration instrument factory class

    This class is a repository of functions that can build instances of \c CalibrationInstrument. The functions are 
    keyed on the calibration instrument type that they can build. An instance of this factory class can be asked to 
    build a particular instance of the CalibrationInstrument class via a call to 
    <code>build(const std::string& instrumentType)</code> with the correct calibration instrument type, 
    \c instrumentType. For example, a call to <code>build("CpiCapFloor")</code> should return a \c CpiCapFloor
    instance if the CpiCapFloor calibration instrument building function has been added to the factory.

    It is up to each class derived from \c CalibrationInstrument to register itself with the 
    \c CalibrationInstrumentFactory via the \c CalibrationInstrumentRegister class below. All registration does is add 
    a function that can build an instance of that class to the factory and store it against its calibration instrument 
    type key.

    \ingroup models
*/
class CalibrationInstrumentFactory
    : public QuantLib::Singleton<CalibrationInstrumentFactory, std::integral_constant<bool, true>> {

    friend class QuantLib::Singleton<CalibrationInstrumentFactory, std::integral_constant<bool, true>>;

public:
    /*! The container type used to store the calibration instrument type key and the function that will be used to
        build a default instance of that calibration instrument type.
    */
    typedef std::map<std::string, std::function<QuantLib::ext::shared_ptr<CalibrationInstrument>()>> map_type;

    /*! A call to \c build should return an instance of \c CalibrationInstrument corresponding to the required
        \p instrumentType. For example, a call to <code>build("CpiCapFloor")</code> should return a \c CpiCapFloor
        instance.

        \warning If the \p instrumentType has not been added to the factory then a call to this method for that
                 \p instrumentType will return a \c nullptr
    */
    QuantLib::ext::shared_ptr<CalibrationInstrument> build(const std::string& instrumentType);

    /*! Add a builder function \p builder for a given \p instrumentType
     */
    void addBuilder(const std::string& instrumentType,
                    std::function<QuantLib::ext::shared_ptr<CalibrationInstrument>()> builder,
                    const bool allowOverwrite = false);

private:
    boost::shared_mutex mutex_;
    map_type map_;
};

}
}
