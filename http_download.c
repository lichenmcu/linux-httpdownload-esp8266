/*******************************************************************************
 * Copyright (c) 2012, 2013 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution. 
 *
 * The Eclipse Public License is available at 
 *   http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at 
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial contribution
 *    Ian Craggs - change delimiter option from char to string
 *    Al Stockdill-Mander - Version using the embedded C client
 *******************************************************************************/

/*
 
 stdout subscriber
 
 compulsory parameters:
 
  topic to subscribe to
 
 defaulted parameters:
 
  --from localhost
  --to

 for example:

    stdoutsub topic/of/interest --host iot.eclipse.org

*/

//#define NUTTX

#ifndef NUTTX
#include "config.h"
#else
#include "nuttx/config.h"
#endif

#include <fcntl.h>
#include <stdio.h>

#include <stdio.h>
#include <signal.h>

#include <sys/time.h>


#ifndef O_BINARY
#define O_BINARY 0
#endif

volatile int toStop = 0;


void usage()
{
  printf("MQTT stdout subscriber\n");
  printf("Usage: stdoutsub topicname <options>, where options are:\n");
  printf("  --from <hostname> (default is localhost)\n");
  printf("  --to   <port> (default is 1883)\n");
  exit(-1);
}

#ifndef NUTTX
void cfinish(int sig)
{
  signal(SIGINT, NULL);
  toStop = 1;
}
#endif


struct opts_struct
{
  char* from;
  char* to;
} opts =
{
  (char*) CONFIG_HTTP_DOWNLOAD_ADDRESS,
  (char*)"./download"
};

void getopts(int argc, char** argv)
{
  int count = 2;

  while (count < argc)
  {
    if (strcmp(argv[count], "--from") == 0)
    {
      if (++count < argc)
        opts.from= argv[count];
      else
        usage();
    }
    else if (strcmp(argv[count], "--to") == 0)
    {
      if (++count < argc)
        opts.to= argv[count];
      else
        usage();
    }
    count++;
  }

}

static int netsetup(void)
{
  int ret;
  int timeout;

  ret = lesp_initialize();
  if (ret != OK)
    {
      perror("ERROR: failed to init network\n");
      return ret;
    }

  ret = lesp_soft_reset();
  if (ret != OK)
    {
      perror("ERROR: failed to reset\n");
      return ret;
    }

  timeout = 5;
  ret = lesp_ap_connect(CONFIG_EXAMPLES_MQTT_SSID, \
                        CONFIG_EXAMPLES_MQTT_PASSWORD, \
                        timeout);
  if (ret != OK)
    {
      perror("ERROR: failed to connect wifi\n");
      return ret;
    }

  return ret;
}

#include "MQTTLinux.h"

uint8_t buf[2048];

#define MIN(a,b)  ((a) < (b) ? a : b)

#define LOAD_SIZE  1024

int fd = NULL;

int flash_write(uint32_t offset, uint8_t *ptr, int len)
{
  (void) offset;
  (void) len;

  if (fd < 0) return fd;
  
  write(fd, ptr, len);
//  printf("%s", ptr);
  return 0;
}
int flash_open(void)
{

  fd = open(opts.to, O_RDWR|O_CREAT|O_TRUNC|O_BINARY, 0666);
  if (fd < 0) {
    printf("OPEN %s error\n", opts.to);
    return fd;
  }

  return 0;
}
int flash_close(void)
{
  if (fd < 0) return fd;
  close (fd);
  return 0;
}


#if 1
int download(Network *n)
{
  int ret;

  int per = LOAD_SIZE;
  int offset = 0;

  int len;
  char *ptr;


  int i;
  char tmp[20];
  memset(tmp, 0, sizeof(tmp));
  char *p = opts.from;
  for (i = 0; i < strlen(p) && i < sizeof(tmp); i++) {

    tmp[i] = p[i];
    if (tmp[i] == '/') {
      tmp[i] = 0;
      break;
    }
  }

  flash_open();

  while (1) {

    snprintf(buf, sizeof(buf), "GET http://%s HTTP/1.1\r\nRange: bytes=%d-%d\r\nHost: %s\r\n\r\n", opts.from, offset, offset+per-1, tmp);
//    snprintf(buf, sizeof(buf), "GET http://%s HTTP/1.1\r\nRange: bytes=%d-%d\r\nConnection: close\r\nHost: %s\r\n\r\n", opts.from, offset, offset+per-1, tmp);

    ret = linux_write(n, buf, strlen(buf), 10 * 1000);
    if (ret < 0) {
      printf("Error CONNECT write offset: %d\n", offset);
      goto quit;
    }
    if (ret != strlen(buf)) {
      printf("Error: Socket Send %dBytes\n", ret);
      continue;
    }

    memset(buf, 0, sizeof(buf));
    ret = linux_read(n, buf, sizeof(buf), 10 * 1000);
    if (ret < 0) {
      printf("Error CONNECT read offset: %d\n", offset);
      goto quit;
    }

#if 1
sscanf(buf, "HTTP/1.1 %d ", &len);
if (len != 206) {
  printf("HTTP RC = %d\n", len);
}
#endif

    ptr = strstr(buf, "Content-Length: ");

    if (ptr == NULL) {
      printf("Packet error\n");
      continue;
    }

    sscanf(ptr, "Content-Length: %d\r\n", &len);
/* debug */
//printf("Get size = %d", len);

    if (len > per) {
      printf("Packet error\n");
      continue;
    }

    ptr = strstr(ptr, "\r\n\r\n");

    flash_write(offset, ptr+4, len);

    if (len < per) {
      printf("\nGet total file, size == %d\n\n", offset + len);
      break;
    }

    offset += per;
  }

  return ret = offset + len;

quit:
  flash_close();
  return ret;
}
#endif

#include "socket_port.h"

int addrStr(char *buf, char* domain)
{
  struct hostent *hptr;

  in_addr_t ip;

  if ((hptr = Gethostbyname(domain)) == NULL)
    return -1;

  ip = *((in_addr_t *)hptr->h_addr);

  printf("%d.%d.%d.%d", *((uint8_t *)&(ip)+0), *((uint8_t *)&(ip)+1),
  *((uint8_t *)&(ip)+2), *((uint8_t *)&(ip)+3));

  sprintf("%d.%d.%d.%d", *((uint8_t *)&(ip)+0), *((uint8_t *)&(ip)+1),
  *((uint8_t *)&(ip)+2), *((uint8_t *)&(ip)+3));

  return 0;
//        address.sin_addr = *((struct in_addr *)hptr->h_addr);
}

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int download_main(int argc, char *argv[])
#endif
{
  int i;
  int rc = 0;
  unsigned char buf[100];
  unsigned char readbuf[100];
  uint8_t *p;

//  if (argc < 3)
//    usage();

//  getopts(argc, argv);

  Network n;

#ifndef NUTTX
  signal(SIGINT, cfinish);
  signal(SIGTERM, cfinish);
#endif

  rc = netsetup();
  if (rc != OK) {
    exit(1);
  }

  p = opts.from;
  for (i = 0; i < strlen(p) && i < sizeof(buf); i++) {

    buf[i] = p[i];
    if (buf[i] == '/') {
      buf[i] = 0;
      break;
    }
  }

  NetworkInit(&n);

retry:
  printf("Connecting to %s %d\n", buf, 80);
  rc = NetworkConnect(&n, buf, 80);
  if (rc == -1) {
    perror("ERROR: failed to connect http server\n");
    goto retry;
    exit(1);
  }

  printf("start download\n");

  download(&n);

  NetworkDisconnect(&n);

  printf("Stopping\n");

  return 0;
}

