/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/ipv6.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>

#include "tun_if.h"
#include "common/platform_constants.h"
#include "common/utils/LOG/log.h"
#include "common/utils/system.h"

int nas_sock_fd[MAX_MOBILES_PER_ENB * 2]; // Allocated for both LTE UE and NR UE.
int nas_sock_mbms_fd;

static int tun_alloc(char *dev)
{
  struct ifreq ifr;
  int fd, err;

  if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
    LOG_E(UTIL, "failed to open /dev/net/tun\n");
    return -1;
  }

  memset(&ifr, 0, sizeof(ifr));
  /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
   *        IFF_TAP   - TAP device
   *
   *        IFF_NO_PI - Do not provide packet information
   */
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

  if (*dev)
    strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name) - 1);

  if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
    close(fd);
    return err;
  }

  strcpy(dev, ifr.ifr_name);
  return fd;
}

int tun_init_mbms(char *ifprefix, int id)
{
  int ret;
  char ifname[64];

  sprintf(ifname, "%s%d", ifprefix, id);
  nas_sock_mbms_fd = tun_alloc(ifname);

  if (nas_sock_mbms_fd == -1) {
    LOG_E(UTIL, "Error opening mbms socket %s (%d:%s)\n", ifname, errno, strerror(errno));
    exit(1);
  }

  LOG_D(UTIL, "Opened socket %s with fd %d\n", ifname, nas_sock_mbms_fd);
  ret = fcntl(nas_sock_mbms_fd, F_SETFL, O_NONBLOCK);

  if (ret == -1) {
    LOG_E(UTIL, "Error fcntl (%d:%s)\n", errno, strerror(errno));
    return 0;
  }

  struct sockaddr_nl nas_src_addr = {0};
  nas_src_addr.nl_family = AF_NETLINK;
  nas_src_addr.nl_pid = 1;
  nas_src_addr.nl_groups = 0; /* not in mcast groups */
  ret = bind(nas_sock_mbms_fd, (struct sockaddr *)&nas_src_addr, sizeof(nas_src_addr));

  return 1;
}

int tun_init(const char *ifprefix, int num_if, int id)
{
  int ret;
  char ifname[64];

  int begx = (id == 0) ? 0 : id - 1;
  int endx = (id == 0) ? num_if : id;
  for (int i = begx; i < endx; i++) {
    sprintf(ifname, "%s%d", ifprefix, i + 1);
    nas_sock_fd[i] = tun_alloc(ifname);

    if (nas_sock_fd[i] == -1) {
      LOG_E(UTIL, "Error opening socket %s (%d:%s)\n", ifname, errno, strerror(errno));
      return 0;
    }

    LOG_I(UTIL, "Opened socket %s with fd nas_sock_fd[%d]=%d\n", ifname, i, nas_sock_fd[i]);
    ret = fcntl(nas_sock_fd[i], F_SETFL, O_NONBLOCK);

    if (ret == -1) {
      LOG_E(UTIL, "Error fcntl (%d:%s)\n", errno, strerror(errno));

      return 0;
    }
  }

  return 1;
}

/*
 * \brief set a genneric interface parameter
 * \param ifn the name of the interface to modify
 * \param if_addr the address that needs to be modified
 * \param operation one of SIOCSIFADDR (set interface address), SIOCSIFNETMASK
 *   (set network mask), SIOCSIFBRDADDR (set broadcast address), SIOCSIFFLAGS
 *   (set flags)
 * \return true on success, false otherwise
 */
static bool setInterfaceParameter(int sock_fd, const char *ifn, int af, const char *if_addr, int operation)
{
  DevAssert(af == AF_INET || af == AF_INET6);
  struct ifreq ifr = {0};
  strncpy(ifr.ifr_name, ifn, sizeof(ifr.ifr_name));
  struct in6_ifreq ifr6 = {0};

  void *ioctl_opt = NULL;
  if (af == AF_INET) {
    struct sockaddr_in addr = {.sin_family = AF_INET};
    int ret = inet_pton(af, if_addr, &addr.sin_addr);
    if (ret != 1) {
      LOG_E(OIP, "inet_pton(): cannot convert %s to IPv4 network address\n", if_addr);
      return false;
    }
    memcpy(&ifr.ifr_ifru.ifru_addr,&addr,sizeof(struct sockaddr_in));
    ioctl_opt = &ifr;
  } else {
    struct sockaddr_in6 addr6 = {.sin6_family = AF_INET6};
    int ret = inet_pton(af, if_addr, &addr6.sin6_addr);
    if (ret != 1) {
      LOG_E(OIP, "inet_pton(): cannot convert %s to IPv6 network address\n", if_addr);
      return false;
    }
    memcpy(&ifr6.ifr6_addr, &addr6.sin6_addr, sizeof(struct in6_addr));
    // we need to get the if index to put it into ifr6
    if (ioctl(sock_fd, SIOGIFINDEX, &ifr) < 0) {
      LOG_E(OIP, "ioctl() failed: errno %d, %s\n", errno, strerror(errno));
      return false;
    }
    ifr6.ifr6_ifindex = ifr.ifr_ifindex;
    ifr6.ifr6_prefixlen = 64;
    ioctl_opt = &ifr6;
  }

  bool success = ioctl(sock_fd, operation, ioctl_opt) == 0;
  if (!success)
    LOG_E(OIP, "Setting operation %d for %s: ioctl call failed: %d, %s\n", operation, ifn, errno, strerror(errno));
  return success;
}

/*
 * \brief bring interface up (up != 0) or down (up == 0)
 */
typedef enum { INTERFACE_DOWN, INTERFACE_UP } if_action_t;
static bool change_interface_state(int sock_fd, const char *ifn, if_action_t if_action)
{
  const char* action = if_action == INTERFACE_DOWN ? "DOWN" : "UP";

  struct ifreq ifr = {0};
  strncpy(ifr.ifr_name, ifn, sizeof(ifr.ifr_name));
  /* get flags of this interface: see netdevice(7) */
  bool success = ioctl(sock_fd, SIOCGIFFLAGS, (caddr_t)&ifr) == 0;
  if (!success)
    goto fail_interface_state;

  if (if_action == INTERFACE_UP) {
    ifr.ifr_flags |= IFF_UP | IFF_NOARP | IFF_POINTOPOINT;
    ifr.ifr_flags &= ~IFF_MULTICAST;
  } else {
    ifr.ifr_flags &= ~IFF_UP;
  }

  success = ioctl(sock_fd, SIOCSIFFLAGS, (caddr_t)&ifr) == 0;
  if (!success)
    goto fail_interface_state;
  return true;

fail_interface_state:
  LOG_E(OIP, "Bringing interface %s for %s: ioctl call failed: %d, %s\n", action, ifn, errno, strerror(errno));
  return false;
}

// non blocking full configuration of the interface (address, and the two lest octets of the address)
bool tun_config(int interface_id, const char *ipv4, const char *ipv6, const char *ifpref)
{
  char interfaceName[IFNAMSIZ];
  snprintf(interfaceName, sizeof(interfaceName), "%s%d", ifpref, interface_id);

  AssertFatal(ipv4 != NULL || ipv6 != NULL, "need to have IP address, but none given\n");

  int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_fd < 0) {
    LOG_E(UTIL, "Failed creating socket for interface management: %d, %s\n", errno, strerror(errno));
    return false;
  }

  change_interface_state(sock_fd, interfaceName, INTERFACE_DOWN);
  bool success = true;
  if (ipv4 != NULL)
    success = setInterfaceParameter(sock_fd, interfaceName, AF_INET, ipv4, SIOCSIFADDR);
  // set the machine network mask for IPv4
  if (success && ipv4 != NULL)
    success = setInterfaceParameter(sock_fd, interfaceName, AF_INET, "255.255.255.0", SIOCSIFNETMASK);

  if (ipv6 != NULL) {
    // for setting the IPv6 address, we need an IPv6 socket. For setting IPv4,
    // we need an IPv4 socket. So do all operations using IPv4 socket, except
    // for setting the IPv6
    int sock_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
      LOG_E(UTIL, "Failed creating socket for interface management: %d, %s\n", errno, strerror(errno));
      success = false;
    }
    success = success && setInterfaceParameter(sock_fd, interfaceName, AF_INET6, ipv6, SIOCSIFADDR);
    close(sock_fd);
  }

  if (success)
    success = change_interface_state(sock_fd, interfaceName, INTERFACE_UP);

  if (success)
    LOG_I(OIP, "Interface %s successfully configured, IPv4 %s, IPv6 %s\n", interfaceName, ipv4, ipv6);
  else
    LOG_E(OIP, "Interface %s couldn't be configured (IPv4 %s, IPv6 %s)\n", interfaceName, ipv4, ipv6);

  close(sock_fd);
  return success;
}

void setup_ue_ipv4_route(int interface_id, const char *ipv4, const char *ifpref)
{
  int table_id = interface_id - 1 + 10000;
  char interfaceName[IFNAMSIZ];
  snprintf(interfaceName, sizeof(interfaceName), "%s%d", ifpref, interface_id);

  char command_line[500];
  int res = sprintf(command_line,
                    "ip rule add from %s/32 table %d && "
                    "ip rule add to %s/32 table %d && "
                    "ip route add default dev %s table %d",
                    ipv4,
                    table_id,
                    ipv4,
                    table_id,
                    interfaceName,
                    table_id);

  if (res < 0) {
    LOG_E(UTIL, "Could not create ip rule/route commands string\n");
    return;
  }
  background_system(command_line);
}

