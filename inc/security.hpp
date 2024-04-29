#pragma once

#include "gimple_parser.hpp"
#include "graph.hpp"

void resolve_dependency(std::vector<std::vector<uint32_t>>& dependency_adjacent_matrix, virtual_variables_vec& virtual_variable);

void sensitivity_propagation(function& func, std::vector<std::vector<uint8_t>>& adjacency_matrix, virtual_variables_vec& virtual_variables);
void static_taint_tracking(function& func, std::vector<std::vector<uint8_t>>& adjacency_matrix, virtual_variables_vec& virtual_variables);