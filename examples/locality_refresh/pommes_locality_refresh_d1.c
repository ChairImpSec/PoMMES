#include "../common.h"
#define POMMES_INPUT_LOCALITY_REFRESH SEC SEC
#define POMMES_FUNCTION_CALL_GETRANDOMNESS SEC PUB
#define N_SHARES 2

void locality_refresh(uint32_t in[N_SHARES], uint32_t out[N_SHARES]){
    out[N_SHARES-1] = in[N_SHARES-1];
    uint32_t s;
    for(uint32_t i = 0; i < N_SHARES-1; ++i){
        getRandomness(&s, i);
        out[i] = s;
        out[N_SHARES-1] = out[N_SHARES-1] ^ (in[i] ^ s);
    }
}
