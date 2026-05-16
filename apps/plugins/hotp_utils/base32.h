#ifndef _BASE32_H_
#define _BASE32_H_

static int base32_decode(const char *in, unsigned char *out, int out_max)
{
  int buf = 0;
  int bits_left = 0;
  int out_len = 0;

  for (; *in && out_len < out_max; in++)
  {
    char c = *in;

    if (c == '=')
      break; // padding - stop
    if (c >= 'a' && c <= 'z')
      c -= 32; // lowercase -> uppercase

    int val;
    if (c >= 'A' && c <= 'Z')
      val = c - 'A'; // A=0 to Z=25
    else if (c >= '2' && c <= '7')
      val = c - '2' + 26; // 2=26 to 7=31
    else
      continue; // skip any invalid char

    buf = (buf << 5) | val; // push 5 bits into buffer
    bits_left += 5;

    if (bits_left >= 8)
    { // enough bits for a full byte?
      bits_left -= 8;
      out[out_len++] = (buf >> bits_left) & 0xFF;
    }
  }

  return out_len; // number of bytes written
}

#endif