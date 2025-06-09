/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#ifndef ored_to_string_i
#define ored_to_string_i

%include ored_market.i

%{
using ore::data::to_string;
using QuantLib::Date;
using QuantLib::Period;
using ore::data::MarketObject;
%}

%typemap(typecheck) MarketObject * {
    $1 = (MarketObject *)0;  // Always allow typechecking to succeed
}

%inline %{
	template std::string to_string<MarketObject>(const MarketObject& mo);

    MarketObject derefMarketObject(MarketObject* ptr) {
        if (!ptr) {
            throw std::invalid_argument("Null pointer passed to deref_A_pointer");
        }
        return *ptr;
    }
%}

std::string to_string(const QuantLib::Date& date);
std::string to_string(bool aBool);
std::string to_string(const QuantLib::Period& period);

template <class T> std::string to_string(const T& t);
%template(MarketObjectString) to_string<MarketObject>;

#endif