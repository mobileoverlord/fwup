FROM alpine:3.8 as build

# Add build tools
RUN apk add --no-cache \
  linux-headers \
  gcc \
  musl-dev \
  autoconf \
  automake \
  libtool \
  git \
  curl \
  make

# Add runtime tools
RUN apk add --no-cache \
  dosfstools \
  mtools \
  zip \
  unzip

RUN git clone https://github.com/fhunleth/fwup
RUN \
  cd fwup \
  && ./scripts/download_deps.sh \
  && ./scripts/build_deps.sh \
  && ./autogen.sh \
  && PKG_CONFIG_PATH=./build/host/deps/usr/lib/pkgconfig ./configure --enable-shared=no \
  && make
WORKDIR /fwup
