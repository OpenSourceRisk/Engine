bin_PROGRAMS = xva

AM_CPPFLAGS = -I${top_srcdir} -I${top_builddir} \
	-I${top_builddir}/../Wrap \
	-I${top_builddir}/../QuantExt \
	-I${top_builddir}/../QuantLib

AM_LDFLAGS = \
	-L${top_builddir}/../Wrap/qlw -lWrap \
	-L${top_builddir}/../QuantExt/qle -lQuantExt \
	-L${top_builddir}/../QuantLib/ql -lQuantLib \
	-lboost_serialization -lboost_filesystem -lboost_regex -lboost_system -lboost_date_time -lboost_iostreams

xva_CPPFLAGS=${DB_INCLUDES}

xva_LDADD = -lz -lboost_iostreams

LDADD = 

EXTRA_DIST = \
	xva.sln \
	xva.vcproj

xva_SOURCES = xva.cpp

include_HEADERS = xva.hpp
