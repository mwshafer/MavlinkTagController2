FROM auterion/ubuntu-mavsdk:v0.40.0 as build-stage

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    libusb-1.0-0-dev pkg-config git \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build-airspy
RUN git clone https://github.com/airspy/airspyhf.git
WORKDIR /build-airspy/airspyhf/build
RUN cmake ../ -DINSTALL_UDEV_RULES=ON
RUN make

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
    airspy \
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

COPY --from=build-stage /build-airspy/airspyhf/build/libairspyhf/src/libairspyhf.so.0 /app/
COPY --from=build-stage /build-airspy/airspyhf/build/libairspyhf/src/libairspyhf.so /app/
COPY --from=build-stage /build-airspy/airspyhf/build/libairspyhf/src/libairspyhf.so.1.6.8 /app/
COPY --from=build-stage /build-airspy/airspyhf/build/tools/src/airspyhf_rx /app/

ENTRYPOINT [ "/app/MavlinkTagController" ]
