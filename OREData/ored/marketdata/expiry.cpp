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

#include <ored/marketdata/expiry.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/serialization/export.hpp>

using namespace QuantLib;
using std::ostream;
using std::string;

namespace ore {
namespace data {

bool operator==(const Expiry& lhs, const Expiry& rhs) { return lhs.equal_to(rhs); }

ExpiryDate::ExpiryDate() {}

ExpiryDate::ExpiryDate(const Date& expiryDate) : expiryDate_(expiryDate) {}

const Date& ExpiryDate::expiryDate() const { return expiryDate_; }

void ExpiryDate::fromString(const string& strExpiryDate) { expiryDate_ = parseDate(strExpiryDate); }

string ExpiryDate::toString() const { return to_string(expiryDate_); }

bool ExpiryDate::equal_to(const Expiry& other) const {
    if (const ExpiryDate* p = dynamic_cast<const ExpiryDate*>(&other)) {
        return expiryDate_ == p->expiryDate();
    } else {
        return false;
    }
}

ExpiryPeriod::ExpiryPeriod() {}

ExpiryPeriod::ExpiryPeriod(const Period& expiryPeriod) : expiryPeriod_(expiryPeriod) {}

const Period& ExpiryPeriod::expiryPeriod() const { return expiryPeriod_; }

void ExpiryPeriod::fromString(const string& strExpiryPeriod) { expiryPeriod_ = parsePeriod(strExpiryPeriod); }

string ExpiryPeriod::toString() const { return to_string(expiryPeriod_); }

bool ExpiryPeriod::equal_to(const Expiry& other) const {
    if (const ExpiryPeriod* p = dynamic_cast<const ExpiryPeriod*>(&other)) {
        return expiryPeriod_ == p->expiryPeriod();
    } else {
        return false;
    }
}

FutureContinuationExpiry::FutureContinuationExpiry(QuantLib::Natural expiryIndex) : expiryIndex_(expiryIndex) {}

QuantLib::Natural FutureContinuationExpiry::expiryIndex() const { return expiryIndex_; }

void FutureContinuationExpiry::fromString(const string& strIndex) {
    QL_REQUIRE(strIndex.size() > 1, "Future continuation expiry must have at least 2 characters");
    QL_REQUIRE(strIndex.at(0) == 'c', "Future continuation expiry string must start with a 'c'");
    expiryIndex_ = parseInteger(strIndex.substr(1));
}

string FutureContinuationExpiry::toString() const { return "c" + to_string(expiryIndex_); }

bool FutureContinuationExpiry::equal_to(const Expiry& other) const {
    if (const FutureContinuationExpiry* p = dynamic_cast<const FutureContinuationExpiry*>(&other)) {
        return expiryIndex_ == p->expiryIndex();
    } else {
        return false;
    }
}

ostream& operator<<(ostream& os, const Expiry& expiry) { return os << expiry.toString(); }

boost::shared_ptr<Expiry> parseExpiry(const string& strExpiry) {

    QL_REQUIRE(strExpiry.size() > 1, "Expiry string must have at least 2 characters");

    if (strExpiry.at(0) == 'c') {
        auto expiry = boost::make_shared<FutureContinuationExpiry>();
        expiry->fromString(strExpiry);
        return expiry;
    } else {
        Date date;
        Period period;
        bool isDate;
        parseDateOrPeriod(strExpiry, date, period, isDate);
        if (isDate) {
            return boost::make_shared<ExpiryDate>(date);
        } else {
            return boost::make_shared<ExpiryPeriod>(period);
        }
    }
}

} // namespace data
} // namespace ore

BOOST_CLASS_EXPORT_GUID(ore::data::ExpiryDate, "ExpiryDate");
BOOST_CLASS_EXPORT_GUID(ore::data::ExpiryPeriod, "ExpiryPeriod");
BOOST_CLASS_EXPORT_GUID(ore::data::FutureContinuationExpiry, "FutureContinuationExpiry");
