#pragma once
#include "gimple_parser.hpp"
#include "security.hpp"
#include "printing.hpp"

void build_graph(function& func,std::vector<std::vector<uint8_t>>& graph, std::map<uint32_t, uint32_t>& node_to_bb_number);
void compute_predecessors(function& func, std::vector<std::vector<uint8_t>>& adjacency_matrix, std::map<uint32_t, std::vector<uint32_t>>& predecessors);
void compute_successors(function& func, std::vector<std::vector<uint8_t>>& adjacency_matrix, std::map<uint32_t, std::vector<uint32_t>>& successors);
