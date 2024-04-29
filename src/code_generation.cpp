#include "code_generation.hpp"


uint32_t v_register_ctr = 0;
uint32_t v_function_borders = 0;
uint32_t stack_pointer_offset_during_funcion = 0;
uint32_t last_bb_number = UINT32_MAX;
bool func_has_return_value = false;

std::string function_name = "";

std::map<std::tuple<std::string, std::string>, uint32_t> virtual_variable_to_v_reg; //original name and ssa_name as key, if original name has no type of arr add on -> ssa_name is empty: e.g. (s, ""), (*_19[i][j], *_19[i_44][j_45])



uint8_t return_sensitivity(int32_t sensitivity){
    if(sensitivity == 1){
        return 1;
    }
    else if((sensitivity != 1) && (sensitivity >= 0)){
        return 0;
    }
    else{
        throw std::runtime_error("in return_sensitivity: sensitivity must not be negative at this point!");
    }
}

bool is_virtual(virtual_variable_information& var_info){
    if((var_info.data_type == DATA_TYPE::VIRTUAL_ARR) || (var_info.data_type == DATA_TYPE::VIRTUAL_ARR_ADDR) || (var_info.data_type == DATA_TYPE::VIRTUAL_ARR_PTR) || (var_info.data_type == DATA_TYPE::VIRTUAL_VAR) || (var_info.data_type == DATA_TYPE::VIRTUAL_VAR_ADDR) || (var_info.data_type == DATA_TYPE::VIRTUAL_VAR_PTR)){
        return true;
    }
    return false;
}

bool is_local(virtual_variable_information& var_info){
    if((var_info.data_type == DATA_TYPE::LOCAL_ARR) || (var_info.data_type == DATA_TYPE::LOCAL_ARR_ADDR) || (var_info.data_type == DATA_TYPE::LOCAL_ARR_PTR) || (var_info.data_type == DATA_TYPE::LOCAL_VAR) || (var_info.data_type == DATA_TYPE::LOCAL_VAR_ADDR) || (var_info.data_type == DATA_TYPE::LOCAL_VAR_PTR)){
        return true;
    }
    return false;
}

bool is_input(virtual_variable_information& var_info){
    if((var_info.data_type == DATA_TYPE::INPUT_ARR) || (var_info.data_type == DATA_TYPE::INPUT_ARR_ADDR) || (var_info.data_type == DATA_TYPE::INPUT_ARR_PTR) || (var_info.data_type == DATA_TYPE::INPUT_VAR) || (var_info.data_type == DATA_TYPE::INPUT_VAR_ADDR) || (var_info.data_type == DATA_TYPE::INPUT_VAR_PTR)){
        return true;
    }
    return false;
}


bool contains_asterisk(SSA_ADD_ON ssa_add_on){
    if((ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON) || (ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON)){
        return true;
    }
    return false;
}

bool contains_amp(SSA_ADD_ON ssa_add_on){
    if((ssa_add_on == SSA_ADD_ON::AMP_ADD_ON) || (ssa_add_on == SSA_ADD_ON::AMP_ARR_ADD_ON)){
        return true;
    }
    return false;
}

bool contains_arr(SSA_ADD_ON ssa_add_on){
    if((ssa_add_on == SSA_ADD_ON::ARR_ADD_ON) || (ssa_add_on == SSA_ADD_ON::AMP_ARR_ADD_ON) || (ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON)){
        return true;
    }
    return false;
}



std::string generate_original_name(virtual_variable_information& virt_var_info, virtual_variables_vec& virtual_variables){
    std::string tmp_var_name = virt_var_info.ssa_name;
    std::string original_name = "";
    //append modifications
    if(contains_amp(virt_var_info.ssa_add_on)){
        original_name += "&";
        tmp_var_name = tmp_var_name.substr(1, tmp_var_name.size() - 1);
    }
    else if(contains_asterisk(virt_var_info.ssa_add_on)){
        original_name += "*";
        tmp_var_name = tmp_var_name.substr(1, tmp_var_name.size() - 1);
    }

    //strip possible array brackets
    tmp_var_name = tmp_var_name.substr(0, tmp_var_name.find("["));


    original_name += virt_var_info.original_name;

    //if virtual variable contains array information add it to string
    if(contains_arr(virt_var_info.ssa_add_on)){
        array_information a_info = boost::get<array_information>(virt_var_info.data_information);
        for(uint32_t dim_idx = 0; dim_idx < a_info.dimension; ++dim_idx){
            if(a_info.depth_per_dimension.at(dim_idx).op_type != DATA_TYPE::NONE){ //skip all dimensions with unkown size
                original_name += "[";
                //entries of array can either be number or virtual variable
                if(a_info.depth_per_dimension.at(dim_idx).op_type == DATA_TYPE::NUM){
                    original_name += std::to_string(a_info.depth_per_dimension.at(dim_idx).op_data);
                }
                else{

                    original_name += virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data).original_name;
                }
                original_name += "]";
            }
        }
    }


    return original_name;
}

std::string generate_ssa_name_for_register_mapping(virtual_variable_information& virt_var){
    if((virt_var.ssa_add_on == SSA_ADD_ON::AMP_ARR_ADD_ON) || (virt_var.ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON) || (virt_var.ssa_add_on == SSA_ADD_ON::ARR_ADD_ON)){
        return virt_var.ssa_name;
    }
    return "";
}

bool variable_assigned_to_register(std::string original_name, std::string ssa_name){
    std::tuple<std::string, std::string> key_tuple = std::make_tuple(original_name, ssa_name); 
    if(virtual_variable_to_v_reg.find(key_tuple) == virtual_variable_to_v_reg.end()){
        return false;
    }
    return true;
}

void generate_generic_tmp_tac(TAC_type& tmp_tac, Operation op, uint8_t sens, TAC_OPERAND dest_type, uint32_t dest_v, uint8_t dest_sens, TAC_OPERAND src1_type, uint32_t src1_v, uint8_t src1_sens, TAC_OPERAND src2_type, uint32_t src2_v, uint8_t src2_sens, bool psr_stat_flag){
    tmp_tac.op = op;
    tmp_tac.sensitivity = sens;

    tmp_tac.dest_type = dest_type;
    tmp_tac.dest_v_reg_idx = dest_v;
    tmp_tac.dest_sensitivity = return_sensitivity(dest_sens);
    
    tmp_tac.src1_type = src1_type;
    tmp_tac.src1_v = src1_v;
    tmp_tac.src1_sensitivity = return_sensitivity(src1_sens);
    
    tmp_tac.src2_type = src2_type;
    tmp_tac.src2_v = src2_v;
    tmp_tac.src2_sensitivity = return_sensitivity(src2_sens);

    tmp_tac.branch_destination = UINT32_MAX;

    tmp_tac.psr_status_flag = psr_stat_flag;
}

void print_tac_sensitivity(uint8_t sensitivity){
    if(sensitivity == 1){
        std::cout << "sec_";
    }
    else{
        std::cout << "pub_";
    }
}

std::string print_tac_status_flag(bool psr_status_flag){
    if(psr_status_flag){
        return "s";
    }
    return "";
}

std::string print_tac_operation(Operation op){
    switch(op){
        case Operation::ADD_INPUT_STACK:
        case Operation::ADD : return "add";
        case Operation::FUNC_MOVE: 
        case Operation::FUNC_RES_MOV:
        case Operation::MOV : return "mov";
        case Operation::SUB :  return "sub" ;
        case Operation::MUL : return "mul";
        case Operation::DIV : return "div";
        case Operation::XOR : return "eor" ;
        case Operation::MOD : return "mod";
        case Operation::REF : return "&";
        case Operation::DEREF : return "*";
        case Operation::LSR : return "lsr" ;
        case Operation::LSL :  return "lsl" ;
        case Operation::LOGICAL_AND: return "and" ;
        case Operation::LOGICAL_OR: return "orr" ;
        case Operation::INVERT: return "neg";
        case Operation::LOADBYTE: return "ldrb";
        case Operation::STOREBYTE: return "strb";
        case Operation::LOADHALFWORD: return "ldrh";
        case Operation::STOREHALFWORD: return "strh";
        case Operation::LOADWORD: return "ldr";
        case Operation::STOREWORD: return "str";
        case Operation::LOADWORD_NUMBER_INTO_REGISTER: return "ldr";
        case Operation::LOADWORD_WITH_LABELNAME: return "ldr";
        case Operation::LOADWORD_WITH_NUMBER_OFFSET: return "ldr";
        case Operation::LOADWORD_FROM_INPUT_STACK: return "ldr";
        case Operation::STOREWORD_WITH_NUMBER_OFFSET: return "str";
        case Operation::COMPARE: return "cmp";
        case Operation::GREATER: return "bgt";
        case Operation::GREATER_EQUAL: return "bge";
        case Operation::LESS: return "blt";
        case Operation::LESS_EQUAL: return "ble";
        case Operation::EQUAL: return "beq";
        case Operation::NOT_EQUAL: return "bne";
        case Operation::BRANCH: return "b";
        case Operation::PARAM: return "param";
        case Operation::FUNC_CALL: return "func_call";
        case Operation::PUSH: return "push";
        case Operation::POP: return "pop";
        case Operation::LABEL: return "";
        case Operation::NEG: return "neg" ;
        default: std::cout << static_cast<uint32_t>(op) << std::endl; throw std::runtime_error("unkown operation during print"); //
    }
}

void print_tac_prolog_push(TAC_type tac){
    std::cout << "push {";

    for(uint32_t i = 0; i < tac.prolog_register_list.size(); ++i){
        std::cout << "r" << tac.prolog_register_list.at(i);
        if(i != tac.prolog_register_list.size() - 1){
            std::cout << ", ";
        }
    }
    std::cout << "}" << std::endl;
}

void print_tac_epilog_pop(TAC_type tac){
    std::cout << "pop {";
    for(uint32_t i = 0; i < tac.epilog_register_list.size(); ++i){
        std::cout << "r" << tac.epilog_register_list.at(i);
        if(i != tac.epilog_register_list.size() - 1){
            std::cout << ", ";
        }
    }
    std::cout << "}" << std::endl;
}

void print_tac_normal(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }

    std::cout << print_tac_operation(tac.op) << print_tac_status_flag(tac.psr_status_flag) << " ";
    if(tac.dest_type == TAC_OPERAND::V_REG){
        std::cout << "v";
    }
    else if(tac.dest_type == TAC_OPERAND::R_REG){
        std::cout << "r";
    }
    else{
        throw std::runtime_error("in print_tac_normal: register is neither virtual nor real");
    }
    std::cout << std::to_string(static_cast<uint32_t>(tac.dest_v_reg_idx)) << ", ";
    if(tac.src1_type == TAC_OPERAND::NUM){
        std::cout << std::to_string(tac.src1_v) << ", ";
    }
    else{
        if(tac.src1_type == TAC_OPERAND::V_REG){
            std::cout << "v";
        }
        else if(tac.src1_type == TAC_OPERAND::R_REG){
            std::cout << "r";
        }
        else{
            throw std::runtime_error("in print_tac_normal: register is neither virtual nor real");
        }
        std::cout<< std::to_string(tac.src1_v) << ", ";
    }
    if(tac.src2_type == TAC_OPERAND::NUM){
        std::cout << "#" << std::to_string(tac.src2_v);
    }
    else{
        if(tac.src2_type == TAC_OPERAND::V_REG){
            std::cout << "v";
        }
        else if(tac.src2_type == TAC_OPERAND::R_REG){
            std::cout << "r";
        }
        else{
            throw std::runtime_error("in print_tac_normal: register is neither virtual nor real");
        }
        std::cout << std::to_string(tac.src2_v);
    }
    std::cout << std::endl;
}

void print_tac_assign(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }
    std::cout << print_tac_operation(tac.op) << print_tac_status_flag(tac.psr_status_flag);
    if(tac.dest_type == TAC_OPERAND::V_REG){
        std::cout << " v";
    }
    else if(tac.dest_type == TAC_OPERAND::R_REG){
        std::cout << " r";
    }
    else{
        throw std::runtime_error("in print_tac_assign: destination register is neither virtual nor real");
    }
    std::cout << std::to_string(tac.dest_v_reg_idx) << ", ";
    if(tac.src1_type == TAC_OPERAND::NUM){
        std::cout << "#" << std::to_string(tac.src1_v);
    }
    else{
        if(tac.src1_type == TAC_OPERAND::V_REG){
            std::cout << "v";
        }
        else if(tac.src1_type == TAC_OPERAND::R_REG){
            std::cout << "r";
        }
        else{
            throw std::runtime_error("in print_tac_assign: source register is neither virtual nor real");
        }
        std::cout<< std::to_string(tac.src1_v);
    }
    std::cout << std::endl;
}

void print_tac_negate(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }
    std::cout << print_tac_operation(tac.op) << print_tac_status_flag(tac.psr_status_flag);
    if(tac.dest_type == TAC_OPERAND::V_REG){
        std::cout << " v";
    }
    else if(tac.dest_type == TAC_OPERAND::R_REG){
        std::cout << " r";
    }
    else{
        throw std::runtime_error("in print_tac_assign: destination register is neither virtual nor real");
    }
    std::cout << std::to_string(tac.dest_v_reg_idx) << ", ";
    if(tac.src1_type == TAC_OPERAND::NUM){
        std::cout << std::to_string(tac.src1_v);
    }
    else{
        if(tac.src1_type == TAC_OPERAND::V_REG){
            std::cout << "v";
        }
        else if(tac.src1_type == TAC_OPERAND::R_REG){
            std::cout << "r";
        }
        else{
            throw std::runtime_error("in print_tac_assign: source register is neither virtual nor real");
        }
        std::cout<< std::to_string(tac.src1_v);
    }
    std::cout << std::endl;
}

void print_tac_branch(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }
    std::cout << print_tac_operation(tac.op) << " .L" + function_name + "_bb" << std::to_string(tac.branch_destination);
    std::cout << std::endl;
}

void print_tac_default(){
    std::cout << "nop" << std::endl;
}

void print_tac_label(TAC_type tac){
    std::cout << ".L" + function_name + "_bb" << std::to_string(tac.label_name) << ":" << std::endl;
}

void print_tac_load_with_labelname(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }
    std::cout << print_tac_operation(tac.op);
    if(tac.dest_type == TAC_OPERAND::V_REG){
        std::cout << " v";
    }
    else if(tac.dest_type == TAC_OPERAND::R_REG){
        std::cout << " r";
    }
    else{
        throw std::runtime_error("in print_tac_deref: register is neither virtual nor real");
    }
    std::cout << std::to_string(tac.dest_v_reg_idx) << ", ";
    std::cout << tac.function_call_name << std::endl;
}

void print_tac_load_with_number_offset(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }
    std::cout << print_tac_operation(tac.op);
    if(tac.dest_type == TAC_OPERAND::V_REG){
        std::cout << " v";
    }
    else if(tac.dest_type == TAC_OPERAND::R_REG){
        std::cout << " r";
    }
    else{
        throw std::runtime_error("in print_tac_deref: register is neither virtual nor real");
    }
    std::cout << std::to_string(tac.dest_v_reg_idx) << ", ";
    if(tac.src1_type == TAC_OPERAND::NUM){
        std::cout << std::to_string(tac.src1_v) << ", ";
    }
    else{
        if(tac.src1_type == TAC_OPERAND::V_REG){
            std::cout << "[v";
        }
        else if(tac.src1_type == TAC_OPERAND::R_REG){
            std::cout << "[r";
        }
        else{
            throw std::runtime_error("in print_tac_deref: register is neither virtual nor real");
        }
        std::cout<< std::to_string(tac.src1_v) << " ";

        if(tac.src2_type == TAC_OPERAND::NUM){
            std::cout << ", #" << std::to_string(tac.src2_v) << "]";
        }
        else if(tac.src2_type == TAC_OPERAND::R_REG){
            std::cout << ", r" << std::to_string(tac.src2_v) << "]";
        }
        else{
            throw std::runtime_error("in print_tac_load_with_number_offset: offset is not number or real register.");
        }

        
    }
    std::cout << std::endl;
}


void print_tac_load(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }
    std::cout << print_tac_operation(tac.op);
    if(tac.dest_type == TAC_OPERAND::V_REG){
        std::cout << " v";
    }
    else if(tac.dest_type == TAC_OPERAND::R_REG){
        std::cout << " r";
    }
    else{
        throw std::runtime_error("in print_tac_load: register is neither virtual nor real");
    }
    std::cout << static_cast<uint32_t>(tac.dest_v_reg_idx) << ", ";
    if(tac.src1_type == TAC_OPERAND::NUM){
        std::cout << "=" << std::to_string(tac.src1_v);
    }
    else{
        if(tac.src1_type == TAC_OPERAND::V_REG){
            std::cout << "[v";
        }
        else if(tac.src1_type == TAC_OPERAND::R_REG){
            std::cout << "[r";
        }
        else{
            throw std::runtime_error("in print_tac_load: register is neither virtual nor real");
        }
        std::cout<< std::to_string(tac.src1_v) << "]";
    }
    std::cout << std::endl;
}

void print_tac_compare(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }
    std::cout << print_tac_operation(tac.op) << " ";
    if(tac.src1_type == TAC_OPERAND::NUM){
        std::cout << std::to_string(tac.src1_v) << ", ";
    }
    else{
        if(tac.src1_type == TAC_OPERAND::V_REG){
            std::cout << "v";
        }
        else if(tac.src1_type == TAC_OPERAND::R_REG){
            std::cout << "r";
        }
        else{
            throw std::runtime_error("in print_tac_compare: register is neither virtual nor real");
        }
        std::cout << std::to_string(tac.src1_v) << ", ";
    }
    if(tac.src2_type == TAC_OPERAND::NUM){
        std::cout << std::to_string(tac.src2_v);
    }
    else{
        if(tac.src2_type == TAC_OPERAND::V_REG){
            std::cout << "v";
        }
        else if(tac.src2_type == TAC_OPERAND::R_REG){
            std::cout << "r";
        }
        else{
            throw std::runtime_error("in print_tac_compare: register is neither virtual nor real");
        }
        std::cout << std::to_string(tac.src2_v);
    }
    std::cout << std::endl;
}

void print_tac_store_with_number_offset(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }
    std::cout << print_tac_operation(tac.op) << " ";
    if(tac.src1_type == TAC_OPERAND::NUM){
        std::cout << std::to_string(tac.src1_v) << ", ";
    }
    else{
        if(tac.src1_type == TAC_OPERAND::V_REG){
            std::cout << "v";
        }
        else if(tac.src1_type == TAC_OPERAND::R_REG){
            std::cout << "r";
        }
        else{
            throw std::runtime_error("in print_tac_store: register is neither virtual nor real");
        }
        std::cout << std::to_string(tac.src1_v) << ", ";
    }
    std::cout << "[";
    if(tac.src2_type == TAC_OPERAND::V_REG){
        std::cout << "v";
    }
    else if(tac.src2_type == TAC_OPERAND::R_REG){
        std::cout << "r";
    }
    else{
        throw std::runtime_error("in print_tac_store: register is neither virtual nor real");
    }
    std::cout << std::to_string(tac.src2_v) << ", #" << std::to_string(tac.dest_v_reg_idx) << "]";
    std::cout << std::endl;
}

void print_tac_store(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }
    std::cout << print_tac_operation(tac.op) << " ";
    if(tac.src1_type == TAC_OPERAND::NUM){
        std::cout << std::to_string(tac.src1_v) << ", ";
    }
    else{
        if(tac.src1_type == TAC_OPERAND::V_REG){
            std::cout << "v";
        }
        else if(tac.src1_type == TAC_OPERAND::R_REG){
            std::cout << "r";
        }
        else{
            throw std::runtime_error("in print_tac_store: register is neither virtual nor real");
        }
        std::cout << std::to_string(tac.src1_v) << ", ";
    }
    std::cout << "[";
    if(tac.src2_type == TAC_OPERAND::V_REG){
        std::cout << "v";
    }
    else if(tac.src2_type == TAC_OPERAND::R_REG){
        std::cout << "r";
    }
    else{
        throw std::runtime_error("in print_tac_store: register is neither virtual nor real");
    }
    std::cout << std::to_string(tac.src2_v) << "]";
    std::cout << std::endl;
}

void print_tac_condition_branch(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }
    std::cout << print_tac_operation(tac.op) << " .L" << function_name << "_bb" << std::to_string(tac.branch_destination);
    std::cout << std::endl;
}

void print_tac_param(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }
    std::cout << print_tac_operation(tac.op);
    if(tac.dest_type == TAC_OPERAND::V_REG){
        std::cout << " v";
    }
    else if(tac.dest_type == TAC_OPERAND::R_REG){
        std::cout << " r";
    }
    else{
        throw std::runtime_error("in print_tac_param: register is neither virtual nor real");
    }
    std::cout << std::to_string(tac.dest_v_reg_idx);
    std::cout << std::endl;
}

void print_tac_function_call(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }
    std::cout << "bl " << tac.function_call_name << std::endl;
}

void print_tac_push(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }
    std::cout << "push {r" << tac.src1_v << "}"<< std::endl;
}

void print_tac_pop(TAC_type tac, bool print_sens = false){

    if(print_sens){
        print_tac_sensitivity(tac.sensitivity);
    }
    std::cout << "pop {r" << tac.src1_v << "}" << std::endl;
}



void get_virtual_address(std::vector<TAC_type>& TAC, uint32_t& calling_operation_src_v, uint8_t& calling_operation_sens, virtual_variables_vec& virtual_variables, virtual_variable_information& virt_var, std::string ssa_original_name, std::string ssa_name, SSA_ADD_ON original_ssa_add_on, bool is_load, ssa& ssa_instr){
    //find base address of array
    std::string original_name;

    original_name = virt_var.original_name;

    std::tuple<std::string, std::string> key_tuple = std::make_tuple(original_name, "");
    uint32_t reg_original_name = virtual_variable_to_v_reg[key_tuple];
    //get array_information, skip all empty dimensions
    array_information a_info = boost::get<array_information>(virt_var.data_information);

    //get original array and its array information to correctly compute the offset within array
    auto it_original_arr = std::find_if(virtual_variables.begin(), virtual_variables.end(), [original_name](const virtual_variable_information& t){return (t.ssa_name == original_name) && (t.index_to_original == -1);});
    array_information a_info_original = boost::get<array_information>(it_original_arr->data_information);

    bool first_non_empty_array_dimension = true;
    bool tmp_mult_destination_register_set = false;
    uint32_t incremental_offset_destination_register = UINT32_MAX;
    uint32_t tmp_mult_destination_register = UINT32_MAX; //incremental_offset_destination_register += i * 8 -> i*8 needs seperate register
    //go over all dimensions (dimidx)
    for(uint32_t dim_idx = 0; dim_idx < a_info.dimension; ++dim_idx){ //compute the offset of all array dimensions
        //skip empty dimension
        if(a_info.depth_per_dimension.at(dim_idx).op_type != DATA_TYPE::NONE){ 
            TAC_type tmp_tac;
            Operation op = Operation::MUL;
            uint8_t sens; 
            TAC_OPERAND dest_type; 
            uint32_t dest_v;
            uint8_t dest_sens; 
            TAC_OPERAND src1_type; 
            uint32_t src1_v;
            uint8_t src1_sens; 
            TAC_OPERAND src2_type; 
            uint32_t src2_v;
            uint8_t src2_sens;
            bool psr_status_flag;

            src2_sens = 0;
            src2_type = TAC_OPERAND::NUM;
            src2_v = 0;
            if(dim_idx != (a_info.dimension - 1)){
                src2_v = virt_var.data_type_size;// 4;
                //build offset incrementally by adding all dimension to generate correct offset
                for(uint32_t lower_dim_idx = dim_idx + 1; lower_dim_idx < a_info.dimension; ++lower_dim_idx){
                    src2_v *= a_info_original.depth_per_dimension.at(lower_dim_idx).op_data;
                }
            }
            else{
                src2_v = virt_var.data_type_size;
            }
            

            if(std::floor(std::log2(src2_v)) == std::log2(src2_v)){
                op = Operation::LSL;
                src2_v = std::log2(src2_v);
            }
            else{
                //mul operation can only handle registers as operands -> have to move value to register before
                TAC_type mov_tac;
                Operation mov_op = Operation::MOV;
                uint8_t mov_sens = 0; 
                TAC_OPERAND mov_dest_type = TAC_OPERAND::V_REG; 
                uint32_t mov_dest_v = v_register_ctr++;
                uint8_t mov_dest_sens = 0; 
                TAC_OPERAND mov_src1_type = TAC_OPERAND::NUM; 
                uint32_t mov_src1_v = src2_v;
                uint8_t mov_src1_sens = 0; 
                TAC_OPERAND mov_src2_type = TAC_OPERAND::NONE; 
                uint32_t mov_src2_v = UINT32_MAX;
                uint8_t mov_src2_sens = 0;
                // bool mov_psr_status_flag = false;

                #ifdef ARMV6_M
                move_specification_armv6m(mov_op, mov_src1_v, psr_status_flag);
                #endif

                generate_generic_tmp_tac(mov_tac, mov_op, mov_sens, mov_dest_type, mov_dest_v, mov_dest_sens, mov_src1_type, mov_src1_v, mov_src1_sens, mov_src2_type, mov_src2_v, mov_src2_sens, psr_status_flag);
                
                
                TAC.push_back(mov_tac);

                src2_v = mov_dest_v;
                src2_type = TAC_OPERAND::V_REG;
            }

            //build offset incrementally by adding all dimension to generate correct offset

            //if dimension element is number -> offset += op_data * 2^(dimension - dimidx + 1) -> register will definitely be public
            if(a_info.depth_per_dimension.at(dim_idx).op_type == DATA_TYPE::NUM){
                src1_sens = 0;
                src1_type = TAC_OPERAND::NUM;
                src1_v = a_info.depth_per_dimension.at(dim_idx).op_data;
                if(first_non_empty_array_dimension){
                    //use destination of mul operation as we have to generate a new virtual register
                    dest_sens = 0;
                    dest_type = TAC_OPERAND::V_REG;
                    dest_v = v_register_ctr++;
                    incremental_offset_destination_register = dest_v;

                    //lsls operation is only valid if first source is regsiter. In this case src1 is a number -> instead of lsls calculate offset and write to register
                    if(op == Operation::LSL){
                        src1_v = (src1_v << src2_v);
                        src2_v = UINT32_MAX;
                        src2_type = TAC_OPERAND::NONE;
                        src2_sens = 0; 

                        #ifdef ARMV6_M
                        move_specification_armv6m(op, src1_v, psr_status_flag);
                        #endif

                        if((src1_sens == 1) || (src2_sens == 1)){
                            dest_sens = 1;
                            sens = 1;
                        }
                        else{
                            dest_sens = 0;
                            sens = 0;
                        }   

                        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);

                        TAC.push_back(tmp_tac);
                    }
                    else{
                        psr_status_flag = true;

                        if((src1_sens == 1) || (src2_sens == 1)){
                            dest_sens = 1;
                            sens = 1;
                        }
                        else{
                            dest_sens = 0;
                            sens = 0;
                        }   

                        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                        TAC.push_back(tmp_tac);
                    }
                }
                else{
                    
                    if(!tmp_mult_destination_register_set){
                        tmp_mult_destination_register_set = true;
                        tmp_mult_destination_register = v_register_ctr++;
                    }

                    if((src1_sens == 1) || (src2_sens == 1)){
                        dest_sens = 1;
                        sens = 1;
                    }
                    else{
                        dest_sens = 0;
                        sens = 0;
                    }                    
                    dest_type = TAC_OPERAND::V_REG;
                    dest_v = tmp_mult_destination_register;
            
                    //lsls operation is only valid if first source is regsiter. In this case src1 is a number -> instead of lsls calculate offset and write to register
                    if(op == Operation::LSL){
                        op = Operation::MOV;

                        src1_v = (src1_v << src2_v);
                        src2_v = UINT32_MAX;
                        src2_type = TAC_OPERAND::NONE;
                        src2_sens = 0; 

                        #ifdef ARMV6_M
                        move_specification_armv6m(op, src1_v, psr_status_flag);
                        #endif

                        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);

                        TAC.push_back(tmp_tac);
                    }
                    else{
                        psr_status_flag = true;
                        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                        TAC.push_back(tmp_tac);
                    }

                    //add operation, add to current offset
                    TAC_type tmp_tac_add_offset;
                    Operation op_add_offset = Operation::ADD;

                    TAC_OPERAND src1_type_add_offset = TAC_OPERAND::V_REG; 
                    uint32_t src1_v_add_offset = incremental_offset_destination_register;
                    uint8_t src1_sens_add_offset = return_sensitivity((TAC.rbegin() + 1)->dest_sensitivity); 

                    TAC_OPERAND src2_type_add_offset = TAC_OPERAND::V_REG; 
                    uint32_t src2_v_add_offset = tmp_mult_destination_register;
                    uint8_t src2_sens_add_offset = return_sensitivity(TAC.back().dest_sensitivity);

                    TAC_OPERAND dest_type_add_offset = TAC_OPERAND::V_REG; 
                    uint32_t dest_v_add_offset = incremental_offset_destination_register;
                    uint8_t dest_sens_add_offset = src1_sens_add_offset | src2_sens_add_offset; 

                    uint8_t sens_add_offset = dest_sens_add_offset; 

                    generate_generic_tmp_tac(tmp_tac_add_offset, op_add_offset, sens_add_offset, dest_type_add_offset, dest_v_add_offset, dest_sens_add_offset, src1_type_add_offset, src1_v_add_offset, src1_sens_add_offset, src2_type_add_offset, src2_v_add_offset, src2_sens_add_offset, false);
                    TAC.push_back(tmp_tac_add_offset);
                }
            }
            //if dimension element not number -> get virtual variable -> offset += var * 2^(dimension - dimidx + 1)
            else{

                src1_type = TAC_OPERAND::V_REG;
                std::string src1_original_name = generate_original_name(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data), virtual_variables);
                std::string src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data));
                if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                    key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                    src1_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                    src1_sens = return_sensitivity(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data).sensitivity);
                }
                else{
                    throw std::runtime_error("in get_virtual_address: variable that is used for index access of array is not in register yet");
                }

                if(first_non_empty_array_dimension){
                    //use destination of mul operation as we have to generate a new virtual register
                    //dest_sens = 0;
                    dest_type = TAC_OPERAND::V_REG;
                    dest_v = v_register_ctr++;
                    incremental_offset_destination_register = dest_v;

                    if((src1_sens == 1) || (src2_sens == 1)){
                        dest_sens = 1;
                        sens = 1;
                    }
                    else{
                        dest_sens = 0;
                        sens = 0;
                    }
                    //tmp_mult_destination_register = v_register_ctr++;
                    psr_status_flag = true;
                    generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                    TAC.push_back(tmp_tac);
                }
                else{

                    if(!tmp_mult_destination_register_set){
                        tmp_mult_destination_register_set = true;
                        tmp_mult_destination_register = v_register_ctr++;
                    }

                    if((src1_sens == 1) || (src2_sens == 1)){
                        dest_sens = 1;
                        sens = 1;
                    }
                    else{
                        dest_sens = 0;
                        sens = 0;
                    }
                    dest_type = TAC_OPERAND::V_REG;
                    dest_v = tmp_mult_destination_register;
                    psr_status_flag = true;
                    generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                    TAC.push_back(tmp_tac);

                    //add operation, add to current offset
                    TAC_type tmp_tac_add_offset;
                    Operation op_add_offset = Operation::ADD;

                    TAC_OPERAND src1_type_add_offset = TAC_OPERAND::V_REG; 
                    uint32_t src1_v_add_offset = incremental_offset_destination_register;
                    uint8_t src1_sens_add_offset = return_sensitivity((TAC.rbegin() + 1)->dest_sensitivity); 

                    TAC_OPERAND src2_type_add_offset = TAC_OPERAND::V_REG; 
                    uint32_t src2_v_add_offset = tmp_mult_destination_register;
                    uint8_t src2_sens_add_offset = return_sensitivity(TAC.back().dest_sensitivity);

                    TAC_OPERAND dest_type_add_offset = TAC_OPERAND::V_REG; 
                    uint32_t dest_v_add_offset = incremental_offset_destination_register;
                    uint8_t dest_sens_add_offset = src1_sens_add_offset | src2_sens_add_offset; 

                    uint8_t sens_add_offset = dest_sens_add_offset; 

                    generate_generic_tmp_tac(tmp_tac_add_offset, op_add_offset, sens_add_offset, dest_type_add_offset, dest_v_add_offset, dest_sens_add_offset, src1_type_add_offset, src1_v_add_offset, src1_sens_add_offset, src2_type_add_offset, src2_v_add_offset, src2_sens_add_offset, false);
                    TAC.push_back(tmp_tac_add_offset);

                }
            }

            first_non_empty_array_dimension = false;
        }

    }


    if(is_load){ //load operations have additional, seperate offset which can be added 
        mem_load_instruction m_instr = boost::get<mem_load_instruction>(ssa_instr.instr);
        if(m_instr.offset.op_type != DATA_TYPE::NUM){
            throw std::runtime_error("in get_irtual_address: offset for load operation is not a number. can not handle case yet");
        }

        if(m_instr.offset.op_data != 0){
            TAC_type tmp_tac;
            Operation op = Operation::ADD;
            uint8_t sens = return_sensitivity(TAC.back().dest_sensitivity); 
            TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
            uint32_t dest_v = TAC.back().dest_v_reg_idx;
            uint8_t dest_sens = return_sensitivity(TAC.back().dest_sensitivity); 
            TAC_OPERAND src1_type = TAC_OPERAND::V_REG; 
            uint32_t src1_v = TAC.back().dest_v_reg_idx;
            uint8_t src1_sens = return_sensitivity(TAC.back().dest_sensitivity); 
            TAC_OPERAND src2_type = TAC_OPERAND::NUM; 
            uint32_t src2_v = m_instr.offset.op_data;
            uint8_t src2_sens = 0;
            bool add_psr_status_flag;

            TAC_type load_number_into_register;
            if(add_specification_armv6m(src2_v, add_psr_status_flag, load_number_into_register)){
                src2_type = TAC_OPERAND::V_REG;
                src2_v = load_number_into_register.dest_v_reg_idx;

                TAC.push_back(load_number_into_register);
            }

            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, add_psr_status_flag);
            TAC.push_back(tmp_tac);
        }


    }

    //final offset = _19 (has to be in register) + address (e.g. i * 8 + j * 4)
    TAC_type tmp_tac;
    Operation op = Operation::ADD;
    uint8_t sens = 0; 
    TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
    uint32_t dest_v = v_register_ctr++;
    uint8_t dest_sens = 0; 
    TAC_OPERAND src1_type = TAC_OPERAND::V_REG; 
    uint32_t src1_v = reg_original_name;
    uint8_t src1_sens = 0; 
    TAC_OPERAND src2_type = TAC_OPERAND::V_REG; 
    uint32_t src2_v = TAC.back().dest_v_reg_idx;
    uint8_t src2_sens = return_sensitivity(TAC.back().dest_sensitivity);

    generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
    TAC.push_back(tmp_tac);


    //save in virt_var_to_v_reg the mapping between the register and the address of this variable
    std::string original_key = "";
    std::string ssa_key = "";
    //if we store to *_19 or *_19[i][j] -> address is _19 or _19[i][j] (currently we only handle ASTERISK_ARR_ADD_ON)
    if((original_ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON) || (original_ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON)){
        original_key = ssa_original_name;
        if(original_ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON){ //_19 has no ssa name, while _19[i][j] can have ssa_name e.g. _19[i_44][j_55]
            ssa_key = ssa_name;
        }
    }
    //if we store to _19[i][j] -> address is &_19[i][j]
    if(original_ssa_add_on == SSA_ADD_ON::ARR_ADD_ON){
        original_key = "&" + ssa_original_name;
        ssa_key = "&" + ssa_name;
        
    }
    
    key_tuple = std::make_tuple(original_key, ssa_key);
    virtual_variable_to_v_reg[key_tuple] = TAC.back().dest_v_reg_idx;

    calling_operation_src_v = TAC.back().dest_v_reg_idx;
    calling_operation_sens = return_sensitivity(TAC.back().dest_sensitivity);
}



void get_input_address(std::vector<TAC_type>& TAC, uint32_t& calling_operation_src_v, uint8_t& calling_operation_sens, virtual_variables_vec& virtual_variables, virtual_variable_information& virt_var, std::string ssa_original_name, std::string ssa_name, SSA_ADD_ON ssa_add_on, SSA_ADD_ON original_ssa_add_on, memory_security_struct& memory_security, bool is_load, ssa& ssa_instr){
    if((ssa_add_on == SSA_ADD_ON::ARR_ADD_ON)){
        //find base address of array
        std::string original_name = "";

        original_name = virt_var.original_name;
        auto it = std::find_if(memory_security.input_parameter_mapping_tracker.begin(), memory_security.input_parameter_mapping_tracker.end(), [original_name](input_parameter_stack_variable_tracking_struct t){return t.base_value == original_name;});
        if(it == memory_security.input_parameter_mapping_tracker.end()){
            throw std::runtime_error("in get_input_address: can not find element on stack");
        }
        //get array_information, skip all empty dimensions
        array_information a_info = boost::get<array_information>(virt_var.data_information);

        //get original array and its array information to correctly compute the offset within array
        auto it_original_arr = std::find_if(virtual_variables.begin(), virtual_variables.end(), [original_name](const virtual_variable_information& t){return (t.ssa_name == original_name) && (t.index_to_original == -1);});
        array_information a_info_original = boost::get<array_information>(it_original_arr->data_information);

        bool first_non_empty_array_dimension = true;
        bool tmp_mult_destination_register_set = false;
        uint32_t incremental_offset_destination_register = UINT32_MAX;
        uint32_t tmp_mult_destination_register = UINT32_MAX; //incremental_offset_destination_register += i * 8 -> i*8 needs seperate register
        //go over all dimensions (dimidx)
        for(uint32_t dim_idx = 0; dim_idx < a_info.dimension; ++dim_idx){
            //skip empty dimension
            if(a_info.depth_per_dimension.at(dim_idx).op_type != DATA_TYPE::NONE){
                TAC_type tmp_tac;
                Operation op = Operation::MUL;
                uint8_t sens; 
                TAC_OPERAND dest_type; 
                uint32_t dest_v;
                uint8_t dest_sens; 
                TAC_OPERAND src1_type; 
                uint32_t src1_v;
                uint8_t src1_sens; 
                TAC_OPERAND src2_type; 
                uint32_t src2_v;
                uint8_t src2_sens;
                bool psr_status_flag;

                src2_sens = 0;
                src2_type = TAC_OPERAND::NUM;
                src2_v = 0;
                if(dim_idx != (a_info.dimension - 1)){
                    src2_v = 4;
                    //build offset incrementally by adding all dimension to generate correct offset
                    for(uint32_t lower_dim_idx = dim_idx + 1; lower_dim_idx < a_info.dimension; ++lower_dim_idx){
                        src2_v *= a_info_original.depth_per_dimension.at(lower_dim_idx).op_data;
                    }
                }
                else{
                    src2_v = 4;
                }
                

                if(std::floor(std::log2(src2_v)) == std::log2(src2_v)){
                    op = Operation::LSL;
                    src2_v = std::log2(src2_v);
                }
                else{
                    //mul operation can only handle registers as operands -> have to move value to register before
                    TAC_type mov_tac;
                    Operation mov_op = Operation::MOV;
                    uint8_t mov_sens = 0; 
                    TAC_OPERAND mov_dest_type = TAC_OPERAND::V_REG; 
                    uint32_t mov_dest_v = v_register_ctr++;
                    uint8_t mov_dest_sens = 0; 
                    TAC_OPERAND mov_src1_type = TAC_OPERAND::NUM; 
                    uint32_t mov_src1_v = src2_v;
                    uint8_t mov_src1_sens = 0; 
                    TAC_OPERAND mov_src2_type = TAC_OPERAND::NONE; 
                    uint32_t mov_src2_v = UINT32_MAX;
                    uint8_t mov_src2_sens = 0;

                    #ifdef ARMV6_M
                    move_specification_armv6m(mov_op, mov_src1_v, psr_status_flag);
                    #endif

                    generate_generic_tmp_tac(mov_tac, mov_op, mov_sens, mov_dest_type, mov_dest_v, mov_dest_sens, mov_src1_type, mov_src1_v, mov_src1_sens, mov_src2_type, mov_src2_v, mov_src2_sens, psr_status_flag);

                    TAC.push_back(mov_tac);

                    src2_v = mov_dest_v;
                    src2_type = TAC_OPERAND::V_REG;
                }

                


                //if dimension element is number -> offset += op_data * 2^(dimension - dimidx + 1) -> register will definitely be public
                if(a_info.depth_per_dimension.at(dim_idx).op_type == DATA_TYPE::NUM){
                    src1_sens = 0;
                    src1_type = TAC_OPERAND::NUM;
                    src1_v = a_info.depth_per_dimension.at(dim_idx).op_data;
                    if(first_non_empty_array_dimension){
                        //use destination of mul operation as we have to generate a new virtual register
                        dest_sens = 0; // destination sensitivity is zero because both sources are fix numbers
                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = v_register_ctr++;
                        incremental_offset_destination_register = dest_v;
                        sens = 0;


                        //lsls operation is only valid if first source is regsiter. In this case src1 is a number -> instead of lsls calculate offset and write to register
                        if(op == Operation::LSL){
                            op = Operation::MOV;

                            src1_v = (src1_v << src2_v);
                            src2_v = UINT32_MAX;
                            src2_type = TAC_OPERAND::NONE;
                            src2_sens = 0; 

                            #ifdef ARMV6_M
                            move_specification_armv6m(op, src1_v, psr_status_flag);
                            #endif

                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);

                            TAC.push_back(tmp_tac);
                        }
                        else{
                            psr_status_flag = true;
                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                            TAC.push_back(tmp_tac);
                        }
                    }
                    else{
                        
                        if(!tmp_mult_destination_register_set){
                            tmp_mult_destination_register_set = true;
                            tmp_mult_destination_register = v_register_ctr++;
                        }

                        if((src1_sens == 1) || (src2_sens == 1)){
                            dest_sens = 1;
                            sens = 1;
                        }
                        else{
                            dest_sens = 0;
                            sens = 0;
                        }
                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = tmp_mult_destination_register;

                        //lsls operation is only valid if first source is regsiter. In this case src1 is a number -> instead of lsls calculate offset and write to register
                        if(op == Operation::LSL){
                            op = Operation::MOV;

                            src1_v = (src1_v << src2_v);
                            src2_v = UINT32_MAX;
                            src2_type = TAC_OPERAND::NONE;
                            src2_sens = 0; 
                            #ifdef ARMV6_M
                            move_specification_armv6m(op, src1_v, psr_status_flag);
                            #endif

                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);

                            TAC.push_back(tmp_tac);
                        }
                        else{
                            psr_status_flag = true;
                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                            TAC.push_back(tmp_tac);
                        }


                        //add operation, add to current offset
                        TAC_type tmp_tac_add_offset;
                        Operation op_add_offset = Operation::ADD;

                        TAC_OPERAND src1_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src1_v_add_offset = incremental_offset_destination_register;
                        uint8_t src1_sens_add_offset = return_sensitivity((TAC.rbegin() + 1)->dest_sensitivity); 

                        TAC_OPERAND src2_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src2_v_add_offset = tmp_mult_destination_register;
                        uint8_t src2_sens_add_offset = return_sensitivity(TAC.back().dest_sensitivity);

                        TAC_OPERAND dest_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t dest_v_add_offset = incremental_offset_destination_register;
                        uint8_t dest_sens_add_offset = src1_sens_add_offset | src2_sens_add_offset; 

                        uint8_t sens_add_offset = dest_sens_add_offset; 

                        generate_generic_tmp_tac(tmp_tac_add_offset, op_add_offset, sens_add_offset, dest_type_add_offset, dest_v_add_offset, dest_sens_add_offset, src1_type_add_offset, src1_v_add_offset, src1_sens_add_offset, src2_type_add_offset, src2_v_add_offset, src2_sens_add_offset, false);
                        TAC.push_back(tmp_tac_add_offset);
                    }
                }
                //if dimension element not number -> get virtual variable -> offset += var * 2^(dimension - dimidx + 1)
                else{

                    src1_type = TAC_OPERAND::V_REG;
                    std::string src1_original_name = generate_original_name(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data), virtual_variables);
                    std::string src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data));
                    if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                        src1_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                        src1_sens = return_sensitivity(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data).sensitivity);
                    }
                    else{
                        throw std::runtime_error("in load_input_from_memory: variable that is used for index access of array is not in register yet");
                    }

                    if(first_non_empty_array_dimension){
                        //use destination of mul operation as we have to generate a new virtual register
                        //dest_sens = 0;
                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = v_register_ctr++;
                        incremental_offset_destination_register = dest_v;
                        if((src1_sens == 1) || (src2_sens == 1)){
                            dest_sens = 1;
                            sens = 1;
                        }
                        else{
                            dest_sens = 0;
                            sens = 0;
                        }
                        psr_status_flag = true;
                        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                        TAC.push_back(tmp_tac);
                    }
                    else{
                        if(!tmp_mult_destination_register_set){
                            tmp_mult_destination_register_set = true;
                            tmp_mult_destination_register = v_register_ctr++;
                        }


                        if((src1_sens == 1) || (src2_sens == 1)){
                            dest_sens = 1;
                            sens = 1;
                        }
                        else{
                            dest_sens = 0;
                            sens = 0;
                        }
                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = tmp_mult_destination_register;
                        psr_status_flag = true;
                        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                        TAC.push_back(tmp_tac);

                        //add operation, add to current offset
                        TAC_type tmp_tac_add_offset;
                        Operation op_add_offset = Operation::ADD;

                        TAC_OPERAND src1_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src1_v_add_offset = incremental_offset_destination_register;
                        uint8_t src1_sens_add_offset = return_sensitivity((TAC.rbegin() + 1)->dest_sensitivity); 

                        TAC_OPERAND src2_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src2_v_add_offset = tmp_mult_destination_register;
                        uint8_t src2_sens_add_offset = return_sensitivity(TAC.back().dest_sensitivity);

                        TAC_OPERAND dest_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t dest_v_add_offset = incremental_offset_destination_register;
                        uint8_t dest_sens_add_offset = src1_sens_add_offset | src2_sens_add_offset; 

                        uint8_t sens_add_offset = dest_sens_add_offset; 

                        generate_generic_tmp_tac(tmp_tac_add_offset, op_add_offset, sens_add_offset, dest_type_add_offset, dest_v_add_offset, dest_sens_add_offset, src1_type_add_offset, src1_v_add_offset, src1_sens_add_offset, src2_type_add_offset, src2_v_add_offset, src2_sens_add_offset, false);
                        TAC.push_back(tmp_tac_add_offset);

                    }
                }

                first_non_empty_array_dimension = false;
            }

        }

        if(is_load){ //load operations have additional, seperate offset which can be added 
            mem_load_instruction m_instr = boost::get<mem_load_instruction>(ssa_instr.instr);
            if(m_instr.offset.op_type != DATA_TYPE::NUM){
                throw std::runtime_error("in get_input_address: offset for load operation is not a number. can not handle case yet");
            }

            if(m_instr.offset.op_data != 0){
                TAC_type tmp_tac;
                Operation op = Operation::ADD;
                uint8_t sens = return_sensitivity(TAC.back().dest_sensitivity); 
                TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
                uint32_t dest_v = TAC.back().dest_v_reg_idx;
                uint8_t dest_sens = return_sensitivity(TAC.back().dest_sensitivity); 
                TAC_OPERAND src1_type = TAC_OPERAND::V_REG; 
                uint32_t src1_v = TAC.back().dest_v_reg_idx;;
                uint8_t src1_sens = return_sensitivity(TAC.back().dest_sensitivity); 
                TAC_OPERAND src2_type = TAC_OPERAND::NUM; 
                uint32_t src2_v = m_instr.offset.op_data;
                uint8_t src2_sens = 0;
                bool add_psr_status_flag;

                TAC_type load_number_into_register;
                if(add_specification_armv6m(src2_v, add_psr_status_flag, load_number_into_register)){
                    src2_type = TAC_OPERAND::V_REG;
                    src2_v = load_number_into_register.dest_v_reg_idx;

                    TAC.push_back(load_number_into_register);
                }

                generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, add_psr_status_flag);
                TAC.push_back(tmp_tac);
            }


        }

        //final offset = fp + address (e.g. &r[i][j])

        TAC_type tmp_tac;
        Operation op = Operation::MOV;
        uint8_t sens = 0; 
        TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
        uint32_t dest_v = v_register_ctr++;
        uint8_t dest_sens = 0; 
        TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
        uint32_t src1_v = 11;
        uint8_t src1_sens = 0; 
        TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
        uint32_t src2_v = UINT32_MAX;
        uint8_t src2_sens = 0;
        
        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        TAC.push_back(tmp_tac);

        op = Operation::ADD_INPUT_STACK;
        sens = TAC.back().dest_sensitivity; 
        dest_type = TAC_OPERAND::V_REG; 
        dest_v = TAC.back().dest_v_reg_idx;
        dest_sens = TAC.back().dest_sensitivity; 
        src1_type = TAC_OPERAND::V_REG; 
        src1_v = TAC.back().dest_v_reg_idx;
        src1_sens = TAC.back().dest_sensitivity; 
        src2_type = TAC_OPERAND::NUM; 
        src2_v = it->position;
        src2_sens = 0;
        bool add_input_stack_psr_status_flag;

        TAC_type load_number_into_register;
        if(add_specification_armv6m(src2_v, add_input_stack_psr_status_flag, load_number_into_register)){
            src2_type = TAC_OPERAND::V_REG;
            src2_v = load_number_into_register.dest_v_reg_idx;

            TAC.push_back(load_number_into_register);
        }
        
        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, add_input_stack_psr_status_flag);
        TAC.push_back(tmp_tac);

        #ifdef ARMV6_M

        op = Operation::ADD;
        dest_type = TAC_OPERAND::V_REG; 
        dest_v = TAC.back().dest_v_reg_idx;
        src1_type = TAC_OPERAND::V_REG; 
        src1_v = TAC.back().dest_v_reg_idx;
        src1_sens = TAC.back().dest_sensitivity; 
        src2_type = TAC_OPERAND::V_REG; 
        src2_v = (*(TAC.rbegin() + 2)).dest_v_reg_idx;
        src2_sens = return_sensitivity((*(TAC.rbegin() + 2)).dest_sensitivity);
        if((src1_sens == 1) || (src2_sens == 1)){
            dest_sens = 1;
            sens = 1;
        }
        else{
            dest_sens = 0;
            sens = 0;
        }

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        TAC.push_back(tmp_tac);
        #endif

    }
    else{
        //find address of element (use original name)
        auto it = std::find_if(memory_security.input_parameter_mapping_tracker.begin(), memory_security.input_parameter_mapping_tracker.end(), [ssa_original_name](input_parameter_stack_variable_tracking_struct& t){return t.base_value == ssa_original_name;});
        
        //offset = fp + address

        #ifdef ARMV6_M
        TAC_type tmp_tac;
        Operation op = Operation::MOV;
        uint8_t sens = 0; 
        TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
        uint32_t dest_v = v_register_ctr++;
        uint8_t dest_sens = 0; 
        TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
        uint32_t src1_v = 11;
        uint8_t src1_sens = 0; 
        TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
        uint32_t src2_v = UINT32_MAX;
        uint8_t src2_sens = 0;
        
        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        TAC.push_back(tmp_tac);

        op = Operation::ADD_INPUT_STACK;
        sens = TAC.back().dest_sensitivity; 
        dest_type = TAC_OPERAND::V_REG; 
        dest_v = TAC.back().dest_v_reg_idx;
        dest_sens = it->sensitivity; 
        src1_type = TAC_OPERAND::V_REG; 
        src1_v = TAC.back().dest_v_reg_idx;
        src1_sens = TAC.back().dest_sensitivity; 
        src2_type = TAC_OPERAND::NUM; 
        src2_v = it->position;
        src2_sens = 0;
        bool add_input_stack_psr_status_flag;
        
        if(is_load){ //load operations have additional, seperate offset which can be added 
            mem_load_instruction m_instr = boost::get<mem_load_instruction>(ssa_instr.instr);
            if(m_instr.offset.op_type != DATA_TYPE::NUM){
                throw std::runtime_error("in get_input_address: offset for load operation is not a number. can not handle case yet");
            }
            src2_v += m_instr.offset.op_data;
        }

        TAC_type load_number_into_register;
        if(add_specification_armv6m(src2_v, add_input_stack_psr_status_flag, load_number_into_register)){
            src2_type = TAC_OPERAND::V_REG;
            src2_v = load_number_into_register.dest_v_reg_idx;

            TAC.push_back(load_number_into_register);
        }

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, add_input_stack_psr_status_flag);
        TAC.push_back(tmp_tac);

        #endif
    }

    TAC_type tmp_tac;
    Operation op = Operation::LOADWORD;
    uint8_t sens = TAC.back().dest_sensitivity;
    TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
    uint32_t dest_v = v_register_ctr++;
    uint8_t dest_sens = TAC.back().dest_sensitivity;
    TAC_OPERAND src1_type = TAC_OPERAND::V_REG; 
    uint32_t src1_v = TAC.back().dest_v_reg_idx;
    uint8_t src1_sens = TAC.back().dest_sensitivity; 
    TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
    uint32_t src2_v = UINT32_MAX;
    uint8_t src2_sens = 0;

    generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
    TAC.push_back(tmp_tac);

    //save in virt_var_to_v_reg the mapping between the register and the address of this variable
    std::string original_key = "";
    std::string ssa_key = "";

    //if we store to *r or *r[i][j] -> we store at address r or r[i][j]
    if((original_ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON) || (original_ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON)){
        original_key = ssa_original_name;
        if(original_ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON){
            ssa_key = ssa_name;
        }
    }
    //if we store to r[i][j] -> we store at address &r[i][j]
    if(original_ssa_add_on == SSA_ADD_ON::ARR_ADD_ON){
        original_key = "&" + ssa_original_name;
        ssa_key = "&" + ssa_name;
    }


    if(original_ssa_add_on == SSA_ADD_ON::AMP_ADD_ON){
        original_key = "&" + ssa_original_name;
        ssa_key = "";
    }

    if(original_ssa_add_on == SSA_ADD_ON::AMP_ARR_ADD_ON){
        original_key = "&" + ssa_original_name;
        ssa_key = "&" + ssa_name;
    }

    std::tuple<std::string, std::string> key_tuple = std::make_tuple(original_key, ssa_key);
    virtual_variable_to_v_reg[key_tuple] = TAC.back().dest_v_reg_idx;

    calling_operation_src_v = TAC.back().dest_v_reg_idx;
    calling_operation_sens = return_sensitivity(TAC.back().dest_sensitivity);
}




void get_local_address(std::vector<TAC_type>& TAC, uint32_t& calling_operation_src_v, uint8_t& calling_operation_sens, virtual_variables_vec& virtual_variables, virtual_variable_information& virt_var, std::string ssa_original_name, std::string ssa_name, SSA_ADD_ON ssa_add_on, SSA_ADD_ON original_ssa_add_on, memory_security_struct& memory_security, bool is_load, ssa& ssa_instr){
    if((ssa_add_on == SSA_ADD_ON::ARR_ADD_ON)){
        //find base address of array
        std::string original_name = "";

        original_name = virt_var.original_name;
        auto it = std::find_if(memory_security.stack_mapping_tracker.begin(), memory_security.stack_mapping_tracker.end(), [original_name](stack_local_variable_tracking_struct t){return t.base_value == original_name;});
        if(it == memory_security.stack_mapping_tracker.end()){
            throw std::runtime_error("in get_local_address: can not find element on stack");
        }
        //get array_information, skip all empty dimensions
        array_information a_info = boost::get<array_information>(virt_var.data_information);

        //get original array and its array information to correctly compute the offset within array
        auto it_original_arr = std::find_if(virtual_variables.begin(), virtual_variables.end(), [original_name](const virtual_variable_information& t){return (t.ssa_name == original_name) && (t.index_to_original == -1);});
        array_information a_info_original = boost::get<array_information>(it_original_arr->data_information);

        bool first_non_empty_array_dimension = true;
        bool tmp_mult_destination_register_set = false;
        uint32_t incremental_offset_destination_register = UINT32_MAX;
        uint32_t tmp_mult_destination_register = UINT32_MAX; //incremental_offset_destination_register += i * 8 -> i*8 needs seperate register
        //go over all dimensions (dimidx)
        for(uint32_t dim_idx = 0; dim_idx < a_info.dimension; ++dim_idx){
            //skip empty dimension
            if(a_info.depth_per_dimension.at(dim_idx).op_type != DATA_TYPE::NONE){
                TAC_type tmp_tac;
                Operation op = Operation::MUL;
                uint8_t sens; 
                TAC_OPERAND dest_type; 
                uint32_t dest_v;
                uint8_t dest_sens; 
                TAC_OPERAND src1_type; 
                uint32_t src1_v;
                uint8_t src1_sens; 
                TAC_OPERAND src2_type; 
                uint32_t src2_v;
                uint8_t src2_sens;
                bool psr_status_flag;

                src2_sens = 0;
                src2_type = TAC_OPERAND::NUM;
                src2_v = 0;
                if(dim_idx != (a_info.dimension - 1)){
                    src2_v = virt_var.data_type_size;
                    //build offset incrementally by adding all dimension to generate correct offset
                    for(uint32_t lower_dim_idx = dim_idx + 1; lower_dim_idx < a_info.dimension; ++lower_dim_idx){
                        src2_v *= a_info_original.depth_per_dimension.at(lower_dim_idx).op_data;
                    }
                }
                else{
                    src2_v = virt_var.data_type_size;
                }
                

                if(std::floor(std::log2(src2_v)) == std::log2(src2_v)){
                    op = Operation::LSL;
                    src2_v = std::log2(src2_v);
                }
                else{
                    //mul operation can only handle registers as operands -> have to move value to register before
                    TAC_type mov_tac;
                    Operation mov_op = Operation::MOV;
                    uint8_t mov_sens = 0; 
                    TAC_OPERAND mov_dest_type = TAC_OPERAND::V_REG; 
                    uint32_t mov_dest_v = v_register_ctr++;
                    uint8_t mov_dest_sens = 0; 
                    TAC_OPERAND mov_src1_type = TAC_OPERAND::NUM; 
                    uint32_t mov_src1_v = src2_v;
                    uint8_t mov_src1_sens = 0; 
                    TAC_OPERAND mov_src2_type = TAC_OPERAND::NONE; 
                    uint32_t mov_src2_v = UINT32_MAX;
                    uint8_t mov_src2_sens = 0;

                    #ifdef ARMV6_M
                    move_specification_armv6m(mov_op, mov_src1_v, psr_status_flag);
                    #endif

                    generate_generic_tmp_tac(mov_tac, mov_op, mov_sens, mov_dest_type, mov_dest_v, mov_dest_sens, mov_src1_type, mov_src1_v, mov_src1_sens, mov_src2_type, mov_src2_v, mov_src2_sens, psr_status_flag);

                    TAC.push_back(mov_tac);

                    src2_v = mov_dest_v;
                    src2_type = TAC_OPERAND::V_REG;
                }

                


                //if dimension element is number -> offset += op_data * 2^(dimension - dimidx + 1) -> register will definitely be public
                if(a_info.depth_per_dimension.at(dim_idx).op_type == DATA_TYPE::NUM){
                    src1_sens = 0;
                    src1_type = TAC_OPERAND::NUM;
                    src1_v = a_info.depth_per_dimension.at(dim_idx).op_data;
                    if(first_non_empty_array_dimension){
                        //use destination of mul operation as we have to generate a new virtual register
                        dest_sens = 0; // destination sensitivity is zero because both sources are fix numbers
                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = v_register_ctr++;
                        incremental_offset_destination_register = dest_v;
                        //tmp_mult_destination_register = v_register_ctr++;
                        sens = 0;

                        //lsls operation is only valid if first source is regsiter. In this case src1 is a number -> instead of lsls calculate offset and write to register
                        if(op == Operation::LSL){
                            op = Operation::MOV;

                            src1_v = (src1_v << src2_v);
                            src2_v = UINT32_MAX;
                            src2_type = TAC_OPERAND::NONE;
                            src2_sens = 0; 

                            #ifdef ARMV6_M
                            move_specification_armv6m(op, src1_v, psr_status_flag);
                            #endif
                            
                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);

                            TAC.push_back(tmp_tac);
                        }
                        else{
                            psr_status_flag= true;
                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                            TAC.push_back(tmp_tac);
                        }

                    }
                    else{
                        
                        if(!tmp_mult_destination_register_set){
                            tmp_mult_destination_register_set = true;
                            tmp_mult_destination_register = v_register_ctr++;
                        }

                        if((src1_sens == 1) || (src2_sens == 1)){
                            dest_sens = 1;
                            sens = 1;
                        }
                        else{
                            dest_sens = 0;
                            sens = 0;
                        }
                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = tmp_mult_destination_register;
                        

                        if(op == Operation::LSL){
                            op = Operation::MOV;

                            src1_v = (src1_v << src2_v);
                            src2_v = UINT32_MAX;
                            src2_type = TAC_OPERAND::NONE;
                            src2_sens = 0; 

                            #ifdef ARMV6_M
                            move_specification_armv6m(op, src1_v, psr_status_flag);
                            #endif

                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);

                            TAC.push_back(tmp_tac);
                        }
                        else{
                            psr_status_flag = true;
                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                            TAC.push_back(tmp_tac);
                        }


                        //add operation, add to current offset
                        TAC_type tmp_tac_add_offset;
                        Operation op_add_offset = Operation::ADD;

                        TAC_OPERAND src1_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src1_v_add_offset = incremental_offset_destination_register;
                        uint8_t src1_sens_add_offset = return_sensitivity((TAC.rbegin() + 1)->dest_sensitivity); 

                        TAC_OPERAND src2_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src2_v_add_offset = tmp_mult_destination_register;
                        uint8_t src2_sens_add_offset = return_sensitivity(TAC.back().dest_sensitivity);

                        TAC_OPERAND dest_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t dest_v_add_offset = incremental_offset_destination_register;
                        uint8_t dest_sens_add_offset = src1_sens_add_offset | src2_sens_add_offset; 

                        uint8_t sens_add_offset = dest_sens_add_offset; 

                        generate_generic_tmp_tac(tmp_tac_add_offset, op_add_offset, sens_add_offset, dest_type_add_offset, dest_v_add_offset, dest_sens_add_offset, src1_type_add_offset, src1_v_add_offset, src1_sens_add_offset, src2_type_add_offset, src2_v_add_offset, src2_sens_add_offset, false);
                        TAC.push_back(tmp_tac_add_offset);
                    }
                }
                //if dimension element not number -> get virtual variable -> offset += var * 2^(dimension - dimidx + 1)
                else{

                    src1_type = TAC_OPERAND::V_REG;
                    std::string src1_original_name = generate_original_name(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data), virtual_variables);
                    std::string src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data));
                    if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                        src1_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                        src1_sens = return_sensitivity(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data).sensitivity);
                    }
                    else{
                        throw std::runtime_error("in load_local_from_memory: variable that is used for index access of array is not in register yet");
                    }

                    if(first_non_empty_array_dimension){
                        //use destination of mul operation as we have to generate a new virtual register
                        //dest_sens = 0;
                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = v_register_ctr++;
                        incremental_offset_destination_register = dest_v;
                        
                        if((src1_sens == 1) || (src2_sens == 1)){
                            dest_sens = 1;
                            sens = 1;
                        }
                        else{
                            dest_sens = 0;
                            sens = 0;
                        }
                        psr_status_flag = true;
                        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                        TAC.push_back(tmp_tac);
                    }
                    else{
                        if(!tmp_mult_destination_register_set){
                            tmp_mult_destination_register_set = true;
                            tmp_mult_destination_register = v_register_ctr++;
                        }

                        if((src1_sens == 1) || (src2_sens == 1)){
                            dest_sens = 1;
                            sens = 1;
                        }
                        else{
                            dest_sens = 0;
                            sens = 0;
                        }
                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = tmp_mult_destination_register;
                        
                        psr_status_flag = true;

                        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                        TAC.push_back(tmp_tac);

                        //add operation, add to current offset
                        TAC_type tmp_tac_add_offset;
                        Operation op_add_offset = Operation::ADD;

                        TAC_OPERAND src1_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src1_v_add_offset = incremental_offset_destination_register;
                        uint8_t src1_sens_add_offset = return_sensitivity((TAC.rbegin() + 1)->dest_sensitivity); 

                        TAC_OPERAND src2_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src2_v_add_offset = tmp_mult_destination_register;
                        uint8_t src2_sens_add_offset = return_sensitivity(TAC.back().dest_sensitivity);

                        TAC_OPERAND dest_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t dest_v_add_offset = incremental_offset_destination_register;
                        uint8_t dest_sens_add_offset = src1_sens_add_offset | src2_sens_add_offset; 

                        uint8_t sens_add_offset = dest_sens_add_offset; 

                        generate_generic_tmp_tac(tmp_tac_add_offset, op_add_offset, sens_add_offset, dest_type_add_offset, dest_v_add_offset, dest_sens_add_offset, src1_type_add_offset, src1_v_add_offset, src1_sens_add_offset, src2_type_add_offset, src2_v_add_offset, src2_sens_add_offset, false);
                        TAC.push_back(tmp_tac_add_offset);

                    }
                }

                first_non_empty_array_dimension = false;
            }

        }

        if(is_load){ //load operations have additional, seperate offset which can be added 
            mem_load_instruction m_instr = boost::get<mem_load_instruction>(ssa_instr.instr);
            if(m_instr.offset.op_type != DATA_TYPE::NUM){
                throw std::runtime_error("in get_local_address: offset for load operation is not a number. can not handle case yet");
            }

            if(m_instr.offset.op_data != 0){
                TAC_type tmp_tac;
                Operation op = Operation::ADD;
                uint8_t sens = return_sensitivity(TAC.back().dest_sensitivity); 
                TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
                uint32_t dest_v = TAC.back().dest_v_reg_idx;
                uint8_t dest_sens = return_sensitivity(TAC.back().dest_sensitivity); 
                TAC_OPERAND src1_type = TAC_OPERAND::V_REG; 
                uint32_t src1_v = TAC.back().dest_v_reg_idx;;
                uint8_t src1_sens = return_sensitivity(TAC.back().dest_sensitivity); 
                TAC_OPERAND src2_type = TAC_OPERAND::NUM; 
                uint32_t src2_v = m_instr.offset.op_data;
                uint8_t src2_sens = 0;
                bool add_psr_status_flag;

                TAC_type load_number_into_register;
                if(add_specification_armv6m(src2_v, add_psr_status_flag, load_number_into_register)){
                    src2_type = TAC_OPERAND::V_REG;
                    src2_v = load_number_into_register.dest_v_reg_idx;

                    TAC.push_back(load_number_into_register);
                }

                generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, add_psr_status_flag);
                TAC.push_back(tmp_tac);
            }


        }

        //final offset = fp + address (e.g. &r[i][j])
        #ifdef ARMV6_M

        TAC_type tmp_tac;
        Operation op = Operation::MOV;
        uint8_t sens = 0; 
        TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
        uint32_t dest_v = v_register_ctr++;
        uint8_t dest_sens = 0; 
        TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
        uint32_t src1_v = 11;
        uint8_t src1_sens = 0; 
        TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
        uint32_t src2_v = UINT32_MAX;
        uint8_t src2_sens = 0;
        

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        

        if(it->is_initialized_array){
            tmp_tac.op = Operation::LOADWORD_WITH_LABELNAME;
            tmp_tac.src1_type = TAC_OPERAND::NONE; 
            tmp_tac.src1_v = UINT32_MAX;
            tmp_tac.src1_sensitivity = 0; 
            tmp_tac.src2_sensitivity = 0;
            tmp_tac.src2_v = UINT32_MAX;
            tmp_tac.src2_type = TAC_OPERAND::NONE;
            tmp_tac.function_call_name = "=" + it->base_value +  "_data";
        }



        TAC.push_back(tmp_tac);

        op = Operation::ADD;
        dest_type = TAC_OPERAND::V_REG; 
        dest_v = TAC.back().dest_v_reg_idx;
        src1_type = TAC_OPERAND::V_REG; 
        src1_v = TAC.back().dest_v_reg_idx;
        src1_sens = TAC.back().dest_sensitivity; 
        src2_type = TAC_OPERAND::V_REG; 
        src2_v = (*(TAC.rbegin() + 1)).dest_v_reg_idx;
        src2_sens = return_sensitivity((*(TAC.rbegin() + 1)).dest_sensitivity);
        if((src1_sens == 1) || (src2_sens == 1)){
            dest_sens = 1;
            sens = 1;
        }
        else{
            dest_sens = 0;
            sens = 0;
        }


        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        TAC.push_back(tmp_tac);

        if(!it->is_initialized_array){
            op = Operation::ADD;
            sens = TAC.back().dest_sensitivity; 
            dest_type = TAC_OPERAND::V_REG; 
            dest_v = TAC.back().dest_v_reg_idx;
            dest_sens = TAC.back().dest_sensitivity; 
            src1_type = TAC_OPERAND::V_REG; 
            src1_v = TAC.back().dest_v_reg_idx;
            src1_sens = TAC.back().dest_sensitivity; 
            src2_type = TAC_OPERAND::NUM; 
            src2_v = it->position;
            src2_sens = 0;
            bool add_psr_status_flag;

            TAC_type load_number_into_register;
            if(add_specification_armv6m(src2_v, add_psr_status_flag, load_number_into_register)){
                src2_type = TAC_OPERAND::V_REG;
                src2_v = load_number_into_register.dest_v_reg_idx;

                TAC.push_back(load_number_into_register);
            }
            
            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, add_psr_status_flag);
            TAC.push_back(tmp_tac);
        }

        #endif

    }
    else{
        //find address of element (use original name)
        auto it = std::find_if(memory_security.stack_mapping_tracker.begin(), memory_security.stack_mapping_tracker.end(), [ssa_original_name](stack_local_variable_tracking_struct& t){return t.base_value == ssa_original_name;});
        
        //offset = fp + address

        #ifdef ARMV6_M
        TAC_type tmp_tac;
        Operation op = Operation::MOV;
        uint8_t sens = 0; 
        TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
        uint32_t dest_v = v_register_ctr++;
        uint8_t dest_sens = 0; 
        TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
        uint32_t src1_v = 11;
        uint8_t src1_sens = 0; 
        TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
        uint32_t src2_v = UINT32_MAX;
        uint8_t src2_sens = 0;
        
        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        TAC.push_back(tmp_tac);

        op = Operation::ADD;
        sens = TAC.back().dest_sensitivity; 
        dest_type = TAC_OPERAND::V_REG; 
        dest_v = TAC.back().dest_v_reg_idx;
        dest_sens = TAC.back().dest_sensitivity; 
        src1_type = TAC_OPERAND::V_REG; 
        src1_v = TAC.back().dest_v_reg_idx;
        src1_sens = TAC.back().dest_sensitivity; 
        src2_type = TAC_OPERAND::NUM; 
        src2_v = it->position;
        src2_sens = 0;
        bool add_psr_status_flag;
        
        if(is_load){ //load operations have additional, seperate offset which can be added 
            mem_load_instruction m_instr = boost::get<mem_load_instruction>(ssa_instr.instr);
            if(m_instr.offset.op_type != DATA_TYPE::NUM){
                throw std::runtime_error("in get_local_address: offset for load operation is not a number. can not handle case yet");
            }
            src2_v += m_instr.offset.op_data;
        }

        TAC_type load_number_into_register;
        if(add_specification_armv6m(src2_v, add_psr_status_flag, load_number_into_register)){
            src2_type = TAC_OPERAND::V_REG;
            src2_v = load_number_into_register.dest_v_reg_idx;

            TAC.push_back(load_number_into_register);
        }

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, add_psr_status_flag);
        TAC.push_back(tmp_tac);

        #endif
    }

    //save in virt_var_to_v_reg the mapping between the register and the address of this variable
    std::string original_key = "";
    std::string ssa_key = "";

    //if we store to *r or *r[i][j] -> we store at address r or r[i][j]
    if((original_ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON) || (original_ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON)){
        original_key = ssa_original_name;
        if(original_ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON){
            ssa_key = ssa_name;
        }
    }
    //if we store to r[i][j] -> we store at address &r[i][j]
    if(original_ssa_add_on == SSA_ADD_ON::ARR_ADD_ON){
        original_key = "&" + ssa_original_name;
        ssa_key = "&" + ssa_name;
    }


    if(original_ssa_add_on == SSA_ADD_ON::AMP_ADD_ON){
        original_key = "&" + ssa_original_name;
        ssa_key = "";
    }

    if(original_ssa_add_on == SSA_ADD_ON::AMP_ARR_ADD_ON){
        original_key = "&" + ssa_original_name;
        ssa_key = "&" + ssa_name;
    }

    std::tuple<std::string, std::string> key_tuple = std::make_tuple(original_key, ssa_key);
    virtual_variable_to_v_reg[key_tuple] = TAC.back().dest_v_reg_idx;

    calling_operation_src_v = TAC.back().dest_v_reg_idx;
    calling_operation_sens = return_sensitivity(TAC.back().dest_sensitivity);
}

uint8_t get_sensitivity_from_stack_memory(SSA_ADD_ON ssa_add_on, std::string ssa_original_name, virtual_variable_information& virt_var, std::vector<stack_local_variable_tracking_struct>& stack_mapping_tracker){
    //1. case: if simple variable or array where all indices are numbers -> can easily track *specific* sensitivity
    if(ssa_add_on == SSA_ADD_ON::NO_ADD_ON){
        auto it = std::find_if(stack_mapping_tracker.begin(), stack_mapping_tracker.end(), [ssa_original_name](stack_local_variable_tracking_struct& t){return t.base_value == ssa_original_name;});
        return return_sensitivity(it->sensitivity);
    }
    else{

        std::string original_name = "";

        original_name = virt_var.original_name;
        auto it = std::find_if(stack_mapping_tracker.begin(), stack_mapping_tracker.end(), [original_name](stack_local_variable_tracking_struct t){return t.base_value == original_name;});
        //get array_information, skip all empty dimensions
        array_information a_info = boost::get<array_information>(virt_var.data_information);

        bool all_indices_numbers = true;
        std::string original_name_with_indices = original_name;
        //check if all indices in array access consist of numbers and create
        for(uint32_t idx = 0; idx < a_info.dimension; ++idx){
            if((a_info.depth_per_dimension.at(idx).op_type != DATA_TYPE::NONE) && (a_info.depth_per_dimension.at(idx).op_type != DATA_TYPE::NUM)){
                all_indices_numbers = false;
                break;
            }
            if(a_info.depth_per_dimension.at(idx).op_type != DATA_TYPE::NONE){
                original_name_with_indices += "[" + std::to_string(a_info.depth_per_dimension.at(idx).op_data) + "]";
            }
        }

        if(all_indices_numbers){
            it = std::find_if(stack_mapping_tracker.begin(), stack_mapping_tracker.end(), [original_name_with_indices](stack_local_variable_tracking_struct t){return t.curr_value == original_name_with_indices;});
            return return_sensitivity(it->sensitivity);
        }
        //2. case: if r[i_44][j_55] -> do not now which specific element is chosen -> make assumption, if any element within this array is sensitive the shadow register will be sensitive
        else{
            for(uint32_t idx = 0; idx < stack_mapping_tracker.size(); ++idx){
                if(stack_mapping_tracker.at(idx).base_value == original_name){
                    if(stack_mapping_tracker.at(idx).sensitivity == 1){
                        return 1;
                    }
                }
            }
            return 0;
        }
    }

}

void load_local_from_memory(std::vector<TAC_type>& TAC, uint32_t& calling_operation_src_v, uint8_t& calling_operation_sens, virtual_variables_vec& virtual_variables, virtual_variable_information& virt_var, std::string ssa_original_name, std::string ssa_name, SSA_ADD_ON ssa_add_on, memory_security_struct& memory_security){
    if((ssa_add_on != SSA_ADD_ON::ARR_ADD_ON) && (ssa_add_on != SSA_ADD_ON::NO_ADD_ON)){
        throw std::runtime_error("in load_local_from_memory: ssa is neither plain array nor plain variable");
    }
    if((ssa_add_on == SSA_ADD_ON::ARR_ADD_ON)){
        //find base address of array
        std::string original_name = "";

        original_name = virt_var.original_name;
        auto it = std::find_if(memory_security.stack_mapping_tracker.begin(), memory_security.stack_mapping_tracker.end(), [original_name](stack_local_variable_tracking_struct t){return t.base_value == original_name;});
        //get array_information, skip all empty dimensions
        array_information a_info = boost::get<array_information>(virt_var.data_information);

        //get original array and its array information to correctly compute the offset within array
        auto it_original_arr = std::find_if(virtual_variables.begin(), virtual_variables.end(), [original_name](const virtual_variable_information& t){return (t.ssa_name == original_name) && (t.index_to_original == -1);});
        array_information a_info_original = boost::get<array_information>(it_original_arr->data_information);

        bool first_non_empty_array_dimension = true;
        bool tmp_mult_destination_register_set = false;
        uint32_t incremental_offset_destination_register = UINT32_MAX;
        uint32_t tmp_mult_destination_register = UINT32_MAX; //incremental_offset_destination_register += i * 8 -> i*8 needs seperate register
        //go over all dimensions (dimidx)
        for(uint32_t dim_idx = 0; dim_idx < a_info.dimension; ++dim_idx){
            //skip empty dimension
            if(a_info.depth_per_dimension.at(dim_idx).op_type != DATA_TYPE::NONE){
                //in r[i_44][j_44] -> this operation computes i_44 * 8, respectively j_44 * 4

                TAC_type tmp_tac;
                Operation op = Operation::MUL;
                uint8_t sens; 
                TAC_OPERAND dest_type; 
                uint32_t dest_v;
                uint8_t dest_sens; 
                TAC_OPERAND src1_type; 
                uint32_t src1_v;
                uint8_t src1_sens; 
                TAC_OPERAND src2_type; 
                uint32_t src2_v;
                uint8_t src2_sens;
                bool psr_status_flag;

                src2_sens = 0;
                src2_type = TAC_OPERAND::NUM;
                src2_v = 0;
                if(dim_idx != (a_info.dimension - 1)){
                    src2_v = virt_var.data_type_size; 
                    //build offset incrementally by adding all dimension to generate correct offset
                    for(uint32_t lower_dim_idx = dim_idx + 1; lower_dim_idx < a_info.dimension; ++lower_dim_idx){
                        src2_v *= a_info_original.depth_per_dimension.at(lower_dim_idx).op_data;
                    }
                }
                else{
                    src2_v = virt_var.data_type_size; 
                }
                

                if(std::floor(std::log2(src2_v)) == std::log2(src2_v)){
                    op = Operation::LSL;
                    src2_v = std::log2(src2_v);
                }
                else{
                    //mul operation can only handle registers as operands -> have to move value to register before
                    TAC_type mov_tac;
                    Operation mov_op = Operation::MOV;
                    uint8_t mov_sens = 0; 
                    TAC_OPERAND mov_dest_type = TAC_OPERAND::V_REG; 
                    uint32_t mov_dest_v = v_register_ctr++;
                    uint8_t mov_dest_sens = 0; 
                    TAC_OPERAND mov_src1_type = TAC_OPERAND::NUM; 
                    uint32_t mov_src1_v = src2_v;
                    uint8_t mov_src1_sens = 0; 
                    TAC_OPERAND mov_src2_type = TAC_OPERAND::NONE; 
                    uint32_t mov_src2_v = UINT32_MAX;
                    uint8_t mov_src2_sens = 0;
                    // bool mov_psr_status_flag;

                    #ifdef ARMV6_M
                    move_specification_armv6m(mov_op, mov_src1_v, psr_status_flag);
                    #endif

                    generate_generic_tmp_tac(mov_tac, mov_op, mov_sens, mov_dest_type, mov_dest_v, mov_dest_sens, mov_src1_type, mov_src1_v, mov_src1_sens, mov_src2_type, mov_src2_v, mov_src2_sens, psr_status_flag);

                    TAC.push_back(mov_tac);

                    src2_v = mov_dest_v;
                    src2_type = TAC_OPERAND::V_REG;
                }


                //build offset incrementally by adding all dimension to generate correct offset


                //if dimension element is number -> offset += op_data * 2^(dimension - dimidx + 1) -> register will definitely be public
                if(a_info.depth_per_dimension.at(dim_idx).op_type == DATA_TYPE::NUM){
                    src1_sens = 0;
                    src1_type = TAC_OPERAND::NUM;
                    src1_v = a_info.depth_per_dimension.at(dim_idx).op_data;
                    if(first_non_empty_array_dimension){
                        //use destination of mul operation as we have to generate a new virtual register
                        dest_sens = 0;
                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = v_register_ctr++;
                        incremental_offset_destination_register = dest_v;


                        if(op == Operation::LSL){
                            op = Operation::MOV;
                            src1_v = (src1_v << src2_v);
                            src2_v = UINT32_MAX;
                            src2_type = TAC_OPERAND::NONE;
                            src2_sens = 0; 

                            #ifdef ARMV6_M
                            move_specification_armv6m(op, src1_v, psr_status_flag);
                            #endif


                            if((src1_sens == 1) || (src2_sens == 1)){
                                dest_sens = 1;
                                sens = 1;
                            }
                            else{
                                dest_sens = 0;
                                sens = 0;
                            }

                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);

                            TAC.push_back(tmp_tac);
                        }
                        else{

                            if((src1_sens == 1) || (src2_sens == 1)){
                                dest_sens = 1;
                                sens = 1;
                            }
                            else{
                                dest_sens = 0;
                                sens = 0;
                            }

                            psr_status_flag = true;
                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                            TAC.push_back(tmp_tac);
                        }
                    }
                    else{
                        
                        if(!tmp_mult_destination_register_set){
                            tmp_mult_destination_register_set = true;
                            tmp_mult_destination_register = v_register_ctr++;
                        }

                        if((src1_sens == 1) || (src2_sens == 1)){
                            dest_sens = 1;
                            sens = 1;
                        }
                        else{
                            dest_sens = 0;
                            sens = 0;
                        }
                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = tmp_mult_destination_register;
                        psr_status_flag = true;


                        if(op == Operation::LSL){
                            op = Operation::MOV;
                            src1_v = (src1_v << src2_v);
                            src2_v = UINT32_MAX;
                            src2_type = TAC_OPERAND::NONE;
                            src2_sens = 0; 

                            #ifdef ARMV6_M
                            move_specification_armv6m(op, src1_v, psr_status_flag);
                            #endif


                            if((src1_sens == 1) || (src2_sens == 1)){
                                dest_sens = 1;
                                sens = 1;
                            }
                            else{
                                dest_sens = 0;
                                sens = 0;
                            }

                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);

                            TAC.push_back(tmp_tac);
                        }
                        else{
                            psr_status_flag = true;
                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                            TAC.push_back(tmp_tac);
                        }

                        //add operation, add to current offset
                        TAC_type tmp_tac_add_offset;
                        Operation op_add_offset = Operation::ADD;

                        TAC_OPERAND src1_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src1_v_add_offset = incremental_offset_destination_register;
                        uint8_t src1_sens_add_offset = return_sensitivity((TAC.rbegin() + 1)->dest_sensitivity); 

                        TAC_OPERAND src2_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src2_v_add_offset = tmp_mult_destination_register;
                        uint8_t src2_sens_add_offset = return_sensitivity(TAC.back().dest_sensitivity);

                        TAC_OPERAND dest_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t dest_v_add_offset = incremental_offset_destination_register;
                        uint8_t dest_sens_add_offset = src1_sens_add_offset | src2_sens_add_offset; 

                        uint8_t sens_add_offset = dest_sens_add_offset; 

                        generate_generic_tmp_tac(tmp_tac_add_offset, op_add_offset, sens_add_offset, dest_type_add_offset, dest_v_add_offset, dest_sens_add_offset, src1_type_add_offset, src1_v_add_offset, src1_sens_add_offset, src2_type_add_offset, src2_v_add_offset, src2_sens_add_offset, false);
                        TAC.push_back(tmp_tac_add_offset);
                    }
                }
                //if dimension element not number -> get virtual variable -> offset += var * 2^(dimension - dimidx + 1)
                else{

                    src1_type = TAC_OPERAND::V_REG;
                    std::string src1_original_name = generate_original_name(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data), virtual_variables);
                    std::string src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data));
                    if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                        src1_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                        src1_sens = return_sensitivity(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data).sensitivity);
                    }
                    else{
                        throw std::runtime_error("in load_local_from_memory: variable that is used for index access of array is not in register yet");
                    }

                    if(first_non_empty_array_dimension){
                        //use destination of mul operation as we have to generate a new virtual register

                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = v_register_ctr++;
                        incremental_offset_destination_register = dest_v;
                        if((src1_sens == 1) || (src2_sens == 1)){
                            dest_sens = 1;
                            sens = 1;
                        }
                        else{
                            dest_sens = 0;
                            sens = 0;
                        }
                        psr_status_flag = true;
                        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                        TAC.push_back(tmp_tac);
                    }
                    else{
                        if(!tmp_mult_destination_register_set){
                            tmp_mult_destination_register_set = true;
                            tmp_mult_destination_register = v_register_ctr++;
                        }

                        if((src1_sens == 1) || (src2_sens == 1)){
                            dest_sens = 1;
                            sens = 1;
                        }
                        else{
                            dest_sens = 0;
                            sens = 0;
                        }
                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = tmp_mult_destination_register;
                
                        psr_status_flag = true;
                        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                        TAC.push_back(tmp_tac);

                        //add operation, add to current offset
                        TAC_type tmp_tac_add_offset;
                        Operation op_add_offset = Operation::ADD;

                        TAC_OPERAND src1_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src1_v_add_offset = incremental_offset_destination_register;
                        uint8_t src1_sens_add_offset = return_sensitivity((TAC.rbegin() + 1)->dest_sensitivity); 

                        TAC_OPERAND src2_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src2_v_add_offset = tmp_mult_destination_register;
                        uint8_t src2_sens_add_offset = return_sensitivity(TAC.back().dest_sensitivity);

                        TAC_OPERAND dest_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t dest_v_add_offset = incremental_offset_destination_register;
                        uint8_t dest_sens_add_offset = src1_sens_add_offset | src2_sens_add_offset; 

                        uint8_t sens_add_offset = dest_sens_add_offset; 

                        generate_generic_tmp_tac(tmp_tac_add_offset, op_add_offset, sens_add_offset, dest_type_add_offset, dest_v_add_offset, dest_sens_add_offset, src1_type_add_offset, src1_v_add_offset, src1_sens_add_offset, src2_type_add_offset, src2_v_add_offset, src2_sens_add_offset, false);
                        TAC.push_back(tmp_tac_add_offset);

                    }
                }

                first_non_empty_array_dimension = false;
            }

        }

        //add offset of local variable
        TAC_type add_local_offset;
        Operation op = Operation::ADD;
        uint8_t sens = return_sensitivity(TAC.back().dest_sensitivity); 
        TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
        uint32_t dest_v = TAC.back().dest_v_reg_idx;
        uint8_t dest_sens = return_sensitivity(TAC.back().dest_sensitivity);
        TAC_OPERAND src1_type = TAC_OPERAND::V_REG; 
        uint32_t src1_v = TAC.back().dest_v_reg_idx;
        uint8_t src1_sens = return_sensitivity(TAC.back().dest_sensitivity);
        TAC_OPERAND src2_type = TAC_OPERAND::NUM; 
        uint32_t src2_v = it->position;
        uint8_t src2_sens = 0;
        bool add_psr_status_flag;

        TAC_type load_number_into_register;
        if(add_specification_armv6m(src2_v, add_psr_status_flag, load_number_into_register)){
            src2_type = TAC_OPERAND::V_REG;
            src2_v = load_number_into_register.dest_v_reg_idx;

            TAC.push_back(load_number_into_register);
        }

        generate_generic_tmp_tac(add_local_offset, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, add_psr_status_flag);
        TAC.push_back(add_local_offset);

        #ifdef ARMV6_M

        TAC_type tmp_tac;
        op = Operation::MOV;
        sens = 0; 
        dest_type = TAC_OPERAND::V_REG; 
        dest_v = v_register_ctr++;
        dest_sens = 0; 
        src1_type = TAC_OPERAND::R_REG; 
        src1_v = 11;
        src1_sens = 0; 
        src2_type = TAC_OPERAND::NONE; 
        src2_v = UINT32_MAX;
        src2_sens = 0;
        
        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        TAC.push_back(tmp_tac);


        op = Operation::ADD;
        dest_type = TAC_OPERAND::V_REG; 
        dest_v = TAC.back().dest_v_reg_idx;
        src1_type = TAC_OPERAND::V_REG; 
        src1_v = TAC.back().dest_v_reg_idx;
        src1_sens = 0; 
        src2_type = TAC_OPERAND::V_REG; 
        src2_v = (*(TAC.rbegin() + 1)).dest_v_reg_idx;
        src2_sens = return_sensitivity((*(TAC.rbegin() + 1)).dest_sensitivity);
        if((src1_sens == 1) || (src2_sens == 1)){
            dest_sens = 1;
            sens = 1;
        }
        else{
            dest_sens = 0;
            sens = 0;
        }

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        TAC.push_back(tmp_tac);

        op = Operation::ADD;
        sens = TAC.back().dest_sensitivity;  
        dest_type = TAC_OPERAND::V_REG; 
        dest_v = TAC.back().dest_v_reg_idx;
        dest_sens = TAC.back().dest_sensitivity; 
        src1_type = TAC_OPERAND::V_REG; 
        src1_v = TAC.back().dest_v_reg_idx;
        src1_sens = TAC.back().dest_sensitivity; 
        src2_type = TAC_OPERAND::NUM; 
        src2_v = it->position;
        src2_sens = 0;
        

        if(add_specification_armv6m(src2_v, add_psr_status_flag, load_number_into_register)){
            src2_type = TAC_OPERAND::V_REG;
            src2_v = load_number_into_register.dest_v_reg_idx;

            TAC.push_back(load_number_into_register);
        }

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, add_psr_status_flag);
        TAC.push_back(tmp_tac);
        #endif

    }
    else{
        //find address of element (use original name)
        auto it = std::find_if(memory_security.stack_mapping_tracker.begin(), memory_security.stack_mapping_tracker.end(), [ssa_original_name](stack_local_variable_tracking_struct& t){return t.base_value == ssa_original_name;});
        
        #ifdef ARMV6_M

        TAC_type tmp_tac;
        Operation op = Operation::MOV;
        uint8_t sens = 0; 
        TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
        uint32_t dest_v = v_register_ctr++;
        uint8_t dest_sens = 0; 
        TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
        uint32_t src1_v = 11;
        uint8_t src1_sens = 0; 
        TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
        uint32_t src2_v = UINT32_MAX;
        uint8_t src2_sens = 0;
        
        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        TAC.push_back(tmp_tac);

        op = Operation::ADD;
        sens = TAC.back().dest_sensitivity; 
        dest_type = TAC_OPERAND::V_REG; 
        dest_v = TAC.back().dest_v_reg_idx;
        dest_sens = TAC.back().dest_sensitivity; 
        src1_type = TAC_OPERAND::V_REG; 
        src1_v = TAC.back().dest_v_reg_idx;
        src1_sens = TAC.back().dest_sensitivity; 
        src2_type = TAC_OPERAND::NUM; 
        src2_v = it->position;
        src2_sens = 0;
        bool add_psr_status_flag;
        
        TAC_type load_number_into_register;
        if(add_specification_armv6m(src2_v, add_psr_status_flag, load_number_into_register)){
            src2_type = TAC_OPERAND::V_REG;
            src2_v = load_number_into_register.dest_v_reg_idx;

            TAC.push_back(load_number_into_register);
        }

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, add_psr_status_flag);
        TAC.push_back(tmp_tac);

        #endif
    }

    //in both cases we have now the address from where to load
    //perform now actual load
    TAC_type tmp_tac;
    Operation op = Operation::LOADWORD;
    
    TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
    uint32_t dest_v = v_register_ctr++;
    uint8_t dest_sens = get_sensitivity_from_stack_memory(ssa_add_on, ssa_original_name, virt_var, memory_security.stack_mapping_tracker); 
    TAC_OPERAND src1_type = TAC_OPERAND::V_REG; 
    uint32_t src1_v = TAC.back().dest_v_reg_idx;
    uint8_t src1_sens = return_sensitivity(TAC.back().dest_sensitivity); 
    TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
    uint32_t src2_v = UINT32_MAX;
    uint8_t src2_sens = 0;
    uint8_t sens = dest_sens; 


    //save in virt_var_to_v_reg the mapping between the register and the address of this variable
    std::string original_key = "&" + ssa_original_name;
    std::string ssa_key = "";
    if(ssa_name != ""){
        ssa_key = "&" + ssa_name;
    }
    std::tuple<std::string, std::string> key_tuple = std::make_tuple(original_key, ssa_key);
    virtual_variable_to_v_reg[key_tuple] = TAC.back().dest_v_reg_idx;

    //as we load the actual value into a register we also have to save in virt_var_to_v_reg the mapping between the register and the actual variable
    original_key = ssa_original_name;
    ssa_key = "";
    if(ssa_name != ""){
        ssa_key = ssa_name;
    }
    key_tuple = std::make_tuple(original_key, ssa_key);
    virtual_variable_to_v_reg[key_tuple] = dest_v;

    calling_operation_src_v = dest_v;
    calling_operation_sens = dest_sens;

    generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
    TAC.push_back(tmp_tac);
}

void load_input_from_memory(std::vector<TAC_type>& TAC, uint32_t& calling_operation_src_v, uint8_t& calling_operation_sens, virtual_variables_vec& virtual_variables, virtual_variable_information& virt_var, std::string ssa_original_name, std::string ssa_name, SSA_ADD_ON ssa_add_on, memory_security_struct& memory_security){
    if((ssa_add_on != SSA_ADD_ON::ARR_ADD_ON) && (ssa_add_on != SSA_ADD_ON::NO_ADD_ON)){
        throw std::runtime_error("in load_input_from_memory: ssa is neither plain array nor plain variable");
    }
    if((ssa_add_on == SSA_ADD_ON::ARR_ADD_ON)){
        //find base address of array
        std::string original_name = "";

        original_name = virt_var.original_name;
        auto it = std::find_if(memory_security.input_parameter_mapping_tracker.begin(), memory_security.input_parameter_mapping_tracker.end(), [original_name](input_parameter_stack_variable_tracking_struct t){return t.base_value == original_name;});
        //get array_information, skip all empty dimensions
        array_information a_info = boost::get<array_information>(virt_var.data_information);

        //get original array and its array information to correctly compute the offset within array
        auto it_original_arr = std::find_if(virtual_variables.begin(), virtual_variables.end(), [original_name](const virtual_variable_information& t){return (t.ssa_name == original_name) && (t.index_to_original == -1);});
        array_information a_info_original = boost::get<array_information>(it_original_arr->data_information);


        bool first_non_empty_array_dimension = true;
        bool tmp_mult_destination_register_set = false;
        uint32_t incremental_offset_destination_register = UINT32_MAX;
        uint32_t tmp_mult_destination_register = UINT32_MAX; //incremental_offset_destination_register += i * 8 -> i*8 needs seperate register
        //go over all dimensions (dimidx)
        for(uint32_t dim_idx = 0; dim_idx < a_info.dimension; ++dim_idx){
            //skip empty dimension
            if(a_info.depth_per_dimension.at(dim_idx).op_type != DATA_TYPE::NONE){
                TAC_type tmp_tac;
                Operation op = Operation::MUL;
                uint8_t sens; 
                TAC_OPERAND dest_type; 
                uint32_t dest_v;
                uint8_t dest_sens; 
                TAC_OPERAND src1_type; 
                uint32_t src1_v;
                uint8_t src1_sens; 
                TAC_OPERAND src2_type; 
                uint32_t src2_v;
                uint8_t src2_sens;
                bool psr_status_flag;

                src2_sens = 0;
                src2_type = TAC_OPERAND::NUM;
                src2_v = 0;
                if(dim_idx != (a_info.dimension - 1)){
                    src2_v = 4;
                    //build offset incrementally by adding all dimension to generate correct offset
                    for(uint32_t lower_dim_idx = dim_idx + 1; lower_dim_idx < a_info.dimension; ++lower_dim_idx){
                        src2_v *= a_info_original.depth_per_dimension.at(lower_dim_idx).op_data;
                    }
                }
                else{
                    src2_v = 4;
                }
                

                if(std::floor(std::log2(src2_v)) == std::log2(src2_v)){
                    op = Operation::LSL;
                    src2_v = std::log2(src2_v);
                }
                else{
                    //mul operation can only handle registers as operands -> have to move value to register before
                    TAC_type mov_tac;
                    Operation mov_op = Operation::MOV;
                    uint8_t mov_sens = 0; 
                    TAC_OPERAND mov_dest_type = TAC_OPERAND::V_REG; 
                    uint32_t mov_dest_v = v_register_ctr++;
                    uint8_t mov_dest_sens = 0; 
                    TAC_OPERAND mov_src1_type = TAC_OPERAND::NUM; 
                    uint32_t mov_src1_v = src2_v;
                    uint8_t mov_src1_sens = 0; 
                    TAC_OPERAND mov_src2_type = TAC_OPERAND::NONE; 
                    uint32_t mov_src2_v = UINT32_MAX;
                    uint8_t mov_src2_sens = 0;
                    // bool mov_psr_status_flag;

                    #ifdef ARMV6_M
                    move_specification_armv6m(mov_op, mov_src1_v, psr_status_flag);
                    #endif

                    generate_generic_tmp_tac(mov_tac, mov_op, mov_sens, mov_dest_type, mov_dest_v, mov_dest_sens, mov_src1_type, mov_src1_v, mov_src1_sens, mov_src2_type, mov_src2_v, mov_src2_sens, psr_status_flag);

                    TAC.push_back(mov_tac);

                    src2_v = mov_dest_v;
                    src2_type = TAC_OPERAND::V_REG;
                }



                //if dimension element is number -> offset += op_data * 2^(dimension - dimidx + 1) -> register will definitely be public
                if(a_info.depth_per_dimension.at(dim_idx).op_type == DATA_TYPE::NUM){
                    src1_sens = 0;
                    src1_type = TAC_OPERAND::NUM;
                    src1_v = a_info.depth_per_dimension.at(dim_idx).op_data;
                    if(first_non_empty_array_dimension){
                        //use destination of mul operation as we have to generate a new virtual register
                        dest_sens = 0;
                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = v_register_ctr++;
                        incremental_offset_destination_register = dest_v;

                        if(op == Operation::LSL){
                            op = Operation::MOV;
                            src1_v = (src1_v << src2_v);
                            src2_v = UINT32_MAX;
                            src2_type = TAC_OPERAND::NONE;
                            src2_sens = 0; 

                            #ifdef ARMV6_M
                            move_specification_armv6m(op, src1_v, psr_status_flag);
                            #endif

                            if((src1_sens == 1) || (src2_sens == 1)){
                                dest_sens = 1;
                                sens = 1;
                            }
                            else{
                                dest_sens = 0;
                                sens = 0;
                            }

                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);

                            TAC.push_back(tmp_tac);
                        }
                        else{
                            psr_status_flag = true;

                            if((src1_sens == 1) || (src2_sens == 1)){
                                dest_sens = 1;
                                sens = 1;
                            }
                            else{
                                dest_sens = 0;
                                sens = 0;
                            }

                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                            TAC.push_back(tmp_tac);
                        }

                    }
                    else{
                        
                        if(!tmp_mult_destination_register_set){
                            tmp_mult_destination_register_set = true;
                            tmp_mult_destination_register = v_register_ctr++;
                        }

                        if((src1_sens == 1) || (src2_sens == 1)){
                            dest_sens = 1;
                            sens = 1;
                        }
                        else{
                            dest_sens = 0;
                            sens = 0;
                        }
                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = tmp_mult_destination_register;
                

                        if(op == Operation::LSL){
                            op = Operation::MOV;
                            src1_v = (src1_v << src2_v);
                            src2_v = UINT32_MAX;
                            src2_type = TAC_OPERAND::NONE;
                            src2_sens = 0; 

                            #ifdef ARMV6_M
                            move_specification_armv6m(op, src1_v, psr_status_flag);
                            #endif

                            if((src1_sens == 1) || (src2_sens == 1)){
                                dest_sens = 1;
                                sens = 1;
                            }
                            else{
                                dest_sens = 0;
                                sens = 0;
                            }

                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);

                            TAC.push_back(tmp_tac);
                        }
                        else{
                            psr_status_flag = true;

                            if((src1_sens == 1) || (src2_sens == 1)){
                                dest_sens = 1;
                                sens = 1;
                            }
                            else{
                                dest_sens = 0;
                                sens = 0;
                            }

                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, incremental_offset_destination_register, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                            TAC.push_back(tmp_tac);
                        }

                        //add operation, add to current offset
                        TAC_type tmp_tac_add_offset;
                        Operation op_add_offset = Operation::ADD;

                        TAC_OPERAND src1_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src1_v_add_offset = incremental_offset_destination_register;
                        uint8_t src1_sens_add_offset = return_sensitivity((TAC.rbegin() + 1)->dest_sensitivity); 

                        TAC_OPERAND src2_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src2_v_add_offset = tmp_mult_destination_register;
                        uint8_t src2_sens_add_offset = return_sensitivity(TAC.back().dest_sensitivity);

                        TAC_OPERAND dest_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t dest_v_add_offset = incremental_offset_destination_register;
                        uint8_t dest_sens_add_offset = src1_sens_add_offset | src2_sens_add_offset; 

                        uint8_t sens_add_offset = dest_sens_add_offset; 

                        generate_generic_tmp_tac(tmp_tac_add_offset, op_add_offset, sens_add_offset, dest_type_add_offset, dest_v_add_offset, dest_sens_add_offset, src1_type_add_offset, src1_v_add_offset, src1_sens_add_offset, src2_type_add_offset, src2_v_add_offset, src2_sens_add_offset, false);
                        TAC.push_back(tmp_tac_add_offset);
                    }
                }
                //if dimension element not number -> get virtual variable -> offset += var * 2^(dimension - dimidx + 1)
                else{

                    src1_type = TAC_OPERAND::V_REG;
                    std::string src1_original_name = generate_original_name(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data), virtual_variables);
                    std::string src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data));
                    if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                        src1_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                        src1_sens = return_sensitivity(virtual_variables.at(a_info.depth_per_dimension.at(dim_idx).op_data).sensitivity);
                    }
                    else{
                        throw std::runtime_error("in load_local_from_memory: variable that is used for index access of array is not in register yet");
                    }

                    if(first_non_empty_array_dimension){
                        //use destination of mul operation as we have to generate a new virtual register

                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = v_register_ctr++;
                        incremental_offset_destination_register = dest_v;
                        // tmp_mult_destination_register = v_register_ctr++;
                        if((src1_sens == 1) || (src2_sens == 1)){
                            dest_sens = 1;
                            sens = 1;
                        }
                        else{
                            dest_sens = 0;
                            sens = 0;
                        }
                        psr_status_flag = true;
                        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                        TAC.push_back(tmp_tac);
                    }
                    else{
                        if(!tmp_mult_destination_register_set){
                            tmp_mult_destination_register_set = true;
                            tmp_mult_destination_register = v_register_ctr++;
                        }

                        if((src1_sens == 1) || (src2_sens == 1)){
                            dest_sens = 1;
                            sens = 1;
                        }
                        else{
                            dest_sens = 0;
                            sens = 0;
                        }
                        dest_type = TAC_OPERAND::V_REG;
                        dest_v = tmp_mult_destination_register;
                
                        psr_status_flag = true;
                        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                        TAC.push_back(tmp_tac);

                        //add operation, add to current offset
                        TAC_type tmp_tac_add_offset;
                        Operation op_add_offset = Operation::ADD;

                        TAC_OPERAND src1_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src1_v_add_offset = incremental_offset_destination_register;
                        uint8_t src1_sens_add_offset = return_sensitivity((TAC.rbegin() + 1)->dest_sensitivity); 

                        TAC_OPERAND src2_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t src2_v_add_offset = tmp_mult_destination_register;
                        uint8_t src2_sens_add_offset = return_sensitivity(TAC.back().dest_sensitivity);

                        TAC_OPERAND dest_type_add_offset = TAC_OPERAND::V_REG; 
                        uint32_t dest_v_add_offset = incremental_offset_destination_register;
                        uint8_t dest_sens_add_offset = src1_sens_add_offset | src2_sens_add_offset; 

                        uint8_t sens_add_offset = dest_sens_add_offset; 

                        generate_generic_tmp_tac(tmp_tac_add_offset, op_add_offset, sens_add_offset, dest_type_add_offset, dest_v_add_offset, dest_sens_add_offset, src1_type_add_offset, src1_v_add_offset, src1_sens_add_offset, src2_type_add_offset, src2_v_add_offset, src2_sens_add_offset, false);
                        TAC.push_back(tmp_tac_add_offset);

                    }
                }

                first_non_empty_array_dimension = false;
            }

        }




        //load base address of array from input stack
        TAC_type tmp_tac;
        Operation op = Operation::MOV;
        uint8_t sens = 0; 
        TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
        uint32_t dest_v = v_register_ctr++;
        uint8_t dest_sens = 0; 
        TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
        uint32_t src1_v = 11;
        uint8_t src1_sens = 0; 
        TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
        uint32_t src2_v = UINT32_MAX;
        uint8_t src2_sens = 0;
        
        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        TAC.push_back(tmp_tac);


        op = Operation::ADD_INPUT_STACK;
        sens = TAC.back().dest_sensitivity;
        dest_type = TAC_OPERAND::V_REG; 
        dest_v = TAC.back().dest_v_reg_idx;
        dest_sens = TAC.back().dest_sensitivity;
        src1_type = TAC_OPERAND::V_REG; 
        src1_v = TAC.back().dest_v_reg_idx;
        src1_sens = TAC.back().dest_sensitivity;
        src2_type = TAC_OPERAND::NUM; 
        src2_v = it->position;
        src2_sens = 0;
        bool add_input_stack_psr_status_flag;

        TAC_type load_number_into_register;
        if(add_specification_armv6m(src2_v, add_input_stack_psr_status_flag, load_number_into_register)){
            src2_type = TAC_OPERAND::V_REG;
            src2_v = load_number_into_register.dest_v_reg_idx;

            TAC.push_back(load_number_into_register);
        }
        
        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, add_input_stack_psr_status_flag);
        TAC.push_back(tmp_tac);

        op = Operation::LOADWORD_FROM_INPUT_STACK;
        sens = TAC.back().dest_sensitivity; 
        dest_type = TAC_OPERAND::V_REG; 
        dest_v = v_register_ctr++;
        dest_sens = TAC.back().dest_sensitivity; 
        src1_type = TAC_OPERAND::V_REG; 
        src1_v = TAC.back().dest_v_reg_idx;
        src1_sens = TAC.back().dest_sensitivity; 
        src2_type = TAC_OPERAND::NONE; 
        src2_v = UINT32_MAX;
        src2_sens = 0;
        
        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        TAC.push_back(tmp_tac);

        #ifdef ARMV6_M

        //add array index offset to base address of array
        op = Operation::ADD;
        dest_type = TAC_OPERAND::V_REG; 
        dest_v = v_register_ctr++;
        src1_type = TAC_OPERAND::V_REG; 
        src1_v = TAC.back().dest_v_reg_idx;
        src1_sens = TAC.back().dest_sensitivity;
        src2_type = (*(TAC.rbegin() + 3)).dest_type; 
        src2_v = (*(TAC.rbegin() + 3)).dest_v_reg_idx;
        src2_sens = 0;
        if((src1_sens == 1) || (src2_sens == 1)){
            dest_sens = 1;
            sens = 1;
        }
        else{
            dest_sens = 0;
            sens = 0;
        }

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        TAC.push_back(tmp_tac);

        #endif

    }
    else{
        //find address of element (use original name)
        auto it = std::find_if(memory_security.input_parameter_mapping_tracker.begin(), memory_security.input_parameter_mapping_tracker.end(), [ssa_original_name](input_parameter_stack_variable_tracking_struct& t){return t.base_value == ssa_original_name;});
        
        #ifdef ARMV6_M

        TAC_type tmp_tac;
        Operation op = Operation::MOV;
        uint8_t sens = 0; 
        TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
        uint32_t dest_v = v_register_ctr++;
        uint8_t dest_sens = 0; 
        TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
        uint32_t src1_v = 11;
        uint8_t src1_sens = 0; 
        TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
        uint32_t src2_v = UINT32_MAX;
        uint8_t src2_sens = 0;
        
        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        TAC.push_back(tmp_tac);

        op = Operation::ADD_INPUT_STACK;
        sens = TAC.back().dest_sensitivity;  
        dest_type = TAC_OPERAND::V_REG; 
        dest_v = TAC.back().dest_v_reg_idx;
        dest_sens = TAC.back().dest_sensitivity; 
        src1_type = TAC_OPERAND::V_REG; 
        src1_v = TAC.back().dest_v_reg_idx;
        src1_sens = TAC.back().dest_sensitivity; 
        src2_type = TAC_OPERAND::NUM; 
        src2_v = it->position;
        src2_sens = 0;
        bool add_input_stack_psr_status_flag;

        TAC_type load_number_into_register;
        if(add_specification_armv6m(src2_v, add_input_stack_psr_status_flag, load_number_into_register)){
            src2_type = TAC_OPERAND::V_REG;
            src2_v = load_number_into_register.dest_v_reg_idx;

            TAC.push_back(load_number_into_register);
        }

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, add_input_stack_psr_status_flag);
        TAC.push_back(tmp_tac);

        #endif
    }

    //in both cases we have now the address from where to load
    //perform now actual load
    TAC_type tmp_tac;
    Operation op = Operation::LOADWORD_FROM_INPUT_STACK;
    
    TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
    uint32_t dest_v = v_register_ctr++;
    uint8_t dest_sens = get_sensitivity_from_stack_memory(ssa_add_on, ssa_original_name, virt_var, memory_security.stack_mapping_tracker); 
    TAC_OPERAND src1_type = TAC_OPERAND::V_REG; 
    uint32_t src1_v = TAC.back().dest_v_reg_idx;
    uint8_t src1_sens = return_sensitivity(TAC.back().dest_sensitivity); 
    TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
    uint32_t src2_v = UINT32_MAX;
    uint8_t src2_sens = 0;
    uint8_t sens = dest_sens; 


    //save in virt_var_to_v_reg the mapping between the register and the address of this variable
    std::string original_key = "&" + ssa_original_name;
    std::string ssa_key = "";
    if(ssa_name != ""){
        ssa_key = "&" + ssa_name;
    }
    std::tuple<std::string, std::string> key_tuple = std::make_tuple(original_key, ssa_key);
    virtual_variable_to_v_reg[key_tuple] = TAC.back().dest_v_reg_idx;

    //as we load the actual value into a register we also have to save in virt_var_to_v_reg the mapping between the register and the actual variable
    original_key = ssa_original_name;
    ssa_key = "";
    if(ssa_name != ""){
        ssa_key = ssa_name;
    }
    key_tuple = std::make_tuple(original_key, ssa_key);
    virtual_variable_to_v_reg[key_tuple] = dest_v;

    calling_operation_src_v = dest_v;
    calling_operation_sens = dest_sens;

    generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
    TAC.push_back(tmp_tac);
}


void tac_func_without_return_instr(std::vector<TAC_type>& TAC,ssa ssa_instr, virtual_variables_vec& virtual_variables, std::map<std::string, bool>& presence_of_local_variables_in_virtual_register, std::map<std::string, bool>& presence_of_input_variables_in_virtual_register, memory_security_struct& memory_security){
    function_call_without_return_instruction f_instr = boost::get<function_call_without_return_instruction>(ssa_instr.instr);


    //1. push as much registers as necessary on stack 

    std::vector<std::tuple<bool, uint32_t, uint32_t>> parameter_information; //(is parameter already assigned a virtual variable, what is the virtual variable, real register index)

    for(uint32_t param_idx = 4; param_idx < f_instr.parameters.size(); ++param_idx){
            TAC_type sub_sp_tac;
            Operation sub_sp_op = Operation::SUB;
            uint8_t sub_sp_sens = 0; 
            TAC_OPERAND sub_sp_dest_type = TAC_OPERAND::R_REG; 
            uint32_t sub_sp_dest_v = 13;
            uint8_t sub_sp_dest_sens = 0; 
            TAC_OPERAND sub_sp_src1_type = TAC_OPERAND::R_REG; 
            uint32_t sub_sp_src1_v = 13;
            uint8_t sub_sp_src1_sens = 0; 
            TAC_OPERAND sub_sp_src2_type = TAC_OPERAND::NUM; 
            uint32_t sub_sp_src2_v = 4;
            uint8_t sub_sp_src2_sens = 0;
            bool sub_sp_psr_status_flag = false;

            generate_generic_tmp_tac(sub_sp_tac, sub_sp_op, sub_sp_sens, sub_sp_dest_type, sub_sp_dest_v, sub_sp_dest_sens, sub_sp_src1_type, sub_sp_src1_v, sub_sp_src1_sens, sub_sp_src2_type, sub_sp_src2_v, sub_sp_src2_sens, sub_sp_psr_status_flag);
            TAC.push_back(sub_sp_tac);

            TAC_type place_to_stack_tac;
            Operation place_to_stack_op = Operation::STOREWORD;
            uint8_t place_to_stack_sens; 
            TAC_OPERAND place_to_stack_dest_type = TAC_OPERAND::NONE; 
            uint32_t place_to_stack_dest_v = UINT32_MAX;
            uint8_t place_to_stack_dest_sens = 0; 
            TAC_OPERAND place_to_stack_src1_type; 
            uint32_t place_to_stack_src1_v;
            uint8_t place_to_stack_src1_sens; 
            TAC_OPERAND place_to_stack_src2_type = TAC_OPERAND::R_REG; 
            uint32_t place_to_stack_src2_v = 13;
            uint8_t place_to_stack_src2_sens = 0;

            //move number to register to be able to store value at all
            if(f_instr.parameters.at(param_idx).op.op_type == DATA_TYPE::NUM){ //simply move number to corresponding register

                TAC_type move_number_to_reg_tac;
                Operation move_number_to_reg_op = Operation::MOV;
                uint8_t move_number_to_reg_sens = 0; 
                TAC_OPERAND move_number_to_reg_dest_type = TAC_OPERAND::V_REG; 
                uint32_t move_number_to_reg_dest_v = v_register_ctr++;
                uint8_t move_number_to_reg_dest_sens = 0; 
                TAC_OPERAND move_number_to_reg_src1_type = TAC_OPERAND::NUM; 
                uint32_t move_number_to_reg_src1_v = f_instr.parameters.at(param_idx).op.op_data;
                uint8_t move_number_to_reg_src1_sens = 0; 
                TAC_OPERAND move_number_to_reg_src2_type = TAC_OPERAND::NONE; 
                uint32_t move_number_to_reg_src2_v = UINT32_MAX;
                uint8_t move_number_to_reg_src2_sens = 0;
                bool move_number_to_reg_psr_status_flag;

                #ifdef ARMV6_M
                move_specification_armv6m(move_number_to_reg_op, move_number_to_reg_src1_v, move_number_to_reg_psr_status_flag);
                #endif

                generate_generic_tmp_tac(move_number_to_reg_tac, move_number_to_reg_op, move_number_to_reg_sens, move_number_to_reg_dest_type, move_number_to_reg_dest_v, move_number_to_reg_dest_sens, move_number_to_reg_src1_type, move_number_to_reg_src1_v, move_number_to_reg_src1_sens, move_number_to_reg_src2_type, move_number_to_reg_src2_v, move_number_to_reg_src2_sens, move_number_to_reg_psr_status_flag);

                TAC.push_back(move_number_to_reg_tac);

                place_to_stack_sens = 0;
                place_to_stack_src1_type = TAC_OPERAND::V_REG;
                place_to_stack_src1_v = TAC.back().dest_v_reg_idx;
                place_to_stack_src1_sens = 0;


                generate_generic_tmp_tac(place_to_stack_tac, place_to_stack_op, place_to_stack_sens, place_to_stack_dest_type, place_to_stack_dest_v, place_to_stack_dest_sens, place_to_stack_src1_type, place_to_stack_src1_v, place_to_stack_src1_sens, place_to_stack_src2_type, place_to_stack_src2_v, place_to_stack_src2_sens, false);
                TAC.push_back(place_to_stack_tac);


            }
            else{
                if(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on == SSA_ADD_ON::NO_ADD_ON){
                    std::string param_original_name = generate_original_name(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), virtual_variables);
                    std::string param_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data));
                    if(is_input(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data)) || is_virtual(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data)) || is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
        
                        if(variable_assigned_to_register(param_original_name, param_ssa_name)){
                            std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);
                            place_to_stack_src1_type = TAC_OPERAND::V_REG;
                            place_to_stack_src1_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                            place_to_stack_src1_sens = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);
                            if(place_to_stack_src1_sens == 1){
                                place_to_stack_sens = 1;
                            }
                            generate_generic_tmp_tac(place_to_stack_tac, place_to_stack_op, place_to_stack_sens, place_to_stack_dest_type, place_to_stack_dest_v, place_to_stack_dest_sens, place_to_stack_src1_type, place_to_stack_src1_v, place_to_stack_src1_sens, place_to_stack_src2_type, place_to_stack_src2_v, place_to_stack_src2_sens, false);
                            TAC.push_back(place_to_stack_tac);
                        }
                        else{
                            if(is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
                                if(presence_of_local_variables_in_virtual_register[param_original_name] == true){ //there was this local variable previously in register -> removed bc value in memory changedd -> load local variable with updated value from memory
                                    //load from memory
                                    load_local_from_memory(TAC, place_to_stack_src1_v, place_to_stack_src1_sens, virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_original_name, param_ssa_name, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security);
                                    presence_of_local_variables_in_virtual_register[param_original_name] = false;
                                }
                                else{
                                    std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);
                                    virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                                    place_to_stack_src1_v = v_register_ctr;
                                    place_to_stack_src1_sens = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);
                                    if(place_to_stack_src1_sens == 1){
                                        place_to_stack_sens = 1;
                                    }
                                    v_register_ctr++;
                                }
                                generate_generic_tmp_tac(place_to_stack_tac, place_to_stack_op, place_to_stack_sens, place_to_stack_dest_type, place_to_stack_dest_v, place_to_stack_dest_sens, place_to_stack_src1_type, place_to_stack_src1_v, place_to_stack_src1_sens, place_to_stack_src2_type, place_to_stack_src2_v, place_to_stack_src2_sens, false);
                                TAC.push_back(place_to_stack_tac);
                            }
                            else if(is_input(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
                                if(presence_of_input_variables_in_virtual_register[param_original_name] == true){
                                    load_local_from_memory(TAC, place_to_stack_src1_v, place_to_stack_src1_sens, virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_original_name, param_ssa_name, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security);
                                    presence_of_input_variables_in_virtual_register[param_original_name] = false;
                                }
                                else{
                                    load_input_from_memory(TAC, place_to_stack_src1_v, place_to_stack_src1_sens, virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_original_name, param_ssa_name, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security);
                                }
                                if(place_to_stack_src1_sens == 1){
                                    place_to_stack_sens = 1;
                                }
                                generate_generic_tmp_tac(place_to_stack_tac, place_to_stack_op, place_to_stack_sens, place_to_stack_dest_type, place_to_stack_dest_v, place_to_stack_dest_sens, place_to_stack_src1_type, place_to_stack_src1_v, place_to_stack_src1_sens, place_to_stack_src2_type, place_to_stack_src2_v, place_to_stack_src2_sens, false);
                                TAC.push_back(place_to_stack_tac);
                            }
                            else{
                                std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);
                                virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                                place_to_stack_src1_v = v_register_ctr;
                                place_to_stack_src1_sens = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);
                                v_register_ctr++;
                                generate_generic_tmp_tac(place_to_stack_tac, place_to_stack_op, place_to_stack_sens, place_to_stack_dest_type, place_to_stack_dest_v, place_to_stack_dest_sens, place_to_stack_src1_type, place_to_stack_src1_v, place_to_stack_src1_sens, place_to_stack_src2_type, place_to_stack_src2_v, place_to_stack_src2_sens, false);
                                TAC.push_back(place_to_stack_tac);
                            }

                        }
                    }
                    else{
                        throw std::runtime_error("in tac_func_without_return_instr: src1 is not input, local or virtual");
                    }
                }
                else if(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on == SSA_ADD_ON::AMP_ADD_ON){
                    std::string param_original_name = generate_original_name(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), virtual_variables);
                    std::string param_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data));
                    if(is_input(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data)) || is_virtual(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data)) || is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){

                        if(variable_assigned_to_register(param_original_name, param_ssa_name)){
                            std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);
                            place_to_stack_src1_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                            place_to_stack_src1_sens = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);

                            //if input to variable is &s and can be found in virtual register -> still need to remove s from virtual_variable_to_v_reg mapping as parameter is passed by reference -> could potentiall alter the value of s on stack address. if after function call s is used -> has to be loaded from memory first
                            if(is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
                                std::string param_without_amp_original = param_original_name.substr(1, param_original_name.length() - 1);
                                std::string param_without_amp_ssa = ""; //as parameter is local without array information -> second value of key is empty
                                key_tuple = std::make_tuple(param_without_amp_original, param_without_amp_ssa);
                                virtual_variable_to_v_reg.erase(key_tuple);
                                presence_of_local_variables_in_virtual_register[param_without_amp_original] = true;
                            }
                            else{
                                throw std::runtime_error("in tac_func_without_return: can not handle &_virtual_variable as parameter yet");

                            }
                        }
                        else{
                            if(is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
                                //if input to variable is &s -> remove s from virtual_variable_to_v_reg mapping as parameter is passed by reference -> could potentiall alter the value of s on stack address. if after function call s is used -> has to be loaded from memory first

                                std::string param_without_amp_original = param_original_name.substr(1, param_original_name.length() - 1);
                                std::string param_without_amp_ssa = ""; //as parameter is local without array information -> second value of key is empty
                                std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_without_amp_original, param_without_amp_ssa);

                                //check if variable is assigned to register -> if yes have to store current value to memory
                                if(virtual_variable_to_v_reg.find(key_tuple) != virtual_variable_to_v_reg.end()){
                                    //store_local_to_memory(); //TODO:
                                    virtual_variable_to_v_reg.erase(key_tuple);
                                    
                                }
                                presence_of_local_variables_in_virtual_register[param_without_amp_original] = true;
                                
                                SSA_ADD_ON ssa_for_local_address = SSA_ADD_ON::NO_ADD_ON;
                                get_local_address(TAC, place_to_stack_src1_v, place_to_stack_src1_sens, virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_without_amp_original, param_without_amp_ssa, ssa_for_local_address, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security, false, ssa_instr);
                            }
                            else{
                                throw std::runtime_error("in tac_func_without_return: can not handle &_virtual_variable as parameter yet");

                            }

                        }

                        generate_generic_tmp_tac(place_to_stack_tac, place_to_stack_op, place_to_stack_sens, place_to_stack_dest_type, place_to_stack_dest_v, place_to_stack_dest_sens, place_to_stack_src1_type, place_to_stack_src1_v, place_to_stack_src1_sens, place_to_stack_src2_type, place_to_stack_src2_v, place_to_stack_src2_sens, false);
                        TAC.push_back(place_to_stack_tac);
                    }
                    else{
                        throw std::runtime_error("in tac_normal_instr: src1 is not input, local or virtual");
                    }
                }

            }
    }



    //2.check if parameter was previously used -> if yes move content from virtual register into real register
    std::vector<std::tuple<uint32_t, uint8_t>> virtual_register_parameters(4); //(virtual register number, sensitivity of virtual register)
    for(uint32_t param_idx = 0; param_idx < f_instr.parameters.size(); ++param_idx){
        if(param_idx < 4){


            if(f_instr.parameters.at(param_idx).op.op_type == DATA_TYPE::NUM){ 
            }
            else{ //data is not a number

                //special care needs to be taken when parameter has amp in front -> s = 4; func(&s); a = s + 1; -> can not take "old" s but have to load updated value from stack
                //additionally we have to declare s previously present in register, otherwise would later instructions assume s was never used and simply assign a new register to it
                if(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on == SSA_ADD_ON::AMP_ADD_ON){
                    std::string param_original_name = generate_original_name(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), virtual_variables);
                    std::string param_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data));
                    if(is_input(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data)) || is_virtual(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data)) || is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){

                        if(variable_assigned_to_register(param_original_name, param_ssa_name)){
                            std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);


                            std::get<0>(virtual_register_parameters.at(param_idx)) = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                            std::get<1>(virtual_register_parameters.at(param_idx)) = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);

                            //if input to variable is &s and can be found in virtual register -> still need to remove s from virtual_variable_to_v_reg mapping as parameter is passed by reference -> could potentiall alter the value of s on stack address. if after function call s is used -> has to be loaded from memory first
                            if(is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
                                std::string param_without_amp_original = param_original_name.substr(1, param_original_name.length() - 1);
                                std::string param_without_amp_ssa = ""; //as parameter is local without array information -> second value of key is empty
                                key_tuple = std::make_tuple(param_without_amp_original, param_without_amp_ssa);
                                virtual_variable_to_v_reg.erase(key_tuple);
                                presence_of_local_variables_in_virtual_register[param_without_amp_original] = true;
                            }
                            else{
                                throw std::runtime_error("in tac_func_without_return: can not handle &_virtual_variable as parameter yet");

                            }
                        }
                        else{
                            if(is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
                                //if input to variable is &s -> remove s from virtual_variable_to_v_reg mapping as parameter is passed by reference -> could potentiall alter the value of s on stack address. if after function call s is used -> has to be loaded from memory first

                                std::string param_without_amp_original = param_original_name.substr(1, param_original_name.length() - 1);
                                std::string param_without_amp_ssa = ""; //as parameter is local without array information -> second value of key is empty
                                std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_without_amp_original, param_without_amp_ssa);

                                //check if variable is assigned to register -> if yes have to store current value to memory
                                if(virtual_variable_to_v_reg.find(key_tuple) != virtual_variable_to_v_reg.end()){
                                    virtual_variable_to_v_reg.erase(key_tuple);
                                    
                                }
                                presence_of_local_variables_in_virtual_register[param_without_amp_original] = true;
                                
                                SSA_ADD_ON ssa_for_local_address = SSA_ADD_ON::NO_ADD_ON;
                                get_local_address(TAC, std::get<0>(virtual_register_parameters.at(param_idx)), std::get<1>(virtual_register_parameters.at(param_idx)), virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_without_amp_original, param_without_amp_ssa, ssa_for_local_address, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security, false, ssa_instr);
                            }
                            else{
                                throw std::runtime_error("in tac_func_without_return: can not handle &_virtual_variable as parameter yet");

                            }

                        }
                    }
                    else{
                        throw std::runtime_error("in tac_normal_instr: src1 is not input, local or virtual");
                    }
                }
                else if(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on == SSA_ADD_ON::NO_ADD_ON){
                    std::string param_original_name = generate_original_name(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), virtual_variables);
                    std::string param_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data));
                    if(is_input(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data)) || is_virtual(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data)) || is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
        
                        if(variable_assigned_to_register(param_original_name, param_ssa_name)){
                            std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);
                            std::get<0>(virtual_register_parameters.at(param_idx)) = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                            std::get<1>(virtual_register_parameters.at(param_idx)) = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);
                        }
                        else{
                            if(is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
                                if(presence_of_local_variables_in_virtual_register[param_original_name] == true){ //there was this local variable previously in register -> removed bc value in memory changedd -> load local variable with updated value from memory
                                    //load from memory
                                    load_local_from_memory(TAC, std::get<0>(virtual_register_parameters.at(param_idx)), std::get<1>(virtual_register_parameters.at(param_idx)), virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_original_name, param_ssa_name, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security);
                                    presence_of_local_variables_in_virtual_register[param_original_name] = false;
                                }
                                else{
                                    std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);
                                    virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                                    std::get<0>(virtual_register_parameters.at(param_idx)) = v_register_ctr;
                                    std::get<1>(virtual_register_parameters.at(param_idx)) = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);
                                    v_register_ctr++;
                                }
                            }
                            else if(is_input(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
                                if(presence_of_input_variables_in_virtual_register[param_original_name] == true){
                                    load_local_from_memory(TAC, std::get<0>(virtual_register_parameters.at(param_idx)), std::get<1>(virtual_register_parameters.at(param_idx)), virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_original_name, param_ssa_name, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security);
                                }
                                else{
                                    load_input_from_memory(TAC, std::get<0>(virtual_register_parameters.at(param_idx)), std::get<1>(virtual_register_parameters.at(param_idx)), virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_original_name, param_ssa_name, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security);
                                }
                            }
                            else{
                                std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);
                                virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                                std::get<0>(virtual_register_parameters.at(param_idx)) = v_register_ctr;
                                std::get<1>(virtual_register_parameters.at(param_idx)) = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);
                                v_register_ctr++;
                            }

                        }
                    }
                    else{
                        throw std::runtime_error("in tac_normal_instr: src1 is not input, local or virtual");
                    }
                }
                else{
                    throw std::runtime_error("in tac_func_without_return_instr: parameter is neither &var or var. Can not handle currently");
                }

            }

        }
        else{

        }
    }


    for(uint32_t param_idx = 0; param_idx < f_instr.parameters.size(); ++param_idx){
        if(param_idx < 4){

            TAC_type tmp_tac;
            Operation op = Operation::FUNC_MOVE;
            uint8_t sens = 0; 
            TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
            uint32_t dest_v = param_idx;
            uint8_t dest_sens = 0; 
            TAC_OPERAND src1_type; 
            uint32_t src1_v; 
            uint8_t src1_sens; 
            TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
            uint32_t src2_v = UINT32_MAX;
            uint8_t src2_sens = 0;
            bool psr_status_flag;

            if(f_instr.parameters.at(param_idx).op.op_type == DATA_TYPE::NUM){ //simply move number to corresponding register

                src1_type = TAC_OPERAND::NUM; 
                src1_v = f_instr.parameters.at(param_idx).op.op_data;
                src1_sens = 0; 

                if(src1_v > 255){
                    psr_status_flag = false;
                }
                else{
                    psr_status_flag = true;
                }

                generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                TAC.push_back(tmp_tac);
            }
            else{
                src1_type = TAC_OPERAND::V_REG; 
                src1_v = std::get<0>(virtual_register_parameters.at(param_idx));
                src1_sens = std::get<1>(virtual_register_parameters.at(param_idx)); 

                generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
                TAC.push_back(tmp_tac);
            }

            TAC.back().func_move_idx = param_idx;
            TAC.back().func_instr_number = TAC.size() + (4 - param_idx);
            
        }
    }

    //insert special tac operation: if this virtual register will be handled by the linear scan algorithm we have a border between pre- and post-function call -> this allows us to check for sensitive registers that have not been desanitized
    v_function_borders++;
    TAC_type function_border_tac;
    Operation function_border_op = Operation::MOV;
    uint8_t function_border_sens = 0; 
    TAC_OPERAND function_border_dest_type = TAC_OPERAND::V_REG; 
    uint32_t function_border_dest_v = UINT32_MAX - v_function_borders;
    uint8_t function_border_dest_sens = 0; 
    TAC_OPERAND function_border_src1_type = TAC_OPERAND::NUM; 
    uint32_t function_border_src1_v = UINT32_MAX - v_function_borders; 
    uint8_t function_border_src1_sens = 0; 
    TAC_OPERAND function_border_src2_type = TAC_OPERAND::NONE; 
    uint32_t function_border_src2_v = UINT32_MAX;
    uint8_t function_border_src2_sens = 0;
    
    generate_generic_tmp_tac(function_border_tac, function_border_op, function_border_sens, function_border_dest_type, function_border_dest_v, function_border_dest_sens, function_border_src1_type, function_border_src1_v, function_border_src1_sens, function_border_src2_type, function_border_src2_v, function_border_src2_sens, false);
    TAC.push_back(function_border_tac);


    //3. call function
    TAC_type tmp_tac;
    tmp_tac.op = Operation::FUNC_CALL;
    tmp_tac.function_call_name = f_instr.func_name;
    tmp_tac.function_arg_num = f_instr.parameters.size();
    if(tmp_tac.function_arg_num > 4){
        tmp_tac.function_arg_num = 4;
    }
    tmp_tac.src1_type = TAC_OPERAND::NONE;
    tmp_tac.src2_type = TAC_OPERAND::NONE;
    tmp_tac.dest_type = TAC_OPERAND::NONE;
    tmp_tac.sensitivity = 0;
    TAC.push_back(tmp_tac);

    //restore original stack size if number of parameters is greater than 4
    if(f_instr.parameters.size() > 4){
        TAC_type restore_stack_tac;
        Operation restore_stack_op = Operation::ADD;
        uint8_t restore_stack_sens = 0; 
        TAC_OPERAND restore_stack_dest_type = TAC_OPERAND::R_REG; 
        uint32_t restore_stack_dest_v = 13;
        uint8_t restore_stack_dest_sens = 0; 
        TAC_OPERAND restore_stack_src1_type = TAC_OPERAND::R_REG; 
        uint32_t restore_stack_src1_v = 13;
        uint8_t restore_stack_src1_sens = 0; 
        TAC_OPERAND restore_stack_src2_type = TAC_OPERAND::NUM; 
        uint32_t restore_stack_src2_v = (f_instr.parameters.size() - 4) * 4;
        uint8_t restore_stack_src2_sens = 0;
        bool add_psr_status_flag;

        TAC_type load_number_into_register;
        if(add_specification_armv6m(restore_stack_src2_v, add_psr_status_flag, load_number_into_register)){
            restore_stack_src2_type = TAC_OPERAND::V_REG;
            restore_stack_src2_v = load_number_into_register.dest_v_reg_idx;

            TAC.push_back(load_number_into_register);
        }

        generate_generic_tmp_tac(restore_stack_tac, restore_stack_op, restore_stack_sens, restore_stack_dest_type, restore_stack_dest_v, restore_stack_dest_sens, restore_stack_src1_type, restore_stack_src1_v, restore_stack_src1_sens, restore_stack_src2_type, restore_stack_src2_v, restore_stack_src2_sens, add_psr_status_flag);
        TAC.push_back(restore_stack_tac);
    }

}


void tac_func_with_return_instr(std::vector<TAC_type>& TAC,ssa ssa_instr, virtual_variables_vec& virtual_variables, std::map<std::string, bool>& presence_of_local_variables_in_virtual_register, std::map<std::string, bool>& presence_of_input_variables_in_virtual_register, memory_security_struct& memory_security){
    function_call_with_return_instruction f_instr = boost::get<function_call_with_return_instruction>(ssa_instr.instr);


    std::vector<std::tuple<bool, uint32_t, uint32_t>> parameter_information; //(is parameter already assigned a virtual variable, what is the virtual variable, real register index)

    for(uint32_t param_idx = 4; param_idx < f_instr.parameters.size(); ++param_idx){
            TAC_type sub_sp_tac;
            Operation sub_sp_op = Operation::SUB;
            uint8_t sub_sp_sens = 0; 
            TAC_OPERAND sub_sp_dest_type = TAC_OPERAND::R_REG; 
            uint32_t sub_sp_dest_v = 13;
            uint8_t sub_sp_dest_sens = 0; 
            TAC_OPERAND sub_sp_src1_type = TAC_OPERAND::R_REG; 
            uint32_t sub_sp_src1_v = 13;
            uint8_t sub_sp_src1_sens = 0; 
            TAC_OPERAND sub_sp_src2_type = TAC_OPERAND::NUM; 
            uint32_t sub_sp_src2_v = 4;
            uint8_t sub_sp_src2_sens = 0;
            bool sub_sp_psr_status_flag = false;

            generate_generic_tmp_tac(sub_sp_tac, sub_sp_op, sub_sp_sens, sub_sp_dest_type, sub_sp_dest_v, sub_sp_dest_sens, sub_sp_src1_type, sub_sp_src1_v, sub_sp_src1_sens, sub_sp_src2_type, sub_sp_src2_v, sub_sp_src2_sens, sub_sp_psr_status_flag);
            TAC.push_back(sub_sp_tac);

            TAC_type place_to_stack_tac;
            Operation place_to_stack_op = Operation::STOREWORD;
            uint8_t place_to_stack_sens; 
            TAC_OPERAND place_to_stack_dest_type = TAC_OPERAND::NONE; 
            uint32_t place_to_stack_dest_v = UINT32_MAX;
            uint8_t place_to_stack_dest_sens = 0; 
            TAC_OPERAND place_to_stack_src1_type; 
            uint32_t place_to_stack_src1_v;
            uint8_t place_to_stack_src1_sens; 
            TAC_OPERAND place_to_stack_src2_type = TAC_OPERAND::R_REG; 
            uint32_t place_to_stack_src2_v = 13;
            uint8_t place_to_stack_src2_sens = 0;

            //move number to register to be able to store value at all
            if(f_instr.parameters.at(param_idx).op.op_type == DATA_TYPE::NUM){ //simply move number to corresponding register

                TAC_type move_number_to_reg_tac;
                Operation move_number_to_reg_op = Operation::MOV;
                uint8_t move_number_to_reg_sens = 0; 
                TAC_OPERAND move_number_to_reg_dest_type = TAC_OPERAND::V_REG; 
                uint32_t move_number_to_reg_dest_v = v_register_ctr++;
                uint8_t move_number_to_reg_dest_sens = 0; 
                TAC_OPERAND move_number_to_reg_src1_type = TAC_OPERAND::NUM; 
                uint32_t move_number_to_reg_src1_v = f_instr.parameters.at(param_idx).op.op_data;
                uint8_t move_number_to_reg_src1_sens = 0; 
                TAC_OPERAND move_number_to_reg_src2_type = TAC_OPERAND::NONE; 
                uint32_t move_number_to_reg_src2_v = UINT32_MAX;
                uint8_t move_number_to_reg_src2_sens = 0;
                bool move_number_to_reg_psr_status_flag;

                #ifdef ARMV6_M
                move_specification_armv6m(move_number_to_reg_op, move_number_to_reg_src1_v, move_number_to_reg_psr_status_flag);

                #endif

                generate_generic_tmp_tac(move_number_to_reg_tac, move_number_to_reg_op, move_number_to_reg_sens, move_number_to_reg_dest_type, move_number_to_reg_dest_v, move_number_to_reg_dest_sens, move_number_to_reg_src1_type, move_number_to_reg_src1_v, move_number_to_reg_src1_sens, move_number_to_reg_src2_type, move_number_to_reg_src2_v, move_number_to_reg_src2_sens, move_number_to_reg_psr_status_flag);

                TAC.push_back(move_number_to_reg_tac);

                place_to_stack_sens = 0;
                place_to_stack_src1_type = TAC_OPERAND::V_REG;
                place_to_stack_src1_v = TAC.back().dest_v_reg_idx;
                place_to_stack_src1_sens = 0;


                generate_generic_tmp_tac(place_to_stack_tac, place_to_stack_op, place_to_stack_sens, place_to_stack_dest_type, place_to_stack_dest_v, place_to_stack_dest_sens, place_to_stack_src1_type, place_to_stack_src1_v, place_to_stack_src1_sens, place_to_stack_src2_type, place_to_stack_src2_v, place_to_stack_src2_sens, false);
                TAC.push_back(place_to_stack_tac);


            }
            else{
                if(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on == SSA_ADD_ON::NO_ADD_ON){
                    std::string param_original_name = generate_original_name(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), virtual_variables);
                    std::string param_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data));
                    if(is_input(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data)) || is_virtual(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data)) || is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
        
                        if(variable_assigned_to_register(param_original_name, param_ssa_name)){
                            std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);
                            place_to_stack_src1_type = TAC_OPERAND::V_REG;
                            place_to_stack_src1_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                            place_to_stack_src1_sens = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);
                            if(place_to_stack_src1_sens == 1){
                                place_to_stack_sens = 1;
                            }
                            generate_generic_tmp_tac(place_to_stack_tac, place_to_stack_op, place_to_stack_sens, place_to_stack_dest_type, place_to_stack_dest_v, place_to_stack_dest_sens, place_to_stack_src1_type, place_to_stack_src1_v, place_to_stack_src1_sens, place_to_stack_src2_type, place_to_stack_src2_v, place_to_stack_src2_sens, false);
                            TAC.push_back(place_to_stack_tac);
                        }
                        else{
                            if(is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
                                if(presence_of_local_variables_in_virtual_register[param_original_name] == true){ //there was this local variable previously in register -> removed bc value in memory changedd -> load local variable with updated value from memory
                                    //load from memory
                                    load_local_from_memory(TAC, place_to_stack_src1_v, place_to_stack_src1_sens, virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_original_name, param_ssa_name, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security);
                                    presence_of_local_variables_in_virtual_register[param_original_name] = false;
                                }
                                else{
                                    std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);
                                    virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                                    place_to_stack_src1_v = v_register_ctr;
                                    place_to_stack_src1_sens = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);
                                    if(place_to_stack_src1_sens == 1){
                                        place_to_stack_sens = 1;
                                    }
                                    v_register_ctr++;
                                }
                                generate_generic_tmp_tac(place_to_stack_tac, place_to_stack_op, place_to_stack_sens, place_to_stack_dest_type, place_to_stack_dest_v, place_to_stack_dest_sens, place_to_stack_src1_type, place_to_stack_src1_v, place_to_stack_src1_sens, place_to_stack_src2_type, place_to_stack_src2_v, place_to_stack_src2_sens, false);
                                TAC.push_back(place_to_stack_tac);
                            }
                            else if(is_input(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
                                if(presence_of_input_variables_in_virtual_register[param_original_name] == true){
                                    load_local_from_memory(TAC, place_to_stack_src1_v, place_to_stack_src1_sens, virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_original_name, param_ssa_name, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security);
                                    presence_of_input_variables_in_virtual_register[param_original_name] = false;
                                }
                                else{
                                    load_input_from_memory(TAC, place_to_stack_src1_v, place_to_stack_src1_sens, virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_original_name, param_ssa_name, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security);
                                }
                                if(place_to_stack_src1_sens == 1){
                                    place_to_stack_sens = 1;
                                }
                                generate_generic_tmp_tac(place_to_stack_tac, place_to_stack_op, place_to_stack_sens, place_to_stack_dest_type, place_to_stack_dest_v, place_to_stack_dest_sens, place_to_stack_src1_type, place_to_stack_src1_v, place_to_stack_src1_sens, place_to_stack_src2_type, place_to_stack_src2_v, place_to_stack_src2_sens, false);
                                TAC.push_back(place_to_stack_tac);
                            }
                            else{
                                std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);
                                virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                                place_to_stack_src1_v = v_register_ctr;
                                place_to_stack_src1_sens = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);
                                v_register_ctr++;
                                generate_generic_tmp_tac(place_to_stack_tac, place_to_stack_op, place_to_stack_sens, place_to_stack_dest_type, place_to_stack_dest_v, place_to_stack_dest_sens, place_to_stack_src1_type, place_to_stack_src1_v, place_to_stack_src1_sens, place_to_stack_src2_type, place_to_stack_src2_v, place_to_stack_src2_sens, false);
                                TAC.push_back(place_to_stack_tac);
                            }

                        }
                    }
                    else{
                        throw std::runtime_error("in tac_normal_instr: src1 is not input, local or virtual");
                    }
                }


            }
    }


    //2.check if parameter was previously used -> if yes move content from virtual register into real register
    std::vector<std::tuple<uint32_t, uint8_t, uint8_t>> virtual_register_parameters(4); //(register number, sensitivity of virtual register, virtual or real register)

    for(uint32_t param_idx = 0; param_idx < f_instr.parameters.size(); ++param_idx){
        if(param_idx < 4){


            if(f_instr.parameters.at(param_idx).op.op_type == DATA_TYPE::NUM){ //simply move number to corresponding register

            }
            else{ //data is not a number
                //special care needs to be taken when parameter has amp in front -> s = 4; func(&s); a = s + 1; -> can not take "old" s but have to load updated value from stack
                //additionally we have to declare s previously present in register, otherwise would later instructions assume s was never used and simply assign a new register to it
                if(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on == SSA_ADD_ON::AMP_ADD_ON){
                    std::string param_original_name = generate_original_name(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), virtual_variables);
                    std::string param_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data));
                    if(is_input(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data)) || is_virtual(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data)) || is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){

                        if(variable_assigned_to_register(param_original_name, param_ssa_name)){
                            std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);
                            std::get<0>(virtual_register_parameters.at(param_idx)) = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                            std::get<1>(virtual_register_parameters.at(param_idx)) = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);

                            //if input to variable is &s and can be found in virtual register -> still need to remove s from virtual_variable_to_v_reg mapping as parameter is passed by reference -> could potentiall alter the value of s on stack address. if after function call s is used -> has to be loaded from memory first
                            if(is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
                                std::string param_without_amp_original = param_original_name.substr(1, param_original_name.length() - 1);
                                std::string param_without_amp_ssa = ""; //as parameter is local without array information -> second value of key is empty
                                key_tuple = std::make_tuple(param_without_amp_original, param_without_amp_ssa);
                                virtual_variable_to_v_reg.erase(key_tuple);
                                presence_of_local_variables_in_virtual_register[param_without_amp_original] = true;
                            }
                            else{
                                throw std::runtime_error("in tac_func_without_return: can not handle &_virtual_variable as parameter yet");

                            }
                        }
                        else{
                            if(is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
                                //if input to variable is &s -> remove s from virtual_variable_to_v_reg mapping as parameter is passed by reference -> could potentiall alter the value of s on stack address. if after function call s is used -> has to be loaded from memory first

                                std::string param_without_amp_original = param_original_name.substr(1, param_original_name.length() - 1);
                                std::string param_without_amp_ssa = ""; //as parameter is local without array information -> second value of key is empty
                                std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_without_amp_original, param_without_amp_ssa);

                                //check if variable is assigned to register -> if yes have to store current value to memory
                                if(virtual_variable_to_v_reg.find(key_tuple) != virtual_variable_to_v_reg.end()){
                                    //store_local_to_memory(); //TODO:
                                    virtual_variable_to_v_reg.erase(key_tuple);
                                    
                                }
                                presence_of_local_variables_in_virtual_register[param_without_amp_original] = true;
                                
                                SSA_ADD_ON ssa_for_local_address = SSA_ADD_ON::NO_ADD_ON;
                                get_local_address(TAC, std::get<0>(virtual_register_parameters.at(param_idx)), std::get<1>(virtual_register_parameters.at(param_idx)), virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_without_amp_original, param_without_amp_ssa, ssa_for_local_address, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security, false, ssa_instr);
                            }

                            else{
                                throw std::runtime_error("in tac_func_without_return: can not handle &_virtual_variable as parameter yet");

                            }

                        }
                    }
                    else{
                        throw std::runtime_error("in tac_normal_instr: src1 is not input, local or virtual");
                    }
                }
                else if(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on == SSA_ADD_ON::NO_ADD_ON){
                    std::string param_original_name = generate_original_name(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), virtual_variables);
                    std::string param_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data));
                    if(is_input(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data)) || is_virtual(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data)) || is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
        
                        if(variable_assigned_to_register(param_original_name, param_ssa_name)){
                            std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);
                            std::get<0>(virtual_register_parameters.at(param_idx)) = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                            std::get<1>(virtual_register_parameters.at(param_idx)) = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);
                        }
                        else{
                            if(is_local(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
                                if(presence_of_local_variables_in_virtual_register[param_original_name] == true){ //there was this local variable previously in register -> removed bc value in memory changedd -> load local variable with updated value from memory
                                    //load from memory
                                    load_local_from_memory(TAC, std::get<0>(virtual_register_parameters.at(param_idx)), std::get<1>(virtual_register_parameters.at(param_idx)), virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_original_name, param_ssa_name, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security);
                                    presence_of_local_variables_in_virtual_register[param_original_name] = false;
                                }
                                else{
                                    std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);
                                    virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                                    std::get<0>(virtual_register_parameters.at(param_idx)) = v_register_ctr;
                                    std::get<1>(virtual_register_parameters.at(param_idx)) = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);
                                    v_register_ctr++;
                                }
                            }
                            else if(is_input(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data))){
                                if(presence_of_input_variables_in_virtual_register[param_original_name] == true){
                                    load_local_from_memory(TAC, std::get<0>(virtual_register_parameters.at(param_idx)), std::get<1>(virtual_register_parameters.at(param_idx)), virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_original_name, param_ssa_name, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security);
                                }
                                else{
                                    load_input_from_memory(TAC, std::get<0>(virtual_register_parameters.at(param_idx)), std::get<1>(virtual_register_parameters.at(param_idx)), virtual_variables, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data), param_original_name, param_ssa_name, virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).ssa_add_on, memory_security);
                                }
                            }
                            else{
                                std::tuple<std::string, std::string> key_tuple = std::make_tuple(param_original_name, param_ssa_name);
                                virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                                std::get<0>(virtual_register_parameters.at(param_idx)) = v_register_ctr;
                                std::get<1>(virtual_register_parameters.at(param_idx)) = return_sensitivity(virtual_variables.at(f_instr.parameters.at(param_idx).op.op_data).sensitivity);
                                v_register_ctr++;
                            }

                        }
                    }
                    else{
                        throw std::runtime_error("in tac_normal_instr: src1 is not input, local or virtual");
                    }
                }
                else{
                    throw std::runtime_error("in tac_func_without_return_instr: parameter is neither &var or var. Can not handle currently");
                }

            }

        }
        else{

        }

    }

    for(uint32_t param_idx = 0; param_idx < f_instr.parameters.size(); ++param_idx){
        if(param_idx < 4){

            TAC_type tmp_tac;
            Operation op = Operation::FUNC_MOVE;
            uint8_t sens = 0; 
            TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
            uint32_t dest_v = param_idx;
            uint8_t dest_sens = 0; 
            TAC_OPERAND src1_type; 
            uint32_t src1_v; 
            uint8_t src1_sens; 
            TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
            uint32_t src2_v = UINT32_MAX;
            uint8_t src2_sens = 0;
            bool psr_status_flag;

            if(f_instr.parameters.at(param_idx).op.op_type == DATA_TYPE::NUM){ //simply move number to corresponding register

                src1_type = TAC_OPERAND::NUM; 
                src1_v = f_instr.parameters.at(param_idx).op.op_data;
                src1_sens = 0; 

                if(src1_v > 255){
                    psr_status_flag = false;
                }
                else{
                    psr_status_flag = true;
                }

                generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
                TAC.push_back(tmp_tac);
            }
            else{
                src1_type = TAC_OPERAND::V_REG; 
                src1_v = std::get<0>(virtual_register_parameters.at(param_idx));
                src1_sens = std::get<1>(virtual_register_parameters.at(param_idx)); 

                generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
                TAC.push_back(tmp_tac);
            }

            TAC.back().func_move_idx = param_idx;
            TAC.back().func_instr_number = TAC.size() + (4 - param_idx);
            
        }
    }



    //insert special tac operation: if this virtual register will be handled by the linear scan algorithm we have a border between pre- and post-function call -> this allows us to check for sensitive registers that have not been desanitized
    v_function_borders++;
    TAC_type function_border_tac;
    Operation function_border_op = Operation::MOV;
    uint8_t function_border_sens = 0; 
    TAC_OPERAND function_border_dest_type = TAC_OPERAND::V_REG; 
    uint32_t function_border_dest_v = UINT32_MAX - v_function_borders;
    uint8_t function_border_dest_sens = 0; 
    TAC_OPERAND function_border_src1_type = TAC_OPERAND::NUM; 
    uint32_t function_border_src1_v = UINT32_MAX - v_function_borders; 
    uint8_t function_border_src1_sens = 0; 
    TAC_OPERAND function_border_src2_type = TAC_OPERAND::NONE; 
    uint32_t function_border_src2_v = UINT32_MAX;
    uint8_t function_border_src2_sens = 0;
    
    generate_generic_tmp_tac(function_border_tac, function_border_op, function_border_sens, function_border_dest_type, function_border_dest_v, function_border_dest_sens, function_border_src1_type, function_border_src1_v, function_border_src1_sens, function_border_src2_type, function_border_src2_v, function_border_src2_sens, false);
    TAC.push_back(function_border_tac);



    //3. call function
    TAC_type tmp_tac;
    tmp_tac.op = Operation::FUNC_CALL;
    tmp_tac.function_call_name = f_instr.func_name;
    tmp_tac.function_arg_num = f_instr.parameters.size();
    if(tmp_tac.function_arg_num > 4){
        tmp_tac.function_arg_num = 4;
    }
    tmp_tac.src1_type = TAC_OPERAND::NONE;
    tmp_tac.src2_type = TAC_OPERAND::NONE;
    tmp_tac.dest_type = TAC_OPERAND::NONE;
    tmp_tac.sensitivity = 0;
    TAC.push_back(tmp_tac);

    //destination register (r0) needs to be assigned to destination virtual register
    TAC_type move_result_tac;
    Operation move_result_op = Operation::FUNC_RES_MOV;
    uint8_t move_result_sens = f_instr.dest_sensitivity; 
    TAC_OPERAND move_result_dest_type = TAC_OPERAND::V_REG; 
    uint32_t move_result_dest_v;
    uint8_t move_result_dest_sens = f_instr.dest_sensitivity; 
    TAC_OPERAND move_result_src1_type = TAC_OPERAND::R_REG; 
    uint32_t move_result_src1_v = 0;
    uint8_t move_result_src1_sens = f_instr.dest_sensitivity; 
    TAC_OPERAND move_result_src2_type = TAC_OPERAND::NONE; 
    uint32_t move_result_src2_v = UINT32_MAX;
    uint8_t move_result_src2_sens = 0;

    if(virtual_variables.at(f_instr.dest.index_to_variable).ssa_add_on == SSA_ADD_ON::NO_ADD_ON){
        std::string dest_original_name = generate_original_name(virtual_variables.at(f_instr.dest.index_to_variable), virtual_variables);
        std::string dest_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(f_instr.dest.index_to_variable));
        if(is_input(virtual_variables.at(f_instr.dest.index_to_variable)) || is_virtual(virtual_variables.at(f_instr.dest.index_to_variable)) || is_local(virtual_variables.at(f_instr.dest.index_to_variable))){
    
            if(variable_assigned_to_register(dest_original_name, dest_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(dest_original_name, dest_ssa_name);
                move_result_dest_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC

                generate_generic_tmp_tac(move_result_tac, move_result_op, move_result_sens, move_result_dest_type, move_result_dest_v, move_result_dest_sens, move_result_src1_type, move_result_src1_v, move_result_src1_sens, move_result_src2_type, move_result_src2_v, move_result_src2_sens, false);
                TAC.push_back(move_result_tac);
            }
            else{
                if(is_local(virtual_variables.at(f_instr.dest.index_to_variable))){
                    if(presence_of_local_variables_in_virtual_register[dest_original_name] == true){ //there was this local variable previously in register -> removed bc value in memory changedd -> load local variable with updated value from memory
                        //load from memory
                        load_local_from_memory(TAC, move_result_dest_v, move_result_dest_sens, virtual_variables, virtual_variables.at(f_instr.dest.index_to_variable), dest_original_name, dest_ssa_name, virtual_variables.at(f_instr.dest.index_to_variable).ssa_add_on, memory_security);
                        presence_of_local_variables_in_virtual_register[dest_original_name] = false;
                    }
                    else{
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(dest_original_name, dest_ssa_name);
                        virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                        move_result_dest_v = v_register_ctr;

                        v_register_ctr++;
                    }
                    generate_generic_tmp_tac(move_result_tac, move_result_op, move_result_sens, move_result_dest_type, move_result_dest_v, move_result_dest_sens, move_result_src1_type, move_result_src1_v, move_result_src1_sens, move_result_src2_type, move_result_src2_v, move_result_src2_sens, false);
                    TAC.push_back(move_result_tac);
                }
                else if(is_input(virtual_variables.at(f_instr.dest.index_to_variable))){
                    if(presence_of_input_variables_in_virtual_register[dest_original_name] == true){
                        load_local_from_memory(TAC, move_result_dest_v, move_result_dest_sens, virtual_variables, virtual_variables.at(f_instr.dest.index_to_variable), dest_original_name, dest_ssa_name, virtual_variables.at(f_instr.dest.index_to_variable).ssa_add_on, memory_security);
                        presence_of_input_variables_in_virtual_register[dest_original_name] = false;
                    }
                    else{
                        load_input_from_memory(TAC, move_result_dest_v, move_result_dest_sens, virtual_variables, virtual_variables.at(f_instr.dest.index_to_variable), dest_original_name, dest_ssa_name, virtual_variables.at(f_instr.dest.index_to_variable).ssa_add_on, memory_security);
                    }

                    generate_generic_tmp_tac(move_result_tac, move_result_op, move_result_sens, move_result_dest_type, move_result_dest_v, move_result_dest_sens, move_result_src1_type, move_result_src1_v, move_result_src1_sens, move_result_src2_type, move_result_src2_v, move_result_src2_sens, false);
                    TAC.push_back(move_result_tac);
                }
                else{
                    std::tuple<std::string, std::string> key_tuple = std::make_tuple(dest_original_name, dest_ssa_name);
                    virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                    move_result_dest_v = v_register_ctr;
                    v_register_ctr++;
                    generate_generic_tmp_tac(move_result_tac, move_result_op, move_result_sens, move_result_dest_type, move_result_dest_v, move_result_dest_sens, move_result_src1_type, move_result_src1_v, move_result_src1_sens, move_result_src2_type, move_result_src2_v, move_result_src2_sens, false);
                    TAC.push_back(move_result_tac);
                }

            }
        }
        else{
            throw std::runtime_error("in tac_func_with_return_instr: src1 is not input, local or virtual");
        }
    }
    else{
        throw std::runtime_error("in tac_func_with_return_instr: destination has add on. Can not handle yet");
    }


    //restore original stack size if number of parameters is greater than 4
    if(f_instr.parameters.size() > 4){
        TAC_type restore_stack_tac;
        Operation restore_stack_op = Operation::ADD;
        uint8_t restore_stack_sens = 0; 
        TAC_OPERAND restore_stack_dest_type = TAC_OPERAND::R_REG; 
        uint32_t restore_stack_dest_v = 13;
        uint8_t restore_stack_dest_sens = 0; 
        TAC_OPERAND restore_stack_src1_type = TAC_OPERAND::R_REG; 
        uint32_t restore_stack_src1_v = 13;
        uint8_t restore_stack_src1_sens = 0; 
        TAC_OPERAND restore_stack_src2_type = TAC_OPERAND::NUM; 
        uint32_t restore_stack_src2_v = (f_instr.parameters.size() - 4) * 4;
        uint8_t restore_stack_src2_sens = 0;
        bool add_psr_status_flag;

        TAC_type load_number_into_register;
        if(add_specification_armv6m(restore_stack_src2_v, add_psr_status_flag, load_number_into_register)){
            restore_stack_src2_type = TAC_OPERAND::V_REG;
            restore_stack_src2_v = load_number_into_register.dest_v_reg_idx;

            TAC.push_back(load_number_into_register);
        }

        generate_generic_tmp_tac(restore_stack_tac, restore_stack_op, restore_stack_sens, restore_stack_dest_type, restore_stack_dest_v, restore_stack_dest_sens, restore_stack_src1_type, restore_stack_src1_v, restore_stack_src1_sens, restore_stack_src2_type, restore_stack_src2_v, restore_stack_src2_sens, add_psr_status_flag);
        TAC.push_back(restore_stack_tac);
    }

}




void tac_condition_instr(std::vector<TAC_type>& TAC,ssa ssa_instr, virtual_variables_vec& virtual_variables, std::map<std::string, bool>& presence_of_local_variables_in_virtual_register, memory_security_struct& memory_security){
 //condition translates into two TAC instructions: compare and branch with condition

    TAC_type tmp_compare_tac;

    Operation op;
    uint8_t sens; 
    TAC_OPERAND dest_type; 
    uint32_t dest_v;
    uint8_t dest_sens; 
    TAC_OPERAND src1_type; 
    uint32_t src1_v;
    uint8_t src1_sens; 
    TAC_OPERAND src2_type; 
    uint32_t src2_v;
    uint8_t src2_sens;

    sens = ssa_instr.sensitivity_level;
    op = Operation::COMPARE;
    
    dest_type = TAC_OPERAND::NONE;
    dest_sens = 0;
    dest_v = UINT32_MAX;
    
    condition_instruction c_instr = boost::get<condition_instruction>(ssa_instr.instr);

    /**
     * @brief Handle src1
     * 
     */
    if(c_instr.op1.op_type == DATA_TYPE::NUM){
        src1_type = TAC_OPERAND::NUM;
        src1_v = c_instr.op1.op_data;
        src1_sens = 0;
    }
    else{
        std::string src1_original_name = generate_original_name(virtual_variables.at(c_instr.op1.op_data), virtual_variables);
        std::string src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(c_instr.op1.op_data));
        if(is_input(virtual_variables.at(c_instr.op1.op_data)) || is_virtual(virtual_variables.at(c_instr.op1.op_data)) || is_local(virtual_variables.at(c_instr.op1.op_data))){


            if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                src1_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                src1_sens = return_sensitivity(virtual_variables.at(c_instr.op1.op_data).sensitivity);
            }
            else{
                if(is_local(virtual_variables.at(c_instr.op1.op_data))){
                    if(presence_of_local_variables_in_virtual_register[src1_original_name] == true){ //there was this local variable previously in register -> removed bc value in memory changedd -> load local variable with updated value from memory
                        //load from memory
                        load_local_from_memory(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(c_instr.op1.op_data), src1_original_name, src1_ssa_name, virtual_variables.at(c_instr.op1.op_data).ssa_add_on, memory_security);
                        presence_of_local_variables_in_virtual_register[src1_original_name] = false;
                    }
                    else{
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                        virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                        src1_v = v_register_ctr;
                        src1_sens = return_sensitivity(virtual_variables.at(c_instr.op1.op_data).sensitivity);
                        v_register_ctr++;
                    }
                }
                else{
                    std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                    virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                    src1_v = v_register_ctr;
                    src1_sens = return_sensitivity(virtual_variables.at(c_instr.op1.op_data).sensitivity);
                    v_register_ctr++;
                }

            }
        }
        else{
            throw std::runtime_error("in tac_condition_instr: src1 is not input, local or virtual");
        }
        src1_type = TAC_OPERAND::V_REG;
    }

    /**
     * @brief Handle src2
     * 
     */
    if(c_instr.op2.op_type == DATA_TYPE::NUM){
        src2_type = TAC_OPERAND::NUM;
        src2_v = c_instr.op2.op_data;
        src2_sens = 0;
    }
    else{
        std::string src2_original_name = generate_original_name(virtual_variables.at(c_instr.op2.op_data), virtual_variables);
        std::string src2_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(c_instr.op2.op_data));
        if(is_input(virtual_variables.at(c_instr.op2.op_data)) || is_virtual(virtual_variables.at(c_instr.op2.op_data)) || is_local(virtual_variables.at(c_instr.op2.op_data))){


            if(variable_assigned_to_register(src2_original_name, src2_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(src2_original_name, src2_ssa_name);
                src2_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                src2_sens = return_sensitivity(virtual_variables.at(c_instr.op2.op_data).sensitivity);
            }
            else{
                if(is_local(virtual_variables.at(c_instr.op2.op_data))){
                    if(presence_of_local_variables_in_virtual_register[src2_original_name] == true){ //there was this local variable previously in register -> removed bc value in memory changedd -> load local variable with updated value from memory
                        //load from memory
                        load_local_from_memory(TAC, src2_v, src2_sens, virtual_variables,virtual_variables.at(c_instr.op2.op_data), src2_original_name, src2_ssa_name, virtual_variables.at(c_instr.op2.op_data).ssa_add_on, memory_security);
                        presence_of_local_variables_in_virtual_register[src2_original_name] = false;
                    }
                    else{
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src2_original_name, src2_ssa_name);
                        virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                        src2_v = v_register_ctr;
                        src2_sens = return_sensitivity(virtual_variables.at(c_instr.op2.op_data).sensitivity);
                        v_register_ctr++;
                    }
                }
                else{
                    std::tuple<std::string, std::string> key_tuple = std::make_tuple(src2_original_name, src2_ssa_name);
                    virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                    src2_v = v_register_ctr;
                    src2_sens = return_sensitivity(virtual_variables.at(c_instr.op2.op_data).sensitivity);
                    v_register_ctr++;
                }

            }
        }
        else{
            throw std::runtime_error("in tac_condition_instr: src2 is not input, local or virtual");
        }
        src2_type = TAC_OPERAND::V_REG;
    }


    generate_generic_tmp_tac(tmp_compare_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
    TAC.push_back(tmp_compare_tac);


    TAC_type tmp_branch_tac;
    tmp_branch_tac.op = c_instr.op;
    tmp_branch_tac.branch_destination = c_instr.true_dest;
    tmp_branch_tac.sensitivity = 0;
    tmp_branch_tac.dest_type = TAC_OPERAND::NONE;
    tmp_branch_tac.src1_type = TAC_OPERAND::NONE;
    tmp_branch_tac.src2_type = TAC_OPERAND::NONE;


    TAC.push_back(tmp_branch_tac);
}

void tac_return_instr(std::vector<TAC_type>& TAC,ssa ssa_instr, virtual_variables_vec& virtual_variables, std::map<std::string, bool>& presence_of_local_variables_in_virtual_register, memory_security_struct& memory_security){
    TAC_type tmp_tac;
    Operation op;
    uint8_t sens; 
    TAC_OPERAND dest_type; 
    uint32_t dest_v;
    uint8_t dest_sens; 
    TAC_OPERAND src1_type; 
    uint32_t src1_v;
    uint8_t src1_sens; 
    TAC_OPERAND src2_type; 
    uint32_t src2_v;
    uint8_t src2_sens;
    bool psr_status_flag;

    return_instruction r_instr = boost::get<return_instruction>(ssa_instr.instr);

    if(r_instr.return_value.op_type != DATA_TYPE::NONE){
        op = Operation::MOV;
        sens = ssa_instr.sensitivity_level;
        dest_type = TAC_OPERAND::R_REG;
        dest_v = 0;
        dest_sens = 0;

        src2_type = TAC_OPERAND::NONE;
        src2_sens = 0;
        src2_v = UINT32_MAX;

        if(r_instr.return_value.op_type == DATA_TYPE::NUM){
            src1_type = TAC_OPERAND::NUM;
            src1_v = r_instr.return_value.op_data;
            psr_status_flag = true;
        }
        else{
            psr_status_flag = false;
            std::string src1_original_name = generate_original_name(virtual_variables.at(r_instr.return_value.op_data), virtual_variables);
            std::string src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(r_instr.return_value.op_data));
            if(is_input(virtual_variables.at(r_instr.return_value.op_data)) || is_virtual(virtual_variables.at(r_instr.return_value.op_data)) || is_local(virtual_variables.at(r_instr.return_value.op_data))){



                if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                    std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                    src1_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                    src1_sens = return_sensitivity(virtual_variables.at(r_instr.return_value.op_data).sensitivity);
                }
                else{
                    if(is_local(virtual_variables.at(r_instr.return_value.op_data))){
                        if(presence_of_local_variables_in_virtual_register[src1_original_name] == true){ //there was this local variable previously in register -> removed bc value in memory changedd -> load local variable with updated value from memory
                            //load from memory
                            load_local_from_memory(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(r_instr.return_value.op_data), src1_original_name, src1_ssa_name, virtual_variables.at(r_instr.return_value.op_data).ssa_add_on, memory_security);
                            presence_of_local_variables_in_virtual_register[src1_original_name] = false;
                        }
                        else{
                            std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                            virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                            src1_v = v_register_ctr;
                            src1_sens = return_sensitivity(virtual_variables.at(r_instr.return_value.op_data).sensitivity);
                            v_register_ctr++;
                        }
                    }
                    else{
                        throw std::runtime_error("in tac_retur_instr: can not assign non-local variable to register in this specific instruction");
                    }

                }
            }
            else{
                throw std::runtime_error("in tac_return_instr: src1 is not input, local or virtual");
            }
            src1_type = TAC_OPERAND::V_REG;

        }



        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
        TAC.push_back(tmp_tac);

        func_has_return_value = true;
    }
    auto it_last_bb_label = std::find_if(TAC.rbegin(), TAC.rend(), [](const TAC_type& t){return t.op == Operation::LABEL;});
    last_bb_number = (it_last_bb_label+1).base()->label_name;
    

}

void tac_branch_instr(std::vector<TAC_type>& TAC,ssa ssa_instr){
    TAC_type tmp_tac;
    branch_instruction b_instr = boost::get<branch_instruction>(ssa_instr.instr);

    tmp_tac.branch_destination = b_instr.dest_bb;
    tmp_tac.sensitivity = ssa_instr.sensitivity_level;
    tmp_tac.op = Operation::BRANCH;
    tmp_tac.dest_type = TAC_OPERAND::NONE;
    tmp_tac.src1_type = TAC_OPERAND::NONE;
    tmp_tac.src2_type = TAC_OPERAND::NONE;


    TAC.push_back(tmp_tac);

}


void tac_default_instr(std::vector<TAC_type>& TAC){
    TAC_type tmp_tac;

    tmp_tac.op = Operation::NONE;
    tmp_tac.dest_type = TAC_OPERAND::NONE;
    tmp_tac.src1_type = TAC_OPERAND::NONE;
    tmp_tac.src2_type = TAC_OPERAND::NONE;


    TAC.push_back(tmp_tac);

}

//we have to call memcpy(dest addr, src addr, number of bytes) in order to initialize the local array
void tac_initialize_local_arr(ssa ssa_instr, virtual_variables_vec& virtual_variables, memory_security_struct& memory_security){
    initialize_local_array_instruction i_instr = boost::get<initialize_local_array_instruction>(ssa_instr.instr);
    //destination address is address on stack that was reserved for this

    //first get address on stack
    std::string param_original_name = generate_original_name(virtual_variables.at(i_instr.virtual_variable_idx), virtual_variables);

    //source address will be placed into R1, use label name of data to get the address

    for(auto& mem_elem: memory_security.stack_mapping_tracker){
        if(mem_elem.base_value == param_original_name){
            mem_elem.is_initialized_array = true;
        }
    }


}

void tac_label_instr(std::vector<TAC_type>& TAC, ssa ssa_instr){
    TAC_type tmp_tac;

    label_instruction l_instr = boost::get<label_instruction>(ssa_instr.instr);

    tmp_tac.op = Operation::LABEL;

    //go over every character, if character is number typed-string just convert, if not (i.e. non-numeric) convert to number
    std::string tmp_number = "";
    for(uint32_t char_pos = 0; char_pos < l_instr.label_name.size(); ++char_pos){
        char c = (char)(l_instr.label_name.at(char_pos));
        if(std::isdigit(l_instr.label_name.at(char_pos))){
            tmp_number += l_instr.label_name.at(char_pos);
        }
        else if(std::isalpha(c)){
            tmp_number += std::to_string(std::toupper(c) - 'A' + 10);
        }
    }

    tmp_tac.label_name = std::stoi(tmp_number);
    tmp_tac.dest_type = TAC_OPERAND::NONE;
    tmp_tac.src1_type = TAC_OPERAND::NONE;
    tmp_tac.src2_type = TAC_OPERAND::NONE;


    TAC.push_back(tmp_tac);

}

/**
 * @brief this instruction writes the address of ONLY arrays entries (for now) into the destination
 * 
 * @param TAC 
 * @param ssa_instr 
 * @param virtual_variables 
 */
void tac_ref_instr(std::vector<TAC_type>& TAC,ssa ssa_instr, virtual_variables_vec& virtual_variables, memory_security_struct& memory_security){
    reference_instruction r_instr = boost::get<reference_instruction>(ssa_instr.instr);

    TAC_type tmp_tac;

    Operation op  = Operation::MOV;
    uint8_t sens; 
    TAC_OPERAND dest_type; 
    uint32_t dest_v;
    uint8_t dest_sens; 
    TAC_OPERAND src1_type; 
    uint32_t src1_v;
    uint8_t src1_sens; 
    uint8_t src2_sens = 0;
    uint32_t src2_v = UINT32_MAX;
    TAC_OPERAND src2_type = TAC_OPERAND::NONE;

    /**
     * @brief Handle source (i.e. address)
     * 
     */
    if(is_local(virtual_variables.at(r_instr.src.index_to_variable))){ //this handles for example &r[i_44][j_55]
        //check if address is already assigned to virtual register
        std::string src_original_name = generate_original_name(virtual_variables.at(r_instr.src.index_to_variable), virtual_variables);
        std::string src_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(r_instr.src.index_to_variable));
        if(virtual_variables.at(r_instr.src.index_to_variable).ssa_add_on == SSA_ADD_ON::AMP_ARR_ADD_ON){
            if(variable_assigned_to_register(src_original_name, src_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(src_original_name, src_ssa_name);
                src1_v = virtual_variable_to_v_reg[key_tuple];
                src1_sens = return_sensitivity(virtual_variables.at(r_instr.src.index_to_variable).sensitivity);
                src1_type = TAC_OPERAND::V_REG;


            }
            else{ //addres is not yet in virtul registe -> have to comput address
                src_original_name = src_original_name.substr(1, src_original_name.length() - 1);
                src_ssa_name = src_ssa_name.substr(1, src_ssa_name.length() - 1);
                SSA_ADD_ON ssa_for_local_address = SSA_ADD_ON::ARR_ADD_ON;
                get_local_address(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(r_instr.src.index_to_variable), src_original_name, src_ssa_name, ssa_for_local_address, virtual_variables.at(r_instr.src.index_to_variable).ssa_add_on, memory_security, false, ssa_instr);
            }
        }
        else{
            throw std::runtime_error("in tac_ref_instr: address is local but not array. Can not handle yet");
        }

    }
    else{
        throw std::runtime_error("in tac_ref_instr: can not handle reference to non-local variables for now!");
    }

    /**
     * @brief Handle destination (i.e. variable where to store address)
     * 
     */
    std::string dest_original_name = generate_original_name(virtual_variables.at(r_instr.dest.index_to_variable), virtual_variables);
    std::string dest_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(r_instr.dest.index_to_variable));
    if(is_input(virtual_variables.at(r_instr.dest.index_to_variable)) || is_virtual(virtual_variables.at(r_instr.dest.index_to_variable)) || is_local(virtual_variables.at(r_instr.dest.index_to_variable))){


        if(variable_assigned_to_register(dest_original_name, dest_ssa_name)){
            std::tuple<std::string, std::string> key_tuple = std::make_tuple(dest_original_name, dest_ssa_name);
            dest_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
            dest_sens = return_sensitivity(virtual_variables.at(r_instr.dest.index_to_variable).sensitivity);
        }
        else{
            std::tuple<std::string, std::string> key_tuple =std::make_tuple(dest_original_name, dest_ssa_name);
            virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
            dest_v = v_register_ctr;
            dest_sens = return_sensitivity(virtual_variables.at(r_instr.dest.index_to_variable).sensitivity);
            v_register_ctr++;
            

        }
    }
    else{
        throw std::runtime_error("in tac_assign_instr: dest is not input, local or virtual");
    }
    dest_type = TAC_OPERAND::V_REG;
    sens = dest_sens;

    generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);


    TAC.push_back(tmp_tac);

}

/**
 * @brief performs simple copy/move instruction in tac form. It is important to note that the source must not be referenced or dereferenced and the destination must not be an array or a pointer dereference
 * 
 * @param TAC 
 * @param ssa_instr 
 * @param virtual_variables 
 */
void tac_assign_instr(std::vector<TAC_type>& TAC,ssa ssa_instr, virtual_variables_vec& virtual_variables, memory_security_struct& memory_security, std::map<std::string, bool>& presence_of_local_variables_in_virtual_register){
    TAC_type tmp_tac;
    Operation op = Operation::MOV;
    uint8_t sens = return_sensitivity(ssa_instr.sensitivity_level); 
    TAC_OPERAND dest_type; 
    uint32_t dest_v;
    uint8_t dest_sens; 
    TAC_OPERAND src1_type; 
    uint32_t src1_v;
    uint8_t src1_sens; 
    TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
    uint32_t src2_v = UINT32_MAX;
    uint8_t src2_sens = 0;
    bool psr_status_flag;

    assignment_instruction a_instr = boost::get<assignment_instruction>(ssa_instr.instr);
    std::string dest_origninal_name = generate_original_name(virtual_variables.at(a_instr.dest.index_to_variable), virtual_variables);
    std::string src_original_name = "";
    if(a_instr.src.op_type == DATA_TYPE::NUM){
        src_original_name = std::to_string(a_instr.src.op_data);
    }
    else{
        src_original_name = generate_original_name(virtual_variables.at(a_instr.src.op_data), virtual_variables);
    }

    if(dest_origninal_name != src_original_name){

        /**
         * @brief Handle src
         * 
         */
        if(a_instr.src.op_type == DATA_TYPE::NUM){
            src1_type = TAC_OPERAND::NUM;
            src1_v = a_instr.src.op_data;
            src1_sens = 0;

            #ifdef ARMV6_M
            move_specification_armv6m(op, src1_v, psr_status_flag);
            #endif
        }
        else{
            psr_status_flag = false;
            std::string src1_original_name = generate_original_name(virtual_variables.at(a_instr.src.op_data), virtual_variables);
            std::string src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(a_instr.src.op_data));
            if(is_input(virtual_variables.at(a_instr.src.op_data)) || is_virtual(virtual_variables.at(a_instr.src.op_data)) || is_local(virtual_variables.at(a_instr.src.op_data))){


                if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                    std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                    src1_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                    src1_sens = return_sensitivity(virtual_variables.at(a_instr.src.op_data).sensitivity);
                }
                else{
                    if(is_local(virtual_variables.at(a_instr.src.op_data))){
                        if(presence_of_local_variables_in_virtual_register[src1_original_name] == true){ //there was this local variable previously in register -> removed bc value in memory changedd -> load local variable with updated value from memory
                            //load from memory
                            load_local_from_memory(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(a_instr.src.op_data), src1_original_name, src1_ssa_name, virtual_variables.at(a_instr.src.op_data).ssa_add_on, memory_security);
                            presence_of_local_variables_in_virtual_register[src1_original_name] = false;
                        }
                        else{
                            std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                            virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                            src1_v = v_register_ctr;
                            src1_sens = return_sensitivity(virtual_variables.at(a_instr.src.op_data).sensitivity);
                            v_register_ctr++;
                        }
                    }
                    else{
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                        virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                        src1_v = v_register_ctr;
                        src1_sens = return_sensitivity(virtual_variables.at(a_instr.src.op_data).sensitivity);
                        v_register_ctr++;
                    }

                }
            }
            else{
                throw std::runtime_error("in tac_assign_instr: src1 is not input, local or virtual");
            }
            src1_type = TAC_OPERAND::V_REG;
        }

        /**
         * @brief Handle dest
         * 
         */


        std::string dest_original_name = generate_original_name(virtual_variables.at(a_instr.dest.index_to_variable), virtual_variables);
        std::string dest_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(a_instr.dest.index_to_variable));
        if(is_input(virtual_variables.at(a_instr.dest.index_to_variable)) || is_virtual(virtual_variables.at(a_instr.dest.index_to_variable)) || is_local(virtual_variables.at(a_instr.dest.index_to_variable))){


            if(variable_assigned_to_register(dest_original_name, dest_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(dest_original_name, dest_ssa_name);
                dest_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                dest_sens = return_sensitivity(virtual_variables.at(a_instr.dest.index_to_variable).sensitivity);
            }
            else{
                std::tuple<std::string, std::string> key_tuple =std::make_tuple(dest_original_name, dest_ssa_name);
                virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                dest_v = v_register_ctr;
                dest_sens = return_sensitivity(virtual_variables.at(a_instr.dest.index_to_variable).sensitivity);
                v_register_ctr++;
                

            }
        }
        else{
            throw std::runtime_error("in tac_assign_instr: dest is not input, local or virtual");
        }
        dest_type = TAC_OPERAND::V_REG;

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);


        TAC.push_back(tmp_tac);
    }


}


void tac_negate_instr(std::vector<TAC_type>& TAC,ssa ssa_instr, virtual_variables_vec& virtual_variables, memory_security_struct& memory_security, std::map<std::string, bool>& presence_of_local_variables_in_virtual_register){
    TAC_type tmp_tac;
    Operation op = Operation::NEG;
    uint8_t sens = return_sensitivity(ssa_instr.sensitivity_level); 
    TAC_OPERAND dest_type; 
    uint32_t dest_v;
    uint8_t dest_sens; 
    TAC_OPERAND src1_type; 
    uint32_t src1_v;
    uint8_t src1_sens; 
    TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
    uint32_t src2_v = UINT32_MAX;
    uint8_t src2_sens = 0;

    normal_instruction n_instr = boost::get<normal_instruction>(ssa_instr.instr);
    std::string dest_origninal_name = generate_original_name(virtual_variables.at(n_instr.dest.index_to_variable), virtual_variables);
    std::string src_original_name = "";
    if(n_instr.src1.op_type == DATA_TYPE::NUM){
        src_original_name = std::to_string(n_instr.src1.op_data);
    }
    else{
        src_original_name = generate_original_name(virtual_variables.at(n_instr.src1.op_data), virtual_variables);
    }



        /**
         * @brief Handle src
         * 
         */
        if(n_instr.src1.op_type == DATA_TYPE::NUM){
            src1_type = TAC_OPERAND::NUM;
            src1_v = n_instr.src1.op_data;
            src1_sens = 0;
        }
        else{
            std::string src1_original_name = generate_original_name(virtual_variables.at(n_instr.src1.op_data), virtual_variables);
            std::string src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(n_instr.src1.op_data));
            if(is_input(virtual_variables.at(n_instr.src1.op_data)) || is_virtual(virtual_variables.at(n_instr.src1.op_data)) || is_local(virtual_variables.at(n_instr.src1.op_data))){


                if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                    std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                    src1_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                    src1_sens = return_sensitivity(virtual_variables.at(n_instr.src1.op_data).sensitivity);
                }
                else{
                    if(is_local(virtual_variables.at(n_instr.src1.op_data))){
                        if(presence_of_local_variables_in_virtual_register[src1_original_name] == true){ //there was this local variable previously in register -> removed bc value in memory changedd -> load local variable with updated value from memory
                            //load from memory
                            load_local_from_memory(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(n_instr.src1.op_data), src1_original_name, src1_ssa_name, virtual_variables.at(n_instr.src1.op_data).ssa_add_on, memory_security);
                            presence_of_local_variables_in_virtual_register[src1_original_name] = false;
                        }
                        else{
                            std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                            virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                            src1_v = v_register_ctr;
                            src1_sens = return_sensitivity(virtual_variables.at(n_instr.src1.op_data).sensitivity);
                            v_register_ctr++;
                        }
                    }
                    else{
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                        virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                        src1_v = v_register_ctr;
                        src1_sens = return_sensitivity(virtual_variables.at(n_instr.src1.op_data).sensitivity);
                        v_register_ctr++;
                    }

                }
            }
            else{
                throw std::runtime_error("in tac_assign_instr: src1 is not input, local or virtual");
            }
            src1_type = TAC_OPERAND::V_REG;
        }

        /**
         * @brief Handle dest
         * 
         */


        std::string dest_original_name = generate_original_name(virtual_variables.at(n_instr.dest.index_to_variable), virtual_variables);
        std::string dest_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(n_instr.dest.index_to_variable));
        if(is_input(virtual_variables.at(n_instr.dest.index_to_variable)) || is_virtual(virtual_variables.at(n_instr.dest.index_to_variable)) || is_local(virtual_variables.at(n_instr.dest.index_to_variable))){


            if(variable_assigned_to_register(dest_original_name, dest_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(dest_original_name, dest_ssa_name);
                dest_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                dest_sens = return_sensitivity(virtual_variables.at(n_instr.dest.index_to_variable).sensitivity);
            }
            else{
                std::tuple<std::string, std::string> key_tuple =std::make_tuple(dest_original_name, dest_ssa_name);
                virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                dest_v = v_register_ctr;
                dest_sens = return_sensitivity(virtual_variables.at(n_instr.dest.index_to_variable).sensitivity);
                v_register_ctr++;
                

            }
        }
        else{
            throw std::runtime_error("in tac_assign_instr: dest is not input, local or virtual");
        }
        dest_type = TAC_OPERAND::V_REG;

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, true);


        TAC.push_back(tmp_tac);
    


}


/**
 * @brief Computes the actual address from load instruction if the offset != 0;
 * 
 */
void get_offset_address(std::vector<TAC_type>& TAC, uint32_t& load_src_v, uint32_t base_address_v_reg, uint32_t offset){

    TAC_type tmp_tac;
    Operation op = Operation::ADD;
    uint8_t sens = 0; 
    TAC_OPERAND dest_type = TAC_OPERAND::V_REG; 
    uint32_t dest_v = v_register_ctr++;
    uint8_t dest_sens = 0; 
    TAC_OPERAND src1_type = TAC_OPERAND::V_REG; 
    uint32_t src1_v;
    uint8_t src1_sens = 0; 
    TAC_OPERAND src2_type = TAC_OPERAND::NUM; 
    uint32_t src2_v;
    uint8_t src2_sens = 0;
    bool add_psr_status_flag;

    //actual address = base_address + offset;
    src1_v = base_address_v_reg;
    src2_v = offset;

    load_src_v = dest_v;

    TAC_type load_number_into_register;
    if(add_specification_armv6m(src2_v, add_psr_status_flag, load_number_into_register)){
        src2_type = TAC_OPERAND::V_REG;
        src2_v = load_number_into_register.dest_v_reg_idx;

        TAC.push_back(load_number_into_register);
    }

    generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, add_psr_status_flag);
    TAC.push_back(tmp_tac);
}

/**
 * @brief ssa instruction is of form var = MEM[base_address + offset]. To get it in TAC we have to split it in multiple instructions. first write the memory address in a register, then perform the load instruction
 * 
 * @param TAC 
 * @param ssa_instr 
 * @param virtual_variables 
 */
void tac_mem_load_instr(std::vector<TAC_type>& TAC,ssa ssa_instr, virtual_variables_vec& virtual_variables, std::map<std::string, bool>& presence_of_local_variables_in_virtual_register, std::map<std::string, bool>& presence_of_input_variables_in_virtual_register, memory_security_struct& memory_security){
    TAC_type tmp_tac;

    Operation op;
    uint8_t sens; 
    TAC_OPERAND dest_type; 
    uint32_t dest_v;
    uint8_t dest_sens; 
    TAC_OPERAND src1_type = TAC_OPERAND::V_REG; 
    uint32_t src1_v;
    uint8_t src1_sens; 
    TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
    uint32_t src2_v = UINT32_MAX;
    uint8_t src2_sens = 0;

    mem_load_instruction m_instr = boost::get<mem_load_instruction>(ssa_instr.instr);
    sens = return_sensitivity(ssa_instr.sensitivity_level);

    /**
     * @brief the sensitivity on the stack does not get changed by loading values from it
     * 
     */

    //decide how many bytes to load 
    if(virtual_variables.at(m_instr.base_address.index_to_variable).data_type_size == 1){
        op = Operation::LOADBYTE;
    }
    else if(virtual_variables.at(m_instr.base_address.index_to_variable).data_type_size == 2){
        op = Operation::LOADHALFWORD;
    }
    //else if(virtual_variables.at(m_instr.base_address.index_to_variable).data_type_size == 4){
    else{
        op = Operation::LOADWORD;
    }

    /**
     * @brief Handle src1 (address from where to load)
     * 
     */

    if(is_local(virtual_variables.at(m_instr.base_address.index_to_variable))){
        //handle *s = _22 -> the address is stored in s => seach if s already in virtual register
        //handle *r[i_44][j_54] -> address is stored in r[i_44][j_54] => search if r[i_44][j_54] in virtual register
        if((virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON) || (virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON)){
            std::string src1_original_name = generate_original_name(virtual_variables.at(m_instr.base_address.index_to_variable), virtual_variables);
            //remove leading asterisk as we search for address 
            src1_original_name = src1_original_name.substr(1, src1_original_name.length() - 1);
            std::string src1_ssa_name;
            SSA_ADD_ON ssa_for_local_address;
            if(virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON){
                src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(m_instr.base_address.index_to_variable));
                src1_ssa_name = src1_ssa_name.substr(1, src1_ssa_name.length() - 1);
                ssa_for_local_address = SSA_ADD_ON::ARR_ADD_ON;
            }
            else{
                src1_ssa_name = "";
                ssa_for_local_address = SSA_ADD_ON::NO_ADD_ON;
            }

            if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                src1_v = virtual_variable_to_v_reg[key_tuple];
                src1_sens = return_sensitivity(virtual_variables.at(m_instr.base_address.index_to_variable).sensitivity);
            }
            else{
                get_local_address(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(m_instr.base_address.index_to_variable), src1_original_name, src1_ssa_name, ssa_for_local_address, virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on, memory_security, true, ssa_instr);
            }

        }
        //handle r[i_44][j_42] = _2; -> address is tored in &r[i_44][j_42] => search if (&r[i][j], &r[i_44][j_42]) already in virtual register
        else if((virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on == SSA_ADD_ON::ARR_ADD_ON)){
            std::string src1_original_name = generate_original_name(virtual_variables.at(m_instr.base_address.index_to_variable), virtual_variables);
            //remove leading asterisk as we search for address 
            src1_original_name = "&" +  src1_original_name;
            std::string src1_ssa_name;
            src1_ssa_name = "&" + virtual_variables.at(m_instr.base_address.index_to_variable).ssa_name;
            SSA_ADD_ON ssa_for_local_address = SSA_ADD_ON::ARR_ADD_ON;
            if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                src1_v = virtual_variable_to_v_reg[key_tuple];
                src1_sens = return_sensitivity(virtual_variables.at(m_instr.base_address.index_to_variable).sensitivity);
            }
            else{
                get_local_address(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(m_instr.base_address.index_to_variable), src1_original_name, src1_ssa_name, ssa_for_local_address, virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on, memory_security, true, ssa_instr);
            }
        }
        else{
            throw std::runtime_error("in tac_mem_load_instr: local variable has wrong add on. Can not handle");
        }
    }
    else if(is_virtual(virtual_variables.at(m_instr.base_address.index_to_variable))){
        //handle *_19 = _22 -> the address is stored in _19 => _19 has to be in register

        if((virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON)){
            std::tuple<std::string, std::string> key_tuple = std::make_tuple(virtual_variables.at(m_instr.base_address.index_to_variable).original_name, "");
            src1_v = virtual_variable_to_v_reg[key_tuple];
            src1_sens = return_sensitivity(virtual_variables.at(m_instr.base_address.index_to_variable).sensitivity);
        }
        //handle *_19[i_44][j_54] -> address is stored in r[i_44][j_54] => search if r[i_44][j_54] in virtual register
        else if((virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON)){
            std::string src1_original_name = generate_original_name(virtual_variables.at(m_instr.base_address.index_to_variable), virtual_variables);
            //remove leading asterisk as we search for address 
            src1_original_name = src1_original_name.substr(1, src1_original_name.length() - 1);
            std::string src1_ssa_name;
            src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(m_instr.base_address.index_to_variable));
            src1_ssa_name = src1_ssa_name.substr(1, src1_ssa_name.length() - 1);

            if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                src1_v = virtual_variable_to_v_reg[key_tuple];
                src1_sens = return_sensitivity(virtual_variables.at(m_instr.base_address.index_to_variable).sensitivity);
            }
            else{
                get_virtual_address(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(m_instr.base_address.index_to_variable), src1_original_name, src1_ssa_name, virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on, true, ssa_instr);
            }

        }
        //handle _19[i_44][j_42] = _2; -> address is tored in &_19[i_44][j_42] => search if (&_19[i][j], &_19[i_44][j_42]) already in virtual register
        else if((virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on == SSA_ADD_ON::ARR_ADD_ON)){
            std::string src1_original_name = generate_original_name(virtual_variables.at(m_instr.base_address.index_to_variable), virtual_variables);
            //add amp as we search for address 
            src1_original_name = "&" +  src1_original_name;
            std::string src1_ssa_name;
            src1_ssa_name = "&" + virtual_variables.at(m_instr.base_address.index_to_variable).ssa_name; //do not have to check if ssa_name is src2_ssa_name is empty as we only have arrays here
            
            if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                src1_v = virtual_variable_to_v_reg[key_tuple];
                src1_sens = return_sensitivity(virtual_variables.at(m_instr.base_address.index_to_variable).sensitivity);
            }
            else{
                throw std::runtime_error("in tac_mem_load_instr: address is virtual array but without dereferencing. Can not handle yet");
            }
        }
        else{
            throw std::runtime_error("in tac_mem_load_instr: virtual address is neither plain virtual nor pointer or array: " + virtual_variables.at(m_instr.base_address.index_to_variable).ssa_name);
        }
    }
    else if(is_input(virtual_variables.at(m_instr.base_address.index_to_variable))){
        //handle _2 = MEM[(uint32_t *)in_19(D) + 4B] -> the address is stored in "in" => "in" has to be in register
        if((virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on == SSA_ADD_ON::NO_ADD_ON)){
            //variable has to be in register
            std::string src1_original_name = generate_original_name(virtual_variables.at(m_instr.base_address.index_to_variable), virtual_variables);
            std::string src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(m_instr.base_address.index_to_variable));
            if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                if(m_instr.offset.op_type == DATA_TYPE::NUM){
                    if(m_instr.offset.op_data == 0){ //load from base address -> no offset
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                        src1_v = virtual_variable_to_v_reg[key_tuple];
                    }
                    else{
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                        uint32_t base_address_v_reg = virtual_variable_to_v_reg[key_tuple];
                        get_offset_address(TAC, src1_v, base_address_v_reg, m_instr.offset.op_data);
                    }
                }
                else{
                    throw std::runtime_error("in tac_mem_load_instr: can not handle offset that is not a number");
                }


                src1_sens = return_sensitivity(virtual_variables.at(m_instr.base_address.index_to_variable).sensitivity);
            }
            else{
                SSA_ADD_ON ssa_for_local_address = SSA_ADD_ON::NO_ADD_ON;
                if(presence_of_input_variables_in_virtual_register[src1_original_name] == true){
                    get_local_address(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(m_instr.base_address.index_to_variable), src1_original_name, src1_ssa_name, ssa_for_local_address, virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on, memory_security, true, ssa_instr);

                }
                else{
                    get_input_address(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(m_instr.base_address.index_to_variable), src1_original_name, src1_ssa_name, ssa_for_local_address, virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on, memory_security, true, ssa_instr);
                }
            }

            #ifdef ARMV6_M
            if((TAC.back().op == Operation::ADD) && (TAC.back().src2_type == TAC_OPERAND::NUM)){
                add_t1_encoding_specification_armv6m(TAC, TAC.back().dest_v_reg_idx, TAC.back().src1_v, TAC.back().src2_v);
            }
            #endif

        }
        else if((virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on == SSA_ADD_ON::ARR_ADD_ON)){
            throw std::runtime_error("in tac_mem_load_instr: source address is input with array add on. Can not handle yet");
        }
        else if(virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON){
            std::string src1_original_name = generate_original_name(virtual_variables.at(m_instr.base_address.index_to_variable), virtual_variables);
            //remove asterisk from original name
            src1_original_name = src1_original_name.substr(src1_original_name.find("*") + 1, std::string::npos - 1);
            std::string src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(m_instr.base_address.index_to_variable));
            if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                if(m_instr.offset.op_type == DATA_TYPE::NUM){
                    if(m_instr.offset.op_data == 0){ //load from base address -> no offset
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                        src1_v = virtual_variable_to_v_reg[key_tuple];
                    }
                    else{
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                        uint32_t base_address_v_reg = virtual_variable_to_v_reg[key_tuple];
                        get_offset_address(TAC, src1_v, base_address_v_reg, m_instr.offset.op_data);
                    }
                }
                else{
                    throw std::runtime_error("in tac_mem_load_instr: can not handle offset that is not a number");
                }


                src1_sens = return_sensitivity(virtual_variables.at(m_instr.base_address.index_to_variable).sensitivity);
            }
            else{
                SSA_ADD_ON ssa_for_local_address = SSA_ADD_ON::NO_ADD_ON;
                if(presence_of_input_variables_in_virtual_register[src1_original_name] == true){
                    get_local_address(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(m_instr.base_address.index_to_variable), src1_original_name, src1_ssa_name, ssa_for_local_address, virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on, memory_security, true, ssa_instr);
                    // presence_of_input_variables_in_virtual_register[src1_original_name] = false;
                }
                else{
                    get_input_address(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(m_instr.base_address.index_to_variable), src1_original_name, src1_ssa_name, ssa_for_local_address, virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on, memory_security, true, ssa_instr);
                }
            }
        }
        else if((virtual_variables.at(m_instr.base_address.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON)){
            throw std::runtime_error("in tac_mem_load_instr: source address is input with asterisk array add on. Can not handle yet");
        }
        else{
            throw std::runtime_error("in tac_mem_load_instr: source address is input with unconsidered add on. Can not handle yet");
        }

    }
    else{
        throw std::runtime_error("in tac_mem_load_instr: source address is neither virtual, local nor input");
    }

    /**
     * @brief Handle destination
     * 
     */

    std::string dest_original_name = generate_original_name(virtual_variables.at(m_instr.dest.index_to_variable), virtual_variables);
    std::string dest_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(m_instr.dest.index_to_variable));
    if(is_input(virtual_variables.at(m_instr.dest.index_to_variable)) || is_virtual(virtual_variables.at(m_instr.dest.index_to_variable)) || is_local(virtual_variables.at(m_instr.dest.index_to_variable))){

        if(variable_assigned_to_register(dest_original_name, dest_ssa_name)){
            std::tuple<std::string, std::string> key_tuple = std::make_tuple(dest_original_name, dest_ssa_name);
            dest_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
            // dest_sens = return_sensitivity(virtual_variables.at(m_instr.dest.index_to_variable).sensitivity);
        }
        else{
            if(is_local(virtual_variables.at(m_instr.dest.index_to_variable))){
                if(presence_of_local_variables_in_virtual_register[dest_original_name] == true){ //there was this local variable previously in register -> removed bc value in memory changedd -> load local variable with updated value from memory
                    //load from memory

                    load_local_from_memory(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(m_instr.dest.index_to_variable), dest_original_name,  dest_ssa_name, virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on, memory_security);
                    presence_of_local_variables_in_virtual_register[dest_original_name] = false;
                }
                else{
                    std::tuple<std::string, std::string> key_tuple = std::make_tuple(dest_original_name, dest_ssa_name);
                    virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                    dest_v = v_register_ctr;
                    // dest_sens = return_sensitivity(virtual_variables.at(m_instr.dest.index_to_variable).sensitivity);
                    v_register_ctr++;
                }
            }
            else{
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(dest_original_name, dest_ssa_name);
                virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                dest_v = v_register_ctr;
                // dest_sens = return_sensitivity(virtual_variables.at(m_instr.dest.index_to_variable).sensitivity);
                v_register_ctr++;
            }

        }
    }
    else{
        throw std::runtime_error("in tac_mem_load_instr: dest is not input, local or virtual");
    }
    dest_type = TAC_OPERAND::V_REG;
    dest_sens = return_sensitivity(virtual_variables.at(m_instr.dest.index_to_variable).sensitivity);


    generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);



    TAC.push_back(tmp_tac);
}

void tac_mem_store_instr(std::vector<TAC_type>& TAC,ssa ssa_instr, virtual_variables_vec& virtual_variables, std::map<std::string, bool>& presence_of_local_variables_in_virtual_register, std::map<std::string, bool>& presence_of_input_variables_in_virtual_register, memory_security_struct& memory_security){
    TAC_type tmp_tac;

    Operation op;
    uint8_t sens; 
    TAC_OPERAND dest_type = TAC_OPERAND::NONE; 
    uint32_t dest_v = UINT32_MAX;
    uint8_t dest_sens = 0; 
    TAC_OPERAND src1_type; 
    uint32_t src1_v;
    uint8_t src1_sens; 
    TAC_OPERAND src2_type = TAC_OPERAND::V_REG; 
    uint32_t src2_v;
    uint8_t src2_sens;

    mem_store_instruction m_instr = boost::get<mem_store_instruction>(ssa_instr.instr);

    sens = return_sensitivity(ssa_instr.sensitivity_level);

    //decide how many bytes to store
    if(virtual_variables.at(m_instr.dest.index_to_variable).data_type_size == 1){
        op = Operation::STOREBYTE;
    }
    else if(virtual_variables.at(m_instr.dest.index_to_variable).data_type_size == 2){
        op = Operation::STOREHALFWORD;
    }
    else{
        op = Operation::STOREWORD;
    }
    // else if(virtual_variables.at(m_instr.dest.index_to_variable).data_type_size == 4){
    //     op = Operation::STOREWORD;
    // }

    /**
     * @brief Handle destination/src2 (i.e. address where to store value to)
     * 
     */

    if(is_local(virtual_variables.at(m_instr.dest.index_to_variable))){
        //handle *s = _22 -> the address is stored in s => seach if s already in virtual register
        //handle *r[i_44][j_54] -> address is stored in r[i_44][j_54] => search if r[i_44][j_54] in virtual register
        if((virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON) || (virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON)){
            std::string src2_original_name = generate_original_name(virtual_variables.at(m_instr.dest.index_to_variable), virtual_variables);
            //remove leading asterisk as we search for address 
            src2_original_name = src2_original_name.substr(1, src2_original_name.length() - 1);
            std::string src2_ssa_name;
            SSA_ADD_ON ssa_for_local_address;
            if(virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON){
                src2_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(m_instr.dest.index_to_variable));
                src2_ssa_name = src2_ssa_name.substr(1, src2_ssa_name.length() - 1);
                ssa_for_local_address = SSA_ADD_ON::ARR_ADD_ON;
            }
            else{
                src2_ssa_name = "";
                ssa_for_local_address = SSA_ADD_ON::NO_ADD_ON;
            }

            if(variable_assigned_to_register(src2_original_name, src2_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(src2_original_name, src2_ssa_name);
                src2_v = virtual_variable_to_v_reg[key_tuple];
                src2_sens = return_sensitivity(virtual_variables.at(m_instr.dest.index_to_variable).sensitivity);
            }
            else{
                get_local_address(TAC, src2_v, src2_sens, virtual_variables, virtual_variables.at(m_instr.dest.index_to_variable), src2_original_name, src2_ssa_name, ssa_for_local_address, virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on, memory_security, false, ssa_instr);
            }

        }
        //handle r[i_44][j_42] = _2; -> address is tored in &r[i_44][j_42] => search if (&r[i][j], &r[i_44][j_42]) already in virtual register
        else if((virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on == SSA_ADD_ON::ARR_ADD_ON)){
            std::string src2_original_name = generate_original_name(virtual_variables.at(m_instr.dest.index_to_variable), virtual_variables);
            //remove leading asterisk as we search for address 
            src2_original_name = "&" +  src2_original_name;
            std::string src2_ssa_name;
            src2_ssa_name = "&" + virtual_variables.at(m_instr.dest.index_to_variable).ssa_name;
            SSA_ADD_ON ssa_for_local_address = SSA_ADD_ON::ARR_ADD_ON;
            
            if(variable_assigned_to_register(src2_original_name, src2_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(src2_original_name, src2_ssa_name);
                src2_v = virtual_variable_to_v_reg[key_tuple];
                src2_sens = return_sensitivity(virtual_variables.at(m_instr.dest.index_to_variable).sensitivity);
            }
            else{
                get_local_address(TAC, src2_v, src2_sens, virtual_variables, virtual_variables.at(m_instr.dest.index_to_variable), src2_original_name, src2_ssa_name, ssa_for_local_address, virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on, memory_security, false, ssa_instr);
            }
        }
    }
    else if(is_virtual(virtual_variables.at(m_instr.dest.index_to_variable))){
        //handle *_19 = _22 -> the address is stored in _19 => _19 has to be in register
        if((virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON)){
            std::tuple<std::string, std::string> key_tuple = std::make_tuple(virtual_variables.at(m_instr.dest.index_to_variable).original_name, "");
            src2_v = virtual_variable_to_v_reg[key_tuple];
            src2_sens = return_sensitivity(virtual_variables.at(m_instr.dest.index_to_variable).sensitivity);
        }
        //handle *_19[i_44][j_54] -> address is stored in r[i_44][j_54] => search if r[i_44][j_54] in virtual register
        else if((virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON)){
            std::string src2_original_name = generate_original_name(virtual_variables.at(m_instr.dest.index_to_variable), virtual_variables);
            //remove leading asterisk as we search for address 
            src2_original_name = src2_original_name.substr(1, src2_original_name.length() - 1);
            std::string src2_ssa_name;
            src2_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(m_instr.dest.index_to_variable));
            src2_ssa_name = src2_ssa_name.substr(1, src2_ssa_name.length() - 1);

            if(variable_assigned_to_register(src2_original_name, src2_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(src2_original_name, src2_ssa_name);
                src2_v = virtual_variable_to_v_reg[key_tuple];
                src2_sens = return_sensitivity(virtual_variables.at(m_instr.dest.index_to_variable).sensitivity);
            }
            else{
                get_virtual_address(TAC, src2_v, src2_sens, virtual_variables, virtual_variables.at(m_instr.dest.index_to_variable), src2_original_name, src2_ssa_name, virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on, false, ssa_instr);
            }

        }
        //handle _19[i_44][j_42] = _2; -> address is tored in &_19[i_44][j_42] => search if (&_19[i][j], &_19[i_44][j_42]) already in virtual register
        else if((virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on == SSA_ADD_ON::ARR_ADD_ON)){
            std::string src2_original_name = generate_original_name(virtual_variables.at(m_instr.dest.index_to_variable), virtual_variables);
            //add amp as we search for address 
            src2_original_name = "&" +  src2_original_name;
            std::string src2_ssa_name;
            src2_ssa_name = "&" + virtual_variables.at(m_instr.dest.index_to_variable).ssa_name; //do not have to check if ssa_name is src2_ssa_name is empty as we only have arrays here
            
            if(variable_assigned_to_register(src2_original_name, src2_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(src2_original_name, src2_ssa_name);
                src2_v = virtual_variable_to_v_reg[key_tuple];
                src2_sens = return_sensitivity(virtual_variables.at(m_instr.dest.index_to_variable).sensitivity);
            }
            else{
                throw std::runtime_error("in tac_mem_store_instr: address is virtual array but without dereferencing. Can not handle yet");
            }
        }
        else{
            throw std::runtime_error("in tac_mem_store_instr: virtual address is neither plain virtual nor pointer or array");
        }
    }
    else if(is_input(virtual_variables.at(m_instr.dest.index_to_variable))){
        if((virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON) || (virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON)){
            std::string src2_original_name = generate_original_name(virtual_variables.at(m_instr.dest.index_to_variable), virtual_variables);
            //remove leading asterisk as we search for address 
            src2_original_name = src2_original_name.substr(1, src2_original_name.length() - 1);
            std::string src2_ssa_name;
            SSA_ADD_ON ssa_for_local_address;
            if(virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON){
                src2_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(m_instr.dest.index_to_variable));
                src2_ssa_name = src2_ssa_name.substr(1, src2_ssa_name.length() - 1);
                ssa_for_local_address = SSA_ADD_ON::ARR_ADD_ON;
            }
            else{
                src2_ssa_name = "";
                ssa_for_local_address = SSA_ADD_ON::NO_ADD_ON;
            }

            if(variable_assigned_to_register(src2_original_name, src2_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(src2_original_name, src2_ssa_name);
                src2_v = virtual_variable_to_v_reg[key_tuple];
                src2_sens = return_sensitivity(virtual_variables.at(m_instr.dest.index_to_variable).sensitivity);
            }
            else if(presence_of_input_variables_in_virtual_register[src2_original_name] == true){
                get_local_address(TAC, src2_v, src2_sens, virtual_variables, virtual_variables.at(m_instr.dest.index_to_variable), src2_original_name, src2_ssa_name, ssa_for_local_address, virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on, memory_security, false, ssa_instr);
            }
            else{
                get_input_address(TAC, src2_v, src2_sens, virtual_variables, virtual_variables.at(m_instr.dest.index_to_variable), src2_original_name, src2_ssa_name, ssa_for_local_address, virtual_variables.at(m_instr.dest.index_to_variable).ssa_add_on, memory_security, false, ssa_instr);
            }
        }
        else{
            throw std::runtime_error("in tac_mem_store_instr: can not handle input destination with array add on yet");
        }
    }
    else{
        throw std::runtime_error("in tac_mem_store_instr: destination address is neither virtual, local nor input");
    }


    /**
     * @brief Handle src1 (value that will be stored)
     * 
     */

    if(m_instr.value.op_type == DATA_TYPE::NUM){        


        //we can not store a number directly. it has to be moved to a register
        TAC_type mov_tac;
        Operation mov_op = Operation::MOV;
        uint8_t mov_sens = 0; 
        TAC_OPERAND mov_dest_type = TAC_OPERAND::V_REG; 
        uint32_t mov_dest_v = v_register_ctr++;
        uint8_t mov_dest_sens = 0; 
        TAC_OPERAND mov_src1_type = TAC_OPERAND::NUM; 
        uint32_t mov_src1_v = m_instr.value.op_data;
        uint8_t mov_src1_sens = 0; 
        TAC_OPERAND mov_src2_type = TAC_OPERAND::NONE; 
        uint32_t mov_src2_v = UINT32_MAX;
        uint8_t mov_src2_sens = 0;
        bool mov_psr_status_flag;

        #ifdef ARMV6_M
        move_specification_armv6m(mov_op, mov_src1_v, mov_psr_status_flag);

        #endif

        generate_generic_tmp_tac(mov_tac, mov_op, mov_sens, mov_dest_type, mov_dest_v, mov_dest_sens, mov_src1_type, mov_src1_v, mov_src1_sens, mov_src2_type, mov_src2_v, mov_src2_sens, mov_psr_status_flag);

        TAC.push_back(mov_tac);

        src1_type = TAC_OPERAND::V_REG;
        src1_v = TAC.back().dest_v_reg_idx;
        src1_sens = 0;

    }
    else{
        std::string src1_original_name = generate_original_name(virtual_variables.at(m_instr.value.op_data), virtual_variables);
        std::string src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(m_instr.value.op_data));
        if(is_input(virtual_variables.at(m_instr.value.op_data)) || is_virtual(virtual_variables.at(m_instr.value.op_data)) || is_local(virtual_variables.at(m_instr.value.op_data))){

            if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                src1_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                src1_sens = return_sensitivity(virtual_variables.at(m_instr.value.op_data).sensitivity);
            }
            else{
                if(is_local(virtual_variables.at(m_instr.value.op_data))){
                    if(presence_of_local_variables_in_virtual_register[src1_original_name] == true){ //there was this local variable previously in register -> removed bc value in memory changedd -> load local variable with updated value from memory

                        load_local_from_memory(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(m_instr.value.op_data), src1_original_name,  src1_ssa_name, virtual_variables.at(m_instr.value.op_data).ssa_add_on, memory_security);

                        presence_of_local_variables_in_virtual_register[src1_original_name] = false;
                    }
                    else{
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                        virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                        src1_v = v_register_ctr;
                        src1_sens = return_sensitivity(virtual_variables.at(m_instr.value.op_data).sensitivity);
                        v_register_ctr++;
                    }
                }
                else{
                    std::tuple<std::string, std::string> key_tuple =std::make_tuple(src1_original_name, src1_ssa_name);
                    virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                    src1_v = v_register_ctr;
                    src1_sens = return_sensitivity(virtual_variables.at(m_instr.value.op_data).sensitivity);
                    v_register_ctr++;
                }

            }
        }
        else{
            throw std::runtime_error("in tac_mem_store_instr: src1 is not input, local or virtual");
        }
        src1_type = TAC_OPERAND::V_REG;


    }


    generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
    TAC.push_back(tmp_tac);


}




void tac_normal_instr(std::vector<TAC_type>& TAC,ssa ssa_instr, virtual_variables_vec& virtual_variables, std::map<std::string, bool>& presence_of_local_variables_in_virtual_register, std::map<std::string, bool>& presence_of_input_variables_in_virtual_register, memory_security_struct& memory_security){
    TAC_type tmp_tac;

    Operation op;
    uint8_t sens; 
    TAC_OPERAND dest_type; 
    uint32_t dest_v;
    uint8_t dest_sens; 
    TAC_OPERAND src1_type; 
    uint32_t src1_v;
    uint8_t src1_sens; 
    TAC_OPERAND src2_type; 
    uint32_t src2_v;
    uint8_t src2_sens;
    bool psr_status_flag;
    
    normal_instruction n_instr = boost::get<normal_instruction>(ssa_instr.instr);
    op = n_instr.operation;
    sens = return_sensitivity(ssa_instr.sensitivity_level);
    

    /**
     * @brief Handle src1
     * 
     */
    if(n_instr.src1.op_type == DATA_TYPE::NUM){
        src1_type = TAC_OPERAND::NUM;
        src1_v = n_instr.src1.op_data;
        src1_sens = 0;
    }
    else{
        std::string src1_original_name = generate_original_name(virtual_variables.at(n_instr.src1.op_data), virtual_variables);
        std::string src1_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(n_instr.src1.op_data));
        if(is_input(virtual_variables.at(n_instr.src1.op_data)) || is_virtual(virtual_variables.at(n_instr.src1.op_data)) || is_local(virtual_variables.at(n_instr.src1.op_data))){

            if(variable_assigned_to_register(src1_original_name, src1_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                src1_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                src1_sens = return_sensitivity(virtual_variables.at(n_instr.src1.op_data).sensitivity);
            }
            else{
                if(is_local(virtual_variables.at(n_instr.src1.op_data))){
                    if(presence_of_local_variables_in_virtual_register[src1_original_name] == true){ //there was this local variable previously in register -> removed bc value in memory changedd -> load local variable with updated value from memory
                        //load from memory
                        
                        load_local_from_memory(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(n_instr.src1.op_data), src1_original_name,  src1_ssa_name, virtual_variables.at(n_instr.src1.op_data).ssa_add_on, memory_security);
                        presence_of_local_variables_in_virtual_register[src1_original_name] = false;
                    }
                    else{
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                        virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                        src1_v = v_register_ctr;
                        src1_sens = return_sensitivity(virtual_variables.at(n_instr.src1.op_data).sensitivity);
                        v_register_ctr++;
                    }
                }
                else if(is_input(virtual_variables.at(n_instr.src1.op_data))){
                    if(presence_of_input_variables_in_virtual_register[src1_original_name] == true){
                        //load from previous stack frame


                        load_local_from_memory(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(n_instr.src1.op_data), src1_original_name,  src1_ssa_name, virtual_variables.at(n_instr.src1.op_data).ssa_add_on, memory_security);
                        presence_of_input_variables_in_virtual_register[src1_original_name] = false;
                    }
                    else{

                        
                        load_input_from_memory(TAC, src1_v, src1_sens, virtual_variables, virtual_variables.at(n_instr.src1.op_data), src1_original_name,  src1_ssa_name, virtual_variables.at(n_instr.src1.op_data).ssa_add_on, memory_security);

                    }
                }
                else{
                    std::tuple<std::string, std::string> key_tuple = std::make_tuple(src1_original_name, src1_ssa_name);
                    virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                    src1_v = v_register_ctr;
                    src1_sens = return_sensitivity(virtual_variables.at(n_instr.src1.op_data).sensitivity);
                    v_register_ctr++;
                }

            }
        }
        else{
            throw std::runtime_error("in tac_normal_instr: src1 is not input, local or virtual");
        }
        src1_type = TAC_OPERAND::V_REG;
    }


    /**
     * @brief Handle src2
     * 
     */
    if(n_instr.src2.op_type != DATA_TYPE::NONE){ //check if unary operation
        if(n_instr.src2.op_type == DATA_TYPE::NUM){
            src2_type = TAC_OPERAND::NUM;
            src2_v = n_instr.src2.op_data;
            src2_sens = 0;
        }
        else{
            std::string src2_original_name = generate_original_name(virtual_variables.at(n_instr.src2.op_data), virtual_variables);
            std::string src2_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(n_instr.src2.op_data));
            if(is_input(virtual_variables.at(n_instr.src2.op_data)) || is_virtual(virtual_variables.at(n_instr.src2.op_data)) || is_local(virtual_variables.at(n_instr.src2.op_data))){


                if(variable_assigned_to_register(src2_original_name, src2_ssa_name)){
                    std::tuple<std::string, std::string> key_tuple = std::make_tuple(src2_original_name, src2_ssa_name);
                    src2_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                    src2_sens = return_sensitivity(virtual_variables.at(n_instr.src2.op_data).sensitivity);
                }
                else{
                    if(is_local(virtual_variables.at(n_instr.src2.op_data))){
                        if(presence_of_local_variables_in_virtual_register[src2_original_name] == true){ //there was this local variable previously in register -> removed bc value in memory changedd -> load local variable with updated value from memory
                            //load from memory
                            load_local_from_memory(TAC, src2_v, src2_sens, virtual_variables, virtual_variables.at(n_instr.src2.op_data), src2_original_name, src2_ssa_name, virtual_variables.at(n_instr.src2.op_data).ssa_add_on, memory_security);
                            presence_of_local_variables_in_virtual_register[src2_original_name] = false;
                        }
                        else{
                            std::tuple<std::string, std::string> key_tuple = std::make_tuple(src2_original_name, src2_ssa_name);
                            virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                            src2_v = v_register_ctr;
                            src2_sens = return_sensitivity(virtual_variables.at(n_instr.src2.op_data).sensitivity);
                            v_register_ctr++;
                        }
                    }
                    else if(is_input(virtual_variables.at(n_instr.src2.op_data))){
                        if(presence_of_input_variables_in_virtual_register[src2_original_name] == true){ //there was this input variable previously in register -> removed bc value in memory changedd -> load input variable with updated value from local memory

                                load_local_from_memory(TAC, src2_v, src2_sens, virtual_variables, virtual_variables.at(n_instr.src2.op_data), src2_original_name,  src2_ssa_name, virtual_variables.at(n_instr.src2.op_data).ssa_add_on, memory_security);
                                presence_of_input_variables_in_virtual_register[src2_original_name] = false;
                            }
                        else{ //input variable was *not* yet in register -> can not simply assign arbitrary register but have to load it from input stack region

                        
                            load_input_from_memory(TAC, src2_v, src2_sens, virtual_variables, virtual_variables.at(n_instr.src2.op_data), src2_original_name,  src2_ssa_name, virtual_variables.at(n_instr.src2.op_data).ssa_add_on, memory_security);

                        }
                    }
                    else{
                        std::tuple<std::string, std::string> key_tuple = std::make_tuple(src2_original_name, src2_ssa_name);
                        virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                        src2_v = v_register_ctr;
                        src2_sens = return_sensitivity(virtual_variables.at(n_instr.src2.op_data).sensitivity);
                        v_register_ctr++;
                    }

                }
            }
            else{
                throw std::runtime_error("in tac_normal_instr: src2 is not input, local or virtual");
            }
            src2_type = TAC_OPERAND::V_REG;

        }
    }
    else{
        src2_type = TAC_OPERAND::NONE;
    }



    #ifdef ARMV6_M
    //AND instruction is only allowed to have registers -> if second source is number we have to move it to register before
    if((n_instr.operation == Operation::LOGICAL_AND) && (n_instr.src2.op_type == DATA_TYPE::NUM)){
        and_specification_armv6m(TAC, src2_type, src2_v);
    }
    if((n_instr.operation == Operation::XOR) && (n_instr.src2.op_type == DATA_TYPE::NUM)){
        and_specification_armv6m(TAC, src2_type, src2_v);
    }
    #endif


    /**
     * @brief handle destination
     * 
     */
    if(is_input(virtual_variables.at(n_instr.dest.index_to_variable)) || is_local(virtual_variables.at(n_instr.dest.index_to_variable)) || is_virtual(virtual_variables.at(n_instr.dest.index_to_variable))){
        std::string dest_original_name = generate_original_name(virtual_variables.at(n_instr.dest.index_to_variable), virtual_variables);
        std::string dest_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(n_instr.dest.index_to_variable));
        if(is_input(virtual_variables.at(n_instr.dest.index_to_variable)) || is_virtual(virtual_variables.at(n_instr.dest.index_to_variable)) || is_local(virtual_variables.at(n_instr.dest.index_to_variable))){


            if(variable_assigned_to_register(dest_original_name, dest_ssa_name)){
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(dest_original_name, dest_ssa_name);
                dest_v = virtual_variable_to_v_reg[key_tuple]; //assign know v_reg for ssa to TAC
                dest_sens = return_sensitivity(virtual_variables.at(n_instr.dest.index_to_variable).sensitivity);
            }
            else{
                std::tuple<std::string, std::string> key_tuple = std::make_tuple(dest_original_name, dest_ssa_name);
                virtual_variable_to_v_reg[key_tuple] = v_register_ctr; //have new mapping between v_reg and ssa
                dest_v = v_register_ctr;
                dest_sens = return_sensitivity(virtual_variables.at(n_instr.dest.index_to_variable).sensitivity);
                v_register_ctr++;

            }
        }
        else{
            throw std::runtime_error("in tac_normal_instr: dest is not input, local or virtual");
        }
        dest_type = TAC_OPERAND::V_REG;

    }
    else{
        throw std::runtime_error("in tac_normal_instr: destination is not input, local or virtual");
    }
    

    if((op == Operation::MUL) && (src2_type == TAC_OPERAND::NUM) && (src2_v % 2 == 0)){
        op = Operation::LSL;
        src2_v = static_cast<uint32_t>(log2(src2_v));
    }

    #ifdef ARMV6_M
    if((op == Operation::MUL) || (op == Operation::LSL) || (op == Operation::LSR) || (op == Operation::ADD) || (op == Operation::XOR) || (op == Operation::SUB) || (op == Operation::LOGICAL_AND) || (op == Operation::LOGICAL_OR)){
        psr_status_flag = true;
    }
    else{
        psr_status_flag = false;
    }

    //if the second operand is a number we have to first move it into a register to be able to use it
    if(src2_type == TAC_OPERAND::NUM){
        if(src2_v > 255){
            TAC_type load_tac;

            //move r11 to register that will contain the new value
            Operation load_op = Operation::LOADWORD;
            uint8_t load_sens = 0; 
            TAC_OPERAND load_src2_type = TAC_OPERAND::NONE; 
            uint32_t load_src2_v = UINT32_MAX;
            uint8_t load_src2_sens = 0; 
            TAC_OPERAND load_src1_type = TAC_OPERAND::NUM; 
            uint32_t load_src1_v = src2_v; 
            uint8_t load_src1_sens = 0; 
            TAC_OPERAND load_dest_type = TAC_OPERAND::V_REG; 
            uint32_t load_dest_v = v_register_ctr++;
            uint8_t load_dest_sens = 0;

            generate_generic_tmp_tac(load_tac, load_op, load_sens, load_dest_type, load_dest_v, load_dest_sens, load_src1_type, load_src1_v, load_src1_sens, load_src2_type, load_src2_v, load_src2_sens, false);
            TAC.push_back(load_tac);

            src2_v = TAC.back().dest_v_reg_idx;
            src2_type = TAC_OPERAND::V_REG;
        }


        
    }
    #endif

    generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, psr_status_flag);
    TAC.push_back(tmp_tac);

    #ifdef ARMV6_M
    if((tmp_tac.op == Operation::ADD) && (tmp_tac.src2_type == TAC_OPERAND::NUM)){
        add_t1_encoding_specification_armv6m(TAC, tmp_tac.dest_v_reg_idx, tmp_tac.src1_v, tmp_tac.src2_v);
    }
    #endif


}

void allocate_array_in_memory_security_stack(std::vector<stack_local_variable_tracking_struct>& stack_mapping_tracker, const virtual_variable_information& virt_var_info, std::string base_name, std::vector<operand>& depth_per_dimension){

    uint32_t full_dimension = 1; //std::accumulate((*depth_per_dimension.begin()).op_data, (*depth_per_dimension.end()).op_data, 1, std::multiplies<uint32_t>());
    for(auto& elem: depth_per_dimension){
        full_dimension *= elem.op_data;
    }
    uint32_t prev_idx = stack_mapping_tracker.size();
    uint32_t prev_position = stack_mapping_tracker.back().position;
    stack_mapping_tracker.resize(stack_mapping_tracker.size() + full_dimension);
    std::vector<uint32_t> entry_ctr(depth_per_dimension.size(), 0);

    std::vector<uint32_t> product_entries;
    for(uint32_t j = 0; j < depth_per_dimension.size(); ++j){
        //product of all following dimensions
        uint32_t prod = 1;

        for(uint32_t k = j + 1; k < depth_per_dimension.size(); ++k){
            prod *= depth_per_dimension.at(k).op_data;
        }
        product_entries.emplace_back(prod);
    }

    for(uint32_t i = 0; i < full_dimension; ++i){
        std::string tmp = base_name;
        std::string add_on = "";

        for(uint32_t j = 0; j < depth_per_dimension.size(); ++j){
            add_on += "[" + std::to_string(entry_ctr.at(j)) + "]";
        }
        for(uint32_t j = 0; j < depth_per_dimension.size(); ++j){
            if(((i+1) % product_entries.at(j) == 0)){
                entry_ctr.at(j)++;
                entry_ctr.at(j) = entry_ctr.at(j) % depth_per_dimension.at(j).op_data;
            }
        }
        tmp += add_on;
        prev_position += virt_var_info.data_type_size;
        stack_mapping_tracker.at(prev_idx + i).curr_value = tmp;
        stack_mapping_tracker.at(prev_idx + i).position = prev_position;
        stack_mapping_tracker.at(prev_idx + i).sensitivity = 0;
        stack_mapping_tracker.at(prev_idx + i).base_value = base_name;
        
    }
}

void generate_local_variable_register_tracker(virtual_variables_vec& virtual_variables, std::map<std::string, bool>& presence_of_local_variables_in_virtual_register){
    for(auto& virt_var: virtual_variables){
        if(is_local(virt_var)){
            std::string original_name = generate_original_name(virt_var, virtual_variables);
            presence_of_local_variables_in_virtual_register[original_name] = false;
        }
    }
}

void generate_input_variable_register_tracker(virtual_variables_vec& virtual_variables, std::map<std::string, bool>& presence_of_input_variables_in_virtual_register){
    for(auto& virt_var: virtual_variables){
        if(is_input(virt_var)){
            std::string original_name = generate_original_name(virt_var, virtual_variables);
            presence_of_input_variables_in_virtual_register[original_name] = false;

            
        }
    }
}

//given the control flow TAC and a use point return the bb number that contains the use point
uint32_t get_bb_number_of_instruction_idx(std::vector<TAC_bb>& control_flow_TAC, uint32_t use_point){
    uint32_t bb_position_in_cfg_tac = 0;
    uint32_t cummulative_number_of_instruction = control_flow_TAC.at(bb_position_in_cfg_tac).TAC.size();
    while(cummulative_number_of_instruction < use_point){
        bb_position_in_cfg_tac++;
        cummulative_number_of_instruction += control_flow_TAC.at(bb_position_in_cfg_tac).TAC.size();
    }

    return control_flow_TAC.at(bb_position_in_cfg_tac).TAC_bb_number;
}

//given the control flow TAC and a use point return the bb idx that contains the use point
uint32_t get_bb_idx_of_instruction_idx(std::vector<TAC_bb>& control_flow_TAC, uint32_t use_point){
    uint32_t bb_position_in_cfg_tac = 0;
    uint32_t cummulative_number_of_instruction = control_flow_TAC.at(bb_position_in_cfg_tac).TAC.size();
    while(cummulative_number_of_instruction < use_point){
        bb_position_in_cfg_tac++;
        cummulative_number_of_instruction += control_flow_TAC.at(bb_position_in_cfg_tac).TAC.size();
    }

    return bb_position_in_cfg_tac;
}


//group use points together that have to follow each other
//All use points within a bb belong to one group
//we can merge the groups of two bb if there is only one path and this path goes through those two bb -> they always will be executed sequentially
void split_into_bb_ranges_of_intervals(std::vector<TAC_bb>& control_flow_TAC, liveness_interval_struct& liveness_interval, const std::map<uint32_t, std::vector<uint32_t>>& predecessors){
    

    liveness_interval.split_bb_ranges.clear();

    for(uint32_t use_point_idx = 0; use_point_idx < liveness_interval.liveness_used.size(); ++use_point_idx){
        std::tuple<std::vector<uint32_t>, std::vector<uint32_t>> split_bb_range;
        std::vector<uint32_t> used_points;
        std::vector<uint32_t> common_bb_numbers;
        uint32_t common_bb_number = get_bb_number_of_instruction_idx(control_flow_TAC, liveness_interval.liveness_used.at(use_point_idx));
        used_points.emplace_back(liveness_interval.liveness_used.at(use_point_idx));
        uint32_t common_use_point = use_point_idx + 1;
        while((common_use_point < liveness_interval.liveness_used.size()) && (common_bb_number == get_bb_number_of_instruction_idx(control_flow_TAC, liveness_interval.liveness_used.at(common_use_point)))){
            used_points.emplace_back(liveness_interval.liveness_used.at(common_use_point));
            common_use_point++;
        }

        use_point_idx = common_use_point - 1;
        common_bb_numbers.emplace_back(common_bb_number);
        split_bb_range = std::make_tuple(common_bb_numbers, used_points);


        //check if new bb always follows one bb from liveness_interval vector or one bb from liveness_interval vector always follows new bb. if yes we can merge the ranges together
        bool merged_ranges = false;
        for(auto& interval_bb_range: liveness_interval.split_bb_ranges){
            std::vector<uint32_t> bbs_in_range = std::get<0>(interval_bb_range);
            for(uint32_t i = 0; i < bbs_in_range.size(); ++i){
                auto predecessors_of_bb_in_range = predecessors.find(bbs_in_range.at(i));
                if((predecessors_of_bb_in_range != predecessors.end()) && (predecessors_of_bb_in_range->second.size() == 1) && (predecessors_of_bb_in_range->second.at(0) == bbs_in_range.at(i))){ //new bb number is only predecessor of bb in range -> can merge

                    std::get<0>(interval_bb_range).emplace_back(bbs_in_range.at(i));

                    std::get<1>(interval_bb_range).insert(std::get<1>(interval_bb_range).end(), used_points.begin(), used_points.end());
                    std::sort( std::get<1>(interval_bb_range).begin(), std::get<1>(interval_bb_range).end() );
                    std::get<1>(interval_bb_range).erase( std::unique( std::get<1>(interval_bb_range).begin(), std::get<1>(interval_bb_range).end() ), std::get<1>(interval_bb_range).end() );
                    merged_ranges = true;
                    break;
                }
                predecessors_of_bb_in_range = predecessors.find(bbs_in_range.at(i));
                if((predecessors_of_bb_in_range != predecessors.end()) && (predecessors_of_bb_in_range->second.size() == 1) && (predecessors_of_bb_in_range->second.at(0) == common_bb_number) && (used_points.back() < std::get<1>(interval_bb_range).at(0))){ //new bb number is only predecessor of bb in range -> can merge

                    std::get<0>(interval_bb_range).emplace_back(predecessors_of_bb_in_range->second.at(0));
                    std::get<1>(interval_bb_range).insert(std::get<1>(interval_bb_range).end(), used_points.begin(), used_points.end());
              
                    std::sort( std::get<1>(interval_bb_range).begin(), std::get<1>(interval_bb_range).end() );
                    std::get<1>(interval_bb_range).erase( std::unique( std::get<1>(interval_bb_range).begin(), std::get<1>(interval_bb_range).end() ), std::get<1>(interval_bb_range).end() );

                    merged_ranges = true;
                    break;
                }
            }
            if(merged_ranges){
                break;
            }
        }
        if(!merged_ranges){
            liveness_interval.split_bb_ranges.emplace_back(split_bb_range);
        }
    }

}



void generate_flow_graph_from_TAC(std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC){
    uint32_t TAC_pos = 0;
    while(TAC_pos != TAC.size()){
        TAC_bb curr_TAC_bb;
        curr_TAC_bb.TAC_bb_number = TAC.at(TAC_pos).label_name;

        uint32_t start_pos_of_bb = TAC_pos;
        TAC_pos += 1;

        while((TAC_pos != TAC.size()) && (TAC.at(TAC_pos).op != Operation::LABEL)){
            TAC_pos++;
        }
        uint32_t end_pos_of_bb = TAC_pos - 1;

        if(start_pos_of_bb <= end_pos_of_bb){
            std::copy(TAC.begin() + start_pos_of_bb, TAC.begin() + end_pos_of_bb + 1, std::back_inserter(curr_TAC_bb.TAC));
        } 
        control_flow_TAC.push_back(curr_TAC_bb);
    }
}

void convert_flow_graph_TAC_to_linear_TAC(std::vector<TAC_bb>& control_flow_TAC, std::vector<TAC_type>& TAC){
    TAC.clear();
    for(uint32_t tac_bb_idx = 0; tac_bb_idx < control_flow_TAC.size(); ++tac_bb_idx){

        TAC.insert(TAC.end(), control_flow_TAC.at(tac_bb_idx).TAC.begin(), control_flow_TAC.at(tac_bb_idx).TAC.end());

    }
}


void synchronize_instruction_index_of_liveness_intervals(std::vector<liveness_interval_struct>& liveness_intervals, uint32_t instruction_idx_to_insert){
    for(uint32_t live_intervals_idx = 0; live_intervals_idx < liveness_intervals.size(); ++live_intervals_idx){
        for(uint32_t use_point_idx = 0; use_point_idx < liveness_intervals.at(live_intervals_idx).liveness_used.size(); ++use_point_idx){
            if(liveness_intervals.at(live_intervals_idx).liveness_used.at(use_point_idx) >= instruction_idx_to_insert){
                liveness_intervals.at(live_intervals_idx).liveness_used.at(use_point_idx) += 1;
            }
        }
        for(uint32_t range_idx = 0; range_idx < liveness_intervals.at(live_intervals_idx).ranges.size(); ++range_idx){
            if(std::get<0>(liveness_intervals.at(live_intervals_idx).ranges.at(range_idx)) >= instruction_idx_to_insert){
                std::get<0>(liveness_intervals.at(live_intervals_idx).ranges.at(range_idx)) += 1;
            }
            if(std::get<1>(liveness_intervals.at(live_intervals_idx).ranges.at(range_idx)) >= instruction_idx_to_insert){
                std::get<1>(liveness_intervals.at(live_intervals_idx).ranges.at(range_idx)) += 1;
            }
        }

        for(uint32_t split_range_idx = 0; split_range_idx < liveness_intervals.at(live_intervals_idx).split_bb_ranges.size(); ++split_range_idx){
            for(uint32_t use_point_idx = 0; use_point_idx < std::get<1>(liveness_intervals.at(live_intervals_idx).split_bb_ranges.at(split_range_idx)).size(); ++use_point_idx){
                if(std::get<1>(liveness_intervals.at(live_intervals_idx).split_bb_ranges.at(split_range_idx)).at(use_point_idx) >= instruction_idx_to_insert){
                    std::get<1>(liveness_intervals.at(live_intervals_idx).split_bb_ranges.at(split_range_idx)).at(use_point_idx) += 1;
                }
            }
        }
    }
}

/**
 * @brief internal function calls might return a sensitive value, this value is in r0 and will be moved in another register, make sure that r0 gets cleared if necessary (i.e. if register where value is returned is not r0)
 * 
 * @param TAC 
 */
void handle_sensitive_internal_return_statements(std::vector<TAC_type>& TAC){
    for(uint32_t tac_idx = 0; tac_idx < TAC.size(); ++tac_idx){
        if((TAC.at(tac_idx).op == Operation::FUNC_RES_MOV) && (TAC.at(tac_idx).dest_sensitivity == 1) && (TAC.at(tac_idx).dest_v_reg_idx != 0)){
            TAC_type tmp_tac;
            tmp_tac.op = Operation::MOV;
            tmp_tac.dest_type = TAC_OPERAND::R_REG;
            tmp_tac.dest_sensitivity = 0;
            tmp_tac.dest_v_reg_idx = 0;
            tmp_tac.src1_type = TAC_OPERAND::R_REG;
            tmp_tac.src1_sensitivity = 0;
            tmp_tac.src1_v = 11;
            tmp_tac.src2_type = TAC_OPERAND::NONE;
            tmp_tac.src2_sensitivity = 0;
            tmp_tac.sensitivity = 0;
            tmp_tac.psr_status_flag = false;

            TAC.insert(TAC.begin() + tac_idx + 1, tmp_tac);
        }
    }
}

void insert_branch_trampoline(std::vector<TAC_type>& TAC, uint32_t position, std::vector<std::tuple<int32_t, int32_t>>& condition_branch_source_destination, uint32_t idx_of_original_conditional_branch){
    //get biggest bb label name
    uint32_t insertion_position = position >> 1; // each instructino is 2 bytes wide thus translate it to instruction size

    uint32_t biggest_label_number = 0;
    for(uint32_t tac_idx = 0; tac_idx < TAC.size(); ++tac_idx){
        if(TAC.at(tac_idx).op == Operation::LABEL){
            if(biggest_label_number < TAC.at(tac_idx).label_name){
                biggest_label_number = TAC.at(tac_idx).label_name;
            }
        }
    }
    

    uint32_t new_label_number = biggest_label_number + 1;

    uint32_t overjump_label_number = biggest_label_number + 2;

    //change label of conditional branch to bigget laben name + 1;
    uint32_t dest_label_of_original_conditional_branch = TAC.at(idx_of_original_conditional_branch).branch_destination;
    TAC.at(idx_of_original_conditional_branch).branch_destination = new_label_number;

    //insert at position: 1.branch to jump over new bb 2. label with biggest label name + 1 3. jump to 
    TAC_type tmp_tac;


    tmp_tac.op = Operation::BRANCH;
    tmp_tac.dest_type = TAC_OPERAND::NONE;
    tmp_tac.dest_sensitivity = 0;
    tmp_tac.src1_type = TAC_OPERAND::NONE;
    tmp_tac.src1_sensitivity = 0;
    tmp_tac.src2_type = TAC_OPERAND::NONE;
    tmp_tac.src2_sensitivity = 0;
    tmp_tac.sensitivity = 0;
    tmp_tac.branch_destination = overjump_label_number;

    TAC.insert(TAC.begin() + insertion_position, tmp_tac);

    for(std::tuple<int32_t, int32_t>& elem: condition_branch_source_destination){
        if(static_cast<uint32_t>(std::get<0>(elem)) >= position){
            std::get<0>(elem) += 2;
        }
        if(static_cast<uint32_t>(std::get<1>(elem)) >= position){
            std::get<1>(elem) += 2;
        }
    }

    tmp_tac.sensitivity = 0;
    tmp_tac.op = Operation::LABEL;
    tmp_tac.dest_type = TAC_OPERAND::NONE;
    tmp_tac.dest_sensitivity = 0;
    tmp_tac.src1_type = TAC_OPERAND::NONE;
    tmp_tac.src1_sensitivity = 0;
    tmp_tac.src2_type = TAC_OPERAND::NONE;
    tmp_tac.src2_sensitivity = 0;
    tmp_tac.sensitivity = 0;
    tmp_tac.label_name = new_label_number;

    TAC.insert(TAC.begin() + insertion_position + 1, tmp_tac);

    for(auto& elem: condition_branch_source_destination){
        if(static_cast<uint32_t>(std::get<0>(elem)) >= position + 2){
            std::get<0>(elem) += 2;
        }
        if(static_cast<uint32_t>(std::get<1>(elem)) >= position + 2){
            std::get<1>(elem) += 2;
        }
    }

    tmp_tac.op = Operation::BRANCH;
    tmp_tac.dest_type = TAC_OPERAND::NONE;
    tmp_tac.dest_sensitivity = 0;
    tmp_tac.src1_type = TAC_OPERAND::NONE;
    tmp_tac.src1_sensitivity = 0;
    tmp_tac.src2_type = TAC_OPERAND::NONE;
    tmp_tac.src2_sensitivity = 0;
    tmp_tac.sensitivity = 0;
    tmp_tac.branch_destination = dest_label_of_original_conditional_branch;

    TAC.insert(TAC.begin() + insertion_position + 2, tmp_tac);

    for(auto& elem: condition_branch_source_destination){
        if(static_cast<uint32_t>(std::get<0>(elem)) >= position + 4){
            std::get<0>(elem) += 2;
        }
        if(static_cast<uint32_t>(std::get<1>(elem)) >= position + 4){
            std::get<1>(elem) += 2;
        }
    }

    tmp_tac.op = Operation::LABEL;
    tmp_tac.dest_type = TAC_OPERAND::NONE;
    tmp_tac.dest_sensitivity = 0;
    tmp_tac.src1_type = TAC_OPERAND::NONE;
    tmp_tac.src1_sensitivity = 0;
    tmp_tac.src2_type = TAC_OPERAND::NONE;
    tmp_tac.src2_sensitivity = 0;
    tmp_tac.sensitivity = 0;
    tmp_tac.label_name = overjump_label_number;

    TAC.insert(TAC.begin() + insertion_position + 3, tmp_tac);

    for(auto& elem: condition_branch_source_destination){
        if(static_cast<uint32_t>(std::get<0>(elem)) >= position + 6){
            std::get<0>(elem) += 2;
        }
        if(static_cast<uint32_t>(std::get<1>(elem)) >= position + 6){
            std::get<1>(elem) += 2;
        }
    }


}



void bring_function_parameters_in_correct_order(std::vector<TAC_type>& TAC){
    for(uint32_t tac_idx = 0; tac_idx < TAC.size(); ++tac_idx){
        if((TAC.at(tac_idx).op == Operation::FUNC_MOVE) && (TAC.at(tac_idx).func_move_idx == 0)){
            //collect all function move operations
            uint32_t func_move_idx = tac_idx;
            std::vector<TAC_type> func_move_instrs;
            std::vector<TAC_type> break_func_move_instrs;
            std::vector<TAC_type> all_instrs_from_first_func_move_to_call;
            func_move_instrs.clear(),
            break_func_move_instrs.clear();
            while(TAC.at(func_move_idx).op != Operation::FUNC_CALL){
                if(TAC.at(func_move_idx).op == Operation::FUNC_MOVE){
                    func_move_instrs.push_back(TAC.at(func_move_idx));
                }
                else{ //has to be break transition instruction
                    break_func_move_instrs.push_back(TAC.at(func_move_idx));
                }
                all_instrs_from_first_func_move_to_call.push_back(TAC.at(func_move_idx));
                func_move_idx++;
            }

            uint32_t number_of_original_func_move_instr = func_move_instrs.size() + break_func_move_instrs.size();


            std::vector<TAC_type> correct_func_move_instr_order;

            //all func_move instructions where the destination register is not a source register in any other instruction is moved to the top
            auto it_func_move = func_move_instrs.begin();
            while(it_func_move != func_move_instrs.end()){
                if(std::find_if(func_move_instrs.begin(), func_move_instrs.end(), [it_func_move](const TAC_type& t){return (it_func_move->func_move_idx != t.func_move_idx) && (t.src1_type == TAC_OPERAND::R_REG) && (static_cast<uint32_t>(it_func_move->dest_v_reg_idx) == t.src1_v);}) == func_move_instrs.end()){
                    correct_func_move_instr_order.push_back(*it_func_move);
                    it_func_move = func_move_instrs.erase(it_func_move);
                }
                else{
                    it_func_move++;
                }
            }

            if(!func_move_instrs.empty()){
                std::vector<TAC_type> src_func_move_instr_is_number;

                bool found_initial_elem = false;
                for(uint32_t i = 0; i < func_move_instrs.size(); ++i){
                    auto comparison_elem = func_move_instrs.at(i);
                    if(std::find_if(func_move_instrs.begin(), func_move_instrs.end(), [comparison_elem](const TAC_type& t){return (comparison_elem.func_move_idx != t.func_move_idx) && (static_cast<uint32_t>(comparison_elem.dest_v_reg_idx) == t.src1_v) && (t.src1_type == TAC_OPERAND::R_REG);}) == func_move_instrs.end()){
                        correct_func_move_instr_order.push_back(comparison_elem);
                        func_move_instrs.erase(func_move_instrs.begin() + i);
                        found_initial_elem = true;
                        break;
                    }
                }

                if(!found_initial_elem){
                    correct_func_move_instr_order.push_back(func_move_instrs.at(0));
                    func_move_instrs.erase(func_move_instrs.begin());
                }


                while(!func_move_instrs.empty()){
                    //get real source register of last correct move instruction
                    if(func_move_instrs.at(0).src1_type == TAC_OPERAND::R_REG){
                        uint32_t real_source_reg_of_correct_move_instr = correct_func_move_instr_order.back().src1_v;
                        if((real_source_reg_of_correct_move_instr != static_cast<uint32_t>(correct_func_move_instr_order.back().dest_v_reg_idx)) && (real_source_reg_of_correct_move_instr < 4)){
                            auto it = std::find_if(func_move_instrs.begin(), func_move_instrs.end(), [real_source_reg_of_correct_move_instr](const TAC_type& t){return static_cast<uint32_t>(t.dest_v_reg_idx) == real_source_reg_of_correct_move_instr;});
                            if(it != func_move_instrs.end()){
                                correct_func_move_instr_order.push_back(*it);

                                func_move_instrs.erase(it);

                            }
                            else{
                                correct_func_move_instr_order.push_back(func_move_instrs.at(0));
                                func_move_instrs.erase(func_move_instrs.begin());
                            }

                        }
                        else{
                            correct_func_move_instr_order.push_back(func_move_instrs.at(0));
                            func_move_instrs.erase(func_move_instrs.begin());

                        }
                    }
                    else{
                        if(func_move_instrs.at(0).src1_v > 255){
                            func_move_instrs.at(0).op = Operation::LOADWORD;
                            func_move_instrs.at(0).psr_status_flag = false;
                        }
                        src_func_move_instr_is_number.push_back(func_move_instrs.at(0));
                        func_move_instrs.erase(func_move_instrs.begin());
                    }

                }

                correct_func_move_instr_order.insert(correct_func_move_instr_order.end(), src_func_move_instr_is_number.begin(), src_func_move_instr_is_number.end());
            }


            //if we have form mov r0, r1; mov r1, r0; mov r2, r3; mov r3, r2; we need to tmp moves but the moves must happen at different positions to not overwrite the first tmp value
            uint32_t second_move_insertion = UINT32_MAX;
            if((correct_func_move_instr_order.size() == 4) && (static_cast<uint32_t>(correct_func_move_instr_order.at(0).dest_v_reg_idx) == correct_func_move_instr_order.at(1).src1_v) && (correct_func_move_instr_order.at(1).src1_type == TAC_OPERAND::R_REG) && (static_cast<uint32_t>(correct_func_move_instr_order.at(2).dest_v_reg_idx) == correct_func_move_instr_order.at(3).src1_v) && (correct_func_move_instr_order.at(3).src1_type == TAC_OPERAND::R_REG)){
                second_move_insertion = 2;
            }


            std::vector<std::tuple<uint32_t, uint32_t>> dirty_registers; //(real register number, move idx)

            uint8_t sensitivity_of_high_register_r8 = 0;

            uint32_t insertion_position_offset = 0;


            for(uint32_t i = 0; i < correct_func_move_instr_order.size(); ++i){
                if(correct_func_move_instr_order.at(i).src1_type == TAC_OPERAND::R_REG){
                    uint32_t real_source_reg_of_correct_move_instr = correct_func_move_instr_order.at(i).src1_v;
                    //register is dirty, i.e. was overwritten with different value -> need to insert temporary move to higher registers
                    if(std::find_if(dirty_registers.begin(), dirty_registers.end(), [real_source_reg_of_correct_move_instr](const std::tuple<uint32_t, uint32_t>& t){return std::get<0>(t) == real_source_reg_of_correct_move_instr;}) != dirty_registers.end()){
                        correct_func_move_instr_order.at(i).src1_v = 8;


                        TAC_type tmp_tac;
                        tmp_tac.op = Operation::MOV;
                        tmp_tac.sensitivity = correct_func_move_instr_order.at(i).src1_sensitivity; 
                        tmp_tac.dest_type = TAC_OPERAND::R_REG; 
                        tmp_tac.dest_v_reg_idx = 8;
                        tmp_tac.dest_sensitivity = correct_func_move_instr_order.at(i).src1_sensitivity; 
                        tmp_tac.src1_type = TAC_OPERAND::R_REG; 
                        tmp_tac.src1_v = real_source_reg_of_correct_move_instr;
                        tmp_tac.src1_sensitivity = correct_func_move_instr_order.at(i).src1_sensitivity; 
                        tmp_tac.src2_type = TAC_OPERAND::NONE; 
                        tmp_tac.src2_v = UINT32_MAX;
                        tmp_tac.src2_sensitivity = 0;

                        if(i == second_move_insertion){
                            correct_func_move_instr_order.insert(correct_func_move_instr_order.begin() + insertion_position_offset + 2, tmp_tac);
                        }
                        else{
                            correct_func_move_instr_order.insert(correct_func_move_instr_order.begin() + insertion_position_offset, tmp_tac);
                        }
                        

                        if(sensitivity_of_high_register_r8 == 1){
                            TAC_type break_high_reg_tac;
                            break_high_reg_tac.op = Operation::MOV;
                            break_high_reg_tac.sensitivity = 0; 
                            break_high_reg_tac.dest_type = TAC_OPERAND::R_REG; 
                            break_high_reg_tac.dest_v_reg_idx = 8;
                            break_high_reg_tac.dest_sensitivity = 0; 
                            break_high_reg_tac.src1_type = TAC_OPERAND::R_REG; 
                            break_high_reg_tac.src1_v = 11;
                            break_high_reg_tac.src1_sensitivity = 0; 
                            break_high_reg_tac.src2_type = TAC_OPERAND::NONE; 
                            break_high_reg_tac.src2_v = UINT32_MAX;
                            break_high_reg_tac.src2_sensitivity = 0;

                            // correct_func_move_instr_order.insert(correct_func_move_instr_order.begin() + insertion_position_offset, tmp_tac);
                            if(i == second_move_insertion){
                                correct_func_move_instr_order.insert(correct_func_move_instr_order.begin() + insertion_position_offset + 2, tmp_tac);
                            }
                            else{
                                correct_func_move_instr_order.insert(correct_func_move_instr_order.begin() + insertion_position_offset, tmp_tac);
                            }
                            insertion_position_offset += 2;
                        }
                        else{
                            insertion_position_offset++;
                        }

                        sensitivity_of_high_register_r8 = correct_func_move_instr_order.at(i).src1_sensitivity;

                    }

                    if(static_cast<uint32_t>(correct_func_move_instr_order.at(i).dest_v_reg_idx) != correct_func_move_instr_order.at(i).src1_v){
                        dirty_registers.emplace_back(std::make_tuple(correct_func_move_instr_order.at(i).dest_v_reg_idx, i));
                    }
                }

            }

            //remove sensitive value from reigster file
            if(sensitivity_of_high_register_r8 == 1){
                TAC_type tmp_tac;
                tmp_tac.op = Operation::MOV;
                tmp_tac.sensitivity = 0; 
                tmp_tac.dest_type = TAC_OPERAND::R_REG; 
                tmp_tac.dest_v_reg_idx = 8;
                tmp_tac.dest_sensitivity = 0; 
                tmp_tac.src1_type = TAC_OPERAND::R_REG; 
                tmp_tac.src1_v = 11;
                tmp_tac.src1_sensitivity = 0; 
                tmp_tac.src2_type = TAC_OPERAND::NONE; 
                tmp_tac.src2_v = UINT32_MAX;
                tmp_tac.src2_sensitivity = 0;

                correct_func_move_instr_order.push_back(tmp_tac);
            }

            //bring break transitions of source reg in func move in correct order
            for(uint32_t i = 0; i < break_func_move_instrs.size(); ++i){
                uint32_t src_reg_func_move = break_func_move_instrs.at(i).dest_v_reg_idx;
                //search where src reg was used last
                auto it = std::find_if(correct_func_move_instr_order.rbegin(), correct_func_move_instr_order.rend(), [src_reg_func_move](const TAC_type& t){return (t.op == Operation::FUNC_MOVE) && (t.src1_type == TAC_OPERAND::R_REG) && (t.src1_v == src_reg_func_move);});
                if(it != correct_func_move_instr_order.rend()){
                    auto forward_it = (it+1).base();
                    correct_func_move_instr_order.insert(forward_it+1, break_func_move_instrs.at(i));
                }
            }

            //more instructions can be found between first func_move and func call (e.g. stores)
            if(correct_func_move_instr_order.size() != all_instrs_from_first_func_move_to_call.size()){
                for(const TAC_type& elem: all_instrs_from_first_func_move_to_call){
                    if(std::find_if(correct_func_move_instr_order.begin(), correct_func_move_instr_order.end(), [elem](const TAC_type& t){return t == elem;}) == correct_func_move_instr_order.end()){
                        uint32_t dist = std::find(all_instrs_from_first_func_move_to_call.begin(), all_instrs_from_first_func_move_to_call.end(), elem) - all_instrs_from_first_func_move_to_call.begin();
                        correct_func_move_instr_order.insert(correct_func_move_instr_order.begin() + dist, elem);                        
                    }
                }
            }
            


            for(uint32_t i = 0; i < number_of_original_func_move_instr; ++i){
                TAC.erase(TAC.begin() + tac_idx);
            }

            for(uint32_t i = 0; i < correct_func_move_instr_order.size(); ++i){
                TAC.insert(TAC.begin() + tac_idx + i, correct_func_move_instr_order.at(i));
            }
            tac_idx = func_move_idx + 1;

        }
    }
}


//The permitted offset of conditional branches is in the range -256 to 254
void build_jump_trampoline_to_fulfill_specification(std::vector<TAC_type>& TAC){
    std::vector<std::tuple<int32_t, int32_t>> condition_branch_source_destination;

    for(uint32_t tac_idx = 0; tac_idx < TAC.size(); ++tac_idx){
        if((TAC.at(tac_idx).op == Operation::GREATER) || (TAC.at(tac_idx).op == Operation::GREATER_EQUAL) || (TAC.at(tac_idx).op == Operation::LESS) || (TAC.at(tac_idx).op == Operation::LESS_EQUAL) || (TAC.at(tac_idx).op == Operation::EQUAL) || (TAC.at(tac_idx).op == Operation::NOT_EQUAL)){
            int32_t source_idx = tac_idx * 2; //each instruction is 16 bit long -> each instruction has size of 2 bytes
            uint32_t dest_bb = TAC.at(tac_idx).branch_destination;

            //find label with same bb number
            auto it = std::find_if(TAC.begin(), TAC.end(), [dest_bb](const TAC_type& t){return (t.op == Operation::LABEL) && (t.label_name == dest_bb);});
            int32_t dest_idx = (it - TAC.begin()) * 2;

            condition_branch_source_destination.push_back(std::make_tuple(source_idx, dest_idx));
        }
    }


    auto cond_branch_it = condition_branch_source_destination.begin();

    while(cond_branch_it != condition_branch_source_destination.end()){
        int32_t dest_idx = std::get<1>(*cond_branch_it);
        int32_t src_idx =  std::get<0>(*cond_branch_it);
        int32_t branch_distance = (dest_idx) - (src_idx); //compute offset between source and destination (both are instruction distance times 2 bc in thumb mode every instruction is 2 bytes)
        if((branch_distance < -256) || (branch_distance > 254)){ //need to insert trampolin
            uint8_t above_below_trampoline_insertion; // = 0 -> bb is below, = 1 -> bb is above
            if(branch_distance < -256){
                above_below_trampoline_insertion = 1;
            }
            else{
                above_below_trampoline_insertion = 0;
            }

            int32_t insertion_range_start; 
            int32_t insertion_range_end;
            if(above_below_trampoline_insertion == 0){
                insertion_range_start = branch_distance - 254;
                insertion_range_end = 254;

                uint32_t range_idx_around_branch_to_check_start = src_idx + insertion_range_start;
                uint32_t range_idx_around_branch_to_check_end = src_idx + insertion_range_end;


                //go through all intervals in that range
                auto tmp_it = std::find_if(condition_branch_source_destination.begin(), condition_branch_source_destination.end(), [cond_branch_it, range_idx_around_branch_to_check_end, range_idx_around_branch_to_check_start](const std::tuple<int32_t, int32_t>& t){return (t != *cond_branch_it) && ((static_cast<uint32_t>(std::get<0>(t)) < range_idx_around_branch_to_check_end) && (static_cast<uint32_t>(std::get<1>(t)) > range_idx_around_branch_to_check_start)) || ((static_cast<uint32_t>(std::get<0>(t)) < range_idx_around_branch_to_check_end) && (static_cast<uint32_t>(std::get<1>(t)) > range_idx_around_branch_to_check_start));});
                while(tmp_it != condition_branch_source_destination.end()){
                    
                    uint32_t partner_dest_idx = static_cast<uint32_t>(std::get<1>(*tmp_it));
                    uint32_t partner_src_idx =  static_cast<uint32_t>(std::get<0>(*tmp_it));
                    int32_t partner_branch_distance = partner_dest_idx - partner_src_idx;

                    int32_t number_of_partner_trampolines;
                    if(partner_branch_distance < 0){
                        number_of_partner_trampolines = partner_branch_distance / -256;
                        partner_branch_distance -= 4; //trampoline inserts four instructions
                        if(number_of_partner_trampolines != (partner_branch_distance / -256)){ //reduce the valid range
                            if((range_idx_around_branch_to_check_end < partner_src_idx) && (range_idx_around_branch_to_check_start < partner_dest_idx)){
                                range_idx_around_branch_to_check_end = partner_dest_idx;
                            }
                            else if((range_idx_around_branch_to_check_end > partner_src_idx) && (range_idx_around_branch_to_check_start > partner_dest_idx)){
                                range_idx_around_branch_to_check_start = partner_src_idx;
                            }
                            else if((range_idx_around_branch_to_check_end > partner_src_idx) && (range_idx_around_branch_to_check_start < partner_dest_idx)){
                                range_idx_around_branch_to_check_end = partner_dest_idx;
                            }
                            else if((range_idx_around_branch_to_check_end < partner_src_idx) && (range_idx_around_branch_to_check_start > partner_dest_idx)){
                                range_idx_around_branch_to_check_end = UINT32_MAX;
                                range_idx_around_branch_to_check_start = UINT32_MAX;
                            }
                        }
                    }
                    else{
                        number_of_partner_trampolines = partner_branch_distance / 254;
                        partner_branch_distance += 4;
                        if(number_of_partner_trampolines != (partner_branch_distance / 254)){
                            if((range_idx_around_branch_to_check_end < partner_dest_idx) && (range_idx_around_branch_to_check_start < partner_src_idx)){
                                range_idx_around_branch_to_check_end = partner_src_idx;
                            }
                            else if((range_idx_around_branch_to_check_end > partner_dest_idx) && (range_idx_around_branch_to_check_start > partner_src_idx)){
                                range_idx_around_branch_to_check_start = partner_dest_idx;
                            }
                            else if((range_idx_around_branch_to_check_end > partner_dest_idx) && (range_idx_around_branch_to_check_start < partner_src_idx)){
                                range_idx_around_branch_to_check_end = partner_src_idx;
                            }
                            else if((range_idx_around_branch_to_check_end < partner_dest_idx) && (range_idx_around_branch_to_check_start > partner_src_idx)){
                                range_idx_around_branch_to_check_end = UINT32_MAX;
                                range_idx_around_branch_to_check_start = UINT32_MAX;
                            }
                        }
                    }
                    

                    tmp_it++;
                    tmp_it = std::find_if(tmp_it, condition_branch_source_destination.end(), [cond_branch_it, range_idx_around_branch_to_check_end, range_idx_around_branch_to_check_start](const std::tuple<int32_t, int32_t>& t){return (t != *cond_branch_it) && (static_cast<uint32_t>(std::get<0>(t)) < range_idx_around_branch_to_check_end) && (static_cast<uint32_t>(std::get<1>(t)) > range_idx_around_branch_to_check_start);});
                }

                if((range_idx_around_branch_to_check_end != UINT32_MAX) && (range_idx_around_branch_to_check_start != UINT32_MAX)){
                    uint32_t position = range_idx_around_branch_to_check_start - 16; // - 16 should be computed based on potential other trampolines inserted
                    uint32_t idx_of_original_conditional_branch = src_idx >> 1;
                    insert_branch_trampoline(TAC, position, condition_branch_source_destination, idx_of_original_conditional_branch);
                    int cond_branch_src_idx = std::get<0>(*cond_branch_it);
                    int cond_branch_dest_idx = std::get<1>(*cond_branch_it);
                    condition_branch_source_destination.emplace_back(std::make_tuple(src_idx, position + 4));
                    cond_branch_it = std::find_if(condition_branch_source_destination.begin(), condition_branch_source_destination.end(), [cond_branch_src_idx, cond_branch_dest_idx](const std::tuple<int32_t, int32_t>& t){return (std::get<0>(t) == cond_branch_src_idx) && (std::get<1>(t) == cond_branch_dest_idx);});

                    condition_branch_source_destination.erase(cond_branch_it);
                }
                else{
                    uint32_t position = 116;
                    uint32_t idx_of_original_conditional_branch = src_idx >> 1;
                    insert_branch_trampoline(TAC, position, condition_branch_source_destination, idx_of_original_conditional_branch);
                    int cond_branch_src_idx = std::get<0>(*cond_branch_it);
                    int cond_branch_dest_idx = std::get<1>(*cond_branch_it);
                    condition_branch_source_destination.emplace_back(std::make_tuple(src_idx, position + 4));
                    cond_branch_it = std::find_if(condition_branch_source_destination.begin(), condition_branch_source_destination.end(), [cond_branch_src_idx, cond_branch_dest_idx](const std::tuple<int32_t, int32_t>& t){return (std::get<0>(t) == cond_branch_src_idx) && (std::get<1>(t) == cond_branch_dest_idx);});

                    condition_branch_source_destination.erase(cond_branch_it);
                }
            }
            else{
                insertion_range_start = (-1 * branch_distance) - 256;
                insertion_range_end = 256;

                uint32_t range_idx_around_branch_to_check_start = src_idx - insertion_range_end;
                uint32_t range_idx_around_branch_to_check_end = src_idx - insertion_range_start;

                //go through all intervals in that range
                auto tmp_it = std::find_if(condition_branch_source_destination.begin(), condition_branch_source_destination.end(), [cond_branch_it, range_idx_around_branch_to_check_end, range_idx_around_branch_to_check_start](const std::tuple<int32_t, int32_t>& t){return (t != *cond_branch_it) && ((static_cast<uint32_t>(std::get<0>(t)) < range_idx_around_branch_to_check_end) && (static_cast<uint32_t>(std::get<1>(t)) > range_idx_around_branch_to_check_start)) || ((static_cast<uint32_t>(std::get<0>(t)) < range_idx_around_branch_to_check_end) && (static_cast<uint32_t>(std::get<1>(t)) > range_idx_around_branch_to_check_start));});
                while(tmp_it != condition_branch_source_destination.end()){
                    
                    uint32_t partner_dest_idx = static_cast<uint32_t>(std::get<1>(*tmp_it));
                    uint32_t partner_src_idx =  static_cast<uint32_t>(std::get<0>(*tmp_it));
                    int32_t partner_branch_distance = partner_dest_idx - partner_src_idx;

                    int32_t number_of_partner_trampolines;
                    if(partner_branch_distance < 0){
                        number_of_partner_trampolines = partner_branch_distance / -256;
                        partner_branch_distance -= 4; //trampoline inserts four instructions
                        if(number_of_partner_trampolines != (partner_branch_distance / -256)){ //reduce the valid range
                            if((range_idx_around_branch_to_check_end < partner_src_idx) && (range_idx_around_branch_to_check_start < partner_dest_idx)){
                                range_idx_around_branch_to_check_end = partner_dest_idx;
                            }
                            else if((range_idx_around_branch_to_check_end > partner_src_idx) && (range_idx_around_branch_to_check_start > partner_dest_idx)){
                                range_idx_around_branch_to_check_start = partner_src_idx;
                            }
                            else if((range_idx_around_branch_to_check_end > partner_src_idx) && (range_idx_around_branch_to_check_start < partner_dest_idx)){
                                range_idx_around_branch_to_check_end = partner_dest_idx;
                            }
                            else if((range_idx_around_branch_to_check_end < partner_src_idx) && (range_idx_around_branch_to_check_start > partner_dest_idx)){
                                range_idx_around_branch_to_check_end = UINT32_MAX;
                                range_idx_around_branch_to_check_start = UINT32_MAX;
                            }
                        }
                    }
                    else{
                        number_of_partner_trampolines = partner_branch_distance / 254;
                        partner_branch_distance += 4;
                        if(number_of_partner_trampolines != (partner_branch_distance / 254)){
                            if((range_idx_around_branch_to_check_end < partner_dest_idx) && (range_idx_around_branch_to_check_start < partner_src_idx)){
                                range_idx_around_branch_to_check_end = partner_src_idx;
                            }
                            else if((range_idx_around_branch_to_check_end > partner_dest_idx) && (range_idx_around_branch_to_check_start > partner_src_idx)){
                                range_idx_around_branch_to_check_start = partner_dest_idx;
                            }
                            else if((range_idx_around_branch_to_check_end > partner_dest_idx) && (range_idx_around_branch_to_check_start < partner_src_idx)){
                                range_idx_around_branch_to_check_end = partner_src_idx;
                            }
                            else if((range_idx_around_branch_to_check_end < partner_dest_idx) && (range_idx_around_branch_to_check_start > partner_src_idx)){
                                range_idx_around_branch_to_check_end = UINT32_MAX;
                                range_idx_around_branch_to_check_start = UINT32_MAX;
                            }
                        }
                    }
                    

                    tmp_it++;
                    tmp_it = std::find_if(tmp_it, condition_branch_source_destination.end(), [cond_branch_it, range_idx_around_branch_to_check_end, range_idx_around_branch_to_check_start](const std::tuple<int32_t, int32_t>& t){return (t != *cond_branch_it) && (static_cast<uint32_t>(std::get<0>(t)) < range_idx_around_branch_to_check_end) && (static_cast<uint32_t>(std::get<1>(t)) > range_idx_around_branch_to_check_start);});
                }

                if((range_idx_around_branch_to_check_end != UINT32_MAX) && (range_idx_around_branch_to_check_start != UINT32_MAX)){


                    uint32_t position = range_idx_around_branch_to_check_end - ((range_idx_around_branch_to_check_end - range_idx_around_branch_to_check_start) >> 1) ;
                    uint32_t idx_of_original_conditional_branch = src_idx >> 1;
                    insert_branch_trampoline(TAC, position, condition_branch_source_destination, idx_of_original_conditional_branch);
                    int cond_branch_src_idx = std::get<0>(*cond_branch_it);
                    int cond_branch_dest_idx = std::get<1>(*cond_branch_it);
                    condition_branch_source_destination.emplace_back(std::make_tuple(position + 4, src_idx));
                    cond_branch_it = std::find_if(condition_branch_source_destination.begin(), condition_branch_source_destination.end(), [cond_branch_src_idx, cond_branch_dest_idx](const std::tuple<int32_t, int32_t>& t){return (std::get<0>(t) == cond_branch_src_idx) && (std::get<1>(t) == cond_branch_dest_idx);});

                    condition_branch_source_destination.erase(cond_branch_it);
                }
                else{
                    uint32_t position = 122;
                    uint32_t idx_of_original_conditional_branch = src_idx >> 1;
                    insert_branch_trampoline(TAC, position, condition_branch_source_destination, idx_of_original_conditional_branch);
                    int cond_branch_src_idx = std::get<0>(*cond_branch_it);
                    int cond_branch_dest_idx = std::get<1>(*cond_branch_it);
                    condition_branch_source_destination.emplace_back(std::make_tuple(position + 4, src_idx));
                    cond_branch_it = std::find_if(condition_branch_source_destination.begin(), condition_branch_source_destination.end(), [cond_branch_src_idx, cond_branch_dest_idx](const std::tuple<int32_t, int32_t>& t){return (std::get<0>(t) == cond_branch_src_idx) && (std::get<1>(t) == cond_branch_dest_idx);});

                    condition_branch_source_destination.erase(cond_branch_it);
                }

            }
             

            cond_branch_it = condition_branch_source_destination.begin();
        }
        else{
            ++cond_branch_it;
        }
    }

}

void dfs_necessary_store_insertion_in_split_range(std::vector<TAC_bb>& control_flow_TAC, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<bool>& visited, uint32_t bb_idx, const std::map<uint32_t, std::vector<uint32_t>>& successor_map, std::vector<uint32_t>& idx_spilled_intervals_with_same_virt_reg){

    visited.at(bb_idx) = true;

    uint32_t bb_number = control_flow_TAC.at(bb_idx).TAC_bb_number;

    bool found_split_range_for_insertion = false;

    for(uint32_t i = 0; i < idx_spilled_intervals_with_same_virt_reg.size(); ++i){
        for(auto split_range: liveness_intervals.at(idx_spilled_intervals_with_same_virt_reg.at(i)).split_bb_ranges){
            if(std::find(std::get<0>(split_range).begin(), std::get<0>(split_range).end(), bb_number) != std::get<0>(split_range).end()){
                liveness_intervals.at(idx_spilled_intervals_with_same_virt_reg.at(i)).bb_need_initial_store.emplace_back(bb_number);
                std::sort(liveness_intervals.at(idx_spilled_intervals_with_same_virt_reg.at(i)).bb_need_initial_store.begin(), liveness_intervals.at(idx_spilled_intervals_with_same_virt_reg.at(i)).bb_need_initial_store.end());
                found_split_range_for_insertion = true;
            }
        }
    }

    if(!found_split_range_for_insertion){
        if(successor_map.find(bb_idx) != successor_map.end()){
            for(auto& succ: successor_map.at(bb_idx)){
                if(visited.at(succ) != true){
                    dfs_necessary_store_insertion_in_split_range(control_flow_TAC, liveness_intervals, visited, succ, successor_map, idx_spilled_intervals_with_same_virt_reg);
                }
            }
        }
    }

}

void split_ranges_between_function_calls(std::vector<TAC_type>& TAC, std::vector<liveness_interval_struct>& liveness_intervals, memory_security_struct& memory_security){

    std::vector<uint32_t> func_call_idices;
    //collect instruction indices that mark a function call
    for(uint32_t i = 0; i < TAC.size(); ++i){
        if(TAC.at(i).op == Operation::FUNC_CALL){
            func_call_idices.emplace_back(i);
        }
    }

    if(!func_call_idices.empty()){
        for(const auto& func_call_idx: func_call_idices){ // go over all indices that are marked as function calls
            for(uint32_t interval_idx = 0; interval_idx < liveness_intervals.size(); ++interval_idx){
                if((liveness_intervals.at(interval_idx).liveness_used.at(0) < func_call_idx) && (liveness_intervals.at(interval_idx).liveness_used.back() > func_call_idx)){
                    //split interval

                    if(liveness_intervals.at(interval_idx).is_spilled == false){
                        assign_memory_position(liveness_intervals.at(interval_idx), memory_security);
                    }

                    //get iterator to first element that is larger than the function call idx
                    auto it_idx_larger_than_func_call_idx = std::find_if(liveness_intervals.at(interval_idx).liveness_used.begin(), liveness_intervals.at(interval_idx).liveness_used.end(), [func_call_idx](const uint32_t& t){return t > func_call_idx;});

                    //new interval
                    liveness_interval_struct after_func_call_interval;
                    after_func_call_interval.sensitivity = liveness_intervals.at(interval_idx).sensitivity;
                    after_func_call_interval.is_spilled = liveness_intervals.at(interval_idx).is_spilled;
                    after_func_call_interval.virtual_register_number = liveness_intervals.at(interval_idx).virtual_register_number;
                    after_func_call_interval.real_register_number = UINT32_MAX;
                    after_func_call_interval.position_on_stack = liveness_intervals.at(interval_idx).position_on_stack;
                    std::copy(it_idx_larger_than_func_call_idx, liveness_intervals.at(interval_idx).liveness_used.end(), std::back_inserter(after_func_call_interval.liveness_used));
                    after_func_call_interval.ranges.push_back(std::make_tuple(after_func_call_interval.liveness_used.at(0), after_func_call_interval.liveness_used.back()));     


                    liveness_intervals.push_back(after_func_call_interval);

                    liveness_intervals.at(interval_idx).liveness_used.erase(it_idx_larger_than_func_call_idx, liveness_intervals.at(interval_idx).liveness_used.end());
                    std::get<1>(liveness_intervals.at(interval_idx).ranges.back()) = liveness_intervals.at(interval_idx).liveness_used.back();

                }
            }
        }
    }

}

void mark_non_store_optimisable_split_ranges(std::vector<TAC_bb>& control_flow_TAC, std::vector<liveness_interval_struct>& liveness_intervals, const std::map<uint32_t, std::vector<uint32_t>>& successor_map){
    std::vector<uint32_t> spilled_variable_already_handled;
    for(uint32_t i = 0; i < liveness_intervals.size(); ++i){
        if(liveness_intervals.at(i).is_spilled == true){
            if(std::find(spilled_variable_already_handled.begin(), spilled_variable_already_handled.end(), liveness_intervals.at(i).virtual_register_number) == spilled_variable_already_handled.end()){
                //collect all intervals that have same virtual register
                std::vector<uint32_t> idx_spilled_intervals_with_same_virt_reg;
                uint32_t curr_virt_reg = liveness_intervals.at(i).virtual_register_number;
                auto it_collector = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [curr_virt_reg](const liveness_interval_struct& t){return t.virtual_register_number == curr_virt_reg;});
                while(it_collector != liveness_intervals.end()){
                    
                    idx_spilled_intervals_with_same_virt_reg.emplace_back(it_collector - liveness_intervals.begin());
                    it_collector++;
                    it_collector = std::find_if(it_collector, liveness_intervals.end(), [curr_virt_reg](const liveness_interval_struct& t){return t.virtual_register_number == curr_virt_reg;});
                }

                //perform DFS from root -> first occurence of split range interval has to be stored in assembly, i.e. can not optimize store away
                std::vector<bool> visited(control_flow_TAC.size(), false);
                dfs_necessary_store_insertion_in_split_range(control_flow_TAC, liveness_intervals, visited, 0, successor_map, idx_spilled_intervals_with_same_virt_reg);

                spilled_variable_already_handled.emplace_back(liveness_intervals.at(i).virtual_register_number);
            }
            
        }
    }
}

void compute_correct_input_parameter_offset(std::vector<TAC_type>& TAC, uint32_t& number_of_elements_pushed_on_stack, uint32_t bytes_to_reserve){
    for(uint32_t i = 0; i < TAC.size(); ++i){
        if(TAC.at(i).op == Operation::ADD_INPUT_STACK){
            if(TAC.at(i).src2_type != TAC_OPERAND::NUM){
                throw std::runtime_error("add input stack instruction: can not handle register as second parameter yet");
            }
            else{
                TAC.at(i).src2_v += ((number_of_elements_pushed_on_stack + 1) * 4) + bytes_to_reserve;
            }
        }
    }
}

void track_used_non_scratch_registers(std::vector<TAC_type>& TAC, std::set<uint32_t>& registers_used){
    for(uint32_t i = 0; i < TAC.size(); ++i){
        if((TAC.at(i).dest_type == TAC_OPERAND::R_REG) && (TAC.at(i).dest_v_reg_idx > 3) && (TAC.at(i).dest_v_reg_idx < 13)){
            registers_used.insert(TAC.at(i).dest_v_reg_idx);
        }
        if((TAC.at(i).src1_type == TAC_OPERAND::R_REG) && (TAC.at(i).src1_v > 3) && (TAC.at(i).src1_v < 13)){
            registers_used.insert(TAC.at(i).src1_v);
        }
        if((TAC.at(i).src2_type == TAC_OPERAND::R_REG) && (TAC.at(i).src2_v > 3) && (TAC.at(i).src2_v < 13)){
            registers_used.insert(TAC.at(i).src2_v);
        }
    }
}

void generate_TAC(function& func, virtual_variables_vec& virtual_variables, std::vector<std::vector<uint8_t>>& adjacency_matrix){
    std::vector<TAC_type> TAC;
    TAC.shrink_to_fit();

    std::vector<virtual_variable_information> input_parameters;

    memory_security_struct memory_security;
    //initialize shadow register tracker
    std::tuple<uint8_t, uint32_t, uint32_t> initial_shadow_memory_tracker;
    std::get<0>(initial_shadow_memory_tracker) = 0;
    std::get<1>(initial_shadow_memory_tracker) = UINT32_MAX;
    memory_security.shadow_memory_tracker.push_back(initial_shadow_memory_tracker);

    std::map<std::string, bool> presence_of_local_variables_in_virtual_register; //original name of ssa: was variable previously assigned a register but is now not there anymore?
    std::map<std::string, bool> presence_of_input_variables_in_virtual_register; //original name of ssa: was variable previously assigned a register but is now not there anymore?

    generate_local_variable_register_tracker(virtual_variables, presence_of_local_variables_in_virtual_register); //initially every entry is set to false
    generate_input_variable_register_tracker(virtual_variables, presence_of_input_variables_in_virtual_register); //initially every entry is set to false

    function_name = func.function_name;

    //calculate how much place on stack at the beginning has to be reserved
    uint32_t idx = 0;
    uint32_t bytes_to_reserve = 4;
    while(virtual_variables.at(idx).ssa_name.at(0) != '_'){ 
        if(idx < func.arguments.size()){ //all inputs get a place on the stack if they are not arrays or pointers (i.e. normal variables) This will handle the case when inside the function a function is called which has the address of the input as parameter
            
                if((virtual_variables.at(idx).data_type == DATA_TYPE::INPUT_VAR_PTR) || (virtual_variables.at(idx).data_type == DATA_TYPE::INPUT_VAR)){
                    memory_security.stack_mapping_tracker.resize(memory_security.stack_mapping_tracker.size() + 1);
                    memory_security.stack_mapping_tracker.back().position = bytes_to_reserve;//idx - func.arguments.size();
                    memory_security.stack_mapping_tracker.back().sensitivity = virtual_variables.at(idx).sensitivity;
                    memory_security.stack_mapping_tracker.back().curr_value = virtual_variables.at(idx).ssa_name;
                    memory_security.stack_mapping_tracker.back().base_value = virtual_variables.at(idx).ssa_name;
                    memory_security.stack_mapping_tracker.back().valid_input_parameter_stored_in_local_stack = false;
                    bytes_to_reserve += 4;

                    if(idx >= 4){
                        memory_security.input_parameter_mapping_tracker.resize(memory_security.input_parameter_mapping_tracker.size() + 1);
                        memory_security.input_parameter_mapping_tracker.back().position = (idx - 4) * 4;
                        memory_security.input_parameter_mapping_tracker.back().sensitivity = virtual_variables.at(idx).sensitivity;
                        memory_security.input_parameter_mapping_tracker.back().curr_value = virtual_variables.at(idx).ssa_name;
                        memory_security.input_parameter_mapping_tracker.back().base_value = virtual_variables.at(idx).ssa_name;
                    }
                }
                else if((virtual_variables.at(idx).data_type == DATA_TYPE::INPUT_ARR_PTR)){
                    memory_security.stack_mapping_tracker.resize(memory_security.stack_mapping_tracker.size() + 1);
                    memory_security.stack_mapping_tracker.back().position = bytes_to_reserve;//idx - func.arguments.size();
                    memory_security.stack_mapping_tracker.back().sensitivity = virtual_variables.at(idx).sensitivity;
                    memory_security.stack_mapping_tracker.back().curr_value = virtual_variables.at(idx).ssa_name;
                    memory_security.stack_mapping_tracker.back().base_value = virtual_variables.at(idx).ssa_name;
                    memory_security.stack_mapping_tracker.back().valid_input_parameter_stored_in_local_stack = false;
                    bytes_to_reserve += 4;

                    if(idx >= 4){
                        memory_security.input_parameter_mapping_tracker.resize(memory_security.input_parameter_mapping_tracker.size() + 1);
                        memory_security.input_parameter_mapping_tracker.back().position = (idx - 4) * 4;
                        memory_security.input_parameter_mapping_tracker.back().sensitivity = virtual_variables.at(idx).sensitivity;
                        memory_security.input_parameter_mapping_tracker.back().curr_value = virtual_variables.at(idx).ssa_name;
                        memory_security.input_parameter_mapping_tracker.back().base_value = virtual_variables.at(idx).ssa_name;
                    }
                }
                else{
                    throw std::runtime_error("in generate_TAC: currently can only handle input parameters that are passed either by single dimension arrays or variables");
                }


        }
        if(idx >= func.arguments.size()){ //before virtual variables: enumerate all local variables
            if(virtual_variables.at(idx).data_type == DATA_TYPE::LOCAL_ARR){ //if we encounter arrays we have to consider the size of all dimensions of the array
                if((bytes_to_reserve % 4) != 0){
                    bytes_to_reserve += (bytes_to_reserve % 4);
                }
                
                array_information a_info = boost::get<array_information>(virtual_variables.at(idx).data_information);
                uint32_t bytes_to_reserve_for_array = 1;
                for(uint32_t i = 0; i < a_info.dimension; ++i){
                    bytes_to_reserve_for_array *= a_info.depth_per_dimension.at(i).op_data;
                }
                bytes_to_reserve += (bytes_to_reserve_for_array * virtual_variables.at(idx).data_type_size);
                allocate_array_in_memory_security_stack(memory_security.stack_mapping_tracker, virtual_variables.at(idx), virtual_variables.at(idx).ssa_name, a_info.depth_per_dimension);                
            } 
            else{
                if((bytes_to_reserve % 4) != 0){
                    bytes_to_reserve += (bytes_to_reserve % 4);
                }
                memory_security.stack_mapping_tracker.resize(memory_security.stack_mapping_tracker.size() + 1);
                memory_security.stack_mapping_tracker.back().position = bytes_to_reserve;
                memory_security.stack_mapping_tracker.back().sensitivity = virtual_variables.at(idx).sensitivity;
                memory_security.stack_mapping_tracker.back().curr_value = virtual_variables.at(idx).ssa_name;
                memory_security.stack_mapping_tracker.back().base_value = virtual_variables.at(idx).ssa_name;
                bytes_to_reserve += 4;
            }
        }
        else{
            input_parameters.push_back(virtual_variables.at(idx));
        }

        idx++;
    }



    if((bytes_to_reserve % 4) != 0){
        bytes_to_reserve += (bytes_to_reserve % 4);
    }


    std::vector<local_live_set> local_live_sets;
    std::vector<uint32_t> start_points_of_blocks;
    std::vector<uint32_t> end_points_of_blocks;

    //make seperate tac operation for label
    TAC_type tac_initial_label;
    tac_initial_label.sensitivity = 0;
    tac_initial_label.op = Operation::LABEL;
    tac_initial_label.dest_type = TAC_OPERAND::NONE;
    tac_initial_label.src1_type = TAC_OPERAND::NONE;
    tac_initial_label.src2_type = TAC_OPERAND::NONE;
    tac_initial_label.label_name = 1;
    TAC.push_back(tac_initial_label);

    uint32_t local_live_sets_start_index = TAC.size() - 1;
    start_points_of_blocks.emplace_back(TAC.size()); //ignore label at start of every bb

    //insert assignment of function parameters //first parameter: R0, second parameter: R1, third parameter: R2, fourth parameter: R4, rest parameter: Stack
    //will have form v0 <- R0, v1 <- R1, v2 <- R2, v3 <- R3, v4 <- sp + X, ...
    for(uint32_t i = 0; i < func.arguments.size(); ++i){
        if(i < 4){ //parameter are in registers
            TAC_type tmp_tac;
            generate_generic_tmp_tac(tmp_tac, Operation::MOV, return_sensitivity(virtual_variables.at(i).sensitivity), TAC_OPERAND::V_REG, i, return_sensitivity(virtual_variables.at(i).sensitivity), TAC_OPERAND::R_REG, i, return_sensitivity(virtual_variables.at(i).sensitivity), TAC_OPERAND::NONE, UINT32_MAX, 0, false);
            TAC.push_back(tmp_tac);
            std::string original_name = generate_original_name(virtual_variables.at(i), virtual_variables);
            std::string argument_ssa_name = generate_ssa_name_for_register_mapping(virtual_variables.at(i));
            std::tuple<std::string, std::string> key_tuple = std::make_tuple(original_name, argument_ssa_name);
            virtual_variable_to_v_reg[key_tuple] = i;
            v_register_ctr++;
        }

        
        
    }


    end_points_of_blocks.emplace_back(TAC.size() - 1);
    compute_local_live_sets(local_live_sets, local_live_sets_start_index, TAC);

    for(auto const& curr_bb: func.bb){

        //make seperate tac operation for label
        TAC_type tac_label;
        tac_label.sensitivity = 0;
        tac_label.op = Operation::LABEL;
        tac_label.dest_type = TAC_OPERAND::NONE;
        tac_label.src1_type = TAC_OPERAND::NONE;
        tac_label.src2_type = TAC_OPERAND::NONE;
        tac_label.label_name = curr_bb.bb_number;
        TAC.push_back(tac_label);

        local_live_sets_start_index = TAC.size() - 1;
        start_points_of_blocks.emplace_back(TAC.size()); //ignore label at start of every bb
        for(auto& ssa_instr: curr_bb.ssa_instruction){
            //determine which kind of instruction it is
            switch(ssa_instr.instr_type){
                case(INSTRUCTION_TYPE::INSTR_DEFAULT)           : tac_default_instr(TAC); break; 
                case(INSTRUCTION_TYPE::INSTR_NORMAL)            : tac_normal_instr(TAC, ssa_instr, virtual_variables, presence_of_local_variables_in_virtual_register, presence_of_input_variables_in_virtual_register, memory_security);    break;
                case(INSTRUCTION_TYPE::INSTR_REF)               : tac_ref_instr(TAC, ssa_instr, virtual_variables, memory_security);       break;
                case(INSTRUCTION_TYPE::INSTR_FUNC_RETURN)       : tac_func_with_return_instr(TAC, ssa_instr, virtual_variables, presence_of_local_variables_in_virtual_register, presence_of_input_variables_in_virtual_register, memory_security); break; //throw std::runtime_error("tac_func_with_return_instr not implemented yet."); //
                case(INSTRUCTION_TYPE::INSTR_FUNC_NO_RETURN)    : tac_func_without_return_instr(TAC, ssa_instr, virtual_variables, presence_of_local_variables_in_virtual_register, presence_of_input_variables_in_virtual_register, memory_security); break;
                case(INSTRUCTION_TYPE::INSTR_BRANCH)            : tac_branch_instr(TAC, ssa_instr);  break;
                case(INSTRUCTION_TYPE::INSTR_CONDITON)          : tac_condition_instr(TAC, ssa_instr, virtual_variables, presence_of_local_variables_in_virtual_register, memory_security); break;
                case(INSTRUCTION_TYPE::INSTR_PHI)               : break;
                case(INSTRUCTION_TYPE::INSTR_ASSIGN)            : tac_assign_instr(TAC, ssa_instr, virtual_variables, memory_security, presence_of_local_variables_in_virtual_register); break;
                case(INSTRUCTION_TYPE::INSTR_SPECIAL)           : break;
                case(INSTRUCTION_TYPE::INSTR_LOAD)              : tac_mem_load_instr(TAC, ssa_instr, virtual_variables, presence_of_local_variables_in_virtual_register, presence_of_input_variables_in_virtual_register, memory_security);  break;
                case(INSTRUCTION_TYPE::INSTR_STORE)             : tac_mem_store_instr(TAC, ssa_instr, virtual_variables, presence_of_local_variables_in_virtual_register, presence_of_input_variables_in_virtual_register, memory_security); break;
                case(INSTRUCTION_TYPE::INSTR_RETURN)            : tac_return_instr(TAC, ssa_instr, virtual_variables, presence_of_local_variables_in_virtual_register, memory_security); break;
                case(INSTRUCTION_TYPE::INSTR_NEGATE)            : tac_negate_instr(TAC, ssa_instr, virtual_variables, memory_security, presence_of_local_variables_in_virtual_register); break;
                case(INSTRUCTION_TYPE::INSTR_LABEL)             : tac_label_instr(TAC, ssa_instr); break;
                case(INSTRUCTION_TYPE::INSTR_INITIALIZE_LOCAL_ARR): tac_initialize_local_arr(ssa_instr, virtual_variables, memory_security); break;
                default: throw std::runtime_error("unkown instruction type found during generate_tac");
            }
        }
        end_points_of_blocks.emplace_back(TAC.size() - 1);
        compute_local_live_sets(local_live_sets, local_live_sets_start_index, TAC);
    }


    uint32_t number_of_virtual_registers = v_register_ctr;
    std::vector<uint32_t> reverse_block_order(func.bb.size() + 1,0); //added extra bb for assigning real registers to virtual registers
    std::iota(std::rbegin(reverse_block_order), std::rend(reverse_block_order), 0);

    std::map<uint32_t, std::vector<uint32_t>> successor_map;
    generate_successor_map(adjacency_matrix, successor_map);

    std::vector<global_live_set> global_live_sets;
    compute_global_live_sets(reverse_block_order, successor_map, local_live_sets, global_live_sets);



    std::vector<liveness_interval_struct> liveness_intervals;
    compute_liveness_intervals(reverse_block_order, start_points_of_blocks, end_points_of_blocks, TAC, global_live_sets, liveness_intervals, number_of_virtual_registers);


    std::vector<TAC_bb> control_flow_TAC;
    generate_flow_graph_from_TAC(TAC, control_flow_TAC);

    std::map<uint32_t, std::vector<uint32_t>> predecessors;
    // compute_predecessors(func, adjacency_matrix, predecessors);
    for(const auto& succ: successor_map){
        for(const auto& s: succ.second){
            uint32_t bb_number_of_idx = control_flow_TAC.at(s).TAC_bb_number;
            if(predecessors.find(bb_number_of_idx) == predecessors.end()){
                std::vector<uint32_t> tmp = {control_flow_TAC.at(succ.first).TAC_bb_number};
                predecessors[bb_number_of_idx] = tmp;
            }
            else{
                predecessors.at(bb_number_of_idx).emplace_back(control_flow_TAC.at(succ.first).TAC_bb_number);
            }
        }
    }




    split_ranges_between_function_calls(TAC, liveness_intervals, memory_security);

    std::sort(liveness_intervals.begin(), liveness_intervals.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){if(std::get<0>(rhs.ranges.at(0)) == std::get<0>(lhs.ranges.at(0))){return rhs.virtual_register_number < lhs.virtual_register_number;}else{return  std::get<0>(rhs.ranges.at(0)) < std::get<0>(lhs.ranges.at(0));};});

    #ifdef VERTICAL_FREE
    break_vertical(liveness_intervals, memory_security);
    #endif


    for(auto& liveness_interval: liveness_intervals){
        try{
            if(TAC.at(liveness_interval.liveness_used.at(0)).op == Operation::FUNC_MOVE){
                std::get<0>(liveness_interval.ranges.back()) = liveness_interval.liveness_used.at(0) - TAC.at(liveness_interval.liveness_used.at(0)).func_move_idx;
                
            }
        }
        catch(...){
            std::cout << "error" << std::endl;
            print_interval(liveness_interval);
            exit(-1);
        }
    }


    #ifdef ARMV6_M
    std::sort(liveness_intervals.begin(), liveness_intervals.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){if(std::get<0>(rhs.ranges.at(0)) == std::get<0>(lhs.ranges.at(0))){return rhs.virtual_register_number < lhs.virtual_register_number;}else{return  std::get<0>(rhs.ranges.at(0)) < std::get<0>(lhs.ranges.at(0));};});

    
    for(auto& elem: liveness_intervals){
        if(std::get<1>(elem.ranges.back()) == UINT32_MAX){
            std::get<1>(elem.ranges.back()) = std::get<0>(elem.ranges.back());
            elem.liveness_used.back() = elem.liveness_used.at(0);
        }
    }
    get_intervals_in_line_with_xor_specification(TAC, liveness_intervals, memory_security); //more precisely, eors have to have same to be of form eor Rdn, Rdn, Rm -> ensures that destination and first source have same register
    
    std::sort(liveness_intervals.begin(), liveness_intervals.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){if(std::get<0>(rhs.ranges.at(0)) == std::get<0>(lhs.ranges.at(0))){return rhs.virtual_register_number < lhs.virtual_register_number;}else{return  std::get<0>(rhs.ranges.at(0)) < std::get<0>(lhs.ranges.at(0));};});

    for(auto& elem: liveness_intervals){
        if(std::get<1>(elem.ranges.back()) == UINT32_MAX){
            std::get<1>(elem.ranges.back()) = std::get<0>(elem.ranges.back());
            elem.liveness_used.back() = elem.liveness_used.at(0);
        }
    }
    #endif

    std::sort(liveness_intervals.begin(), liveness_intervals.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){if(std::get<0>(rhs.ranges.at(0)) == std::get<0>(lhs.ranges.at(0))){return rhs.virtual_register_number < lhs.virtual_register_number;}else{return  std::get<0>(rhs.ranges.at(0)) < std::get<0>(lhs.ranges.at(0));};});
    for(auto& elem: liveness_intervals){
        if(std::get<1>(elem.ranges.back()) == UINT32_MAX){
            std::get<1>(elem.ranges.back()) = std::get<0>(elem.ranges.back());
            elem.liveness_used.back() = elem.liveness_used.at(0);
        }
    }


    //compute split_ranges for every interval to handle loads and stores properly 
    for(auto& elem: liveness_intervals){
        split_into_bb_ranges_of_intervals(control_flow_TAC, elem, predecessors);
    }


    //every interval has exactly one split range -> if there are multiple split ranges -> divide them into two distinct elements
    one_interval_has_one_split_range(liveness_intervals , memory_security);
    std::sort(liveness_intervals.begin(), liveness_intervals.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){if(std::get<0>(rhs.ranges.at(0)) == std::get<0>(lhs.ranges.at(0))){return rhs.virtual_register_number < lhs.virtual_register_number;}else{return  std::get<0>(rhs.ranges.at(0)) < std::get<0>(lhs.ranges.at(0));};});
    for(auto& elem: liveness_intervals){
        if(std::get<1>(elem.ranges.back()) == UINT32_MAX){
            std::get<1>(elem.ranges.back()) = std::get<0>(elem.ranges.back());
            elem.liveness_used.back() = elem.liveness_used.at(0);
        }
    }


    mark_non_store_optimisable_split_ranges(control_flow_TAC, liveness_intervals, successor_map);


    linear_scan_algorithm_rolled(TAC, control_flow_TAC, liveness_intervals, memory_security, predecessors, func.arguments.size());


    check_vertical_correctness_in_CFG(liveness_intervals, control_flow_TAC, TAC, successor_map);
    check_vertical_correctness_internal_functions_in_CFG(liveness_intervals, control_flow_TAC, TAC, successor_map);


    #ifdef MEMORY_SHADOW_FREE

    for(uint32_t interval_idx = 0; interval_idx < liveness_intervals.size(); ++interval_idx){
        if(liveness_intervals.at(interval_idx).sensitivity == 1){
            set_internal_mem_op_to_public(TAC, liveness_intervals.at(interval_idx));
        }
    }

    break_memory_overwrite(TAC, liveness_intervals, memory_security);



    control_flow_TAC.clear();
    generate_flow_graph_from_TAC(TAC, control_flow_TAC);

    #ifdef ARMV6_M

    break_shadow_register_overwrite_armv6m(control_flow_TAC, predecessors, liveness_intervals);
    #endif
    #endif




    //update bytes to reserve
    for(auto& stack_elem: memory_security.stack_mapping_tracker){
        if(stack_elem.is_spill_element == true){
            bytes_to_reserve += 4;
        }
    }

    convert_flow_graph_TAC_to_linear_TAC(control_flow_TAC, TAC);


    //keep track which non-scratch registers were used for prolog and epilog
    std::set<uint32_t> real_register_usage; 
    for(uint32_t i = 0; i < liveness_intervals.size(); ++i){
        uint32_t start_tac_instr = std::get<0>(liveness_intervals.at(i).ranges.at(0));
        uint32_t end_tac_instr = std::get<1>(liveness_intervals.at(i).ranges.back());
        uint32_t virt_reg = liveness_intervals.at(i).virtual_register_number;
        uint32_t real_reg = liveness_intervals.at(i).real_register_number;
        

        for(; start_tac_instr <= end_tac_instr; ++start_tac_instr){
            if((TAC.at(start_tac_instr).dest_type == TAC_OPERAND::V_REG) && (static_cast<uint32_t>(TAC.at(start_tac_instr).dest_v_reg_idx) == virt_reg)){
                TAC.at(start_tac_instr).dest_type = TAC_OPERAND::R_REG;
                TAC.at(start_tac_instr).dest_v_reg_idx = real_reg;

            }
            if((TAC.at(start_tac_instr).src1_type == TAC_OPERAND::V_REG) && (TAC.at(start_tac_instr).src1_v == virt_reg)){
                TAC.at(start_tac_instr).src1_type = TAC_OPERAND::R_REG;
                TAC.at(start_tac_instr).src1_v = real_reg;

            }
            if((TAC.at(start_tac_instr).src2_type == TAC_OPERAND::V_REG) && (TAC.at(start_tac_instr).src2_v == virt_reg)){
                TAC.at(start_tac_instr).src2_type = TAC_OPERAND::R_REG;
                TAC.at(start_tac_instr).src2_v = real_reg;

            }
        }
    }

    track_used_non_scratch_registers(TAC, real_register_usage);

    //all required information for prolog and epilog are only available after all previouse steps
    #ifdef ARMV6_M
    
    insert_prolog_armv6m(TAC, bytes_to_reserve, real_register_usage);
    insert_epilog_armv6m(TAC, real_register_usage, bytes_to_reserve, memory_security.stack_mapping_tracker);
    #endif

    uint32_t number_of_elements_pushed_on_stack = real_register_usage.size() + 1;
    compute_correct_input_parameter_offset(TAC, number_of_elements_pushed_on_stack, bytes_to_reserve);

    bring_function_parameters_in_correct_order(TAC);

    #ifdef ARMV6_M
    build_jump_trampoline_to_fulfill_specification(TAC);
    #endif

    handle_sensitive_internal_return_statements(TAC);



    #ifdef TAC_PRINT

    //print content of data section / local array
    //print data section / local variables  
    std::cout << ".data" << std::endl;
    for(const auto& data: func.data_section){
        std::cout << data.data_section_name << "_data:" << std::endl;
        if(data.data_storage == DATA_STORAGE_TYPE::ASCII){
            std::cout << ".ascii \"" << data.ascii_data << "\"" << std::endl;
        }
        std::cout << std::endl;
    }


    for(uint32_t i = 0; i < TAC.size(); ++i){
        switch(TAC.at(i).op){
            case Operation::NONE: break;
            case Operation::STOREBYTE:
            case Operation::STOREHALFWORD:
            case Operation::STOREWORD: print_tac_store(TAC.at(i)); break;
            case Operation::LOADBYTE:
            case Operation::LOADHALFWORD:
            case Operation::LOADWORD_FROM_INPUT_STACK: 
            case Operation::LOADWORD: print_tac_load(TAC.at(i)); break;
            case Operation::ADD:
            case Operation::ADD_INPUT_STACK:
            case Operation::SUB:
            case Operation::MUL:
            case Operation::DIV:
            case Operation::LSL:
            case Operation::LSR:
            case Operation::XOR:
            case Operation::LOGICAL_AND:
            case Operation::LOGICAL_OR: print_tac_normal(TAC.at(i)); break;
            case Operation::NEG: print_tac_negate(TAC.at(i)); break;
            case Operation::EQUAL:
            case Operation::NOT_EQUAL:
            case Operation::LESS:
            case Operation::LESS_EQUAL:
            case Operation::GREATER:
            case Operation::GREATER_EQUAL: print_tac_condition_branch(TAC.at(i)); break;
            case Operation::FUNC_MOVE:
            case Operation::FUNC_RES_MOV:
            case Operation::MOV: if(static_cast<uint32_t>(TAC.at(i).dest_v_reg_idx) < (UINT32_MAX - v_function_borders)) {print_tac_assign(TAC.at(i));}; break;
            case Operation::BRANCH: print_tac_branch(TAC.at(i)); break;
            case Operation::COMPARE: print_tac_compare(TAC.at(i)); break;
            case Operation::PARAM: print_tac_param(TAC.at(i)); break;
            case Operation::FUNC_CALL: print_tac_function_call(TAC.at(i)); break;
            case Operation::POP: print_tac_pop(TAC.at(i)); break;
            case Operation::PUSH: print_tac_push(TAC.at(i)); break;
            case Operation::LABEL: print_tac_label(TAC.at(i)); break;
            case Operation::PROLOG_PUSH: print_tac_prolog_push(TAC.at(i)); break;
            case Operation::EPILOG_POP: print_tac_epilog_pop(TAC.at(i)); break;
            case Operation::LOADWORD_WITH_LABELNAME: print_tac_load_with_labelname(TAC.at(i)); break;
            case Operation::LOADWORD_WITH_NUMBER_OFFSET: print_tac_load_with_number_offset(TAC.at(i)); break;
            case Operation::STOREWORD_WITH_NUMBER_OFFSET: print_tac_store_with_number_offset(TAC.at(i)); break;
            default: std::cout << static_cast<uint32_t>(TAC.at(i).op) << std::endl; throw std::runtime_error("print TAC after register allocation: can not find operation");
        }
    }
    #endif




}


void generate_successor_map(std::vector<std::vector<uint8_t>>& adjacency_matrix, std::map<uint32_t, std::vector<uint32_t>>& successor_map){
    //take into consideration the newly added block bb1 for adding the real registers to the corresponding virtual registers -> every element + 1; additionally add mapping bb1 -> bb2
    successor_map[0] = {1};
    for(uint32_t row = 0; row < adjacency_matrix.size(); ++row){
        std::vector<uint32_t> tmp_successors;
        for(uint32_t col = 0; col < adjacency_matrix.size(); ++col){
            if(adjacency_matrix.at(row).at(col) == 1){
                tmp_successors.emplace_back(col + 1);
            }
        }
        successor_map[row + 1] = tmp_successors;
    }
}


int32_t get_free_spill_position_on_stack(memory_security_struct& memory_security, uint8_t sensitivity_of_spilled_value){
    int32_t final_spill_position = -1;
    for(uint32_t stack_elem_idx = 0; stack_elem_idx < memory_security.stack_mapping_tracker.size(); ++stack_elem_idx){
        if(memory_security.stack_mapping_tracker.at(stack_elem_idx).is_spill_element == true){
            //if position sensitive and spilled value non-sensitive -> place value there and change sensitivity
            if(sensitivity_of_spilled_value == 1){
                if((memory_security.stack_mapping_tracker.at(stack_elem_idx).free_spill_position == true) && (memory_security.stack_mapping_tracker.at(stack_elem_idx).sensitivity == 0)){
                    return stack_elem_idx;
                }
            }
            else{ //if spilled value is non-sensitive -> preferably spill at free sensitive position, but if no sensitive position is free take any free non-sensitive position
                if((memory_security.stack_mapping_tracker.at(stack_elem_idx).free_spill_position == true) && (memory_security.stack_mapping_tracker.at(stack_elem_idx).sensitivity == 1)){
                    return stack_elem_idx;
                }
                else if((memory_security.stack_mapping_tracker.at(stack_elem_idx).free_spill_position == true) && (memory_security.stack_mapping_tracker.at(stack_elem_idx).sensitivity == 0)){
                    final_spill_position = stack_elem_idx;
                }
            }
        }
    }
    return final_spill_position;
}


uint32_t get_idx_of_bb_start(std::vector<TAC_bb>& control_flow_TAC, uint32_t bb_idx){
    uint32_t idx_of_start_bb = 0;
    uint32_t ctr = 0;
    while(ctr < bb_idx){
        idx_of_start_bb += control_flow_TAC.at(ctr).TAC.size();
        ctr++;
    }
    return idx_of_start_bb;
}

uint32_t get_idx_of_bb_end(std::vector<TAC_bb>& control_flow_TAC, uint32_t bb_idx){
    uint32_t idx_of_end_bb = 0;
    uint32_t ctr = 0;
    while(ctr <= bb_idx){
        idx_of_end_bb += control_flow_TAC.at(ctr).TAC.size();
        ctr++;
    }
    return idx_of_end_bb - 1;
}


/*****
 * 
 * 
 * 
 * 
 * 
 *              DEBUG PRINT FUNCTIONS
 * 
 * 
 * 
 * 
 * 
*/

void print_available_registers(std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers){
    std::cout << "available registers begin " << std::endl;
    for(const auto& elem: available_registers){
        std::cout << "real reg " << std::get<0>(elem) << std::endl;
        std::cout << "sensitivity " << static_cast<uint32_t>(std::get<1>(elem)) << std::endl;
        std::cout << "virtual variable " << static_cast<uint32_t>(std::get<3>(elem)) << std::endl;
        std::cout << std::endl;
    }
    std::cout << "available registers end " << std::endl;
}

void print_allocated_registers(std::vector<std::tuple<uint32_t, uint32_t, uint8_t>>& allocated_registers){
    std::cout << "allocated registers begin " << std::endl;
    for(const auto& elem: allocated_registers){
        std::cout << "virt reg " << std::get<0>(elem) << std::endl;
        std::cout << "real reg " << static_cast<uint32_t>(std::get<1>(elem)) << std::endl;
        std::cout << "sens " << static_cast<uint32_t>(std::get<2>(elem)) << std::endl;
        std::cout << std::endl;
    }
    std::cout << "allocated registers end " << std::endl;
}




void print_interval(liveness_interval_struct liveness_interval){
    std::cout << "LIVENESS INTERVAL BEGIN" << std::endl;
    std::cout << "\t virtual register number v" << liveness_interval.virtual_register_number << std::endl;
    std::cout << "\t real register number r" << liveness_interval.real_register_number << std::endl;
    std::cout << "\t is spilled " << std::to_string(liveness_interval.is_spilled) << std::endl;
    std::cout << "\t position on stack " << liveness_interval.position_on_stack << std::endl;
    std::cout << "\t sensitivity " << std::to_string(liveness_interval.sensitivity) << std::endl;
    if(liveness_interval.ranges.size() > 1){
        throw std::runtime_error("interval has more than one range");
    }
    std::cout << "\t range ";
    for(const auto range: liveness_interval.ranges){
        std::cout << std::get<0>(range) << " - " << std::get<1>(range);
    }
    std::cout << std::endl;
    std::cout << "\t liveness used ";
    for(const auto used: liveness_interval.liveness_used){
        std::cout << used << " ";
    }
    std::cout << std::endl;
    std::cout << "\t split bb ranges ";
    for(const auto bb_ranges: liveness_interval.split_bb_ranges){
        for(const auto use_points: std::get<0>(bb_ranges)){
            std::cout << use_points << " ";
        }
        std::cout << " : ";
        for(const auto use_points: std::get<1>(bb_ranges)){
            std::cout << use_points << " ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
    std::cout << "\t arm specification xor pairs " << std::endl;;
    for(auto& armv6_spec: liveness_interval.arm6_m_xor_specification){
        std::cout << std::get<0>(armv6_spec) << std::endl;
        std::cout << std::get<1>(armv6_spec) << std::endl;
    }
    std::cout << std::endl;
    std::cout << "\t contains break transition: ";
    if(liveness_interval.contains_break_transition){
        std::cout << "true" << std::endl;
    }
    else{
        std::cout << "false" << std::endl;
    }


    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << "LIVENESS INTERVAL END" << std::endl;
}

void print_liveness_intervals(std::vector<liveness_interval_struct> liveness_intervals){
    std::cout << "ALL INTERVALS BEGIN" << std::endl;
    for(const auto elem: liveness_intervals){
        print_interval(elem);
    }
    std::cout << "ALL INTERVALS END" << std::endl;
}

void print_active(std::vector<liveness_interval_struct> active){
    std::cout << "ACTIVE BEGIN" << std::endl;
    for(const auto elem: active){
        print_interval(elem);
    }
    std::cout << "ACTIVE END" << std::endl;
}



void print_TAC(std::vector<TAC_type>& TAC){
    std::cout << "BEGIN TAC " << std::endl;
    for(uint32_t i = 0; i < TAC.size(); ++i){
        std::cout << "instruction " << i << ": ";
        switch(TAC.at(i).op){
            case Operation::NONE: std::cout << "none instruction found" << std::endl; break;
            case Operation::STOREBYTE:
            case Operation::STOREHALFWORD:
            case Operation::STOREWORD: print_tac_store(TAC.at(i)); break;
            case Operation::LOADBYTE:
            case Operation::LOADHALFWORD:
            case Operation::LOADWORD_FROM_INPUT_STACK:
            case Operation::LOADWORD: print_tac_load(TAC.at(i)); break;
            case Operation::LOADWORD_WITH_LABELNAME: print_tac_load_with_labelname(TAC.at(i)); break;
            //case Operation::LOADWORD_NUMBER_INTO_REGISTER: print_tac_load_number_into_register(TAC.at(i)); break;
            case Operation::ADD:
            case Operation::ADD_INPUT_STACK: 
            case Operation::SUB:
            case Operation::MUL:
            case Operation::DIV:
            case Operation::LSL:
            case Operation::LSR:
            case Operation::XOR:
            case Operation::LOGICAL_AND:
            case Operation::LOGICAL_OR: print_tac_normal(TAC.at(i)); break;
            case Operation::NEG: print_tac_negate(TAC.at(i)); break;
            case Operation::EQUAL:
            case Operation::NOT_EQUAL:
            case Operation::LESS:
            case Operation::LESS_EQUAL:
            case Operation::GREATER:
            case Operation::GREATER_EQUAL: print_tac_condition_branch(TAC.at(i)); break;
            case Operation::FUNC_MOVE : 
            case Operation::FUNC_RES_MOV :
            case Operation::MOV: print_tac_assign(TAC.at(i)); break;
            case Operation::BRANCH: print_tac_branch(TAC.at(i)); break;
            case Operation::COMPARE: print_tac_compare(TAC.at(i)); break;
            case Operation::PARAM: print_tac_param(TAC.at(i)); break;
            case Operation::FUNC_CALL: print_tac_function_call(TAC.at(i)); break;
            case Operation::POP: print_tac_pop(TAC.at(i)); break;
            case Operation::PUSH: print_tac_push(TAC.at(i)); break;
            case Operation::LABEL: print_tac_label(TAC.at(i)); break;
            case Operation::PROLOG_PUSH: print_tac_prolog_push(TAC.at(i)); break;
            case Operation::EPILOG_POP: print_tac_epilog_pop(TAC.at(i)); break;
            case Operation::LOADWORD_WITH_NUMBER_OFFSET: std::cout << "--------> "; print_tac_load_with_number_offset(TAC.at(i)); break;
            case Operation::STOREWORD_WITH_NUMBER_OFFSET: std::cout << "--------> "; print_tac_store_with_number_offset(TAC.at(i)); break;
            default: std::cout << static_cast<uint32_t>(TAC.at(i).op) << std::endl; throw std::runtime_error("print TAC after register allocation: can not find operation");
        }
    }
    std::cout << "END TAC " << std::endl;
}

/*****
 * 
 * 
 * 
 * 
 * 
 *              DEBUG PRINT FUNCTIONS
 * 
 * 
 * 
 * 
 * 
*/



void remove_from_active(std::vector<liveness_interval_struct>& active, uint32_t index_of_spilled_interval){
    active.erase(active.begin() + index_of_spilled_interval);
}

void synchronize_instruction_index(std::vector<liveness_interval_struct>& liveness_intervals, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers, uint32_t instruction_idx_to_insert, std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m, std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion){
    for(uint32_t live_intervals_idx = 0; live_intervals_idx < liveness_intervals.size(); ++live_intervals_idx){
        for(uint32_t use_point_idx = 0; use_point_idx < liveness_intervals.at(live_intervals_idx).liveness_used.size(); ++use_point_idx){
            if(liveness_intervals.at(live_intervals_idx).liveness_used.at(use_point_idx) >= instruction_idx_to_insert){
                liveness_intervals.at(live_intervals_idx).liveness_used.at(use_point_idx) += 1;
            }
        }
        for(uint32_t range_idx = 0; range_idx < liveness_intervals.at(live_intervals_idx).ranges.size(); ++range_idx){
            if(std::get<0>(liveness_intervals.at(live_intervals_idx).ranges.at(range_idx)) >= instruction_idx_to_insert){
                std::get<0>(liveness_intervals.at(live_intervals_idx).ranges.at(range_idx)) += 1;
            }
            if(std::get<1>(liveness_intervals.at(live_intervals_idx).ranges.at(range_idx)) >= instruction_idx_to_insert){
                std::get<1>(liveness_intervals.at(live_intervals_idx).ranges.at(range_idx)) += 1;
            }
        }

        for(uint32_t split_range_idx = 0; split_range_idx < liveness_intervals.at(live_intervals_idx).split_bb_ranges.size(); ++split_range_idx){
            for(uint32_t use_point_idx = 0; use_point_idx < std::get<1>(liveness_intervals.at(live_intervals_idx).split_bb_ranges.at(split_range_idx)).size(); ++use_point_idx){
                if(std::get<1>(liveness_intervals.at(live_intervals_idx).split_bb_ranges.at(split_range_idx)).at(use_point_idx) >= instruction_idx_to_insert){
                    std::get<1>(liveness_intervals.at(live_intervals_idx).split_bb_ranges.at(split_range_idx)).at(use_point_idx) += 1;
                }
            }
        }

        if(!armv6_m_xor_specification_is_empty(liveness_intervals.at(live_intervals_idx).arm6_m_xor_specification)){
            for(auto& arm_spec: liveness_intervals.at(live_intervals_idx).arm6_m_xor_specification){
                if(std::get<1>(arm_spec) >= instruction_idx_to_insert){
                    std::get<1>(arm_spec) += 1;
                }
            }
        }
    }

    for(uint32_t active_idx = 0; active_idx < active.size(); ++active_idx){
        for(uint32_t use_point_idx = 0; use_point_idx < active.at(active_idx).liveness_used.size(); ++use_point_idx){
            if(active.at(active_idx).liveness_used.at(use_point_idx) >= instruction_idx_to_insert){
                active.at(active_idx).liveness_used.at(use_point_idx) += 1;
            }
        }
        for(uint32_t range_idx = 0; range_idx < active.at(active_idx).ranges.size(); ++range_idx){
            if(std::get<0>(active.at(active_idx).ranges.at(range_idx)) >= instruction_idx_to_insert){
                std::get<0>(active.at(active_idx).ranges.at(range_idx)) += 1;
            }
            if(std::get<1>(active.at(active_idx).ranges.at(range_idx)) >= instruction_idx_to_insert){
                std::get<1>(active.at(active_idx).ranges.at(range_idx)) += 1;
            }
        }

        for(uint32_t split_range_idx = 0; split_range_idx < active.at(active_idx).split_bb_ranges.size(); ++split_range_idx){
            for(uint32_t use_point_idx = 0; use_point_idx < std::get<1>(active.at(active_idx).split_bb_ranges.at(split_range_idx)).size(); ++use_point_idx){
                if(std::get<1>(active.at(active_idx).split_bb_ranges.at(split_range_idx)).at(use_point_idx) >= instruction_idx_to_insert){
                    std::get<1>(active.at(active_idx).split_bb_ranges.at(split_range_idx)).at(use_point_idx) += 1;
                }
            }
        }


        if(!armv6_m_xor_specification_is_empty(active.at(active_idx).arm6_m_xor_specification)){
            for(auto& arm_spec: active.at(active_idx).arm6_m_xor_specification){
                if(std::get<1>(arm_spec) >= instruction_idx_to_insert){
                    std::get<1>(arm_spec) += 1;
                }
            }
        }
    }

    for(uint32_t available_idx = 0; available_idx < available_registers.size(); ++available_idx){

        if(std::get<2>(available_registers.at(available_idx)) >= instruction_idx_to_insert){
            std::get<2>(available_registers.at(available_idx)) += 1;
        }
    }

    for(uint32_t track_xor_idx = 0; track_xor_idx < track_xor_specification_armv6m.size(); ++track_xor_idx){
        if(std::get<1>(track_xor_specification_armv6m.at(track_xor_idx)) >= instruction_idx_to_insert){
            std::get<1>(track_xor_specification_armv6m.at(track_xor_idx)) += 1;
        }
    }

    for(uint32_t postponed_idx = 0; postponed_idx < postponed_store_insertion.size(); ++postponed_idx){
        if((std::get<0>(postponed_store_insertion.at(postponed_idx)) -1) >= instruction_idx_to_insert){
            std::get<0>(postponed_store_insertion.at(postponed_idx)) += 1;
        }
    }
}



void insert_new_instruction_in_CFG_TAC(std::vector<TAC_bb>& control_flow_TAC, uint32_t instruction_idx_to_insert, const TAC_type& TAC_instr){
    uint32_t bb_position_in_cfg_tac = 0;
    uint32_t cummulative_number_of_instruction = control_flow_TAC.at(bb_position_in_cfg_tac).TAC.size();
    while(cummulative_number_of_instruction < instruction_idx_to_insert){
        bb_position_in_cfg_tac++;
        cummulative_number_of_instruction += control_flow_TAC.at(bb_position_in_cfg_tac).TAC.size();
    }
    cummulative_number_of_instruction -= control_flow_TAC.at(bb_position_in_cfg_tac).TAC.size();
    uint32_t local_bb_offset = instruction_idx_to_insert - cummulative_number_of_instruction;
    control_flow_TAC.at(bb_position_in_cfg_tac).TAC.insert(control_flow_TAC.at(bb_position_in_cfg_tac).TAC.begin() + local_bb_offset, TAC_instr);
}


void load_spill_from_memory_address(std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers, uint32_t& use_point, uint32_t real_register_of_spill, uint32_t index_of_liveness_interval, memory_security_struct& memory_security, std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m, std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion){

    uint32_t instruction_idx_to_insert = use_point;

    std::string virt_reg_in_spill = "v" + std::to_string(liveness_intervals.at(index_of_liveness_interval).virtual_register_number);
    auto mem_it = std::find_if(memory_security.stack_mapping_tracker.begin(), memory_security.stack_mapping_tracker.end(), [virt_reg_in_spill](stack_local_variable_tracking_struct& t){return (t.base_value == virt_reg_in_spill) && (t.free_spill_position == false) && (t.is_spill_element == true);});
    if(mem_it == memory_security.stack_mapping_tracker.end()){
        throw std::runtime_error("in load_spilled_variable_from_stack: cannot find virtual variable in spilled stack region");
    }

    //add load
    TAC_type tmp_tac;

    #ifdef ARMV6_M
    use_point += insert_load_armv6m(real_register_of_spill, mem_it->sensitivity, mem_it->sensitivity, mem_it->position, TAC, control_flow_TAC, instruction_idx_to_insert, liveness_intervals, active, available_registers, track_xor_specification_armv6m, postponed_store_insertion);
    #endif



}

void store_spill_to_memory_address(std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers, uint32_t& use_point, uint32_t real_register_of_spill, uint32_t index_of_liveness_interval_to_spill, uint32_t start_range_of_latest_interval, memory_security_struct& memory_security, std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m, std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion, const std::map<uint32_t, std::vector<uint32_t>>& predecessors){
    uint32_t instruction_idx_to_insert = use_point + 1;

    std::string virt_reg_in_spill = "v" + std::to_string(liveness_intervals.at(index_of_liveness_interval_to_spill).virtual_register_number);
    auto mem_it = std::find_if(memory_security.stack_mapping_tracker.begin(), memory_security.stack_mapping_tracker.end(), [virt_reg_in_spill](stack_local_variable_tracking_struct& t){return (t.base_value == virt_reg_in_spill) && (t.free_spill_position == false) && (t.is_spill_element == true);});
    if(mem_it == memory_security.stack_mapping_tracker.end()){
        throw std::runtime_error("in store_spilled_variable_from_stack: cannot find virtual variable in spilled stack region");
    }

    //add store
    TAC_type tmp_tac;

    uint8_t sens = 0;


    if((liveness_intervals.at(index_of_liveness_interval_to_spill).sensitivity == 1)){
        sens = 1;
    }

    mem_it->sensitivity = liveness_intervals.at(index_of_liveness_interval_to_spill).sensitivity;

    #ifdef ARMV6_M
    insert_store_armv6m(mem_it->position, mem_it->sensitivity, sens, real_register_of_spill, liveness_intervals.at(index_of_liveness_interval_to_spill).sensitivity, TAC, control_flow_TAC, instruction_idx_to_insert, start_range_of_latest_interval, liveness_intervals, active, available_registers, track_xor_specification_armv6m, index_of_liveness_interval_to_spill, postponed_store_insertion, predecessors);
    #endif


}



bool virt_reg_at_use_point_is_source(TAC_type& tac, uint32_t virtual_register_number){

        switch(tac.op){
            case Operation::NONE: return false; break;
            case(Operation::NEG):
            case(Operation::FUNC_MOVE):
            case(Operation::FUNC_RES_MOV):
            case(Operation::MOV): {
                if(tac.src1_type == TAC_OPERAND::V_REG){
                    if(tac.src1_v == virtual_register_number){
                        return true;
                    }
                }
                return false;

                break;
            }
            case Operation::ADD :
            case Operation::ADD_INPUT_STACK: 
            case Operation::SUB :
            case Operation::MUL :
            case Operation::DIV :
            case Operation::XOR :
            case Operation::LSL :
            case Operation::LSR :
            case Operation::LOGICAL_AND:
            case Operation::LOGICAL_OR: {
                if(tac.src1_type == TAC_OPERAND::V_REG){
                    if(tac.src1_v == virtual_register_number){
                        return true;
                    }
                }

                if(tac.src2_type == TAC_OPERAND::V_REG){
                    if(tac.src2_v == virtual_register_number){
                        return true;
                    }
                }

                return false;
                break;
            }
            case Operation::LOADBYTE : 
            case Operation::LOADHALFWORD : 
            case Operation::LOADWORD_WITH_NUMBER_OFFSET : 
            case Operation::LOADWORD_FROM_INPUT_STACK:
            case(Operation::LOADWORD_WITH_LABELNAME):
            case(Operation::LOADWORD_NUMBER_INTO_REGISTER):
            case Operation::LOADWORD : 
            {
                if(tac.src1_type == TAC_OPERAND::V_REG){
                    if(tac.src1_v == virtual_register_number){
                        return true;
                    }
                }
                return false;
                break;
            }
            case Operation::STOREBYTE : 
            case Operation::STOREHALFWORD : 
            case Operation::STOREWORD_WITH_NUMBER_OFFSET:
            case Operation::STOREWORD : 
            {
                if(tac.src1_type == TAC_OPERAND::V_REG){
                    if(tac.src1_v == virtual_register_number){
                        return true;
                    }
                }

                if(tac.src2_type == TAC_OPERAND::V_REG){
                    if(tac.src2_v == virtual_register_number){
                        return true;
                    }
                }

                return false;
                break;
            }
            case Operation::PUSH:
            case Operation::POP: 
            {
                if(tac.src1_type == TAC_OPERAND::V_REG){
                    if(tac.src1_v == virtual_register_number){
                        return true;
                    }
                }

                return false;
                break;
            }
            case Operation::BRANCH:
            case Operation::LESS_EQUAL:
            case Operation::LESS:
            case Operation::GREATER:
            case Operation::GREATER_EQUAL:
            case Operation::EQUAL:
            case Operation::NOT_EQUAL:
            
            case Operation::FUNC_CALL:
            case Operation::LABEL: return false; break;

            case Operation::COMPARE: {
                if(tac.src1_type == TAC_OPERAND::V_REG){
                    if(tac.src1_v == virtual_register_number){
                        return true;
                    }
                }

                if(tac.src2_type == TAC_OPERAND::V_REG){
                    if(tac.src2_v == virtual_register_number){
                        return true;
                    }
                }
                return false;
                break;
            }

            default: std::cout << static_cast<uint32_t>(tac.op) << std::endl; throw std::runtime_error("1 in compute_local_live_sets: operation unknown");
        }

}

bool virt_reg_at_use_point_is_destination(TAC_type& tac, uint32_t virtual_register_number){
    
        switch(tac.op){
            case Operation::NONE: return false; break;
            case(Operation::NEG): 
            case(Operation::FUNC_MOVE):
            case(Operation::FUNC_RES_MOV):
            case(Operation::MOV): {

                if(tac.dest_type == TAC_OPERAND::V_REG){
                    if(static_cast<uint32_t>(tac.dest_v_reg_idx) == virtual_register_number){
                        return true;
                    }
                }

                return false;

                break;
            }
            case Operation::ADD :
            case Operation::ADD_INPUT_STACK:
            case Operation::SUB :
            case Operation::MUL :
            case Operation::DIV :
            case Operation::XOR :
            case Operation::LSL :
            case Operation::LSR :
            case Operation::LOGICAL_AND:
            case Operation::LOGICAL_OR: {

                if(tac.dest_type == TAC_OPERAND::V_REG){
                    if(static_cast<uint32_t>(tac.dest_v_reg_idx) == virtual_register_number){
                        return true;
                    }
                }
                return false;
                break;
            }
            case Operation::LOADBYTE : 
            case Operation::LOADHALFWORD : 
            case Operation::LOADWORD_WITH_NUMBER_OFFSET :
            case Operation::LOADWORD_FROM_INPUT_STACK:
            case(Operation::LOADWORD_WITH_LABELNAME):
            case(Operation::LOADWORD_NUMBER_INTO_REGISTER):
            case Operation::LOADWORD : 
            {

                if(tac.dest_type == TAC_OPERAND::V_REG){
                    if(static_cast<uint32_t>(tac.dest_v_reg_idx) == virtual_register_number){
                        return true;
                    }
                }
                return false;
                break;
            }
            case Operation::STOREBYTE : 
            case Operation::STOREHALFWORD :
            case Operation::STOREWORD_WITH_NUMBER_OFFSET : 
            case Operation::STOREWORD : 
            {
                return false;
                break;
            }
            case Operation::PUSH:
            case Operation::POP: 
            {
                return false;
                break;
            }
            case Operation::BRANCH:
            case Operation::LESS_EQUAL:
            case Operation::LESS:
            case Operation::GREATER:
            case Operation::GREATER_EQUAL:
            case Operation::EQUAL:
            case Operation::NOT_EQUAL:
            
            case Operation::FUNC_CALL: 
            case Operation::LABEL: return false; break;

            case Operation::COMPARE: {
                return false;
                break;
            }

            default: std::cout << static_cast<uint32_t>(tac.op) << std::endl; throw std::runtime_error("0 in compute_local_live_sets: operation unknown");
        }


}



void assign_memory_position(liveness_interval_struct& interval_to_memory, memory_security_struct& memory_security){
        //find suitable memory stack position
        uint8_t sensitivity_of_spilled_value = interval_to_memory.sensitivity;
        int32_t memory_spill_position = UINT32_MAX;

        memory_spill_position = get_free_spill_position_on_stack(memory_security, sensitivity_of_spilled_value);
        if(memory_spill_position != -1){ //there is a free spill position
            //spill position gets sensitivity of spilled variable
            memory_security.stack_mapping_tracker.at(memory_spill_position).free_spill_position = false;
            memory_security.stack_mapping_tracker.at(memory_spill_position).is_spill_element = true;
            memory_security.stack_mapping_tracker.at(memory_spill_position).sensitivity = sensitivity_of_spilled_value;
            memory_security.stack_mapping_tracker.at(memory_spill_position).base_value = "v" + std::to_string(interval_to_memory.virtual_register_number);
            memory_security.stack_mapping_tracker.at(memory_spill_position).curr_value = "v" + std::to_string(interval_to_memory.virtual_register_number);
            memory_security.stack_mapping_tracker.at(memory_spill_position).position = memory_spill_position * 4;

        }
        else{ // there is no free spill position -> add entry on stack
            //new position gets sensitivity of spilled variable
            stack_local_variable_tracking_struct new_stack_local_variable_element;
            new_stack_local_variable_element.free_spill_position = false;
            new_stack_local_variable_element.is_spill_element = true;
            new_stack_local_variable_element.sensitivity = sensitivity_of_spilled_value;
            new_stack_local_variable_element.base_value = "v" + std::to_string(interval_to_memory.virtual_register_number);
            new_stack_local_variable_element.curr_value = "v" + std::to_string(interval_to_memory.virtual_register_number); 
            new_stack_local_variable_element.position = memory_security.stack_mapping_tracker.back().position + (memory_security.stack_mapping_tracker.back().position % 4) + 4; //memory_security.stack_mapping_tracker.size() * 4;
            memory_spill_position = memory_security.stack_mapping_tracker.size();
            memory_security.stack_mapping_tracker.emplace_back(new_stack_local_variable_element);
        }

        interval_to_memory.is_spilled = true;
        interval_to_memory.position_on_stack = memory_spill_position;
}


bool necessity_to_insert_store(std::vector<TAC_bb>& control_flow_TAC, const uint32_t& bb_idx, uint32_t& root_bb_idx, const std::vector<liveness_interval_struct>& liveness_intervals, liveness_interval_struct& root_liveness_interval, std::vector<bool> visited, const std::map<uint32_t, std::vector<uint32_t>>& successor_map){
    visited.at(bb_idx) = true;

    const uint32_t start_idx_of_bb = static_cast<uint32_t>(get_idx_of_bb_start(control_flow_TAC, bb_idx));
    const uint32_t end_idx_of_bb = static_cast<uint32_t>(get_idx_of_bb_end(control_flow_TAC, bb_idx));

    auto it_first_interval_with_same_virt_reg_in_bb = liveness_intervals.end();
    uint32_t use_point_of_next_interval = UINT32_MAX;

    //only check split ranges that come after initial split range in original basic block
    if(root_bb_idx == bb_idx){

        //get latest use point of root interval in initial bb
        uint32_t latest_use_point_of_original_closest_to_bb_end = UINT32_MAX; 
        for(auto it_i = root_liveness_interval.liveness_used.rbegin(); it_i != root_liveness_interval.liveness_used.rend(); ++it_i){
            if((*it_i <= end_idx_of_bb) && (*it_i >= start_idx_of_bb)){
                latest_use_point_of_original_closest_to_bb_end = *it_i;
                break;
            }
        }


        uint32_t use_point_closest_to_bb_end = UINT32_MAX;
        for(uint32_t i = 0; i < liveness_intervals.size(); ++i){
            //interval has same virtual register
            if(liveness_intervals.at(i).virtual_register_number == root_liveness_interval.virtual_register_number){
                //interval has use point in bb
                if(std::any_of(liveness_intervals.at(i).liveness_used.begin(), liveness_intervals.at(i).liveness_used.end(), [latest_use_point_of_original_closest_to_bb_end, end_idx_of_bb](const uint32_t& t){return (t <= end_idx_of_bb) && (t >= latest_use_point_of_original_closest_to_bb_end);})){
                    //interval is not original interval
                    if(!((std::get<0>(liveness_intervals.at(i).ranges.back()) == std::get<0>(root_liveness_interval.ranges.back())) && (std::get<1>(liveness_intervals.at(i).ranges.back()) == std::get<1>(root_liveness_interval.ranges.back())))){
                        
                        for(auto it_i = liveness_intervals.at(i).liveness_used.begin(); it_i != liveness_intervals.at(i).liveness_used.end(); ++it_i){
                            if((*it_i <= end_idx_of_bb) && (*it_i >= latest_use_point_of_original_closest_to_bb_end)){
                                use_point_closest_to_bb_end = *it_i;
                                use_point_of_next_interval = use_point_closest_to_bb_end;
                                break;
                            }
                        }
                        
                        if(it_first_interval_with_same_virt_reg_in_bb == liveness_intervals.end()){
                            it_first_interval_with_same_virt_reg_in_bb = liveness_intervals.begin() + i;
                        }
                        else{
                            uint32_t tmp_use_point_closest_to_bb_end = UINT32_MAX; 
                            
                            for(auto it_i = liveness_intervals.at(i).liveness_used.begin(); it_i != liveness_intervals.at(i).liveness_used.end(); ++it_i){
                                if((*it_i <= end_idx_of_bb) && (*it_i >= latest_use_point_of_original_closest_to_bb_end)){
                                    tmp_use_point_closest_to_bb_end = *it_i;
                                    break;
                                }
                            }
                            
                            if(tmp_use_point_closest_to_bb_end < use_point_closest_to_bb_end){
                                it_first_interval_with_same_virt_reg_in_bb = liveness_intervals.begin() + i;
                                use_point_closest_to_bb_end = tmp_use_point_closest_to_bb_end;
                                use_point_of_next_interval = tmp_use_point_closest_to_bb_end;
                            }
                        }
                    }
                }
            }
        }

    }
    else{
        
        uint32_t use_point_closest_to_bb_start = UINT32_MAX;
        for(uint32_t i = 0; i < liveness_intervals.size(); ++i){
            //interval has same virtual register
            if(liveness_intervals.at(i).virtual_register_number == root_liveness_interval.virtual_register_number){
                //interval has use point in bb
                if(std::any_of(liveness_intervals.at(i).liveness_used.begin(), liveness_intervals.at(i).liveness_used.end(), [start_idx_of_bb, end_idx_of_bb](const uint32_t& t){return (t <= end_idx_of_bb) && (t >= start_idx_of_bb);})){
                    //interval is not original interval
                    if(!((std::get<0>(liveness_intervals.at(i).ranges.back()) == std::get<0>(root_liveness_interval.ranges.back())) && (std::get<1>(liveness_intervals.at(i).ranges.back()) == std::get<1>(root_liveness_interval.ranges.back())))){
                        //intervals_with_same_virt_reg_in_bb.push_back(liveness_intervals.at(i));
                        if(it_first_interval_with_same_virt_reg_in_bb == liveness_intervals.end()){
                            it_first_interval_with_same_virt_reg_in_bb = liveness_intervals.begin() + i;
                            //use_point_closest_to_bb_start = *std::min_element(liveness_intervals.at(i).liveness_used.begin(), liveness_intervals.at(i).liveness_used.end(), [start_idx_of_bb, end_idx_of_bb](const uint32_t& t){return (t <= end_idx_of_bb) && (t >= start_idx_of_bb);});
                        
                            for(auto it_i = liveness_intervals.at(i).liveness_used.begin(); it_i != liveness_intervals.at(i).liveness_used.end(); ++it_i){
                                if((*it_i <= end_idx_of_bb) && (*it_i >= start_idx_of_bb)){
                                    use_point_closest_to_bb_start = *it_i;
                                    use_point_of_next_interval = use_point_closest_to_bb_start;
                                    break;
                                }
                            }
                        
                        }
                        else{
                            //uint32_t tmp_use_point_closest_to_bb_start = *std::min_element(liveness_intervals.at(i).liveness_used.begin(), liveness_intervals.at(i).liveness_used.end(), [start_idx_of_bb, end_idx_of_bb](uint32_t& t){return ((const uint32_t)(t) <= (const uint32_t)(end_idx_of_bb)) && ((const uint32_t)(t) >= (const uint32_t)(start_idx_of_bb));});
                            uint32_t tmp_use_point_closest_to_bb_start = UINT32_MAX;

                            for(auto it_i = liveness_intervals.at(i).liveness_used.begin(); it_i != liveness_intervals.at(i).liveness_used.end(); ++it_i){
                                if((*it_i <= end_idx_of_bb) && (*it_i >= start_idx_of_bb)){
                                    tmp_use_point_closest_to_bb_start = *it_i;
                                    
                                    break;
                                }
                            }
                            
                            if(tmp_use_point_closest_to_bb_start < use_point_closest_to_bb_start){
                                use_point_of_next_interval = tmp_use_point_closest_to_bb_start;
                                use_point_closest_to_bb_start = tmp_use_point_closest_to_bb_start;
                                it_first_interval_with_same_virt_reg_in_bb = liveness_intervals.begin() + i;
                            }
                        }
                    }
                }
            }
        }
    }


    //if split range can be found in this basic block
    if(it_first_interval_with_same_virt_reg_in_bb != liveness_intervals.end()){
        //get split range of this instruction idx

        auto it_split_range_of_instr_idx_in_bb = std::find_if(it_first_interval_with_same_virt_reg_in_bb->split_bb_ranges.begin(), it_first_interval_with_same_virt_reg_in_bb->split_bb_ranges.end(), [use_point_of_next_interval](const std::tuple<std::vector<uint32_t>, std::vector<uint32_t>>& t){return std::find(std::get<1>(t).begin(), std::get<1>(t).end(),use_point_of_next_interval ) != std::get<1>(t).end();});
        if(it_split_range_of_instr_idx_in_bb == it_first_interval_with_same_virt_reg_in_bb->split_bb_ranges.end()){
            throw std::runtime_error("in necessity_to_insert_store: can not find split range in interval that follows root interval");
        }
        
        

        uint32_t instruction_idx_to_check = std::get<1>(*it_split_range_of_instr_idx_in_bb).at(0);
        uint32_t bb_idx_for_instr_check;
        uint32_t cfg_instr_for_instr_check;
        get_bb_idx_and_cfg_instr_idx_from_use_point(control_flow_TAC, bb_idx_for_instr_check, cfg_instr_for_instr_check, instruction_idx_to_check);


        //if first instruction has virtual variable as destination and not as source -> no need to store back 
        if((!virt_reg_at_use_point_is_destination(control_flow_TAC.at(bb_idx_for_instr_check).TAC.at(cfg_instr_for_instr_check), root_liveness_interval.virtual_register_number)) && (virt_reg_at_use_point_is_source(control_flow_TAC.at(bb_idx_for_instr_check).TAC.at(cfg_instr_for_instr_check), root_liveness_interval.virtual_register_number))){

            return false;//true;
        }
        else{ //in any other case we have to store back
            return true; //false;
        }

    }
    //else perform deeper dfs
    else{
        bool at_least_one_successor_start_not_with_pure_destination_of_virt_reg = false;
        if(successor_map.find(bb_idx) != successor_map.end()){
            for(auto& succ: successor_map.at(bb_idx)){
                if(visited.at(succ) != true){
                    at_least_one_successor_start_not_with_pure_destination_of_virt_reg |= necessity_to_insert_store(control_flow_TAC, succ, root_bb_idx, liveness_intervals, root_liveness_interval, visited, successor_map);
                }
            }
        }
        return at_least_one_successor_start_not_with_pure_destination_of_virt_reg; 
    }
}

bool remove_str_optimization_possible(std::vector<TAC_bb>& control_flow_TAC, const std::vector<liveness_interval_struct>& liveness_intervals, liveness_interval_struct& current_liveness_interval, std::tuple<std::vector<uint32_t>, std::vector<uint32_t>>& split_range){
    //is there any other interval with same register, i.e., split?
    auto it_interval_with_same_virt_reg = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [current_liveness_interval](const liveness_interval_struct& t){return (t.virtual_register_number == current_liveness_interval.virtual_register_number) && (!t.ranges.empty()) && (!(((std::get<0>(t.ranges.back()) == std::get<0>(current_liveness_interval.ranges.back()))) && (std::get<1>(t.ranges.back()) == std::get<1>(current_liveness_interval.ranges.back()))));});

    //does the interval have more than one split range?
    //if both are not fulfilled -> no need to store
    if((it_interval_with_same_virt_reg == liveness_intervals.end()) && (current_liveness_interval.split_bb_ranges.size() < 2)){


        return true;
    }
    else{

        for(auto use_point: std::get<1>(split_range)){
            uint32_t bb_idx_for_instr_check;
            uint32_t cfg_instr_for_instr_check;
            get_bb_idx_and_cfg_instr_idx_from_use_point(control_flow_TAC, bb_idx_for_instr_check, cfg_instr_for_instr_check, use_point);



            if((!((control_flow_TAC.at(bb_idx_for_instr_check).TAC.at(cfg_instr_for_instr_check).op == Operation::LOADBYTE) || (control_flow_TAC.at(bb_idx_for_instr_check).TAC.at(cfg_instr_for_instr_check).op == Operation::LOADHALFWORD) || (control_flow_TAC.at(bb_idx_for_instr_check).TAC.at(cfg_instr_for_instr_check).op == Operation::LOADWORD) || (control_flow_TAC.at(bb_idx_for_instr_check).TAC.at(cfg_instr_for_instr_check).op == Operation::LOADWORD_WITH_NUMBER_OFFSET))) && (virt_reg_at_use_point_is_destination(control_flow_TAC.at(bb_idx_for_instr_check).TAC.at(cfg_instr_for_instr_check), current_liveness_interval.virtual_register_number))){
                return false;
            }
        }

        //if intersection of bb_need_initial_store with split range bb is not empty -> need to insert store
        std::vector<uint32_t> bb_intersection;
        std::set_intersection(current_liveness_interval.bb_need_initial_store.begin(), current_liveness_interval.bb_need_initial_store.end(), std::get<0>(split_range).begin(), std::get<0>(split_range).end(), std::back_inserter(bb_intersection));

        if(!bb_intersection.empty()){
            return false;
        }
        else{
            return true;
        }
    
    }

}

// go over every split range of a sensitive interval -> except the first load and store and the last load and store all other memory operations will be set to public
void set_internal_mem_op_to_public(std::vector<TAC_type>& TAC, liveness_interval_struct& liveness_interval){
    for(uint32_t split_range_idx = 0; split_range_idx < liveness_interval.split_bb_ranges.size(); ++split_range_idx){
        auto it_first_load_instr_in_range = std::find_if(liveness_interval.liveness_used.begin(), liveness_interval.liveness_used.end(), [TAC](const uint32_t& use_point){return ( (TAC.at(use_point).op == Operation::LOADBYTE) ||  (TAC.at(use_point).op == Operation::LOADHALFWORD) ||  (TAC.at(use_point).op == Operation::LOADWORD) ||  (TAC.at(use_point).op == Operation::LOADWORD_WITH_NUMBER_OFFSET));});
        auto it_last_load_instr_in_range = std::find_if(liveness_interval.liveness_used.rbegin(), liveness_interval.liveness_used.rend(), [TAC](const uint32_t& use_point){return ( (TAC.at(use_point).op == Operation::LOADBYTE) ||  (TAC.at(use_point).op == Operation::LOADHALFWORD) ||  (TAC.at(use_point).op == Operation::LOADWORD) ||  (TAC.at(use_point).op == Operation::LOADWORD_WITH_NUMBER_OFFSET));});

        auto it_first_store_instr_in_range = std::find_if(liveness_interval.liveness_used.begin(), liveness_interval.liveness_used.end(), [TAC](const uint32_t& use_point){return ( (TAC.at(use_point).op == Operation::STOREBYTE) ||  (TAC.at(use_point).op == Operation::STOREHALFWORD) ||  (TAC.at(use_point).op == Operation::STOREWORD) ||  (TAC.at(use_point).op == Operation::STOREWORD_WITH_NUMBER_OFFSET));});
        auto it_last_store_instr_in_range = std::find_if(liveness_interval.liveness_used.rbegin(), liveness_interval.liveness_used.rend(), [TAC](const uint32_t& use_point){return ( (TAC.at(use_point).op == Operation::STOREBYTE) ||  (TAC.at(use_point).op == Operation::STOREHALFWORD) ||  (TAC.at(use_point).op == Operation::STOREWORD) ||  (TAC.at(use_point).op == Operation::STOREWORD_WITH_NUMBER_OFFSET));});

        if(it_first_load_instr_in_range != liveness_interval.liveness_used.end()){
            it_first_load_instr_in_range++;
            while((it_first_load_instr_in_range != liveness_interval.liveness_used.end()) && ((*it_first_load_instr_in_range) < (*it_last_load_instr_in_range))){
                if((TAC.at(*it_first_load_instr_in_range).op == Operation::LOADBYTE) ||  (TAC.at(*it_first_load_instr_in_range).op == Operation::LOADHALFWORD) ||  (TAC.at(*it_first_load_instr_in_range).op == Operation::LOADWORD) ||  (TAC.at(*it_first_load_instr_in_range).op == Operation::LOADWORD_WITH_NUMBER_OFFSET)){
                    TAC.at(*it_first_load_instr_in_range).sensitivity = 0;
                }
                it_first_load_instr_in_range++;
            }
        }

        if(it_first_store_instr_in_range != liveness_interval.liveness_used.end()){
            it_first_store_instr_in_range++;
            while((it_first_store_instr_in_range != liveness_interval.liveness_used.end()) && ((*it_first_store_instr_in_range) < (*it_last_store_instr_in_range))){
                if((TAC.at(*it_first_store_instr_in_range).op == Operation::STOREBYTE) ||  (TAC.at(*it_first_store_instr_in_range).op == Operation::STOREHALFWORD) ||  (TAC.at(*it_first_store_instr_in_range).op == Operation::STOREWORD) ||  (TAC.at(*it_first_store_instr_in_range).op == Operation::STOREWORD_WITH_NUMBER_OFFSET)){
                    TAC.at(*it_first_store_instr_in_range).sensitivity = 0;
                }
                it_first_store_instr_in_range++;
            }
        }

    }

}

void one_interval_has_one_split_range(std::vector<liveness_interval_struct>& liveness_intervals, memory_security_struct &memory_security){
    for(auto& elem: liveness_intervals){
        if((elem.split_bb_ranges.size() > 1) && (elem.sensitivity == 1)){

            if(!elem.is_spilled){
                assign_memory_position(elem, memory_security);
            }

            liveness_interval_struct one_range_interval;
            one_range_interval.virtual_register_number = elem.virtual_register_number;
            one_range_interval.sensitivity = elem.sensitivity;
            one_range_interval.arm6_m_xor_specification = elem.arm6_m_xor_specification;
            one_range_interval.available_registers_after_own_allocation = elem.available_registers_after_own_allocation;
            one_range_interval.is_spilled = elem.is_spilled;
            one_range_interval.real_register_number = elem.real_register_number;
            one_range_interval.position_on_stack = elem.position_on_stack;
            one_range_interval.postponed_store = elem.postponed_store;
            one_range_interval.contains_break_transition = elem.contains_break_transition;
            one_range_interval.bb_need_initial_store = elem.bb_need_initial_store;
            for(uint32_t i = 1; i < elem.split_bb_ranges.size(); ++i){
                one_range_interval.split_bb_ranges.push_back(elem.split_bb_ranges.at(i));
                one_range_interval.liveness_used = std::get<1>(one_range_interval.split_bb_ranges.at(0));
                one_range_interval.ranges.push_back(std::make_tuple(one_range_interval.liveness_used.at(0), one_range_interval.liveness_used.back()));


                liveness_intervals.push_back(one_range_interval);

                one_range_interval.split_bb_ranges.clear();
                one_range_interval.split_bb_ranges.shrink_to_fit();
                one_range_interval.liveness_used.clear();
                one_range_interval.liveness_used.shrink_to_fit();
                one_range_interval.ranges.clear();
                one_range_interval.ranges.shrink_to_fit();
            }

            elem.split_bb_ranges.resize(1);
            elem.liveness_used = std::get<1>(elem.split_bb_ranges.at(0));
            std::get<0>(elem.ranges.at(0)) = elem.liveness_used.at(0);
            std::get<1>(elem.ranges.at(0)) = elem.liveness_used.back();

        }
    }
}

uint32_t get_first_use_point_within_range(uint32_t range_start, liveness_interval_struct& interval){
    return *std::find_if(interval.liveness_used.begin(), interval.liveness_used.end(), [range_start](const uint32_t& t){return range_start <= t;});
}



bool check_vertical_violation(std::vector<TAC_bb>& control_flow_TAC, uint32_t original_interval_idx, uint32_t sensitive_interval_real_reg, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<bool>& visited, uint32_t original_bb_idx, uint32_t succ_bb_idx, const std::map<uint32_t, std::vector<uint32_t>>& successor_map){

    visited.at(succ_bb_idx) = true;

    if(succ_bb_idx == original_bb_idx){
        /**
         * @brief Check within BB -> after original interval
         * 
         */

        //get last instruction of interval (that is not caused by spill store)
        uint32_t start_instr_idx = get_idx_of_bb_start(control_flow_TAC, succ_bb_idx);
        uint32_t end_instr_idx = liveness_intervals.at(original_interval_idx).liveness_used.at(0) - 1;

        std::vector<liveness_interval_struct> intervals_in_succ_bb;


        for(uint32_t second_interval_idx = 0; second_interval_idx < liveness_intervals.size(); ++second_interval_idx){
            if(second_interval_idx != original_interval_idx){
                //if(std::find_if(liveness_intervals.at(second_interval_idx).liveness_used.begin(),liveness_intervals.at(second_interval_idx).liveness_used.end(), [start_instr_idx, end_instr_idx](const uint32_t& t){return (start_instr_idx <= t) && (t <= end_instr_idx);}) != liveness_intervals.at(second_interval_idx).liveness_used.end()){
                if((liveness_intervals.at(second_interval_idx).liveness_used.at(0) >= start_instr_idx) && (liveness_intervals.at(second_interval_idx).liveness_used.at(0) <= end_instr_idx)){
                    intervals_in_succ_bb.push_back(liveness_intervals.at(second_interval_idx));
                }
            }

        }

        uint32_t closest_use_point_to_start = UINT32_MAX;
        uint32_t position_of_interval_in_vector = UINT32_MAX;
        for(uint32_t i = 0; i < intervals_in_succ_bb.size(); ++i){
            if(intervals_in_succ_bb.at(i).sensitivity == 1){
                liveness_interval_struct second_sensitive_interval_after_curr_interval_in_bb = intervals_in_succ_bb.at(i);
                //check if it contains use point that is closer to last_use_point
                uint32_t tmp_closest_to_start_point = *std::find_if(second_sensitive_interval_after_curr_interval_in_bb.liveness_used.begin(), second_sensitive_interval_after_curr_interval_in_bb.liveness_used.end(), [start_instr_idx](const uint32_t& t){return start_instr_idx <= t;});
                if(tmp_closest_to_start_point < closest_use_point_to_start){
                    closest_use_point_to_start = tmp_closest_to_start_point;
                    position_of_interval_in_vector = i;
                }
            }
        }
        auto first_sensitive_interval_in_bb = intervals_in_succ_bb.end(); 
        if(position_of_interval_in_vector != UINT32_MAX){
            first_sensitive_interval_in_bb = intervals_in_succ_bb.begin() + position_of_interval_in_vector;
        }


        // auto first_interval_with_same_real_reg = std::find_if(intervals_in_succ_bb.begin(), intervals_in_succ_bb.end(), [sensitive_interval_real_reg, last_use_point](const liveness_interval_struct& t){return (t.real_register_number == sensitive_interval_real_reg) && (t.liveness_used.at(0) >= last_use_point);});
        uint32_t closest_use_point_to_start_same_real_reg = UINT32_MAX;
        uint32_t position_of_interval_in_vector_same_real_reg = UINT32_MAX;
        for(uint32_t i = 0; i < intervals_in_succ_bb.size(); ++i){
            if(intervals_in_succ_bb.at(i).real_register_number == sensitive_interval_real_reg){
                liveness_interval_struct second_same_real_reg_interval = intervals_in_succ_bb.at(i);
                //check if it contains use point that is closer to last_use_point
                uint32_t tmp_closest_to_start_point_real_reg = get_first_use_point_within_range(start_instr_idx, second_same_real_reg_interval);//*std::find_if(second_same_real_reg_interval.liveness_used.begin(), second_same_real_reg_interval.liveness_used.end(), [start_instr_idx](const uint32_t& t){return start_instr_idx <= t;});
                if(tmp_closest_to_start_point_real_reg < closest_use_point_to_start_same_real_reg){
                    closest_use_point_to_start_same_real_reg = tmp_closest_to_start_point_real_reg;
                    closest_use_point_to_start_same_real_reg = i;
                }
            }
        }
        auto first_interval_with_same_real_reg = intervals_in_succ_bb.end(); 
        if(position_of_interval_in_vector_same_real_reg != UINT32_MAX){
            first_interval_with_same_real_reg = intervals_in_succ_bb.begin() + position_of_interval_in_vector;
        }

        if((first_sensitive_interval_in_bb == intervals_in_succ_bb.end()) && (first_interval_with_same_real_reg == intervals_in_succ_bb.end())){
            return false;
        }
        if((first_sensitive_interval_in_bb != intervals_in_succ_bb.end()) && (first_interval_with_same_real_reg != intervals_in_succ_bb.end()) && (get_first_use_point_within_range(start_instr_idx, *first_interval_with_same_real_reg) < get_first_use_point_within_range(start_instr_idx, *first_sensitive_interval_in_bb)) && (first_interval_with_same_real_reg->sensitivity != 1)){
            return false;
        }
        if((first_sensitive_interval_in_bb == intervals_in_succ_bb.end()) && (first_interval_with_same_real_reg != intervals_in_succ_bb.end())){
            return false;
        }


        return true;
    }
    else{
        /**
         * @brief Check other BB
         * 
         */
        uint32_t start_instr_idx = get_idx_of_bb_start(control_flow_TAC, succ_bb_idx);
        uint32_t end_instr_idx = get_idx_of_bb_end(control_flow_TAC, succ_bb_idx);

        std::vector<liveness_interval_struct> intervals_in_succ_bb;

        for(uint32_t second_interval_idx = 0; second_interval_idx < liveness_intervals.size(); ++second_interval_idx){
            if(second_interval_idx != original_interval_idx){
                //if(std::find_if(liveness_intervals.at(second_interval_idx).liveness_used.begin(),liveness_intervals.at(second_interval_idx).liveness_used.end(), [start_instr_idx, end_instr_idx](const uint32_t& t){return (start_instr_idx <= t) && (t <= end_instr_idx);}) != liveness_intervals.at(second_interval_idx).liveness_used.end()){
                if((liveness_intervals.at(second_interval_idx).liveness_used.at(0) >= start_instr_idx) && (liveness_intervals.at(second_interval_idx).liveness_used.at(0) <= end_instr_idx)){
                    intervals_in_succ_bb.push_back(liveness_intervals.at(second_interval_idx));
                }
            }

        }
        uint32_t closest_use_point_to_start = UINT32_MAX;
        uint32_t position_of_interval_in_vector = UINT32_MAX;
        for(uint32_t i = 0; i < intervals_in_succ_bb.size(); ++i){
            if(intervals_in_succ_bb.at(i).sensitivity == 1){
                liveness_interval_struct second_sensitive_interval_after_curr_interval_in_bb = intervals_in_succ_bb.at(i);
                //check if it contains use point that is closer to last_use_point
                uint32_t tmp_closest_to_start_point = *std::find_if(second_sensitive_interval_after_curr_interval_in_bb.liveness_used.begin(), second_sensitive_interval_after_curr_interval_in_bb.liveness_used.end(), [start_instr_idx](const uint32_t& t){return start_instr_idx <= t;});
                if(tmp_closest_to_start_point < closest_use_point_to_start){
                    closest_use_point_to_start = tmp_closest_to_start_point;
                    position_of_interval_in_vector = i;
                }
            }
        }
        auto first_sensitive_interval_in_bb = intervals_in_succ_bb.end(); 
        if(position_of_interval_in_vector != UINT32_MAX){
            first_sensitive_interval_in_bb = intervals_in_succ_bb.begin() + position_of_interval_in_vector;
        }

        // auto first_interval_with_same_real_reg = std::find_if(intervals_in_succ_bb.begin(), intervals_in_succ_bb.end(), [sensitive_interval_real_reg, last_use_point](const liveness_interval_struct& t){return (t.real_register_number == sensitive_interval_real_reg) && (t.liveness_used.at(0) >= last_use_point);});
        uint32_t closest_use_point_to_start_same_real_reg = UINT32_MAX;
        uint32_t position_of_interval_in_vector_same_real_reg = UINT32_MAX;
        for(uint32_t i = 0; i < intervals_in_succ_bb.size(); ++i){
            if(intervals_in_succ_bb.at(i).real_register_number == sensitive_interval_real_reg){
                liveness_interval_struct second_same_real_reg_interval = intervals_in_succ_bb.at(i);
                //check if it contains use point that is closer to last_use_point
                uint32_t tmp_closest_to_start_point_real_reg = get_first_use_point_within_range(start_instr_idx, second_same_real_reg_interval);//*std::find_if(second_same_real_reg_interval.liveness_used.begin(), second_same_real_reg_interval.liveness_used.end(), [start_instr_idx](const uint32_t& t){return start_instr_idx <= t;});
                if(tmp_closest_to_start_point_real_reg < closest_use_point_to_start_same_real_reg){
                    closest_use_point_to_start_same_real_reg = tmp_closest_to_start_point_real_reg;
                    closest_use_point_to_start_same_real_reg = i;
                }
            }
        }
        auto first_interval_with_same_real_reg = intervals_in_succ_bb.end(); 
        if(position_of_interval_in_vector_same_real_reg != UINT32_MAX){
            first_interval_with_same_real_reg = intervals_in_succ_bb.begin() + position_of_interval_in_vector;
        }

        if((first_sensitive_interval_in_bb == intervals_in_succ_bb.end()) && (first_interval_with_same_real_reg == intervals_in_succ_bb.end())){
            bool violation_detected = false;
            if(successor_map.find(succ_bb_idx) != successor_map.end()){
                for(auto succ_of_succ_bb_idx: successor_map.at(succ_bb_idx)){
                    violation_detected = check_vertical_violation(control_flow_TAC, original_interval_idx, sensitive_interval_real_reg, liveness_intervals, visited, original_bb_idx, succ_of_succ_bb_idx, successor_map);
                    if(violation_detected){
                        return true;
                    }
                }
            }
            return false;
        }
        else if((first_sensitive_interval_in_bb == intervals_in_succ_bb.end()) && (first_interval_with_same_real_reg != intervals_in_succ_bb.end())){
            return false;
        }
        else if((first_sensitive_interval_in_bb != intervals_in_succ_bb.end()) && (first_interval_with_same_real_reg != intervals_in_succ_bb.end()) && (get_first_use_point_within_range(start_instr_idx, *first_interval_with_same_real_reg) < get_first_use_point_within_range(start_instr_idx, *first_sensitive_interval_in_bb)) && (first_interval_with_same_real_reg->sensitivity != 1)){
            return false;
        }
        else{
            return true;
        }

    }

}

void check_vertical_correctness_in_CFG(std::vector<liveness_interval_struct>& liveness_intervals, std::vector<TAC_bb>& control_flow_TAC, std::vector<TAC_type>& TAC, const std::map<uint32_t, std::vector<uint32_t>>& successor_map){

    for(uint32_t interval_idx = 0;  interval_idx < liveness_intervals.size(); ++interval_idx){
        if(liveness_intervals.at(interval_idx).sensitivity == 1){
            uint32_t sensitive_interval_real_reg = liveness_intervals.at(interval_idx).real_register_number;
            auto curr_interval = liveness_intervals.at(interval_idx);


            /**
             * @brief Check within BB -> after original interval
             * 
             */

            //get last instruction of interval (that is not caused by spill store)
            uint32_t last_use_point = liveness_intervals.at(interval_idx).liveness_used.back();

            std::vector<liveness_interval_struct> intervals_in_same_bb_start_after_curr_interval;

            uint32_t bb_idx = get_bb_idx_of_instruction_idx(control_flow_TAC, last_use_point);
            uint32_t end_instr_idx_curr_bb = get_idx_of_bb_end(control_flow_TAC, bb_idx);

            for(uint32_t second_interval_idx = 0; second_interval_idx < liveness_intervals.size(); ++second_interval_idx){
                if(second_interval_idx != interval_idx){
                    //if(std::find_if(liveness_intervals.at(second_interval_idx).liveness_used.begin(),liveness_intervals.at(second_interval_idx).liveness_used.end(), [last_use_point, end_instr_idx_curr_bb](const uint32_t& t){return (last_use_point <= t) && (t <= end_instr_idx_curr_bb);}) != liveness_intervals.at(second_interval_idx).liveness_used.end()){
                    if((liveness_intervals.at(second_interval_idx).liveness_used.at(0) >= last_use_point) && (liveness_intervals.at(second_interval_idx).liveness_used.at(0) <= end_instr_idx_curr_bb)){
                        intervals_in_same_bb_start_after_curr_interval.push_back(liveness_intervals.at(second_interval_idx));
                    }
                }
            }

            
            uint32_t closest_use_point_to_last_use_point = UINT32_MAX;
            uint32_t position_of_interval_in_vector = UINT32_MAX;
            for(uint32_t i = 0; i < intervals_in_same_bb_start_after_curr_interval.size(); ++i){
                if(intervals_in_same_bb_start_after_curr_interval.at(i).sensitivity == 1){
                    liveness_interval_struct second_sensitive_interval_after_curr_interval_in_bb = intervals_in_same_bb_start_after_curr_interval.at(i);

                    //check if it contains use point that is closer to last_use_point
                    uint32_t tmp_closest_to_use_point = *std::find_if(second_sensitive_interval_after_curr_interval_in_bb.liveness_used.begin(), second_sensitive_interval_after_curr_interval_in_bb.liveness_used.end(), [last_use_point](const uint32_t& t){return t >= last_use_point;});
                    
                    if(tmp_closest_to_use_point < closest_use_point_to_last_use_point){
                        closest_use_point_to_last_use_point = tmp_closest_to_use_point;
                        position_of_interval_in_vector = i;
                    }
                }
            }

            auto first_sensitive_interval_after_curr_interval_in_bb = intervals_in_same_bb_start_after_curr_interval.end(); 
            if(position_of_interval_in_vector != UINT32_MAX){
                first_sensitive_interval_after_curr_interval_in_bb = intervals_in_same_bb_start_after_curr_interval.begin() + position_of_interval_in_vector;
            }

            auto first_interval_with_same_real_reg = std::find_if(intervals_in_same_bb_start_after_curr_interval.begin(), intervals_in_same_bb_start_after_curr_interval.end(), [sensitive_interval_real_reg, last_use_point](const liveness_interval_struct& t){return (t.real_register_number == sensitive_interval_real_reg) && (t.liveness_used.at(0) >= last_use_point);});

            //there is no sensitive interval in this bb afterwards

            if((first_interval_with_same_real_reg == intervals_in_same_bb_start_after_curr_interval.end()) && (first_sensitive_interval_after_curr_interval_in_bb == intervals_in_same_bb_start_after_curr_interval.end())){

                /**
                 * @brief Check other BB
                 * 
                 */
                bool violation_detected = false;
                std::vector<bool> visited(control_flow_TAC.size(), false);


                
                if(successor_map.find(bb_idx) != successor_map.end()){
                    for(auto& succ_bb_idx: successor_map.at(bb_idx)){
                        violation_detected = check_vertical_violation(control_flow_TAC, interval_idx, sensitive_interval_real_reg, liveness_intervals, visited, bb_idx, succ_bb_idx, successor_map);
                        if(violation_detected){


                            uint32_t instruction_idx_to_insert = liveness_intervals.at(interval_idx).liveness_used.back() + 1;
                            TAC_type tmp_tac;

                            Operation op = Operation::MOV;
                            uint8_t sens = 0; 
                            TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
                            uint32_t src2_v = UINT32_MAX;
                            uint8_t src2_sens = 0; 
                            TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
                            uint32_t src1_v = 11; //the real register that will be spilled has to be stored in memory
                            uint8_t src1_sens = 0; 
                            TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
                            uint32_t dest_v = sensitive_interval_real_reg;
                            uint8_t dest_sens = 0;

                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

                            TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
                            insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

                            synchronize_instruction_index_of_liveness_intervals(liveness_intervals, instruction_idx_to_insert);
                            liveness_intervals.at(interval_idx).contains_break_transition = true;

                            break;
                        }
                    }
                }

            }
            else if((first_interval_with_same_real_reg != intervals_in_same_bb_start_after_curr_interval.end()) && (first_sensitive_interval_after_curr_interval_in_bb != intervals_in_same_bb_start_after_curr_interval.end()) && (*first_interval_with_same_real_reg == *first_sensitive_interval_after_curr_interval_in_bb) && (first_sensitive_interval_after_curr_interval_in_bb->liveness_used.at(0) == last_use_point)){
                continue;
            }
            else if((first_interval_with_same_real_reg != intervals_in_same_bb_start_after_curr_interval.end()) && (first_sensitive_interval_after_curr_interval_in_bb != intervals_in_same_bb_start_after_curr_interval.end()) && (get_first_use_point_within_range(last_use_point, *first_interval_with_same_real_reg) < get_first_use_point_within_range(last_use_point, *first_sensitive_interval_after_curr_interval_in_bb) && (first_interval_with_same_real_reg->sensitivity != 1))){
                continue;
            }
            else if((first_sensitive_interval_after_curr_interval_in_bb == intervals_in_same_bb_start_after_curr_interval.end()) && (first_interval_with_same_real_reg != intervals_in_same_bb_start_after_curr_interval.end())){
                continue;
            }
            else{

                uint32_t instruction_idx_to_insert = liveness_intervals.at(interval_idx).liveness_used.back() + 1;
                TAC_type tmp_tac;

                Operation op = Operation::MOV;
                uint8_t sens = 0; 
                TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
                uint32_t src2_v = UINT32_MAX;
                uint8_t src2_sens = 0; 
                TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
                uint32_t src1_v = 11; //the real register that will be spilled has to be stored in memory
                uint8_t src1_sens = 0; 
                TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
                uint32_t dest_v = sensitive_interval_real_reg;
                uint8_t dest_sens = 0;

                generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);


                TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
                insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

                synchronize_instruction_index_of_liveness_intervals(liveness_intervals, instruction_idx_to_insert);

                liveness_intervals.at(interval_idx).contains_break_transition = true;
            }


        }
    }

}










bool check_vertical_violation_internal_function_call(std::vector<TAC_bb>& control_flow_TAC, uint32_t original_tac_instr_idx, uint32_t real_reg, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<bool>& visited, uint32_t original_bb_idx, uint32_t succ_bb_idx, const std::map<uint32_t, std::vector<uint32_t>>& successor_map){

    visited.at(succ_bb_idx) = true;

    if(succ_bb_idx == original_bb_idx){
        /**
         * @brief Check within BB -> after original interval
         * 
         */

        //get last instruction of interval (that is not caused by spill store)
        uint32_t start_instr_idx = get_idx_of_bb_start(control_flow_TAC, succ_bb_idx);
        uint32_t end_instr_idx = original_tac_instr_idx;

        std::vector<liveness_interval_struct> intervals_in_succ_bb;


        for(uint32_t second_interval_idx = 0; second_interval_idx < liveness_intervals.size(); ++second_interval_idx){

            //if(std::find_if(liveness_intervals.at(second_interval_idx).liveness_used.begin(),liveness_intervals.at(second_interval_idx).liveness_used.end(), [start_instr_idx, end_instr_idx](const uint32_t& t){return (start_instr_idx <= t) && (t <= end_instr_idx);}) != liveness_intervals.at(second_interval_idx).liveness_used.end()){
            if((liveness_intervals.at(second_interval_idx).liveness_used.at(0) >= start_instr_idx) && (liveness_intervals.at(second_interval_idx).liveness_used.at(0) <= end_instr_idx)){
                intervals_in_succ_bb.push_back(liveness_intervals.at(second_interval_idx));
            }
            

        }

        uint32_t closest_use_point_to_start = UINT32_MAX;
        uint32_t position_of_interval_in_vector = UINT32_MAX;
        for(uint32_t i = 0; i < intervals_in_succ_bb.size(); ++i){
            if(intervals_in_succ_bb.at(i).sensitivity == 1){
                liveness_interval_struct second_sensitive_interval_after_curr_interval_in_bb = intervals_in_succ_bb.at(i);
                //check if it contains use point that is closer to last_use_point
                uint32_t tmp_closest_to_start_point = *std::find_if(second_sensitive_interval_after_curr_interval_in_bb.liveness_used.begin(), second_sensitive_interval_after_curr_interval_in_bb.liveness_used.end(), [start_instr_idx](const uint32_t& t){return start_instr_idx <= t;});
                if(tmp_closest_to_start_point < closest_use_point_to_start){
                    closest_use_point_to_start = tmp_closest_to_start_point;
                    position_of_interval_in_vector = i;
                }
            }
        }
        auto first_sensitive_interval_in_bb = intervals_in_succ_bb.end(); 
        if(position_of_interval_in_vector != UINT32_MAX){
            first_sensitive_interval_in_bb = intervals_in_succ_bb.begin() + position_of_interval_in_vector;
        }


        // auto first_interval_with_same_real_reg = std::find_if(intervals_in_succ_bb.begin(), intervals_in_succ_bb.end(), [sensitive_interval_real_reg, last_use_point](const liveness_interval_struct& t){return (t.real_register_number == sensitive_interval_real_reg) && (t.liveness_used.at(0) >= last_use_point);});
        uint32_t closest_use_point_to_start_same_real_reg = UINT32_MAX;
        uint32_t position_of_interval_in_vector_same_real_reg = UINT32_MAX;
        for(uint32_t i = 0; i < intervals_in_succ_bb.size(); ++i){
            if(intervals_in_succ_bb.at(i).real_register_number == real_reg){
                liveness_interval_struct second_same_real_reg_interval = intervals_in_succ_bb.at(i);
                //check if it contains use point that is closer to last_use_point
                uint32_t tmp_closest_to_start_point_real_reg = get_first_use_point_within_range(start_instr_idx, second_same_real_reg_interval);//*std::find_if(second_same_real_reg_interval.liveness_used.begin(), second_same_real_reg_interval.liveness_used.end(), [start_instr_idx](const uint32_t& t){return start_instr_idx <= t;});
                if(tmp_closest_to_start_point_real_reg < closest_use_point_to_start_same_real_reg){
                    closest_use_point_to_start_same_real_reg = tmp_closest_to_start_point_real_reg;
                    closest_use_point_to_start_same_real_reg = i;
                }
            }
        }
        auto first_interval_with_same_real_reg = intervals_in_succ_bb.end(); 
        if(position_of_interval_in_vector_same_real_reg != UINT32_MAX){
            first_interval_with_same_real_reg = intervals_in_succ_bb.begin() + position_of_interval_in_vector;
        }

        if((first_sensitive_interval_in_bb == intervals_in_succ_bb.end()) && (first_interval_with_same_real_reg == intervals_in_succ_bb.end())){
            return false;
        }
        if((first_sensitive_interval_in_bb != intervals_in_succ_bb.end()) && (first_interval_with_same_real_reg != intervals_in_succ_bb.end()) && (get_first_use_point_within_range(start_instr_idx, *first_interval_with_same_real_reg) < get_first_use_point_within_range(start_instr_idx, *first_sensitive_interval_in_bb)) && (first_interval_with_same_real_reg->sensitivity != 1)){
            return false;
        }
        if((first_sensitive_interval_in_bb == intervals_in_succ_bb.end()) && (first_interval_with_same_real_reg != intervals_in_succ_bb.end())){
            return false;
        }


        return true;
    }
    else{
        /**
         * @brief Check other BB
         * 
         */
        uint32_t start_instr_idx = get_idx_of_bb_start(control_flow_TAC, succ_bb_idx);
        uint32_t end_instr_idx = get_idx_of_bb_end(control_flow_TAC, succ_bb_idx);

        std::vector<liveness_interval_struct> intervals_in_succ_bb;

        for(uint32_t second_interval_idx = 0; second_interval_idx < liveness_intervals.size(); ++second_interval_idx){
            //if(std::find_if(liveness_intervals.at(second_interval_idx).liveness_used.begin(),liveness_intervals.at(second_interval_idx).liveness_used.end(), [start_instr_idx, end_instr_idx](const uint32_t& t){return (start_instr_idx <= t) && (t <= end_instr_idx);}) != liveness_intervals.at(second_interval_idx).liveness_used.end()){
            if((liveness_intervals.at(second_interval_idx).liveness_used.at(0) >= start_instr_idx) && (liveness_intervals.at(second_interval_idx).liveness_used.at(0) <= end_instr_idx)){
                intervals_in_succ_bb.push_back(liveness_intervals.at(second_interval_idx));
            }
            

        }
        uint32_t closest_use_point_to_start = UINT32_MAX;
        uint32_t position_of_interval_in_vector = UINT32_MAX;
        for(uint32_t i = 0; i < intervals_in_succ_bb.size(); ++i){
            if(intervals_in_succ_bb.at(i).sensitivity == 1){
                liveness_interval_struct second_sensitive_interval_after_curr_interval_in_bb = intervals_in_succ_bb.at(i);
                //check if it contains use point that is closer to last_use_point
                uint32_t tmp_closest_to_start_point = *std::find_if(second_sensitive_interval_after_curr_interval_in_bb.liveness_used.begin(), second_sensitive_interval_after_curr_interval_in_bb.liveness_used.end(), [start_instr_idx](const uint32_t& t){return start_instr_idx <= t;});
                if(tmp_closest_to_start_point < closest_use_point_to_start){
                    closest_use_point_to_start = tmp_closest_to_start_point;
                    position_of_interval_in_vector = i;
                }
            }
        }
        auto first_sensitive_interval_in_bb = intervals_in_succ_bb.end(); 
        if(position_of_interval_in_vector != UINT32_MAX){
            first_sensitive_interval_in_bb = intervals_in_succ_bb.begin() + position_of_interval_in_vector;
        }

        // auto first_interval_with_same_real_reg = std::find_if(intervals_in_succ_bb.begin(), intervals_in_succ_bb.end(), [sensitive_interval_real_reg, last_use_point](const liveness_interval_struct& t){return (t.real_register_number == sensitive_interval_real_reg) && (t.liveness_used.at(0) >= last_use_point);});
        uint32_t closest_use_point_to_start_same_real_reg = UINT32_MAX;
        uint32_t position_of_interval_in_vector_same_real_reg = UINT32_MAX;
        for(uint32_t i = 0; i < intervals_in_succ_bb.size(); ++i){
            if(intervals_in_succ_bb.at(i).real_register_number == real_reg){
                liveness_interval_struct second_same_real_reg_interval = intervals_in_succ_bb.at(i);
                //check if it contains use point that is closer to last_use_point
                uint32_t tmp_closest_to_start_point_real_reg = get_first_use_point_within_range(start_instr_idx, second_same_real_reg_interval);//*std::find_if(second_same_real_reg_interval.liveness_used.begin(), second_same_real_reg_interval.liveness_used.end(), [start_instr_idx](const uint32_t& t){return start_instr_idx <= t;});
                if(tmp_closest_to_start_point_real_reg < closest_use_point_to_start_same_real_reg){
                    closest_use_point_to_start_same_real_reg = tmp_closest_to_start_point_real_reg;
                    closest_use_point_to_start_same_real_reg = i;
                }
            }
        }
        auto first_interval_with_same_real_reg = intervals_in_succ_bb.end(); 
        if(position_of_interval_in_vector_same_real_reg != UINT32_MAX){
            first_interval_with_same_real_reg = intervals_in_succ_bb.begin() + position_of_interval_in_vector;
        }

        if((first_sensitive_interval_in_bb == intervals_in_succ_bb.end()) && (first_interval_with_same_real_reg == intervals_in_succ_bb.end())){
            bool violation_detected = false;
            if(successor_map.find(succ_bb_idx) != successor_map.end()){
                for(auto succ_of_succ_bb_idx: successor_map.at(succ_bb_idx)){
                    violation_detected = check_vertical_violation_internal_function_call(control_flow_TAC, original_tac_instr_idx, real_reg, liveness_intervals, visited, original_bb_idx, succ_of_succ_bb_idx, successor_map);
                    if(violation_detected){
                        return true;
                    }
                }
            }
            return false;
        }
        else if((first_sensitive_interval_in_bb == intervals_in_succ_bb.end()) && (first_interval_with_same_real_reg != intervals_in_succ_bb.end())){
            return false;
        }
        else if((first_sensitive_interval_in_bb != intervals_in_succ_bb.end()) && (first_interval_with_same_real_reg != intervals_in_succ_bb.end()) && (get_first_use_point_within_range(start_instr_idx, *first_interval_with_same_real_reg) < get_first_use_point_within_range(start_instr_idx, *first_sensitive_interval_in_bb)) && (first_interval_with_same_real_reg->sensitivity != 1)){
            return false;
        }
        else{
            return true;
        }

    }

}


void check_vertical_correctness_internal_functions_in_CFG(std::vector<liveness_interval_struct>& liveness_intervals, std::vector<TAC_bb>& control_flow_TAC, std::vector<TAC_type>& TAC, const std::map<uint32_t, std::vector<uint32_t>>& successor_map){

    for(uint32_t tac_instr_idx = 0; tac_instr_idx < TAC.size(); ++tac_instr_idx){
        if(TAC.at(tac_instr_idx).op == Operation::FUNC_CALL){

            std::vector<uint32_t> list_of_critical_registers = {1,2,3};
            if(TAC.at(tac_instr_idx+1).op != Operation::FUNC_RES_MOV){
                list_of_critical_registers.emplace_back(0);
            }

            for(auto real_reg: list_of_critical_registers){


            /**
             * @brief Check within BB -> after func call
             * 
             */
            std::vector<liveness_interval_struct> intervals_in_same_bb_start_after_func_call;

            uint32_t bb_idx = get_bb_idx_of_instruction_idx(control_flow_TAC, tac_instr_idx);
            uint32_t end_instr_idx_curr_bb = get_idx_of_bb_end(control_flow_TAC, bb_idx);

            for(uint32_t second_interval_idx = 0; second_interval_idx < liveness_intervals.size(); ++second_interval_idx){
                if((liveness_intervals.at(second_interval_idx).liveness_used.at(0) >= tac_instr_idx) && (liveness_intervals.at(second_interval_idx).liveness_used.at(0) <= end_instr_idx_curr_bb)){
                    intervals_in_same_bb_start_after_func_call.push_back(liveness_intervals.at(second_interval_idx));
                }
            }



            uint32_t closest_use_point_to_func_call = UINT32_MAX;
            uint32_t position_of_interval_in_vector = UINT32_MAX;
            for(uint32_t i = 0; i < intervals_in_same_bb_start_after_func_call.size(); ++i){
                if(intervals_in_same_bb_start_after_func_call.at(i).sensitivity == 1){
                    liveness_interval_struct second_sensitive_interval_after_func_call_in_bb = intervals_in_same_bb_start_after_func_call.at(i);

                    //check if it contains use point that is closer to last_use_point
                    uint32_t tmp_closest_to_use_point = *std::find_if(second_sensitive_interval_after_func_call_in_bb.liveness_used.begin(), second_sensitive_interval_after_func_call_in_bb.liveness_used.end(), [tac_instr_idx](const uint32_t& t){return t >= tac_instr_idx;});
                    
                    if(tmp_closest_to_use_point < closest_use_point_to_func_call){
                        closest_use_point_to_func_call = tmp_closest_to_use_point;
                        position_of_interval_in_vector = i;
                    }
                }
            }

            auto first_sensitive_interval_after_func_call_in_bb = intervals_in_same_bb_start_after_func_call.end(); 
            if(position_of_interval_in_vector != UINT32_MAX){
                first_sensitive_interval_after_func_call_in_bb = intervals_in_same_bb_start_after_func_call.begin() + position_of_interval_in_vector;
            }


            auto first_interval_with_same_real_reg = std::find_if(intervals_in_same_bb_start_after_func_call.begin(), intervals_in_same_bb_start_after_func_call.end(), [real_reg, tac_instr_idx](const liveness_interval_struct& t){return (t.real_register_number == real_reg) && (t.liveness_used.at(0) >= tac_instr_idx);});


                if((first_interval_with_same_real_reg == intervals_in_same_bb_start_after_func_call.end()) && (first_sensitive_interval_after_func_call_in_bb == intervals_in_same_bb_start_after_func_call.end())){
                    /**
                     * @brief Check other BB
                     * 
                     */
                    bool violation_detected = false;
                    std::vector<bool> visited(control_flow_TAC.size(), false);


                    
                    if(successor_map.find(bb_idx) != successor_map.end()){
                        for(auto& succ_bb_idx: successor_map.at(bb_idx)){
                            violation_detected = check_vertical_violation_internal_function_call(control_flow_TAC, tac_instr_idx, real_reg, liveness_intervals, visited, bb_idx, succ_bb_idx, successor_map);
                            if(violation_detected){


                                uint32_t instruction_idx_to_insert = tac_instr_idx + 1;
                                TAC_type tmp_tac;

                                Operation op = Operation::MOV;
                                uint8_t sens = 0; 
                                TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
                                uint32_t src2_v = UINT32_MAX;
                                uint8_t src2_sens = 0; 
                                TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
                                uint32_t src1_v = 11; //the real register that will be spilled has to be stored in memory
                                uint8_t src1_sens = 0; 
                                TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
                                uint32_t dest_v = real_reg;
                                uint8_t dest_sens = 0;

                                generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

                                TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
                                insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

                                synchronize_instruction_index_of_liveness_intervals(liveness_intervals, instruction_idx_to_insert);
                                // liveness_intervals.at(interval_idx).contains_break_transition = true;

                                break;
                            }
                        }
                    }
                }
                else if((first_interval_with_same_real_reg != intervals_in_same_bb_start_after_func_call.end()) && (first_sensitive_interval_after_func_call_in_bb != intervals_in_same_bb_start_after_func_call.end()) && (get_first_use_point_within_range(tac_instr_idx, *first_interval_with_same_real_reg) < get_first_use_point_within_range(tac_instr_idx, *first_sensitive_interval_after_func_call_in_bb) && (first_interval_with_same_real_reg->sensitivity != 1))){
                    continue;
                }
                else if((first_sensitive_interval_after_func_call_in_bb == intervals_in_same_bb_start_after_func_call.end()) && (first_interval_with_same_real_reg != intervals_in_same_bb_start_after_func_call.end())){
                    continue;
                }
                else{

                    uint32_t instruction_idx_to_insert = tac_instr_idx + 1;
                    TAC_type tmp_tac;

                    Operation op = Operation::MOV;
                    uint8_t sens = 0; 
                    TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
                    uint32_t src2_v = UINT32_MAX;
                    uint8_t src2_sens = 0; 
                    TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
                    uint32_t src1_v = 11; //the real register that will be spilled has to be stored in memory
                    uint8_t src1_sens = 0; 
                    TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
                    uint32_t dest_v = real_reg;
                    uint8_t dest_sens = 0;

                    generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

                    TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
                    insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);


                    synchronize_instruction_index_of_liveness_intervals(liveness_intervals, instruction_idx_to_insert);

                    // liveness_intervals.at(interval_idx).contains_break_transition = true;
                }

            }
        }
    }

}