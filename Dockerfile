FROM auterion/ubuntu-mavsdk:v0.40.0 as build-stage

WORKDIR /buildroot
COPY CMakeLists.txt /buildroot/
COPY *.h /buildroot/
COPY *.cpp /buildroot/

RUN mkdir -p /buildroot/build && \
    cd /buildroot/build && \
    cmake .. && \
    make -j12

FROM arm64v8/ubuntu:focal as release-stage

ARG DEBIAN_FRONTEND=noninteractive

# MAVSDK dependencies
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    libjsoncpp1 \
    libcurl4 \
    libncurses5 \
    libtinyxml2-6a \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=auterion/ubuntu-mavsdk:v0.40.0 /buildroot/libmavsdk*.deb /tmp/
COPY --from=build-stage /buildroot/build/MavlinkTagController /app/
RUN dpkg -i /tmp/*.deb

ENTRYPOINT [ "/app/MavlinkTagController" ]
