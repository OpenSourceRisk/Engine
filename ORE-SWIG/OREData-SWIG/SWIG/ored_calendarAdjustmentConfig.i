/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#ifndef ored_CalendarAdjustmentConfig_i
#define ored_CalendarAdjustmentConfig_i

%include ored_xmlutils.i

%shared_ptr(ore::data::CalendarAdjustmentConfig)
%shared_ptr(ore::data::CurrencyConfig)

namespace ore {
namespace data {
class CalendarAdjustmentConfig {
  public:

    CalendarAdjustmentConfig();

    void addHolidays(const std::string& calname, const Date& d);

    void addBusinessDays(const std::string& calname, const Date& d);

    void addBaseCalendar(const std::string& calname, const std::string& d);

    const std::set<Date>& getHolidays(const std::string& calname);

    const std::set<Date>& getBusinessDays(const std::string& calname);

    std::set<std::string> getCalendars() const;

    const std::string& getBaseCalendar(const std::string& calname);

    void append(const ore::data::CalendarAdjustmentConfig& c);

    void fromFile(const std::string& name);

};

class CurrencyConfig : public ore::data::XMLSerializable {
  public:

    CurrencyConfig();

    void addCurrencies();

    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
};

} // namespace data
} // namespace ore


#endif
