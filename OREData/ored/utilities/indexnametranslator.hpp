/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file ored/utilities/indexnametranslator.hpp
    \brief translates between QuantLib::Index::name() and ORE names
    \ingroup utilities
*/

#pragma once

#include <ql/patterns/singleton.hpp>

#include <boost/bimap.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_types.hpp>

#include <string>

namespace ore {
namespace data {

//! IndexNameTranslator
/*! Allows translating from QuantLib::Index::name() to the ORE name that parses to that index and vice versa. */
class IndexNameTranslator : public QuantLib::Singleton<IndexNameTranslator, std::integral_constant<bool, true>> {
public:
    //! throws if qlName is not known
    std::string oreName(const std::string& qlName) const;

    //! throws if oreName is not known
    std::string qlName(const std::string& oreName) const;

    //! adds a pair to the mapping
    void add(const std::string& qlName, const std::string& oreName);

    //! clears the mapping
    void clear();

private:
    boost::bimap<std::string, std::string> data_;
    mutable boost::shared_mutex mutex_;
};

} // namespace data
} // namespace ore
