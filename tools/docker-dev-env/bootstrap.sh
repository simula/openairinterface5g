#!/bin/bash
set -e
CONTAINER_NAME=openair-dev-$USER
if [ "$EUID" -eq 0 ]; then
    read -p "You are running this script as root. This might not work as expected. Do you want to continue? (Y/N): " response
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        echo "Exiting script."
        exit 1
    fi
fi

if ! docker images | grep -q "ran-base"; then
    echo "ran-base:latest image not found. Building the image..."
    docker build -f ../../docker/Dockerfile.base.ubuntu -t ran-base:latest ../../
else
    echo "ran-base:latest image found."
fi

echo "Starting the build process for the Docker container..."
docker build -f Dockerfile -t openair-dev:$USER --build-arg USERNAME=$USER --build-arg USER_UID=$(id -u) --build-arg USER_GID=$(id -u) ../../

echo -e "\e[93mInstalling aliases to ~/.bash_aliases. Make sure to add 'source ~/.bash_aliases' in ~/.bashrc \e[0m"
mkdir -p ~/oai-dev-env
cp docker-compose.yml ~/oai-dev-env

touch ~/.bash_aliases

function add_alias() {
  local alias=${1} cmd=${2}
  if grep -q "${alias}" ~/.bash_aliases; then echo "cannot add alias ${alias}: already exists in ~/.bash_aliases"; return 1; fi
  echo "alias ${alias}='${cmd}'" >> ~/.bash_aliases
}

add_alias oai_start_dev_env 'docker compose -f ~/oai-dev-env/docker-compose.yml up -d' || true
add_alias oai_stop_dev_env 'docker compose -f ~/oai-dev-env/docker-compose.yml down' || true
add_alias oai_enter_dev_env "docker exec -it -w \$(pwd) $CONTAINER_NAME /bin/bash" || true
add_alias oai_destroy_dev_env "docker container rm -f ${CONTAINER_NAME}" || true

source ~/.bash_aliases
