#pragma once
#include "gimple_parser.hpp"
void print_func(function& func, virtual_variables_vec& virtual_variables, bool& print_security);
void print_bb(basic_block& bb, virtual_variables_vec& virtual_variables, bool& print_security);
void print_node_variable_relation(function& func, virtual_variables_vec& virtual_variables);

void print_call(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security);
void print_nop_expr(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security);
void print_normal(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security);
void print_condition(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security);
void print_branch(ssa& instr);
void print_func_return(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security);
void print_func_no_return(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security);
void print_mem_deref(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security);
void print_arr_deref(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security);
void print_ref(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security);
void print_deref(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security);
void print_negate_expr(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security);