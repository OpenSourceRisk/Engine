ARG ore_version=latest
ARG debian_tag=latest

FROM env_ore:${ore_version} as env_ore

FROM debian:${debian_tag}

COPY Examples /ore/Examples

RUN mkdir /ore/App
COPY --from=env_ore /usr/local/bin /ore/App

COPY --from=env_ore /usr/local/lib /usr/local/lib

# Install Python solely to run the examples
# Can leave out Python and run examples manually for slimmer image
RUN apt-get update \
  && DEBIAN_FRONTEND=noninteractive apt-get install -y dos2unix python3 python3-pip \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

RUN pip3 install matplotlib pandas

# For the examples to work, input txt files need unix line endings
RUN cd /ore/Examples && find -regex ".*\.\(sh\|txt\)" -exec dos2unix {} ';'

WORKDIR /ore/Examples/Example_1

CMD bash