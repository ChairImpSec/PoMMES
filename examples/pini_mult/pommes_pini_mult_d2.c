#include "../common.h"
#define POMMES_INPUT_PINI_MULTIPLICATION SEC SEC SEC
#define POMMES_FUNCTION_CALL_GETRANDOMNESS SEC
#define N_SHARES 3

void pini_multiplication(uint32_t in_x[N_SHARES], uint32_t in_y[N_SHARES], uint32_t out_z[N_SHARES]){
    uint32_t r[N_SHARES][N_SHARES];
    for(uint8_t i = 0; i < N_SHARES; ++i){
        for(uint8_t j = i + 1; j < N_SHARES; ++j){
            getRandomness(&r[i][j]);
            r[j][i] = -r[i][j];
        }
    }
    
    for(uint8_t k = 0; k < N_SHARES; ++k){
        out_z[k] = in_x[k] * in_y[k];
        for(uint8_t l = 0; l < N_SHARES; ++l){
            if(l != k){
                out_z[k] = out_z[k] + ((in_x[k] + 1) * r[k][l] + in_x[k] * (in_y[l] - r[k][l]));
            }
        }
    }
}