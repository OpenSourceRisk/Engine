/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file models/amcmodelcg.hpp
    \brief additional interface for amc enabled models
    \ingroup utilities
*/

#pragma once

#include <qle/math/randomvariable.hpp>

namespace oreplus {
namespace data {

class AmcModelCG {
public:
    virtual ~AmcModelCG() {}
    virtual void injectPaths(const std::vector<QuantLib::Real>* pathTimes,
                             const std::vector<std::vector<std::size_t>>* variates,
                             const std::vector<bool>* isRelevantTime, const bool stickyCloseOutRun) = 0;
};

} // namespace data
} // namespace oreplus
