bin_PROGRAMS = ore

AM_CPPFLAGS = -I${top_srcdir} -I${top_builddir} \
	-I${top_builddir}/../Wrap \
	-I${top_builddir}/../QuantExt \
	-I${top_builddir}/../QuantLib

AM_LDFLAGS = \
	-L${top_builddir}/../Wrap/qlw -lWrap \
	-L${top_builddir}/../QuantExt/qle -lQuantExt \
	-L${top_builddir}/../QuantLib/ql -lQuantLib \
	-lboost_serialization -lboost_date_time -lboost_regex

#	-lboost_serialization -lboost_filesystem -lboost_regex -lboost_system -lboost_date_time -lboost_iostreams

LDADD = 

EXTRA_DIST =

ore_SOURCES = ore.cpp

include_HEADERS = ore.hpp
