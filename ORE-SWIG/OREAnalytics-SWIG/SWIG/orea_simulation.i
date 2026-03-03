/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef orea_simulation_i
#define orea_simulation_i

%include types.i
%include ored_market.i
%include ored_portfolio.i

%{
using ore::analytics::FixingManager;
%}

%shared_ptr(FixingManager)
class FixingManager {
public:
    explicit FixingManager(Date today);
    void initialise(const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                    const QuantLib::ext::shared_ptr<Market>& market,
                    const std::string& configuration = Market::defaultConfiguration);
    void update(Date d);
    void reset();
};

#endif
