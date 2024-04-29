#include "../common.h"
#define POMMES_INPUT_ISW_MULTIPLICATION SEC SEC SEC
#define POMMES_FUNCTION_CALL_GETRANDOMNESS SEC
#define N_SHARES 2

void isw_multiplication(uint32_t in_a[N_SHARES], uint32_t in_b[N_SHARES], uint32_t out[N_SHARES]){

	for(uint32_t k = 0; k < N_SHARES; ++k){
		out[k] = in_a[k] & in_b[k];
	}

	for(uint32_t i = 0; i < N_SHARES; ++i){
		for(uint32_t j = i + 1; j < N_SHARES; ++j){
			uint32_t s;
			getRandomness(&s);
			uint32_t r = (s ^ (in_a[i] & in_b[j])) ^ (in_a[j] & in_b[i]);
			out[i] = out[i] ^ s;
			out[j] = out[j] ^ r;
		}
	}

}