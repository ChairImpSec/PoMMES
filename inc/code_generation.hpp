#pragma once
#include "gimple_parser.hpp"
#include <map>
#include <set>
#include <tuple>
#include <string>
#include <numeric>
#include <math.h>
#include "POMMES.hpp"
#include <cmath>

#define TAC_PRINT
#define TRANSITION_FREE
#define MEMORY_SHADOW_FREE
#define MEMORY_OVERWRITE_FREE
#define VERTICAL_FREE
#define ARMV6_M

#ifdef ARMV6_M
#define NUM_PHYSICAL_REGISTERS 8
#else
#define NUM_PHYSICAL_REGISTERS 12
#endif

enum class TAC_OPERAND{
    R_REG,
    V_REG,
    NUM,
    BEGIN_FUNC_RESERVE,
    END_FUNC_FREE,
    FUNC_CALL,
    PARAM,
    NONE

};

extern uint32_t v_register_ctr;
extern uint32_t v_function_borders;
extern uint32_t stack_pointer_offset_during_funcion;
extern uint32_t last_bb_number;
extern bool func_has_return_value;


typedef struct TAC_type{ //tac is quadruple (dest, op, src1, src2) plus sensitivity
    //all virtual registers are of form v_X where X is {0,...,infinity}
    bool                    psr_status_flag = false;
    int32_t                 dest_v_reg_idx;
    uint32_t                branch_destination;
    uint32_t                label_name;
    std::string             function_call_name;
    uint32_t                func_move_idx = UINT32_MAX;
    uint32_t                func_instr_number = UINT32_MAX;
    uint32_t                function_arg_num = UINT32_MAX;
    TAC_OPERAND             dest_type;
    TAC_OPERAND             src1_type;
    TAC_OPERAND             src2_type;
    uint32_t                src1_v;
    uint32_t                src2_v;
    Operation               op;
    uint8_t                 sensitivity;
    uint8_t                 dest_sensitivity;
    uint8_t                 src1_sensitivity;
    uint8_t                 src2_sensitivity;
    std::vector<uint32_t>   epilog_register_list;
    std::vector<uint32_t>   prolog_register_list;
    std::set<std::tuple<uint32_t,uint8_t>>   liveness_in;       //marks the point where a virtual register is used for the first time
    std::set<std::tuple<uint32_t,uint8_t>>   liveness_out;      //marks the point where a virtual register is used for the last time
    std::set<std::tuple<uint32_t,uint8_t>>   liveness_used;     //marks all points between liveness_in and liveness_out where virtual register is used a source
} TAC_type;

typedef struct local_live_set{
    std::set<uint32_t> live_gen; //contains virtual register numbers that are used in the block before they are defined
    std::set<uint32_t> live_kill; //contains all operands that are defined in the block
}local_live_set;

typedef struct global_live_set{
    std::set<uint32_t> live_in; //contains virtual register numbers that are used in the block before they are defined
    std::set<uint32_t> live_out; //contains all operands that are defined in the block
}global_live_set;

typedef struct stack_local_variable_tracking_struct{
    uint32_t    position; //relative to fp
    std::string curr_value;
    std::string base_value;
    bool        is_spill_element; //check if the position in the stack was ever used
    uint8_t     sensitivity;
    bool        free_spill_position;
    bool        valid_input_parameter_stored_in_local_stack = false;
    bool        is_initialized_array = false;
}stack_local_variable_tracking_struct;

typedef struct input_parameter_stack_variable_tracking_struct{
    uint32_t    position; //relative to fp
    std::string curr_value;
    std::string base_value;
    bool        is_spill_element; //check if the position in the stack was ever used
    uint8_t     sensitivity;
    bool        free_spill_position;
}input_parameter_stack_variable_tracking_struct;

typedef struct liveness_interval_struct{
    uint32_t virtual_register_number;

    std::vector<uint32_t> liveness_used;
    uint32_t real_register_number;
    bool    is_spilled;
    uint32_t position_on_stack;
    uint8_t sensitivity;
    std::vector<std::tuple<uint32_t, uint32_t>> ranges;
    std::vector<std::tuple<std::vector<uint32_t>, std::vector<uint32_t>>> split_bb_ranges; //(basic blocks, vector of used points in this basic block)
    std::vector<std::tuple<uint32_t, uint32_t>>                           arm6_m_xor_specification;// = std::make_tuple(UINT32_MAX, UINT32_MAX); //(other virtual register number that has to have same real register, used_point)
    std::vector<uint8_t>                                                  available_registers_after_own_allocation;
    bool                                                                  postponed_store = false;
    std::vector<uint32_t>                                                 bb_need_initial_store;
    bool                                                                  contains_break_transition = false;

}liveness_interval_struct;

typedef struct memory_security_struct{
    std::vector<stack_local_variable_tracking_struct>           stack_mapping_tracker;
    std::vector<input_parameter_stack_variable_tracking_struct> input_parameter_mapping_tracker; //from the 5th parameter onward are placed on the stack
    std::vector<std::tuple<uint8_t, uint32_t, uint32_t>>        shadow_memory_tracker;//(sensitivity, number of assigned virtual register, i.e. X of vX, instruction number where shadow register was modified last) //(sensitivity, index to virtual variable entry)

} memory_security_struct;

/**** required for later steps such as memory shadow removing ****/
typedef struct TAC_bb{
    std::vector<TAC_type> TAC;
    uint32_t TAC_bb_number;
    std::vector<int8_t>    local_shadow_memory_tracker; //(sensitivity, number of assigned virtual register, i.e. X of vX, instruction number where shadow register was modified last) //(sensitivity, index to virtual variable entry)
    int8_t                 global_shadow_memory_tracker; //memory operation that will be visible by other basic blocks
}TAC_bb;


#include "register_allocation.hpp"
#include "armv6_m_specifics.hpp"
#include "removing_security_hazard.hpp"
#include "operator.hpp"


void instruction_selection();
void generate_TAC(function& func, virtual_variables_vec& virtual_variables, std::vector<std::vector<uint8_t>>& adjacency_matrix);
void liveness_analysis(std::vector<TAC_type>& TAC, std::vector<std::tuple<uint32_t, float, float, std::vector<float>, uint32_t, uint8_t, bool, uint32_t>>& liveness_interval);
void linear_scan_algorithm_rolled(std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC, std::vector<liveness_interval_struct>& liveness_intervals, memory_security_struct& memory_security, const std::map<uint32_t, std::vector<uint32_t>>& predecessors, uint32_t number_of_input_registers);
void compute_local_live_sets(std::vector<local_live_set>& local_live_sets, uint32_t local_live_sets_start_index, std::vector<TAC_type>& TAC);
void compute_global_live_sets(std::vector<uint32_t>& reverse_block_order, std::map<uint32_t, std::vector<uint32_t>>& successor_map, std::vector<local_live_set>& local_live_sets, std::vector<global_live_set>& global_live_sets);
void generate_successor_map(std::vector<std::vector<uint8_t>>& adjacency_matrix, std::map<uint32_t, std::vector<uint32_t>>& successor_map);
void compute_liveness_intervals(std::vector<uint32_t>& reverse_block_order, std::vector<uint32_t>& start_points_of_blocks, std::vector<uint32_t>& end_points_of_blocks, std::vector<TAC_type>& TAC, std::vector<global_live_set>& global_live_sets, std::vector<liveness_interval_struct>& liveness_intervals, uint32_t number_of_virtual_registers);
void assign_memory_position(liveness_interval_struct& interval_to_memory, memory_security_struct& memory_security);
void print_interval(liveness_interval_struct liveness_interval);
void print_available_registers(std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers);
void print_allocated_registers(std::vector<std::tuple<uint32_t, uint32_t, uint8_t>>& allocated_registers);
void print_liveness_intervals(std::vector<liveness_interval_struct> liveness_intervals);
void synchronize_instruction_index(std::vector<liveness_interval_struct>& liveness_intervals, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers, uint32_t instruction_idx_to_insert,  std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m, std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion);
void insert_new_instruction_in_CFG_TAC(std::vector<TAC_bb>& control_flow_TAC, uint32_t instruction_idx_to_insert, const TAC_type& TAC_instr);
void print_TAC(std::vector<TAC_type>& TAC);
uint32_t get_bb_number_of_instruction_idx(std::vector<TAC_bb>& control_flow_TAC, uint32_t use_point);
uint32_t get_bb_idx_of_instruction_idx(std::vector<TAC_bb>& control_flow_TAC, uint32_t use_point);
uint32_t get_idx_of_bb_start(std::vector<TAC_bb>& control_flow_TAC, uint32_t bb_idx);
uint32_t get_idx_of_bb_end(std::vector<TAC_bb>& control_flow_TAC, uint32_t bb_idx);
void generate_generic_tmp_tac(TAC_type& tmp_tac, Operation op, uint8_t sens, TAC_OPERAND dest_type, uint32_t dest_v, uint8_t dest_sens, TAC_OPERAND src1_type, uint32_t src1_v, uint8_t src1_sens, TAC_OPERAND src2_type, uint32_t src2_v, uint8_t src2_sens, bool psr_stat_flag);
bool virt_reg_at_use_point_is_source(TAC_type& tac, uint32_t virtual_register_number);
bool virt_reg_at_use_point_is_destination(TAC_type& tac, uint32_t virtual_register_number);
void load_spill_from_memory_address(std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers, uint32_t& use_point, uint32_t real_register_of_spill, uint32_t index_of_liveness_interval, memory_security_struct& memory_security, std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m, std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion);
void store_spill_to_memory_address(std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers, uint32_t& use_point, uint32_t real_register_of_spill, uint32_t index_of_liveness_interval_to_spill, uint32_t start_range_of_latest_interval, memory_security_struct& memory_security, std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m, std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion, const std::map<uint32_t, std::vector<uint32_t>>& predecessors);
bool remove_str_optimization_possible(std::vector<TAC_bb>& control_flow_TAC, const std::vector<liveness_interval_struct>& liveness_intervals, liveness_interval_struct& current_liveness_interval, std::tuple<std::vector<uint32_t>, std::vector<uint32_t>>& split_range);
void synchronize_instruction_index_of_liveness_intervals(std::vector<liveness_interval_struct>& liveness_intervals, uint32_t instruction_idx_to_insert);
void remove_from_active(std::vector<liveness_interval_struct>& active, uint32_t index_of_spilled_interval);
void set_internal_mem_op_to_public(std::vector<TAC_type>& TAC, liveness_interval_struct& liveness_interval);
uint8_t return_sensitivity(int32_t sensitivity);
void one_interval_has_one_split_range(std::vector<liveness_interval_struct>& liveness_intervals, memory_security_struct &memory_security);
void check_vertical_correctness_in_CFG(std::vector<liveness_interval_struct>& liveness_intervals, std::vector<TAC_bb>& control_flow_TAC, std::vector<TAC_type>& TAC, const std::map<uint32_t, std::vector<uint32_t>>& successor_map);
void check_vertical_correctness_internal_functions_in_CFG(std::vector<liveness_interval_struct>& liveness_intervals, std::vector<TAC_bb>& control_flow_TAC, std::vector<TAC_type>& TAC, const std::map<uint32_t, std::vector<uint32_t>>& successor_map);