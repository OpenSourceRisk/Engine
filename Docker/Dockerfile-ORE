ARG quantlib_version=latest
FROM env_quantlib:${quantlib_version}

MAINTAINER Quaternion Risk Management
LABEL Description="Build ORE and add to the QuantLib build environment"

# Argument for number of cores to use while building
ARG num_cores=2

# Copy ORE sources for libs and app
COPY QuantExt /ore/QuantExt
COPY OREData /ore/OREData
COPY OREAnalytics /ore/OREAnalytics
COPY App /ore/App
COPY ThirdPartyLibs /ore/ThirdPartyLibs

# Need the dos2unix all if building from Windows because the shell 
# scripts fail if there are CRLF present in the files
RUN cd /ore \
  && find -regex ".*\.\(sh\|in\|ac\|am\)" -exec dos2unix {} ';' \
  && cd QuantExt \
  && ./autogen.sh \
  && ./configure --enable-static CXXFLAGS=-O2 \
  && cd qle \
  && make -j ${num_cores} \
  && make install \
  && cd ../../OREData \
  && ./autogen.sh \
  && ./configure --enable-static CXXFLAGS="-O2 -std=c++11" \
  && cd ored \
  && make -j ${num_cores} \
  && make install \
  && cd ../../OREAnalytics \
  && ./autogen.sh \
  && ./configure --enable-static CXXFLAGS="-O2 -std=c++11" \
  && cd orea \
  && make -j ${num_cores} \
  && make install \
  && ldconfig \
  && cd ../../App \
  && ./autogen.sh \
  && ./configure --enable-static CXXFLAGS="-O2 -std=c++11" \
  && make -j ${num_cores} \
  && make install \
  && cd / \
  && rm -rf ore

CMD bash
