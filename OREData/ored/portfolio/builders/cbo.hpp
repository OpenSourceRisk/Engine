/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file portfolio/builders/cbo.hpp
\brief
\ingroup portfolio
*/

#pragma once

#include <ored/portfolio/enginefactory.hpp>
#include <ql/experimental/credit/pool.hpp>

namespace ore {
namespace data {

class CboMCEngineBuilder : public EngineBuilder {
public:
    //! constructor that builds a usual pricing engine
    CboMCEngineBuilder() : EngineBuilder("OneFactorCopula", "MonteCarloCBOEngine", {"CBO"}) {};

    boost::shared_ptr<QuantLib::PricingEngine> engine(const boost::shared_ptr<QuantLib::Pool>& pool);
};

} // namespace data
} // namespace ore
