ARG boost_version=latest
FROM build_env_boost:${boost_version}

MAINTAINER Quaternion Risk Management
LABEL Description="Build QuantLib and add to the Boost build environment"

# Argument for number of cores to use while building
ARG num_cores=2

# Exclusions are performed by .dockerignore
COPY QuantLib /ore/QuantLib

# Need the dos2unix all if building from Windows because the shell 
# scripts fail if there are CRLF present in the files
RUN cd /ore/QuantLib \
  && find -regex ".*\.\(sh\|in\|ac\|am\)" -exec dos2unix {} ';' \
  && ./autogen.sh \
  && ./configure --enable-static CXXFLAGS=-O2 \
  && cd ql \
  && make -j ${num_cores} \
  && make install \
  && cd / \
  && rm -rf ore \
  && ldconfig

CMD bash