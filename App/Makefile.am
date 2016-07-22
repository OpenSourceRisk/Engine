SUBDIRS = m4

ACLOCAL_AMFLAGS = -I m4 

bin_PROGRAMS = ore

AM_CPPFLAGS = -I${top_srcdir} -I${top_builddir} \
	-I${top_builddir}/../OpenRiskEngineAnalytics \
	-I${top_builddir}/../OpenRiskEngineData \
	-I${top_builddir}/../QuantExt \
	-I${top_builddir}/../QuantLib

AM_LDFLAGS = \
	-L${top_builddir}/../OpenRiskEngineAnalytics/orea -lOpenRiskEngineAnalytics \
	-L${top_builddir}/../OpenRiskEngineData/ored -lOpenRiskEngineData \
	-L${top_builddir}/../QuantExt/qle -lQuantExt \
	-L${top_builddir}/../QuantLib/ql -lQuantLib \
	-lboost_serialization -lboost_date_time -lboost_regex

LDADD = 

EXTRA_DIST = autogen.sh

ore_SOURCES = ore.cpp

include_HEADERS = ore.hpp
