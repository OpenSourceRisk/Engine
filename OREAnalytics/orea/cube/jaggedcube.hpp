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

/*! \file orea/cube/jaggedcube.hpp
    \brief A cube implementation that stores the cube in memory
    \ingroup cube
*/
#pragma once

#include <fstream>
#include <iostream>
#include <ql/errors.hpp>
#include <vector>

#include <boost/make_shared.hpp>
#include <orea/cube/npvcube.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/serializationdate.hpp>

namespace ore {
namespace analytics {
using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Size;
using std::vector;

template <typename T> class TradeBlock {
private:
    Size dateLen_;
    Size depth_;
    Size samples_;
    vector<T> data_;

public:
    TradeBlock() : dateLen_(0), depth_(0), samples_(0), data_() {}

    TradeBlock(Size inputDates, Size inputDepth, Size samples)
        : dateLen_(inputDates), depth_(inputDepth), samples_(samples) {
        Size len = (1 + (dateLen_ * samples_)) * depth_;
        data_ = vector<T>(len, T());
    }

    Size index(Size date, Size dep, Size sample) const {
        // calculate index of element
        return depth_ + // T0 block
               dep + (sample * depth_) + (date * depth_ * samples_);
    }

    Size indexT0(Size dep) const { return dep; }

    bool isValid(Size date, Size dep, Size sample) const {
        // return true if this is valid
        return date < dateLen_ && sample < samples_ && dep < depth_;
    }

    bool isValidT0(Size dep) const { return dep < depth_; }

    Real getT0(Size dep) const {
        if (this->isValidT0(dep)) {
            return static_cast<Real>(this->data_[indexT0(dep)]);
        } else {
            return 0;
        }
    }

    void setT0(Real value, Size dep) {
        if (this->isValidT0(dep)) {
            this->data_[indexT0(dep)] = static_cast<T>(value);
        } else {
            QL_REQUIRE(value == 0, "Cannot set nonzero value for T0 dep = " << dep);
        }
    }

    Real get(Size date, Size sample, Size dep) const {
        // if isValid return the value cast to a Real
        if (this->isValid(date, dep, sample)) {
            return static_cast<Real>(this->data_[index(date, dep, sample)]);
        } else {
            return 0;
        }
    }

    void set(Real value, Size date, Size sample, Size dep) {
        // if isValid set in the buffer
        if (this->isValid(date, dep, sample)) {
            this->data_[index(date, dep, sample)] = static_cast<T>(value);
        } else {
            QL_REQUIRE(value == 0,
                       "Cannot set nonzero value for date: " << date << ", depth: " << dep << ", sample: " << sample);
        }
    }

private:
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int) {
        ar& depth_;
        ar& dateLen_;
        ar& samples_;
        ar& data_;
    }
};

class DepthCalculator {
public:
    virtual ~DepthCalculator() {}
    virtual Size depth(const QuantLib::ext::shared_ptr<ore::data::Trade>& t) const = 0;
};

class ConstantDepthCalculator : public DepthCalculator {
public:
    ConstantDepthCalculator(Size d = 1) : d_(d) {}
    Size depth(const QuantLib::ext::shared_ptr<ore::data::Trade>&) const override { return d_; }

private:
    Size d_;
};

//! JaggedCube stores the cube in memory using a vector of trade specific blocks
/*! JaggedCube stores the cube in memory using a vector of trade specific blocks
 *  to allow both single and double precision implementations.
 *
 \ingroup cube
 */
template <typename T> class JaggedCube : public ore::analytics::NPVCube {
public:
    JaggedCube(Date asof, QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio, const vector<Date>& dates, Size samples,
               Size depth) {
        init(asof, portfolio, dates, samples, ConstantDepthCalculator(depth));
    }

    JaggedCube(Date asof, QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio, const vector<Date>& dates, Size samples,
               const DepthCalculator& dc) {
        init(asof, portfolio, dates, samples, dc);
    }

    // go back to depth, dep??? they are different
    void init(Date asof, QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio, const vector<Date>& dates, Size samples,
              const DepthCalculator& dc) {
        asof_ = asof;
        dates_ = dates;
        samples_ = samples;
        maxDepth_ = 0;
        // Loop over each trade, calculate it's dateLength by checking the trade maturity
        // get depth with the depth calculator
        // create a TradeBlock with these parameters and store it.
        //
        // setup asof, .....
        Size pos = 0;
        for (const auto& [tid, t] : portfolio->trades()) {
            // for each trade: set id and set a tradeblock
            ids_[tid] = pos++;
            // TradeBlock
            Size depth = dc.depth(t);
            maxDepth_ = std::max(maxDepth_, depth);

            Size dateLen = 0;
            while (dateLen < dates_.size() && dates_[dateLen] < t->maturity())
                dateLen++;
            blocks_.push_back(TradeBlock<T>(dateLen, depth, samples));
        }
    }

    //! Return the length of each dimension
    Size numIds() const override { return ids_.size(); }
    Size numDates() const override { return dates_.size(); }
    Size samples() const override { return samples_; }
    Size depth() const override { return maxDepth_; }

    Real avgDateLen() const {
        // loop  over tradeblocks
        Size dateTotal = 0;
        Real dateLenAvg = 0.0;
        for (auto b : blocks_)
            dateTotal += b.dateLen;
        // get date length of each
        // return avg date (float)
        dateLenAvg = dateTotal / blocks_.size();
        return dateLenAvg;
    }

    Real avgDepth() const {
        // same as above
        Size depthTotal = 0;
        Real depthAvg = 0.0;
        for (auto b : blocks_)
            depthTotal += b.depth;
        depthAvg = depthTotal / blocks_.size();
        return depthAvg;
    }

    //! Get the vector of ids for this cube
    const std::map<std::string, Size>& idsAndIndexes() const override { return ids_; }
    //! Get the vector of dates for this cube
    const std::vector<QuantLib::Date>& dates() const override { return dates_; }

    //! Return the asof date (T0 date)
    QuantLib::Date asof() const override { return asof_; }

    //! Get a T0 value from the cube
    Real getT0(Size i, Size d) const override {
        QL_REQUIRE(i < blocks_.size(), "Invalid trade index i " << i);
        return blocks_[i].getT0(d);
    }

    //! Set a value in the cube
    void setT0(Real value, Size i, Size d) override {
        QL_REQUIRE(i < blocks_.size(), "Invalid trade index i " << i);
        TradeBlock<T>& tb = blocks_[i];
        return tb.setT0(value, d);
    }

    //! Get a value from the cube
    Real get(Size i, Size j, Size k, Size d) const override {
        check(i, j, k, d);
        return blocks_[i].get(j, k, d);
    }

    //! Set a value in the cube
    void set(Real value, Size i, Size j, Size k, Size d) override {
        check(i, j, k, d);
        TradeBlock<T>& tb = blocks_[i];
        return tb.set(value, j, k, d);
    }

protected:
    void check(Size i, Size j, Size k, Size d) const {
        QL_REQUIRE(i < numIds(), "Out of bounds on ids (i=" << i << ")");
        QL_REQUIRE(j < numDates(), "Out of bounds on dates (j=" << j << ")");
        QL_REQUIRE(k < samples(), "Out of bounds on samples (k=" << k << ")");
        QL_REQUIRE(d < depth(), "Out of bounds on depth (d=" << d << ")");
    }

private:
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int) {
        ar& asof_;
        ar& ids_;
        ar& dates_;
        ar& samples_;
        ar& maxDepth_;
        ar& blocks_;
    }

    QuantLib::Date asof_;
    std::map<std::string, Size> ids_;
    vector<QuantLib::Date> dates_;
    Size samples_;
    Size maxDepth_;
    vector<TradeBlock<T>> blocks_;
};

//! Jagged cube with single precision floating point numbers.
using SinglePrecisionJaggedCube = JaggedCube<float>;

//! Jagged cube with double precision floating point numbers.
using DoublePrecisionJaggedCube = JaggedCube<double>;
} // namespace analytics
} // namespace ore
