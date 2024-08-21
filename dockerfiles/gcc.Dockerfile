FROM gcc:14.2.0

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
  jq \
  bc \
  cmake \
  lcov \
  doxygen \
  texlive-full\
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

USER dev

CMD ["/bin/bash"]

