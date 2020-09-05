FROM ubuntu:focal

ENV APPDIR "/usr/src/kmcc"
RUN mkdir -p $APPDIR
WORKDIR $APPDIR

RUN apt-get update && \
  apt-get install -y build-essential gcc make git binutils libc6-dev vim
