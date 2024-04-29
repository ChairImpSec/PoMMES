#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <regex>
#include <stdlib.h>
#include "boost/tokenizer.hpp"
#include "boost/algorithm/string.hpp"
#include <algorithm>
#include <tuple>
#include <map>
#include <memory>

#include "boost/regex.hpp"
#include "boost/algorithm/string/regex.hpp"
#include "boost/variant.hpp"

enum class SIGNED_TYPE{
    UNSIGNED,
    SIGNED
};



enum class INSTRUCTION_TYPE{
    INSTR_DEFAULT,
    INSTR_NORMAL,
    INSTR_DEREF,
    INSTR_REF,
    INSTR_FUNC_RETURN,
    INSTR_FUNC_NO_RETURN,
    INSTR_BRANCH,
    INSTR_CONDITON,
    INSTR_PHI,
    INSTR_ASSIGN,
    INSTR_SPECIAL,
    INSTR_LOAD,
    INSTR_STORE,
    INSTR_RETURN,
    INSTR_LABEL,
    INSTR_NEGATE,
    INSTR_INITIALIZE_LOCAL_ARR
};

/**
 * @brief information what changes are made inside the ssa file to the original variable
 * 
 */
enum class SSA_ADD_ON{
    NO_ADD_ON,
    ARR_ADD_ON,
    ASTERISK_ADD_ON,
    AMP_ADD_ON,
    ASTERISK_ARR_ADD_ON,
    AMP_ARR_ADD_ON
};


enum class DATA_STORAGE_TYPE{
    ASCII,
    WORD,
    HALFWORD,
    BYTE
};

enum class DATA_TYPE{
    UNDEFINED,

    INPUT_ARR,          //e.g. in[3][3]
    INPUT_ARR_PTR,      //e.g. * in[3][3]
    INPUT_ARR_ADDR,     //e.g. &in[3][3]
    INPUT_VAR,          //e.g. in
    INPUT_VAR_ADDR,     //e.g. &in
    INPUT_VAR_PTR,      //e.g. *in
    
    LOCAL_ARR,
    LOCAL_ARR_ADDR,
    LOCAL_ARR_PTR,
    LOCAL_VAR,
    LOCAL_VAR_ADDR,
    LOCAL_VAR_PTR,
    
    VIRTUAL_ARR,
    VIRTUAL_ARR_ADDR,
    VIRTUAL_ARR_PTR,
    VIRTUAL_VAR,
    VIRTUAL_VAR_ADDR,
    VIRTUAL_VAR_PTR,
    
    NUM,
    
    NONE

};

enum class Operation{
    NONE,
    MOV,
    FUNC_RES_MOV,
    ADD,
    ADD_INPUT_STACK,
    SUB,
    MUL,
    REF,
    LSL,
    LSR,
    XOR,
    DIV,
    MOD,
    PHI,
    DEREF,
    LOGICAL_AND,
    LOGICAL_OR,
    EQUAL,
    NOT_EQUAL,
    INVERT,
    LESS_EQUAL,
    GREATER_EQUAL,
    LESS,
    GREATER,
    LOADWORD_WITH_NUMBER_OFFSET,
    STOREWORD_WITH_NUMBER_OFFSET,
    LOADWORD_FROM_INPUT_STACK,
    LOADWORD_WITH_LABELNAME,
    LOADWORD_NUMBER_INTO_REGISTER,
    LOADWORD,
    STOREWORD,
    LOADHALFWORD,
    STOREHALFWORD,
    LOADBYTE,
    STOREBYTE,
    PARAM,
    FUNC_CALL,
    FUNC_MOVE,
    COMPARE,
    BRANCH,
    PUSH,
    POP,
    LABEL,
    PROLOG_PUSH,
    EPILOG_POP,
    NEG
};

/**
 * @brief if operand is variable: op_data is index to variable in virtual_variables, if operand is number: number itself, DATA_TYPE distinguishes what op_data is
 * 
 */
typedef struct operand{
    DATA_TYPE                        op_type = DATA_TYPE::UNDEFINED;
    uint32_t                         op_data;
}operand;

/**
 * @brief handles variables of form 'type * id'
 * 
 */
typedef struct pointer_information{
    uint32_t dimension = 0; //contains #non-fix-sized dimensions, in example dimension = 1
}pointer_information;

/**
 * @brief handles variables of form 'type [dimX][dimY] * * * id', i.e. variables that have multiple dimensions
 * 
 */
typedef struct array_information{       
    uint32_t dimension = 0;     //contains #fix-sized dimensions + # , in example dimension = 5
    uint32_t dimension_without_size = 0; //contains #non-fix-sized dimensions in example dimension = 3
    std::vector<operand> depth_per_dimension; //contais information about fix-sized dimensions
}array_information;


typedef struct struct_information{ //TODO: fill

}struct_information;

typedef struct default_information{
    int8_t d_info = -1;
}default_information;

typedef boost::variant<pointer_information, array_information,default_information> data_specific_information;

/**
 * @brief handles internal variable information
 * 
 */
typedef struct virtual_variable_information{
    std::string                 ssa_name; //name found in ssa file, as ssa representation is unique some variables might be variations of the actual variable (e.g. s.0_5 for original variable s)
    int32_t                     sensitivity; //0 = public , 1 = secret; > 1 = contains secret data but itself is not secret
    DATA_TYPE                   data_type;
    data_specific_information   data_information; //depending on data type we have additional information
    SIGNED_TYPE                 signedness;
    std::string                 original_name; //original name of variable (e.g. s for variable s.0_5)
    int32_t                     index_to_original; //if variable is variation of original variable this tells where the original variable inside the vector of variables is
    uint8_t                     data_type_size;
    SSA_ADD_ON                  ssa_add_on; 
} virtual_variable_information;
                                //ssa name, sensitivity, data type, type information, signdness, original name, data type size
typedef std::vector<virtual_variable_information> virtual_variables_vec;


typedef struct default_instruction{
    std::string err;
}default_instruction;

typedef struct variable{
    uint32_t        index_to_variable;
}variable;



typedef struct normal_instruction{
    variable dest;
    operand src1;
    operand src2;
    Operation operation;
}normal_instruction;

typedef struct dereference_instruction{
    variable dest;
    variable src;
}dereference_instruction;

typedef struct mem_load_instruction{
    variable dest;
    variable base_address;
    operand offset;
}mem_load_instruction;

typedef struct mem_store_instruction{
    variable dest;
    operand value;
}mem_store_instruction;

typedef struct reference_instruction{
    variable dest;
    variable src;
}reference_instruction;

typedef struct param_struct{
    operand op;
    // PASS_TYPE pass;
    uint8_t sensitivity;
}param_struct;

typedef struct function_call_with_return_instruction{
    variable dest;
    uint8_t dest_sensitivity;
    std::string func_name;
    std::vector<param_struct> parameters;
}function_call_with_return_instruction;

typedef struct function_call_without_return_instruction{
    std::string func_name;
    std::vector<param_struct> parameters;
}function_call_without_return_instruction;

typedef struct branch_instruction{
    uint32_t dest_bb;
}branch_instruction;

typedef struct label_instruction{
    std::string label_name;
}label_instruction;


typedef struct condition_instruction{
    // std::string key;
    operand op1;
    operand op2;
    Operation op;
    uint32_t true_dest;
    uint32_t false_dest;
}condition_instruction;

typedef struct phi_instruction{
    variable dest;
    std::vector<std::tuple<uint32_t, uint32_t>> srcs; //(name, bb_number)
}phi_instruction;

typedef struct assignment_instruction{
    variable    dest;
    operand     src;
}assignment_instruction;

typedef struct initialize_local_array_instruction{
    std::string                 data_section_name;
    uint32_t                    data_size;
    std::vector<uint32_t>       num_data;
    std::string                 ascii_data;
    DATA_STORAGE_TYPE           data_storage;
    uint32_t                    virtual_variable_idx;
}initialize_local_array_instruction;

typedef struct special_instruction{

}special_instruction;


typedef struct return_instruction{
    operand     return_value;
}return_instruction;


typedef boost::variant<normal_instruction, mem_load_instruction, mem_store_instruction, dereference_instruction, reference_instruction, function_call_with_return_instruction, function_call_without_return_instruction, branch_instruction, condition_instruction, phi_instruction, assignment_instruction, special_instruction, return_instruction, label_instruction, initialize_local_array_instruction, default_instruction> instruction;


typedef struct ssa{
    INSTRUCTION_TYPE                                instr_type = INSTRUCTION_TYPE::INSTR_DEFAULT;
    instruction                                     instr;
    int32_t                                         sensitivity_level = -1;
}ssa;


typedef struct basic_block{
    std::vector<ssa>                                        ssa_instruction;
    uint32_t                                                bb_number;
    std::vector<std::tuple<uint32_t, int32_t>>              in; //required for static forward dataflow taint tracking
    std::vector<std::tuple<uint32_t, int32_t>>              out;//required for static forward dataflow taint tracking

} basic_block;

typedef struct parameter{
    std::string                 type; //uint32_t, int, float, double, struct, ...
    std::string                 id; //name of input e.g. func(uint32 in_a) -> id = in_a
    uint8_t                     sensitivity;

} parameter;

typedef struct internal_function_calls_struct{
    std::string             function_name;
    std::vector<uint8_t>    parameter_sensitivity;
    uint8_t                 return_sensitivity;
} internal_function_calls_struct;


typedef struct data_section_struct{
    std::string                 data_section_name;
    uint32_t                    data_size;
    std::vector<uint32_t>       num_data;
    std::string                 ascii_data;
    DATA_STORAGE_TYPE           data_storage;
}data_section_struct;

typedef struct function{
    std::string                 function_name;
    uint32_t                    biggest_virtual_variable_number; //required for loop unrolling
    //std::vector<uint8_t>        sensitivity; //0 -> public input, 1 -> secret
    std::vector<parameter>      arguments;
    std::vector<basic_block>    bb;
    std::vector<internal_function_calls_struct>    internal_function_calls;
    uint8_t                     return_sensitivity;
    std::vector<data_section_struct>         data_section;       
} function;


void gimple_parser(function& func, virtual_variables_vec& virtual_variables, std::ifstream& infile);
void split_line_into_tokens(std::vector<std::string>& tokens, const std::string& line);
uint32_t get_index_of_virtual_variable(virtual_variables_vec& virtual_variables, const std::string& var, std::string modification);
void get_operand_type(operand& op, virtual_variables_vec& virtual_variables, const std::string& token);
void get_variables_from_phi(virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens);
void clean_tokens(std::vector<std::string>& tokens);
std::tuple<std::string, int32_t> get_original_name_and_idx_of_ssa_name(const std::string ssa_name,const virtual_variables_vec& virtual_variables);
