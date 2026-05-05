/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#ifndef qle_i
#define qle_i

%{
#if !defined(BOOST_ALL_NO_LIB) && defined(BOOST_MSVC)
#include <qle/auto_link.hpp>
#endif
#include <qle/quantext.hpp>
%}
%include common.i
%include qle_common_typemaps.i
%include qle_common.i
%include qle_calendars.i
%include qle_cashflows.i
%include qle_cashflows_ext.i
%include qle_coupons.i
%include qle_indexes.i
%include qle_currencies.i
%include qle_termstructures.i
%include qle_termstructures_ext.i
%include qle_crossccyfixfloatswap.i
%include qle_instruments.i
%include qle_pricingengines.i
%include qle_models.i
%include qle_ratehelpers.i
%include qle_ccyswap.i
%include qle_equityforward.i
%include qle_futureexpirycalculator.i
%include qle_averageois.i
%include qle_tenorbasisswap.i
%include qle_creditdefaultswap.i
%include qle_averageoisratehelper.i
%include qle_processes.i

// CrossCcyFixFloatSwapHelper is intentionally not exposed yet.

#if defined(SWIGPYTHON)
%pythoncode %{
_QLE_MODULE_OVERVIEW = """QuantExt layer bindings.

Contains extended instruments, cashflows, pricing engines, term structures,
models, and processes used by OREData and OREAnalytics workflows.
"""
if __doc__:
	__doc__ = __doc__ + "\n\n" + _QLE_MODULE_OVERVIEW
else:
	__doc__ = _QLE_MODULE_OVERVIEW
%}
#endif

#endif
