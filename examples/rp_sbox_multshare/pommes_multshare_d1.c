#include "../common.h"
#define POMMES_INPUT_MULTSHARE SEC SEC SEC PUB
#define POMMES_FUNCTION_CALL_GETRANDOMBYTE RETURNSEC
#define POMMES_FUNCTION_CALL_MULTTABLE RETURNSEC SEC SEC

void multshare(uint8_t *a,uint8_t *b,uint8_t *c,uint32_t n)
{
  // Memory: 4 bytes
  uint32_t i,j,k; 
  for(i=0;i<n;i++)
    c[i]=multtable(a[i],b[i]);

  for(k=0;k<n;k++)
  {
    for(j=k+1;j<n;j++)
    {
      uint8_t tmp=getRandomByte();//xorshf96(); //rand(); //randomness;
      uint8_t tmp2=(tmp ^ multtable(a[k],b[j])) ^ multtable(a[j],b[k]);
      c[k]^=tmp;
      c[j]^=tmp2;
    }
  }
}