# FROM frolvlad/alpine-glibc:glibc-2.30

# RUN apk --no-cache add ca-certificates wget vim
# RUN apk --no-cache add ca-certificates gcc libc-dev alsa-lib-dev
# RUN apk --no-cache add alsa-utils alsa-utils-doc alsa-lib alsaconf

# VERSION 0.1
# AUTHOR:	Alexandre Fiori <fiorix@gmail.com>
# DESCRIPTION:	crosstool-ng for arm (Raspberry Pi)
# BUILD:	docker build --rm -t fiorix/crosstool-ng-arm .
# Taken from: https://github.com/fiorix/crosstool-ng-arm/
FROM ubuntu

RUN apt-get update
RUN apt-get install -y \
	autoconf \
	automake \
	binutils \
	bison \
	build-essential \
	curl \
	flex \
	gawk \
	git \
	gperf \
	help2man \
	libncurses5-dev \
	libtool \
	libtool-bin \
	python3 \
	python3-distutils \
	rsync \
	subversion \
	texinfo \
	tmux \
	unzip \
	wget

RUN curl -s http://crosstool-ng.org/download/crosstool-ng/crosstool-ng-1.25.0.tar.bz2 | tar -jx -C /usr/src
WORKDIR /usr/src/crosstool-ng-1.25.0
RUN ./configure --prefix=/opt/cross
RUN make
RUN make install

RUN mkdir /root/ct-ng-conf
WORKDIR /root/ct-ng-conf
COPY crosstool/ct-ng-config /root/ct-ng-conf/.config
COPY crosstool/ct-ng-env /usr/local/bin/ct-ng-env
RUN chmod 755 /usr/local/bin/ct-ng-env
RUN ct-ng-env ct-ng build
RUN rm -rf /root/ct-ng-conf/.build
RUN rm -f /root/ct-ng-conf/build.log
ENV CC=/root/x-tools/armv7-unknown-linux-gnueabihf/bin/armv7-unknown-linux-gnueabihf-cc

# Compile Alsa
WORKDIR /root/
RUN git clone https://github.com/alsa-project/alsa-lib.git
WORKDIR /root/alsa-lib/
RUN autoreconf -i
RUN ./configure --host armv7-unknown-linux-gnueabihf --prefix=/root/x-tools/armv7-unknown-linux-gnueabihf/
RUN make
RUN make install

# LET'S GOOOO!!
WORKDIR /src/


