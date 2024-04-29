#pragma once
#include "code_generation.hpp"
#include "removing_security_hazard.hpp"

void add_range(liveness_interval_struct& liveness_interval, uint32_t block_from, uint32_t instruction_idx);

void compute_liveness_intervals(std::vector<uint32_t>& reverse_block_order, std::vector<uint32_t>& start_points_of_blocks, std::vector<uint32_t>& end_points_of_blocks, std::vector<TAC_type>& TAC, std::vector<global_live_set>& global_live_sets, std::vector<liveness_interval_struct>& liveness_intervals, uint32_t number_of_virtual_registers);

bool any_live_set_changed(std::vector<global_live_set>& old_global_live_sets,  std::vector<global_live_set> updated_global_live_sets);

void compute_global_live_sets(std::vector<uint32_t>& reverse_block_order, std::map<uint32_t, std::vector<uint32_t>>& successor_map, std::vector<local_live_set>& local_live_sets, std::vector<global_live_set>& global_live_sets);

void compute_local_live_sets(std::vector<local_live_set>& local_live_sets, uint32_t local_live_sets_start_index, std::vector<TAC_type>& TAC);

void liveness_analysis(std::vector<TAC_type>& TAC, std::vector<std::tuple<uint32_t, float, float, std::vector<float>, uint32_t, uint8_t, bool, uint32_t>>& liveness_interval);