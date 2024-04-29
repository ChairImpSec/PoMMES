#pragma once
#include "gimple_parser.hpp"
#include "check_functions.hpp"
#include "operator.hpp"
#include <string>
#include <vector>
#include <stdint.h>
#include <bits/stdc++.h>
#include <sstream>


void parse_dereference(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens);
void parse_phi(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens);
void parse_condition(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens);
void parse_ssa(function& func, const std::string& line, virtual_variables_vec& virtual_variables);
void parse_security_level_of_parameter(function& func, virtual_variables_vec& virtual_variables);
void parse_single_parameter(function& func, const std::string& param);
void parse_parameter_list(function& func, virtual_variables_vec& virtual_variables, const std::string& parameters);
void parse_function_header(function& func, virtual_variables_vec& virtual_variables, const std::string& line);
void parse_function_body(function& func, virtual_variables_vec& virtual_variables, std::ifstream& infile);
void parse_local_variables(virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens, bool modified_local_variable);
void parse_bb_start(function& func, const std::string& line);
void parse_variable(virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens);
void parse_load_from_memory(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens);
void parse_virtual_variables(virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens);
void parse_raw_ssa(function& func, const std::string& line, virtual_variables_vec& virtual_variables, std::ifstream& infile);
void make_virtual_variable_information(virtual_variables_vec& virtual_variables, std::string ssa_name, int32_t sensitivity, DATA_TYPE data_type, data_specific_information data_information, SIGNED_TYPE signedness, std::string original_name, uint8_t data_type_size, int32_t index_to_original, SSA_ADD_ON ssa_add_on);