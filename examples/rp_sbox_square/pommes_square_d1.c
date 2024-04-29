#include "../common.h"
#define POMMES_INPUT_SQUARE RETURNSEC SEC

uint8_t square(uint8_t x)
{

uint8_t sq[256]={
0x00,0x01,0x04,0x05,0x10,0x11,0x14,0x15,
0x40,0x41,0x44,0x45,0x50,0x51,0x54,0x55,
0x1b,0x1a,0x1f,0x1e,0x0b,0x0a,0x0f,0x0e,
0x5b,0x5a,0x5f,0x5e,0x4b,0x4a,0x4f,0x4e,
0x6c,0x6d,0x68,0x69,0x7c,0x7d,0x78,0x79,
0x2c,0x2d,0x28,0x29,0x3c,0x3d,0x38,0x39,
0x77,0x76,0x73,0x72,0x67,0x66,0x63,0x62,
0x37,0x36,0x33,0x32,0x27,0x26,0x23,0x22,
0xab,0xaa,0xaf,0xae,0xbb,0xba,0xbf,0xbe,
0xeb,0xea,0xef,0xee,0xfb,0xfa,0xff,0xfe,
0xb0,0xb1,0xb4,0xb5,0xa0,0xa1,0xa4,0xa5,
0xf0,0xf1,0xf4,0xf5,0xe0,0xe1,0xe4,0xe5,
0xc7,0xc6,0xc3,0xc2,0xd7,0xd6,0xd3,0xd2,
0x87,0x86,0x83,0x82,0x97,0x96,0x93,0x92,
0xdc,0xdd,0xd8,0xd9,0xcc,0xcd,0xc8,0xc9,
0x9c,0x9d,0x98,0x99,0x8c,0x8d,0x88,0x89,
0x9a,0x9b,0x9e,0x9f,0x8a,0x8b,0x8e,0x8f,
0xda,0xdb,0xde,0xdf,0xca,0xcb,0xce,0xcf,
0x81,0x80,0x85,0x84,0x91,0x90,0x95,0x94,
0xc1,0xc0,0xc5,0xc4,0xd1,0xd0,0xd5,0xd4,
0xf6,0xf7,0xf2,0xf3,0xe6,0xe7,0xe2,0xe3,
0xb6,0xb7,0xb2,0xb3,0xa6,0xa7,0xa2,0xa3,
0xed,0xec,0xe9,0xe8,0xfd,0xfc,0xf9,0xf8,
0xad,0xac,0xa9,0xa8,0xbd,0xbc,0xb9,0xb8,
0x31,0x30,0x35,0x34,0x21,0x20,0x25,0x24,
0x71,0x70,0x75,0x74,0x61,0x60,0x65,0x64,
0x2a,0x2b,0x2e,0x2f,0x3a,0x3b,0x3e,0x3f,
0x6a,0x6b,0x6e,0x6f,0x7a,0x7b,0x7e,0x7f,
0x5d,0x5c,0x59,0x58,0x4d,0x4c,0x49,0x48,
0x1d,0x1c,0x19,0x18,0x0d,0x0c,0x09,0x08,
0x46,0x47,0x42,0x43,0x56,0x57,0x52,0x53,
0x06,0x07,0x02,0x03,0x16,0x17,0x12,0x13};

  //return mult(x,x);
  return sq[x];
}