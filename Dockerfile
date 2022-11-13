FROM arm64v8/ubuntu:focal as build-stage

ARG DEBIAN_FRONTEND=noninteractive

# Packages required to build Mavsdk
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    build-essential cmake git openssh-client \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    python3 python3-future \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN git config --global http.sslverify false
RUN git config --global https.sslverify false

WORKDIR /build-mavsdk
RUN git clone https://github.com/mavlink/MAVSDK.git
WORKDIR /build-mavsdk/MAVSDK
RUN git checkout v1.4.7
RUN git submodule update --init --recursive

RUN cmake -Bbuild/default -DCMAKE_BUILD_TYPE=Release -H.
RUN cmake --build build/default -j12
RUN cmake --build build/default --target install

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
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

#    libjsoncpp1 \
#    libcurl4 \
#    libncurses5 \
#    libtinyxml2-6a \

WORKDIR /app
COPY --from=build-stage /usr/local/lib/libmavsdk*.* /app/

COPY --from=build-stage /build-airspy/airspyhf/build/libairspyhf/src/libairspyhf.so.0 /app/
COPY --from=build-stage /build-airspy/airspyhf/build/libairspyhf/src/libairspyhf.so /app/
COPY --from=build-stage /build-airspy/airspyhf/build/libairspyhf/src/libairspyhf.so.1.6.8 /app/
COPY --from=build-stage /build-airspy/airspyhf/build/tools/src/airspyhf_rx /app/

ENTRYPOINT [ "/app/MavlinkTagController" ]
