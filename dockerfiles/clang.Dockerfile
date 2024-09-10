FROM silkeh/clang:18

LABEL description="Esta imagen requiere privilegios elevados para ejecutar io_uring. Use '--privileged' al ejecutar el contenedor."

ARG USER_ID=1000
ARG GROUP_ID=1000

WORKDIR /app

RUN groupadd -g ${GROUP_ID} devs \
  && useradd -u ${USER_ID} -g devs -m -s /bin/bash dev \
  && echo 'dev ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

RUN apt update && apt install -y \
  liburing-dev \
  git \
  libc++-dev \
  cmake \
  jq \
  autotools-dev \
  libtool \
  bear \
  libc6-dbg \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

RUN wget https://sourceware.org/pub/valgrind/valgrind-3.23.0.tar.bz2 && tar xf valgrind-3.23.0.tar.bz2 && cd valgrind-3.23.0 && \
  ./configure && make && make install && \
  cd .. && rm -rf valgrind-3.23.0*

USER dev

CMD ["/bin/bash"]
