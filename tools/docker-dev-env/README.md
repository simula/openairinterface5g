# Containerized development environment for OAI

## Description

This contains bash scripts for managing a ubuntu24 dockerized development enviroment for OAI. No need to install any
tools on host machines except for docker

 - `bootstrap.sh` - build development environment image and install the following bash aliases in `~/.bash_aliases`:
   * `oai_start_dev_env` - start the development environment container and detach
   * `oai_stop_dev_env` - stop the development environment container
   * `oai_destroy_dev_env` - remove the container, deleting all data that is not on mapped volumes. All packages installed
     using `apt` or other methods in system folders (e.g. `/usr/`) will be removed.
   * `oai_enter_dev_env` - enter development environment

## Installing

Run `./bootstrap.sh`. This will build the `ran-base:latest` image if its not present on the system and 
build the `openair-dev:$USER` image and install the aliases above.

If not present, please add `source ~/.bash_aliases` into your `~/.bashrc`.

## Assumptions

 - The `bootstrap.sh` script will read the current environment to generate the docker image, running
   it with sudo is not recommended.
 - Your home directory is shared with the home directory inside the docker container.
 - You can use the `oai_enter_dev_env` alias to enter the container from any directory within
   your home directory - your home directory is mapped directly in the container. If you
   need other directories to be mapped, add extra volumes to the `docker-compose.yml`

## Using sudo in container

`sudo` is not necessary in the container as it should already have the linux capabilities that running
OAI requires. However it is still possible to run `sudo` in the container.

## Updating and preserving your image

Each image is personally tagged with username. You can update your image (e.g. install ubuntu system packages)
either through modifying the Dockerfile (not recommended) or installing packages manually through `apt` and
saving the docker container snapshot. If the environment is running just type

```
docker commit openair-dev-$USER openair-dev:$USER
```

This will update the image that is used in the aliases and preserve the modifications when you start the environment again.

If you ever want to reset your image to the base prepared here just rerun the `bootstrap.sh` script
