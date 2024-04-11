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

/*! \file ored/marketdata/strike.hpp
    \brief Classes for representing a strike using various conventions.
    \ingroup marketdata
*/

#pragma once

#include <ql/experimental/fx/deltavolquote.hpp>
#include <ql/option.hpp>
#include <ql/types.hpp>

#include <boost/optional.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/optional.hpp>
#include <ql/shared_ptr.hpp>

namespace ore {
namespace data {

/*! Abstract base class to hold information that describes a strike.
    Need to use Base in class Name to differentiate from existing ore::data::Strike.
*/
class BaseStrike {
public:
    virtual ~BaseStrike() {}

    //! Populate the Strike object from \p strStrike
    virtual void fromString(const std::string& strStrike) = 0;

    //! Write the Strike object to string
    virtual std::string toString() const = 0;

    //! Will be used for Strike comparison.
    friend bool operator==(const BaseStrike& lhs, const BaseStrike& rhs);

protected:
    //! Override in derived classes to compare specific Strikes.
    virtual bool equal_to(const BaseStrike& other) const = 0;

private:
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

/*! Strike implementation where the strike is described by a single number that represents the absolute strike level.
 */
class AbsoluteStrike : public BaseStrike {
public:
    //! Default constructor.
    AbsoluteStrike();

    //! Constructor with explicit strike.
    AbsoluteStrike(QuantLib::Real strike);

    //! Return the absolute strike level.
    QuantLib::Real strike() const;

    /*! Populate AbsoluteStrike object from \p strStrike which should be a number.
        An exception is thrown if \p strStrike cannot be parsed as a \c QuantLib::Real.
    */
    void fromString(const std::string& strStrike) override;

    /*! Writes the AbsoluteStrike object to string. This returns the string representation of
        the absolute strike number.
    */
    std::string toString() const override;

protected:
    bool equal_to(const BaseStrike& other) const override;

private:
    QuantLib::Real strike_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

/*! Strike implementation where the strike is described by a delta type, an option type and a delta level.
 */
class DeltaStrike : public BaseStrike {
public:
    //! Default constructor.
    DeltaStrike();

    //! Explicit constructor
    DeltaStrike(QuantLib::DeltaVolQuote::DeltaType deltaType, QuantLib::Option::Type optionType, QuantLib::Real delta);

    //! \name Inspectors
    //@{
    //! Return the delta type
    QuantLib::DeltaVolQuote::DeltaType deltaType() const;

    //! Return the option type
    QuantLib::Option::Type optionType() const;

    //! Return the delta level
    QuantLib::Real delta() const;
    //@}

    /*! Populate DeltaStrike object from \p strStrike.

        The \p strStrike is expected to be of the form `DEL / Spot|Fwd|PaSpot|PaFwd / Call|Put / DELTA_VALUE`. An
        exception is thrown if \p strStrike is not of this form and cannot be parsed properly.
    */
    void fromString(const std::string& strStrike) override;

    /*! Writes the DeltaStrike object to string.

        The string representation of the DeltaStrike object is of the form
        `DEL / Spot|Fwd|PaSpot|PaFwd / Call|Put / DELTA_VALUE`.
    */
    std::string toString() const override;

protected:
    bool equal_to(const BaseStrike& other) const override;

private:
    QuantLib::DeltaVolQuote::DeltaType deltaType_;
    QuantLib::Option::Type optionType_;
    QuantLib::Real delta_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

/*! Strike implementation for an at-the-money strike of various types.
 */
class AtmStrike : public BaseStrike {
public:
    //! Default constructor.
    AtmStrike();

    /*! Explicit constructor. Note that:
        - an \p atmType of \c AtmNull throws an error.
        - if \p atmType is \c AtmDeltaNeutral, a \c deltaType is needed.
        - if \p atmType is not \c AtmDeltaNeutral, \c deltaType should not be provided.
        - if \p atmType is \c AtmPutCall50, \p deltaType must be \c DeltaVolQuote::Fwd.
    */
    AtmStrike(QuantLib::DeltaVolQuote::AtmType atmType,
              boost::optional<QuantLib::DeltaVolQuote::DeltaType> deltaType = boost::none);

    //! \name Inspectors
    //@{
    //! Return the ATM type
    QuantLib::DeltaVolQuote::AtmType atmType() const;

    //! Return the delta type
    boost::optional<QuantLib::DeltaVolQuote::DeltaType> deltaType() const;
    //@}

    /*! Populate AtmStrike object from \p strStrike.

        The \p strStrike is expected to be of the form:
        `ATM / AtmSpot|AtmFwd|AtmDeltaNeutral|AtmVegaMax|AtmGammaMax|AtmPutCall50` followed by an optional
        `/ DEL / Spot|Fwd|PaSpot|PaFwd` to specify the delta if it is needed. An exception is thrown if \p strStrike
        is not of this form and cannot be parsed properly.
    */
    void fromString(const std::string& strStrike) override;

    /*! Writes the AtmStrike object to string.

        The string representation of the DeltaStrike object is of the form
        `ATM / AtmSpot|AtmFwd|AtmDeltaNeutral|AtmVegaMax|AtmGammaMax|AtmPutCall50` followed by an optional
        `/ DEL / Spot|Fwd|PaSpot|PaFwd` if the delta type has been populated.
    */
    std::string toString() const override;

protected:
    bool equal_to(const BaseStrike& other) const override;

private:
    QuantLib::DeltaVolQuote::AtmType atmType_;
    boost::optional<QuantLib::DeltaVolQuote::DeltaType> deltaType_;

    //! Perform validation
    void check() const;

    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

/*! Strike implementation where the strike is described by a moneyness type and a moneyness level.
 */
class MoneynessStrike : public BaseStrike {
public:
    /*! The MoneynessStrike type.
        - When the moneyness type is \c Spot, the moneyness level will be interpreted as the implicit strike divided
          by the spot value.
        - When the moneyness type is \c Forward, the moneyness level will be interpreted as the implicit strike
          divided by the forward value.
    */
    enum class Type { Spot, Forward };

    //! Default constructor.
    MoneynessStrike();

    //! Explicit constructor.
    MoneynessStrike(Type type, QuantLib::Real moneyness);

    //! \name Inspectors
    //@{
    //! Return the moneyness type
    Type type() const;

    //! Return the moneyness level.
    QuantLib::Real moneyness() const;
    //@}

    /*! Populate MoneynessStrike object from \p strStrike.

        The \p strStrike is expected to be of the form `MNY / Spot|Fwd / MONEYNESS_VALUE`.
    */
    void fromString(const std::string& strStrike) override;

    /*! Writes the MoneynessStrike object to string.

        The string representation of the MoneynessStrike object is of the form `MNY / Spot|Fwd / MONEYNESS_VALUE`.
    */
    std::string toString() const override;

protected:
    bool equal_to(const BaseStrike& other) const override;

private:
    Type type_;
    QuantLib::Real moneyness_;

    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Write \p strike to stream.
std::ostream& operator<<(std::ostream& os, const BaseStrike& strike);

//! Write \p deltaType to stream. Not provided in QuantLib so add it here.
std::ostream& operator<<(std::ostream& os, QuantLib::DeltaVolQuote::DeltaType type);

//! Write \p atmType to stream. Not provided in QuantLib so add it here.
std::ostream& operator<<(std::ostream& os, QuantLib::DeltaVolQuote::AtmType type);

//! Write MoneynessStrike::Type, \p type, to stream.
std::ostream& operator<<(std::ostream& os, MoneynessStrike::Type type);

//! Parse MoneynessStrike::Type from \p type
MoneynessStrike::Type parseMoneynessType(const std::string& type);

//! Parse a Strike from its string representation, \p strStrike.
QuantLib::ext::shared_ptr<BaseStrike> parseBaseStrike(const std::string& strStrike);

template <class Archive> void registerBaseStrike(Archive& ar) {
    ar.template register_type<AbsoluteStrike>();
    ar.template register_type<DeltaStrike>();
    ar.template register_type<AtmStrike>();
    ar.template register_type<MoneynessStrike>();
}

} // namespace data
} // namespace ore

BOOST_CLASS_EXPORT_KEY(ore::data::AbsoluteStrike);
BOOST_CLASS_EXPORT_KEY(ore::data::DeltaStrike);
BOOST_CLASS_EXPORT_KEY(ore::data::AtmStrike);
BOOST_CLASS_EXPORT_KEY(ore::data::MoneynessStrike);
