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

/*! \file ored/marketdata/expiry.hpp
    \brief Classes for representing an expiry for use in market quotes.
    \ingroup marketdata
*/

#pragma once

#include <ql/shared_ptr.hpp>
#include <ored/utilities/serializationdate.hpp>
#include <ored/utilities/serializationperiod.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>
#include <string>

#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>

namespace ore {
namespace data {

/*! Abstract base class to hold information that describes an expiry.
 */
class Expiry {
public:
    virtual ~Expiry() {}

    //! Populate the Expiry object from \p strExpiry
    virtual void fromString(const std::string& strExpiry) = 0;

    //! Write the Expiry object to string
    virtual std::string toString() const = 0;

    //! Will be used for Expiry comparison.
    friend bool operator==(const Expiry& lhs, const Expiry& rhs);

protected:
    //! Override in derived classes to compare specific expiries.
    virtual bool equal_to(const Expiry& other) const = 0;

private:
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

/*! Expiry consisting of an explicit expiry date
 */
class ExpiryDate : public Expiry {
public:
    //! Default constructor.
    ExpiryDate();

    //! Constructor with explicit expiry date.
    ExpiryDate(const QuantLib::Date& expiryDate);

    //! Return the expiry date.
    const QuantLib::Date& expiryDate() const;

    /*! Populate ExpiryDate object from \p strExpiryDate which should be a date.
        An exception is thrown if \p strExpiryDate cannot be parsed as a \c QuantLib::Date.
    */
    void fromString(const std::string& strExpiryDate) override;

    /*! Writes the ExpiryDate object to string. This returns the string representation of
        the expiry date.
    */
    std::string toString() const override;

protected:
    bool equal_to(const Expiry& other) const override;

private:
    QuantLib::Date expiryDate_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

/*! Expiry consisting of a period
 */
class ExpiryPeriod : public Expiry {
public:
    //! Default constructor.
    ExpiryPeriod();

    //! Constructor with expiry period.
    ExpiryPeriod(const QuantLib::Period& expiryPeriod);

    //! Return the expiry period.
    const QuantLib::Period& expiryPeriod() const;

    /*! Populate ExpiryPeriod object from \p strExpiryPeriod which should be a period.
        An exception is thrown if \p strExpiryPeriod cannot be parsed as a \c QuantLib::Period.
    */
    void fromString(const std::string& strExpiryPeriod) override;

    /*! Writes the ExpiryPeriod object to string. This returns the string representation of
        the expiry period.
    */
    std::string toString() const override;

protected:
    bool equal_to(const Expiry& other) const override;

private:
    QuantLib::Period expiryPeriod_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

/*! Expiry represented by a future continuation index
 */
class FutureContinuationExpiry : public Expiry {
public:
    //! Constructor with optional explicit future continuation index.
    FutureContinuationExpiry(QuantLib::Natural expiryIndex = 1);

    //! Return the future continuation expiry index.
    QuantLib::Natural expiryIndex() const;

    /*! Populate FutureContinuationExpiry object from \p strIndex which should be of the form \c c<Index> where
        Index is a positive integer. An exception is thrown if \p strIndex is not of this form.
    */
    void fromString(const std::string& strIndex) override;

    /*! Writes the FutureContinuationExpiry object to string. This returns the string representation of
        the future continuation index i.e. a string of the form \c c<Index>.
    */
    std::string toString() const override;

protected:
    bool equal_to(const Expiry& other) const override;

private:
    QuantLib::Natural expiryIndex_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Write \p strike to stream.
std::ostream& operator<<(std::ostream& os, const Expiry& expiry);

//! Parse an Expiry from its string representation, \p strExpiry.
QuantLib::ext::shared_ptr<Expiry> parseExpiry(const std::string& strExpiry);

} // namespace data
} // namespace ore

BOOST_CLASS_EXPORT_KEY(ore::data::ExpiryDate);
BOOST_CLASS_EXPORT_KEY(ore::data::ExpiryPeriod);
BOOST_CLASS_EXPORT_KEY(ore::data::FutureContinuationExpiry);
