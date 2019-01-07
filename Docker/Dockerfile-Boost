ARG tag=latest
FROM debian:${tag}

MAINTAINER Quaternion Risk Management
LABEL Description="Provide a base environment for building in C++ with Boost"

RUN apt-get update \
 && apt-get install -y build-essential wget libbz2-dev autoconf libtool dos2unix cmake\
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*

ARG boost_version
ARG boost_dir
ENV boost_version ${boost_version}

RUN wget http://downloads.sourceforge.net/project/boost/boost/${boost_version}/${boost_dir}.tar.gz \
    && tar xfz ${boost_dir}.tar.gz \
    && rm ${boost_dir}.tar.gz \
    && cd ${boost_dir} \
    && ./bootstrap.sh \
    && ./b2 --without-python -j 4 link=shared runtime-link=shared install \
    && cd .. && rm -rf ${boost_dir} && ldconfig

CMD bash
