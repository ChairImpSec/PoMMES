#pragma once

#include "code_generation.hpp"

void break_vertical(std::vector<liveness_interval_struct>& liveness_intervals, memory_security_struct& memory_security);
void split_into_bb_ranges_of_intervals(std::vector<TAC_bb>& control_flow_TAC, liveness_interval_struct& liveness_interval, const std::map<uint32_t, std::vector<uint32_t>>& predecessors);

void break_vertical_in_loops(std::vector<TAC_bb>& control_flow_TAC, std::vector<TAC_type>& TAC, const std::map<uint32_t, std::vector<uint32_t>>& successor_map,  std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m, std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion);

void break_shadow_register_overwrite_armv6m(std::vector<TAC_bb>& control_flow_TAC, std::map<uint32_t, std::vector<uint32_t>>& predecessors, std::vector<liveness_interval_struct>& liveness_intervals);
void break_memory_overwrite(std::vector<TAC_type>& TAC, std::vector<liveness_interval_struct>& liveness_intervals, memory_security_struct& memory_security);


void necessity_to_remove_vertical_dependency(std::vector<TAC_bb>& control_flow_TAC, uint32_t bb_idx_of_available_sens_reg, std::vector<liveness_interval_struct>& liveness_intervals, uint32_t succ, const std::map<uint32_t, std::vector<uint32_t>>& successor_map, bool& need_to_clear_register, std::vector<bool>& visited, uint32_t available_sensitive_real_register);
void break_transition_armv7m(std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers, uint32_t idx_of_current_interval, uint32_t& use_point, std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m, std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion);

void get_bb_idx_and_cfg_instr_idx_from_use_point( std::vector<TAC_bb>& control_flow_TAC, uint32_t& bb_idx, uint32_t& cfg_instr_idx, uint32_t use_point);
void clear_sens_reg_for_potential_external_functions(std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC, const std::map<uint32_t, std::vector<uint32_t>>& predecessors, liveness_interval_struct& sensitive_interval);