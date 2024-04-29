#pragma once
#include <string>
#include <vector>
#include <tuple>
#include "boost/regex.hpp"
#include "boost/algorithm/string/regex.hpp"
#include <iostream>
#include <stdint.h>
#include "gimple_parser.hpp"

bool is_comment(std::string& line);
bool is_number(const std::string token);
bool is_array(const std::string token);
bool is_virtual_variable(const std::string token);
bool is_end_of_function(const std::string& line);
bool is_start_of_bb(const std::string& line);
bool is_end_of_bb(const std::string& line);
bool is_empty_or_blank(const std::string &s);
bool is_phi_ssa(std::vector<std::string> tokens);
bool is_variable(const std::string token);