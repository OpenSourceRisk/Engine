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

#include <qle/indexes/compositeindex.hpp>

#include <qle/indexes/bondindex.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/genericindex.hpp>
#include <ql/time/calendars/jointcalendar.hpp>

namespace QuantExt {

CompositeIndex::CompositeIndex(const std::string& name, const std::vector<QuantLib::ext::shared_ptr<QuantLib::Index>>& indices,
                               const std::vector<Real>& weights,
                               const std::vector<QuantLib::ext::shared_ptr<FxIndex>>& fxConversion)
    : name_(name), indices_(indices), weights_(weights), fxConversion_(fxConversion) {
    QL_REQUIRE(indices_.size() == weights_.size(), "CompositeIndex: indices size (" << indices_.size()
                                                                                    << ") must match weights size ("
                                                                                    << weights_.size() << ")");
    QL_REQUIRE(fxConversion_.empty() || fxConversion_.size() == indices_.size(),
               "CompositeIndex: fx conversion size (" << fxConversion_.size() << ") must match indices size ("
                                                      << indices_.size() << ")");

    for (auto const& f : fxConversion) {
        registerWith(f);
    }

    std::vector<Calendar> cals;
    for (auto const& i : indices) {
        registerWith(i);
        cals.push_back(i->fixingCalendar());
    }

    fixingCalendar_ = JointCalendar(cals);
}

std::string CompositeIndex::name() const { return name_; }

Calendar CompositeIndex::fixingCalendar() const { return fixingCalendar_; }

bool CompositeIndex::isValidFixingDate(const Date& fixingDate) const {
    return fixingCalendar_.isBusinessDay(fixingDate);
}

Real CompositeIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing) const {
    Real result = 0.0;
    for (Size i = 0; i < indices_.size(); ++i) {
        Real indexFixing;
        try {
            indexFixing = indices_[i]->fixing(fixingDate, forecastTodaysFixing);
        } catch (const std::exception&) {
            auto gi = QuantLib::ext::dynamic_pointer_cast<QuantExt::GenericIndex>(indices_[i]);
            if (gi && gi->expiry() != Date() && fixingDate >= gi->expiry())
                indexFixing = 0.0; 
            else
                throw;
        }

        // if the fixing date is not a valid fx fixing date, adjust the latter to the preceding valid date
        result += indexFixing * weights_[i] *
                  (fxConversion_.empty() || fxConversion_[i] == nullptr
                       ? 1.0
                       : fxConversion_[i]->fixing(fxConversion_[i]->fixingCalendar().adjust(fixingDate, Preceding),
                                                  forecastTodaysFixing));
    }
    return result;
}

Real CompositeIndex::dividendsBetweenDates(const Date& startDate, const Date& endDate) const {
    const Date& today = Settings::instance().evaluationDate();
    Real dividends = 0.0;
    for (Size i = 0; i < indices_.size(); ++i) {
        if (auto ei = QuantLib::ext::dynamic_pointer_cast<EquityIndex2>(indices_[i])) {
            for (auto const& d : ei->dividendFixings()) {
                if (d.exDate >= startDate && d.exDate <= std::min(endDate, today)) {
                    // if the fixing date is not a valid fx fixing date, adjust the latter to the preceding valid date
                    dividends +=
                        d.rate * weights_[i] *
                        (fxConversion_.empty() || fxConversion_[i] == nullptr
                             ? 1.0
                             : fxConversion_[i]->fixing(fxConversion_[i]->fixingCalendar().adjust(d.exDate, Preceding)));
                }
            }
        }
    }
    return dividends;
}

std::vector<std::pair<QuantLib::Date, std::string>> CompositeIndex::dividendFixingDates(const Date& startDate,
    const Date& endDate) {

    std::vector<std::pair<QuantLib::Date, std::string>> fixings;
    const Date& eDate = endDate == Date() ? Settings::instance().evaluationDate() : endDate;
    for (Size i = 0; i < indices_.size(); ++i) {
        if (QuantLib::ext::dynamic_pointer_cast<EquityIndex2>(indices_[i]) && !fxConversion_.empty() && fxConversion_[i]) {
            Date d = fxConversion_[i]->fixingCalendar().adjust(startDate, Preceding);
            while (d <= eDate) {
                fixings.push_back(std::make_pair<Date, std::string>(
                        fxConversion_[i]->fixingCalendar().adjust(d, Preceding), fxConversion_[i]->name()));
                d = fxConversion_[i]->fixingCalendar().advance(d, 1, Days);
            }
        }
    }
    return fixings;
 }

} // namespace QuantExt
