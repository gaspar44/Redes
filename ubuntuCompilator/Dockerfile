FROM debian:9.8
RUN apt update && apt upgrade -y
RUN apt install -y gcc libssl-dev build-essential vim
ARG USER_NAME
ENV USER_NAME ${USER_NAME}
ARG GID
ENV GID ${GID}
ARG UID
ENV UID ${UID}
RUN groupadd -g ${GID} ${USER_NAME}
EXPOSE 8080
RUN adduser --gid ${GID} --uid ${UID} --disabled-password --gecos ",,," ${USER_NAME}
USER ${USER_NAME}
WORKDIR /home/${USER_NAME}