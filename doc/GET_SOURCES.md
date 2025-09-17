The OpenAirInterface software can be obtained from our gitLab server. You will
need a git client to get the sources. The repository is used for main
developments.

# Prerequisites

You need to install `git` using the following commands:

```shell
sudo apt-get update
sudo apt-get install git
```

# Clone the Git repository (for OAI Users without login to gitlab server)

The [openairinterface5g repository](https://gitlab.eurecom.fr/oai/openairinterface5g.git)
holds the source code for the RAN (4G and 5G).

## All users, anonymous access

Clone the RAN repository:

```shell
git clone https://gitlab.eurecom.fr/oai/openairinterface5g.git
```

## For contributors

Configure git with your name/email address, important if you are developer and
want to contribute by pushing code. Please put your full name and the e-mail
address you use in Gitlab.

```shell
git config --global user.name "Your Name"
git config --global user.email "Your email address"
```

More information can be found in [the contributing page](../CONTRIBUTING.md).

# Which branch to checkout?

- `develop`: contains recent commits that are tested on our CI test bench. The
  update frequency is about once a week. 5G is only in this branch. **It is the
  recommended and default branch.**
- `master`: contains a known stable version.

You can find the latest stable tag release here:
https://gitlab.eurecom.fr/oai/openairinterface5g/tags

The tag naming conventions are:

- On `master` branch: **v`x`.`y`.`z`**
- On `develop` branch **`yyyy`.w`xx`**
  * `yyyy` is the calendar year
  * `xx` the week number within the year

More information on work flow and policies can be found in [this
document](./code-style-contrib.md).
