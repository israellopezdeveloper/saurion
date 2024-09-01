FROM node:18-alpine

ARG USER_ID=1000
ARG GROUP_ID=1000

WORKDIR /app

# Instalar bash y otros paquetes útiles
RUN apk add --no-cache bash shadow sudo make git

# Obtiene el nombre del grupo con GID 1000, si existe
RUN group_name=$(getent group 1000 | cut -d: -f1) && \
  if [ -z "$group_name" ]; then \
  group_name="mygroup"; \
  addgroup -g 1000 $group_name; \
  fi && \
  echo "Using group: $group_name"

# Verifica si el usuario con UID 1000 existe, y si no, créalo
RUN if id -u 1000 >/dev/null 2>&1; then \
  user_name=$(getent passwd 1000 | cut -d: -f1); \
  usermod -g 1000 -s /bin/bash $user_name; \
  else \
  adduser -D -u 1000 -G $group_name -s /bin/bash myuser; \
  user_name="myuser"; \
  fi && \
  echo "Using user: $user_name"

# Añade el usuario con UID 1000 al grupo sudoers
RUN echo "$user_name ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

RUN npm install -g pkg

# Cambia al usuario creado/modificado
USER $user_name

SHELL ["/bin/bash", "-c"]
CMD ["/bin/bash"]
