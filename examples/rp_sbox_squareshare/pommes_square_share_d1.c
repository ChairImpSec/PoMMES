#include "../common.h"
#define POMMES_INPUT_SQUARE_SHARE SEC PUB
#define POMMES_FUNCTION_CALL_SQUARE RETURNSEC SEC

void square_share(uint8_t *a,uint32_t n)
{
  uint32_t i;
  for(i=0;i<n;i++)
    a[i]=square(a[i]);
}
