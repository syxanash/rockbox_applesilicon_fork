#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include "base32.h"
#include "hmac.h"

#ifndef _TOTP_H_
#define _TOTP_H_

static uint32_t totp(char *secret)
{

  // decode the seed

  // const char *secret = "MU6W5Q54RXBSVW5V";
  uint8_t key[32];

  int keylen = base32_decode(secret, key, sizeof(key));

  printf("length: %d\n", keylen);
  printf("secret bytes: ");

  for (int i = 0; i < keylen; i++)
  {
    printf("%02X ", key[i]);
  }

  printf("\n------\n");

  // calculate the counter

  uint64_t counter = time(NULL) / 30;
  uint8_t msg[8];

  for (int i = 7; i >= 0; i--)
  {
    msg[i] = counter & 0xFF;
    counter >>= 8;
  }

  printf("counter: %llu\n", (unsigned long long)(time(NULL)) / 30);
  printf("counter bytes: ");

  for (int i = 0; i < 8; i++)
  {
    printf("%02X ", msg[i]);
  }

  printf("\n------\n");

  // HMAC SHA1

  uint8_t digest[20];
  hmac_sha1(key, keylen, msg, 8, digest);

  printf("hmac: ");
  for (int i = 0; i < 20; i++)
  {
    printf("%02X ", digest[i]);
  }

  printf("\n------\n");

  // dynamic trunc

  int offset = digest[19] & 0x0F;

  uint32_t code = ((digest[offset]     & 0x7F) << 24)
                | ((digest[offset + 1] & 0xFF) << 16)
                | ((digest[offset + 2] & 0xFF) << 8)
                |  (digest[offset + 3] & 0xFF);

  uint32_t otp = code % 1000000;

  printf("OTP: %06d\n", otp);

  return otp;
}

#endif