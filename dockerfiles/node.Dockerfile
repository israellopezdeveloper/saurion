FROM node:18-alpine

WORKDIR /app

# Instalar bash y otros paquetes útiles
RUN apk add --no-cache bash shadow

RUN npm install -g pkg

USER 1000

SHELL ["/bin/bash", "-c"]
CMD ["/bin/bash"]
