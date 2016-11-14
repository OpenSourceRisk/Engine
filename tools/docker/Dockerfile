FROM lballabio/boost

# Add and configure required tools

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y python python-pip unzip \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*

RUN pip install jupyter ipywidgets jupyter_dashboards pythreejs bqplot matplotlib scipy

RUN jupyter dashboards quick-setup --sys-prefix
RUN jupyter nbextension enable --py --sys-prefix widgetsnbextension


# Download and uncompress ORE

ARG ore_short_version=1.8
ARG ore_version=1.8.0.7
ARG ore_archive=ORE-${ore_short_version}.zip
ARG ore_folder=ORE-${ore_short_version}

ENV ore_version ${ore_version}

RUN wget https://github.com/OpenSourceRisk/Engine/releases/download/v${ore_version}/${ore_archive} \
    && unzip ${ore_archive} \
    && rm ${ore_archive}

# Whatever we do next, we do in the ORE folder

WORKDIR ${ore_folder}


# Add QuantLib

ARG quantlib_version=1.8
ENV quantlib_version ${quantlib_version}

RUN wget http://downloads.sourceforge.net/project/quantlib/QuantLib/${quantlib_version}/QuantLib-${quantlib_version}.tar.gz \
    && tar xfz QuantLib-${quantlib_version}.tar.gz \
    && rm QuantLib-${quantlib_version}.tar.gz \
    && mv QuantLib-${quantlib_version} QuantLib


# Compile, as per instructions in the ORE user guide

RUN cd QuantLib \
    && ./configure \
    && make -j4

RUN cd QuantExt \
    && ./configure \
    && make -j4 \
    && ./test/quantext-test-suite

RUN cd OREData \
    && ./configure \
    && make -j4 \
    && ./test/ored-test-suite

RUN cd OREAnalytics \
    && ./configure \
    && make -j4 \
    && ./test/orea-test-suite

RUN cd App \
    && ./configure \
    && make -j4

ENV LC_NUMERIC C

