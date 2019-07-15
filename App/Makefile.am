SUBDIRS = m4

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = ore

AM_CPPFLAGS = -I${top_srcdir} -I${top_builddir} \
	-I${top_builddir}/../OREAnalytics \
	-I${top_builddir}/../OREData \
	-I${top_builddir}/../QuantExt

AM_LDFLAGS = \
	-L${top_builddir}/../OREAnalytics/orea -lOREAnalytics \
	-L${top_builddir}/../OREData/ored -lOREData \
	-L${top_builddir}/../QuantExt/qle -lQuantExt \
	-lQuantLib \
	-lboost_serialization -lboost_date_time -lboost_regex -lboost_filesystem -lboost_system

LDADD =

EXTRA_DIST = \
	autogen.sh \
	ore.sln \
	ore.vcxproj

ore_SOURCES = ore.cpp
