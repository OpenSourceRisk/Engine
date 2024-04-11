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

/*! \file ored/marketdata/loader.hpp
    \brief Market Datum Loader Interface
    \ingroup marketdata
    \invariant Concrete instantiations of this virtual base class guarantee
    that all of the MarketDatum objects that they store are unique, e.g. by
    discarding any duplicates during initialization.
*/

#pragma once

#include <ored/marketdata/fixings.hpp>
#include <ored/marketdata/marketdatum.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/wildcard.hpp>

#include <qle/indexes/dividendmanager.hpp>
#include <ql/time/date.hpp>

#include <ql/shared_ptr.hpp>

#include <vector>

namespace ore {
namespace data {

//! Market data loader base class
/*! \ingroup marketdata
 */
class Loader {
public:
    virtual ~Loader() {}

    //! \name Interface
    //@{

    //! get all quotes, TODO change the return value to std::set
    virtual std::vector<QuantLib::ext::shared_ptr<MarketDatum>> loadQuotes(const QuantLib::Date&) const = 0;

    //! get quote by its unique name, throws if not existent, override in derived classes for performance
    virtual QuantLib::ext::shared_ptr<MarketDatum> get(const std::string& name, const QuantLib::Date& d) const;

    //! get quotes matching a set of names, this should be overridden in derived classes for performance
    virtual std::set<QuantLib::ext::shared_ptr<MarketDatum>> get(const std::set<std::string>& names,
                                                         const QuantLib::Date& asof) const;

    //! get quotes matching a wildcard, this should be overriden in derived classes for performance
    virtual std::set<QuantLib::ext::shared_ptr<MarketDatum>> get(const Wildcard& wildcard, const QuantLib::Date& asof) const;

    //! Default implementation, returns false if get throws or returns a null pointer
    virtual bool has(const std::string& name, const QuantLib::Date& d) const;

    //! check if there are quotes for a date
    virtual bool hasQuotes(const QuantLib::Date& d) const;

    /*! Default implementation for get that allows for the market data item to be optional. The first element of
        the \p name pair is the name of the market point being sought and the second element of the \p name pair
        is a flag to indicate if the market data point is optional, <code>true</code>, or not, <code>false</code>.
        - if the quote is in the loader for date \p d, it is returned
        - if the quote is not in the loader for date \p d and it is optional,
          a warning is logged and a <code>QuantLib::ext::shared_ptr<MarketDatum>()</code> is returned
        - if the quote is not in the loader for date \p d and it is not optional, an exception is thrown
     */
    virtual QuantLib::ext::shared_ptr<MarketDatum> get(const std::pair<std::string, bool>& name, const QuantLib::Date& d) const;

    virtual std::set<Fixing> loadFixings() const = 0;

    virtual bool hasFixing(const string& name, const QuantLib::Date& d) const;

    //! Default implementation for getFixing
    virtual Fixing getFixing(const string& name, const QuantLib::Date& d) const;
    //@}

    //! Optional load dividends method
    virtual std::set<QuantExt::Dividend> loadDividends() const;

    void setActualDate(const QuantLib::Date& d) { actualDate_ = d; }
    const Date& actualDate() const { return actualDate_; }

	std::pair<bool, string> checkFxDuplicate(const ext::shared_ptr<MarketDatum>, const QuantLib::Date&);

private:
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version) {}

protected:
    /*! For lagged market data, where we need to take data from a different date but want to treat it as belonging to
       the valuation date.
     */
    Date actualDate_ = Date();
};
} // namespace data
} // namespace ore
