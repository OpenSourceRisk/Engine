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

/*! \file orea/cube/npvcube.hpp
    \brief The base NPV cube class
    \ingroup cube
*/

#pragma once

#include <ql/shared_ptr.hpp>
#include <ql/errors.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>
#include <vector>
#include <map>
#include <set>

namespace ore {
namespace analytics {

//! NPV Cube class stores both future and current NPV values.
/*! The cube class stores future NPV values in a 4-D array.
 *
 *  This abstract base class is just used for the storage of a cube.
 *  This class also stores the tradeIds, dates and vector of T0 NPVs
 *
 *  The values in the cube must be set according to the following rules to ensure consistent behavior:
 *  - T0 values need to be set first using setT0(), in arbitrary order for (id, date, sample, depth), not all
 *    possible tuples have to be covered
 *  - after that the other values can be set using set(), again in arbitrary order for (id, date, sample, depth)
 *    and again not all possible tuples have to be covered
 *  - for each tuple (id, date, sample, depth) setT0() and set() should only be called once
 *
    \ingroup cube
 */
class NPVCube {
public:
    //! default ctor
    NPVCube() {}

    //! Do not allow cube copying
    NPVCube(NPVCube&) = delete;
    NPVCube& operator=(NPVCube const&) = delete;

    //! dtor
    virtual ~NPVCube() {}

    //! Return the length of each dimension
    virtual QuantLib::Size numIds() const = 0;
    virtual QuantLib::Size numDates() const = 0;
    virtual QuantLib::Size samples() const = 0;
    virtual QuantLib::Size depth() const = 0;

    //! Get a map of id and their index position in this cube 
    virtual const std::map<std::string, QuantLib::Size>& idsAndIndexes() const = 0;

    //! Get a set of all ids in the cube
    const std::set<std::string> ids() const {
        std::set<std::string> result;
        for (const auto& [id, pos] : idsAndIndexes()) {
            result.insert(id);
        }
        return result;
    }

    //! Get the vector of dates for this cube
    virtual const std::vector<QuantLib::Date>& dates() const = 0; // TODO: Should this be the full date grid?

    //! Return the asof date (T0 date)
    virtual QuantLib::Date asof() const = 0;
    //! Get a T0 value from the cube using index
    virtual QuantLib::Real getT0(QuantLib::Size id, QuantLib::Size depth = 0) const = 0;
    //! Get a T0 value from the cube using trade id
    virtual QuantLib::Real getT0(const std::string& id, QuantLib::Size depth = 0) const { return getT0(index(id), depth); };
    //! Set a value in the cube using index
    virtual void setT0(QuantLib::Real value, QuantLib::Size id, QuantLib::Size depth = 0) = 0;
    //! Set a value in the cube using trade id
    virtual void setT0(QuantLib::Real value, const std::string& id, QuantLib::Size depth = 0) {
        setT0(value, index(id), depth);
    };

    //! Get a value from the cube using index
    virtual QuantLib::Real get(QuantLib::Size id, QuantLib::Size date, QuantLib::Size sample,
                               QuantLib::Size depth = 0) const = 0;
    //! Set a value in the cube using index
    virtual void set(QuantLib::Real value, QuantLib::Size id, QuantLib::Size date, QuantLib::Size sample,
                     QuantLib::Size depth = 0) = 0;

    //! Get a value from the cube using trade id and date
    virtual QuantLib::Real get(const std::string& id, const QuantLib::Date& date, QuantLib::Size sample, QuantLib::Size depth = 0) const {
        return get(index(id), index(date), sample, depth);
    };
    //! Set a value in the cube using trade id and date
    virtual void set(QuantLib::Real value, const std::string& id, const QuantLib::Date& date, QuantLib::Size sample,
                     QuantLib::Size depth = 0) {
        set(value, index(id), index(date), sample, depth);
    }

    /*! Remove t0 values for a given id */
    virtual void removeT0(QuantLib::Size id);

    /*! Set non-t0 value to either 0 or the t0 value for a given id and sample. If sample is null, all samples are removed */
    virtual void remove(QuantLib::Size id, QuantLib::Size sample, bool setToT0Value);

    QuantLib::Size getTradeIndex(const std::string& id) const { return index(id); }
    QuantLib::Size getDateIndex(const QuantLib::Date& date) const { return index(date); }

    virtual bool usesDoublePrecision() const = 0;

protected:
    virtual QuantLib::Size index(const std::string& id) const {
        const auto& it = idsAndIndexes().find(id);
        QL_REQUIRE(it != idsAndIndexes().end(), "NPVCube can't find an index for id " << id);
        return it->second;
    };

    virtual QuantLib::Size index(const QuantLib::Date& date) const {
        auto it = std::find(dates().begin(), dates().end(), date);
        QL_REQUIRE(it != dates().end(), "NPVCube can't find an index for date " << date);
        return std::distance(dates().begin(), it);
    };

};

// impl

inline void NPVCube::removeT0(QuantLib::Size id) {
    for (QuantLib::Size depth = 0; depth < this->depth(); ++depth) {
        setT0(0.0, id, depth);
    }
}

inline void NPVCube::remove(QuantLib::Size id, QuantLib::Size sample, bool setToT0Value) {
    for (QuantLib::Size date = 0; date < this->numDates(); ++date) {
        for (QuantLib::Size depth = 0; depth < this->depth(); ++depth) {
            QuantLib::Real value = setToT0Value ? getT0(id, depth) : 0.0;
            if (sample != QuantLib::Null<QuantLib::Size>()) {
                set(value, id, date, sample, depth);
            } else {
                for (QuantLib::Size sample = 0; sample < this->samples(); ++sample) {
                    set(value, id, date, sample, depth);
                }
            }
        }
    }
}

} // namespace analytics
} // namespace ore
