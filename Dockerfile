FROM docker.pkg.github.com/kmdkuk/dev-image/ubuntu

ENV APPDIR "/usr/src/kmcc"
RUN mkdir -p $APPDIR
WORKDIR $APPDIR

RUN apt-get update && \
  apt-get install -y build-essential gcc make binutils libc6-dev uuid-runtime
