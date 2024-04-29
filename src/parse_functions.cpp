#include "parse_functions.hpp"

void make_virtual_variable_information(virtual_variables_vec& virtual_variables, std::string ssa_name, int32_t sensitivity, DATA_TYPE data_type, data_specific_information data_information, SIGNED_TYPE signedness, std::string original_name, uint8_t data_type_size, int32_t index_to_original, SSA_ADD_ON ssa_add_on){
    virtual_variable_information virtual_variable;
    virtual_variable.ssa_name = ssa_name;
    virtual_variable.sensitivity = sensitivity;
    virtual_variable.data_type = data_type;
    virtual_variable.data_information = data_information;
    virtual_variable.signedness = signedness;
    virtual_variable.original_name = original_name;
    virtual_variable.data_type_size = data_type_size;
    virtual_variable.index_to_original = index_to_original;
    virtual_variable.ssa_add_on = ssa_add_on;
    virtual_variables.emplace_back(virtual_variable);
}

/**
 * @brief constructs a ssa datatype that holds a binary operation
 * 
 * @param ssa_instruction [IN-OUT] internal ssa datastructure
 * @param virtual_variables [IN-OUT] list of variables encounterd in the function
 * @param tokens [IN] tokenized input parameter string
 * @param operation [IN] explicit binary operation type
 */
void parseBinaryOpExpr(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, const std::vector<std::string>& tokens, const Operation operation){
    ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_NORMAL;
    
    normal_instruction n_instr;
    n_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables, tokens.at(1), "");
    get_operand_type(n_instr.src1, virtual_variables, tokens.at(2));
    get_operand_type(n_instr.src2, virtual_variables, tokens.at(3));
    n_instr.operation = operation;

    ssa_instruction.instr = n_instr;
}

/**
 * @brief constructs a ssa datatype that holds a unary operation
 * 
 * @param ssa_instruction [IN-OUT] internal ssa datastructure
 * @param virtual_variables [IN-OUT] list of variables encounterd in the function
 * @param tokens [IN] tokenized input parameter string
 * @param operation [IN] explicit unary operation type
 */
void parseUnaryOpExpr(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, const std::vector<std::string>& tokens, const Operation operation){
    ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_NORMAL;
    
    normal_instruction n_instr;
    n_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables, tokens.at(1), "");
    get_operand_type(n_instr.src1, virtual_variables, tokens.at(2));
    n_instr.src2.op_data = UINT32_MAX;
    n_instr.src2.op_type = DATA_TYPE::NONE;
    n_instr.operation = operation;

    ssa_instruction.instr = n_instr;
}

/**
 * @brief constructs a ssa datatype that holds a conditional statement
 * 
 * @param ssa_instruction [IN-OUT] internal ssa datastructure
 * @param virtual_variables [IN-OUT] list of variables encounterd in the function
 * @param tokens [IN] tokenized input parameter string
 * @param bb_number [IN] basic block number
 * @param true_op [IN] condition, follows true branch
 * @param false_op [IN] negated true condition, follows false branch
 */
void parseConditionOpExpr(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, const std::vector<std::string>& tokens, const uint32_t bb_number, const Operation true_op, const Operation false_op){
    ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_CONDITON;
    condition_instruction c_instr;
    
    if(is_number(tokens.at(1))){
        c_instr.op1.op_type = DATA_TYPE::NUM;
        c_instr.op1.op_data = stoi(tokens.at(1));
    }
    else{
        c_instr.op1.op_data = get_index_of_virtual_variable(virtual_variables, tokens.at(1), "");
        c_instr.op1.op_type = virtual_variables.at(c_instr.op1.op_data).data_type;
    }

    if(is_number(tokens.at(2))){
        c_instr.op2.op_type = DATA_TYPE::NUM;
        c_instr.op2.op_data = stoi(tokens.at(2));
    }
    else{
        c_instr.op2.op_data = get_index_of_virtual_variable(virtual_variables, tokens.at(2), "");
        c_instr.op2.op_type = virtual_variables.at(c_instr.op2.op_data).data_type;
    }

    if(static_cast<uint32_t>(stoi(tokens.at(3))) != bb_number + 1){
        c_instr.op = true_op;
        c_instr.true_dest = static_cast<uint32_t>(stoi(tokens.at(3)));
        c_instr.false_dest = static_cast<uint32_t>(stoi(tokens.at(4)));
    }
    else{
        c_instr.op = false_op;
        c_instr.false_dest = static_cast<uint32_t>(stoi(tokens.at(3)));
        c_instr.true_dest = static_cast<uint32_t>(stoi(tokens.at(4)));
    }

    ssa_instruction.instr = c_instr;
}

void parse_phi(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens){
    ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_PHI;
    tokens.erase(tokens.begin());
    clean_tokens(tokens);
    phi_instruction phi_instr;
    phi_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables, tokens.at(0), "");

    for(uint32_t i = 1; i < tokens.size(); ++i){
        std::string origin_name = tokens.at(i).substr(0, tokens.at(i).find("("));
        uint32_t origin_name_index = get_index_of_virtual_variable(virtual_variables,origin_name, "");

        uint32_t bb_number = static_cast<uint32_t>(stoi(tokens.at(i).substr(tokens.at(i).rfind("(")+1, tokens.at(i).rfind(")"))));
        phi_instr.srcs.push_back(std::make_tuple(origin_name_index, bb_number));  
    }

    ssa_instruction.instr = phi_instr;
}

void parse_addr_expr(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens){
    ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_REF;
    reference_instruction r_instr;
    r_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables, tokens.at(1), "");

    std::string token = tokens.at(2);
    std::string modifcation = "";
    if(tokens.at(2).find("&") != std::string::npos){
        modifcation = "&";
        token = token.substr(1, token.length() - 1);
    }
    r_instr.src.index_to_variable = get_index_of_virtual_variable(virtual_variables, token, modifcation);
    ssa_instruction.instr = r_instr;
}

void parse_nop_expr(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens){
    ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_ASSIGN;

    assignment_instruction a_instr;
    a_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables,tokens.at(1), "");
    get_operand_type(a_instr.src, virtual_variables,tokens.at(2));

    ssa_instruction.instr = a_instr;
}


void parse_negate_expr(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens){
    ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_NEGATE;
    
    normal_instruction n_instr;
    n_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables, tokens.at(1), "");
    get_operand_type(n_instr.src1, virtual_variables, tokens.at(2));
    n_instr.src2.op_type = DATA_TYPE::NONE;
    n_instr.src2.op_data = UINT32_MAX;
    n_instr.operation = Operation::NEG;

    ssa_instruction.instr = n_instr; 
}


void parse_integer_cst_expr(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens){
    if(is_array(tokens.at(1))){
        ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_STORE;

        mem_store_instruction m_instr;
        std::string dest = tokens.at(1);
        m_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables, dest, "");

        if(is_number(tokens.at(2))){
            m_instr.value.op_type = DATA_TYPE::NUM;
            m_instr.value.op_data = stoi(tokens.at(2));
        }
        else{
            m_instr.value.op_data = get_index_of_virtual_variable(virtual_variables, tokens.at(2), "");
            m_instr.value.op_type = virtual_variables.at(m_instr.value.op_data).data_type;
        }

        ssa_instruction.instr = m_instr;
    }
    else{
        ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_ASSIGN;
        assignment_instruction a_instr;

        a_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables, tokens.at(1), "");
        
        a_instr.src.op_type = DATA_TYPE::NUM;
        a_instr.src.op_data = stoi(tokens.at(2));

        ssa_instruction.instr = a_instr;
    }

}
void parse_ssa_name_expr(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens){
    //check if instruction represents store
    //check if destination has dereference
    if((tokens.at(1).at(0) == '*') || is_array(tokens.at(1))){ //TODO: assume this will always declare a store
        ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_STORE;

        mem_store_instruction m_instr;
        std::string dest = tokens.at(1);
        std::string modification = "";
        if(tokens.at(1).find("*") != std::string::npos){
            modification = "*";
            dest = dest.substr(1, dest.length() - 1);
        }
        m_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables, dest, modification);

        if(is_number(tokens.at(2))){
            m_instr.value.op_type = DATA_TYPE::NUM;
            m_instr.value.op_data = stoi(tokens.at(2));
        }
        else{
            m_instr.value.op_data = get_index_of_virtual_variable(virtual_variables, tokens.at(2), "");
            m_instr.value.op_type = virtual_variables.at(m_instr.value.op_data).data_type;
        }

        ssa_instruction.instr = m_instr;
    }
    else if(is_variable(tokens.at(1))){
        ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_ASSIGN;

        assignment_instruction a_instr;
        std::string dest = tokens.at(1);
        a_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables, dest, "");

        if(is_number(tokens.at(2))){
            a_instr.src.op_type = DATA_TYPE::NUM;
            a_instr.src.op_data = stoi(tokens.at(2));
        }
        else{
            a_instr.src.op_data = get_index_of_virtual_variable(virtual_variables, tokens.at(2), "");
            a_instr.src.op_type = virtual_variables.at(a_instr.src.op_data).data_type;
        }

        ssa_instruction.instr = a_instr;
    }
    else{
        throw std::runtime_error("in parse_ssa_name_expr: destination is not pointer, array or normal varialbe. Can not handle yet");
    }
    
}
void parse_array_ref_expr(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens){
    //is load
    ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_LOAD;
    mem_load_instruction m_instr;
    m_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables, tokens.at(1), "");
    std::string token = tokens.at(2);
    std::string modification = "";
    if(token.find("*") != std::string::npos){
        modification = "*";
        token = token.substr(1, token.length() - 1);
    }
    m_instr.base_address.index_to_variable = get_index_of_virtual_variable(virtual_variables, token, modification);
    m_instr.offset.op_data = 0;
    m_instr.offset.op_type = DATA_TYPE::NUM;
    ssa_instruction.instr = m_instr;


}
void parse_mem_ref_expr(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens){
    //is load, distinguish simple mem_ref and MEM[...]
    ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_LOAD;
    mem_load_instruction m_instr;

    if(tokens.at(2).rfind("MEM[") != 0){
        m_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables, tokens.at(1), "");
        std::string token = tokens.at(2); //.substr(tokens.at(2).find("*")+1, tokens.at(2).length()); //TODO: maybe remove substr func
        std::string modification = "";
        if(tokens.at(2).find("*") != std::string::npos){
            modification = "*";
            token = token.substr(1, token.length() - 1);
        }
        m_instr.base_address.index_to_variable = get_index_of_virtual_variable(virtual_variables, token, modification);
        m_instr.offset.op_data = 0;
        m_instr.offset.op_type = DATA_TYPE::NUM;
        ssa_instruction.instr = m_instr;

    }
    else{
        ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_LOAD;
        // mem_load_instruction m_instr;
        m_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables, tokens.at(1), "");
        //TODO: not sure if this is sufficent
        //remove cast of base address"
        if(tokens.at(2).at(4) == '('){
            uint32_t idx = 0;
            for(uint32_t i = 0; i < tokens.size(); ++i){
                if(tokens.at(i).find(")") != std::string::npos){
                    idx = i;
                    break;
                }
            }
            std::string base = tokens.at(idx).substr(tokens.at(idx).find(")")+1, std::string::npos - 1);
            m_instr.base_address.index_to_variable = get_index_of_virtual_variable(virtual_variables, base, "");
        }
        else{
            std::string base = tokens.at(2).substr(4, std::string::npos - 1);
            m_instr.base_address.index_to_variable = get_index_of_virtual_variable(virtual_variables, base, "");
        }


        //get offset TODO: for now only numbers
        m_instr.offset.op_type = DATA_TYPE::NUM;
        m_instr.offset.op_data = stoi(tokens.at(tokens.size() - 3).substr(0, tokens.at(tokens.size() - 3).find("B")));
        ssa_instruction.instr = m_instr;
    }




}


void parse_var_decl_expr(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens){
    ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_ASSIGN;

    assignment_instruction a_instr;
    a_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables,tokens.at(1), "");
    get_operand_type(a_instr.src, virtual_variables,tokens.at(2));

    ssa_instruction.instr = a_instr;
}

uint32_t decimalToOctal(uint8_t n) {
    int remainder; 
    uint32_t octal = 0, i = 1;
   
    while(n != 0) {
        remainder = n%8;
        n = n/8;
        octal = octal + (remainder*i);
        i = i*10;
    }
    return octal;
}

void parse_string_cst(function& func, ssa& ssa_instruction, virtual_variables_vec& virtual_variables, const std::string& line){
    std::vector<std::string> tokens;
    //split line into tokens, bc data string might contain comma or spaces
    std::string tmp_line(line);
    std::string id = "gimple_assign <string_cst, ";
    tmp_line = tmp_line.substr(tmp_line.find("gimple_assign <string_cst, ") + id.length(), std::string::npos);
    std::string tmp_token = tmp_line.substr(0, tmp_line.find(","));
    tmp_line = tmp_line.substr(tmp_line.find(tmp_token) + tmp_token.length(), std::string::npos);
    tokens.emplace_back(tmp_token);

    tmp_token = tmp_line.substr(2, tmp_line.rfind("\"") - 1);


    //transform into correct format (hex numbers need to be converted into octal representatio)
    std::string sanitized_token = "";
    boost::regex expression("\\\\x[0-9A-Fa-f]{2}"); //detect string of form \x?? where ? is any hex value
    boost::regex newline_expression("\\\\n");
    for(uint32_t i = 1; i < tmp_token.size()-1; ++i){
        std::string tmp = tmp_token.substr(i, 4);
        
        if(boost::regex_match(tmp, expression)){

            uint32_t hex_string_to_int;   
            tmp = tmp.substr(2, std::string::npos);
            std::istringstream iss(tmp);
            iss >> std::hex >> hex_string_to_int;

            uint32_t octal_number = decimalToOctal(hex_string_to_int);

            std::string str = std::to_string(octal_number);
            std::ostringstream os;
            os << std::setw(3) << std::setfill('0') << str;
            str = os.str();
            sanitized_token += "\\" + str;
            i+=3;

        }
        else if(tmp.at(0) == '\\'){
            if(tmp.at(1) == '\\'){
                sanitized_token += "\\\\";
            }
            else if(tmp.at(1) == 'n'){
                sanitized_token += "\\012";
            }
            else if(tmp.at(1) == 'v'){
                sanitized_token += "\\013";
            }
            else if(tmp.at(1) == 'r'){
                sanitized_token += "\\015";
            }
            else if(tmp.at(1) == 't'){
                sanitized_token += "\\011";
            }
            else if(tmp.at(1) == '\''){
                sanitized_token += "\\\'";
            }
            else if(tmp.at(1) == '"'){
                sanitized_token += "\\\"";
            }
            else if(tmp.at(1) == 'f'){
                sanitized_token += "\\014";
            }
            else if(tmp.at(1) == 'b'){
                sanitized_token += "\\010";
            }
            else{
                std::cout << tmp << std::endl;
                throw std::runtime_error("parsing string array: can not identify byte value");
            }
            i+=1;
        }
        else{
            tmp = tmp.substr(0, 1);
            sanitized_token += tmp;
        }
    }

    tokens.emplace_back(sanitized_token);

    //get index of variable 
    uint32_t idx_of_variable = get_index_of_virtual_variable(virtual_variables, tokens.at(0), "");
    if(virtual_variables.at(idx_of_variable).data_type == DATA_TYPE::LOCAL_ARR){
        ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_INITIALIZE_LOCAL_ARR;

        initialize_local_array_instruction i_instr;

        i_instr.data_section_name = tokens.at(0);
        array_information a_info = boost::get<array_information>(virtual_variables.at(idx_of_variable).data_information);
        uint32_t data_size = 1;
        for(uint32_t dim = 0; dim < a_info.dimension; ++dim){
            data_size *= a_info.depth_per_dimension.at(dim).op_data;
        }
        i_instr.data_size = data_size * virtual_variables.at(idx_of_variable).data_type_size;
        i_instr.data_storage = DATA_STORAGE_TYPE::ASCII;
        i_instr.ascii_data = tokens.at(1);
        i_instr.virtual_variable_idx  = idx_of_variable;

        ssa_instruction.instr = i_instr;
    
        func.data_section.resize(func.data_section.size() + 1);
        func.data_section.back().ascii_data = tokens.at(1);
        func.data_section.back().data_storage = DATA_STORAGE_TYPE::ASCII;
        func.data_section.back().data_section_name = tokens.at(0);
    }
    else{
        std::cout << "data type: " << static_cast<uint32_t>(virtual_variables.at(idx_of_variable).data_type) << std::endl;
        throw std::runtime_error("in parse_string_cst: can not handle this datatype yet");
    }

}


void parse_expression(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens, uint32_t bb_number){
    if(tokens.at(0) == "addr_expr"){
        parse_addr_expr(ssa_instruction, virtual_variables, tokens);
    }
    else if(tokens.at(0) == "array_ref"){
        parse_array_ref_expr(ssa_instruction, virtual_variables, tokens);
    }
    else if(tokens.at(0) == "nop_expr"){
        parse_nop_expr(ssa_instruction, virtual_variables, tokens);
    }
    else if(tokens.at(0) == "mem_ref"){
        parse_mem_ref_expr(ssa_instruction, virtual_variables, tokens);
    }
    else if(tokens.at(0) == "pointer_plus_expr"){
        parseBinaryOpExpr(ssa_instruction, virtual_variables, tokens, Operation::ADD);
    }
    else if(tokens.at(0) == "plus_expr"){
        parseBinaryOpExpr(ssa_instruction, virtual_variables, tokens, Operation::ADD);
    }
    else if(tokens.at(0) == "minus_expr"){
        parseBinaryOpExpr(ssa_instruction, virtual_variables, tokens, Operation::SUB);
    }
    else if(tokens.at(0) == "mult_expr"){
        parseBinaryOpExpr(ssa_instruction, virtual_variables, tokens, Operation::MUL);
    }
    else if(tokens.at(0) == "negate_expr"){
        parse_negate_expr(ssa_instruction, virtual_variables, tokens);
    }
    else if(tokens.at(0) == "bit_xor_expr"){
        parseBinaryOpExpr(ssa_instruction, virtual_variables, tokens, Operation::XOR);
    }
    else if(tokens.at(0) == "bit_ior_expr"){
        parseBinaryOpExpr(ssa_instruction, virtual_variables, tokens, Operation::LOGICAL_OR);
    }
    else if(tokens.at(0) == "bit_and_expr"){
        parseBinaryOpExpr(ssa_instruction, virtual_variables, tokens, Operation::LOGICAL_AND);
    }
    else if(tokens.at(0) == "bit_not_expr"){
        parseUnaryOpExpr(ssa_instruction, virtual_variables, tokens, Operation::INVERT);
    }
    else if(tokens.at(0) == "le_expr"){
        parseConditionOpExpr(ssa_instruction, virtual_variables, tokens, bb_number, Operation::LESS_EQUAL, Operation::GREATER);
    }
    else if(tokens.at(0) == "lt_expr"){
        parseConditionOpExpr(ssa_instruction, virtual_variables, tokens, bb_number, Operation::LESS, Operation::GREATER_EQUAL);
    }
    else if(tokens.at(0) == "ge_expr"){
        parseConditionOpExpr(ssa_instruction, virtual_variables, tokens, bb_number, Operation::GREATER_EQUAL, Operation::LESS);
    }
    else if(tokens.at(0) == "gt_expr"){
        parseConditionOpExpr(ssa_instruction, virtual_variables, tokens, bb_number, Operation::GREATER, Operation::LESS_EQUAL);
    }
    else if(tokens.at(0) == "eq_expr"){
        parseConditionOpExpr(ssa_instruction, virtual_variables, tokens, bb_number, Operation::EQUAL, Operation::NOT_EQUAL);
    }
    else if(tokens.at(0) == "ltgt_expr"){
        throw std::runtime_error("parse_ltgt_expr not implemented yet");
    }
    else if(tokens.at(0) == "ne_expr"){
        parseConditionOpExpr(ssa_instruction, virtual_variables, tokens, bb_number, Operation::NOT_EQUAL, Operation::EQUAL);
    }
    else if(tokens.at(0) == "integer_cst"){
        parse_integer_cst_expr(ssa_instruction, virtual_variables, tokens);
    }
    else if(tokens.at(0) == "lshift_expr"){
        parseBinaryOpExpr(ssa_instruction, virtual_variables, tokens, Operation::LSL);
    }
    else if(tokens.at(0) == "rshift_expr"){
        parseBinaryOpExpr(ssa_instruction, virtual_variables, tokens, Operation::LSR);
    }
    else if(tokens.at(0) == "lrotate_expr"){
        throw std::runtime_error("parse_lrotate_expr not implemented yet");
    }
    else if(tokens.at(0) == "rrotate_expr"){
        throw std::runtime_error("parse_rrotate_expr not implemented yet");
    }
    else if(tokens.at(0) == "ssa_name"){
        parse_ssa_name_expr(ssa_instruction, virtual_variables, tokens);
    }
    else if(tokens.at(0) == "var_decl"){
        parse_var_decl_expr(ssa_instruction, virtual_variables, tokens);
    }

    else if(tokens.at(0) == "constructor"){

    }
    else{
        throw std::runtime_error(tokens.at(0) + " expression not implemented");
    }
}

/**
 * @brief handles cases that have pre-fix gimple statement 'gimple_assign'
 * 
 * @param func [IN-OUT] internal data-structure to represent function, gets filled with information
 * @param ssa_instruction 
 * @param virtual_variables [IN-OUT] list of variables encounterd in the function
 * @param tokens [IN] tokenized input parameter string
 * @param bb_number 
 */
void parse_assign(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens, const uint32_t bb_number){
    clean_tokens(tokens);
    parse_expression(ssa_instruction, virtual_variables, tokens, bb_number);
}

void parse_return(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens){
    clean_tokens(tokens);
    return_instruction r_instr;
    ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_RETURN;
    if(tokens.at(0) == "NULL"){
        r_instr.return_value.op_type = DATA_TYPE::NONE;
        r_instr.return_value.op_data = UINT32_MAX;
    }
    else{
        r_instr.return_value.op_data = get_index_of_virtual_variable(virtual_variables, tokens.at(0), "");
        r_instr.return_value.op_type = virtual_variables.at(r_instr.return_value.op_data).data_type;
    }
    ssa_instruction.instr = r_instr;
}

void parse_label(ssa& ssa_instruction, std::vector<std::string>& tokens){
    clean_tokens(tokens);
    label_instruction l_instr;
    ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_LABEL;

    //remove leading < > brackets and store 
    l_instr.label_name = tokens.at(0).substr(1, tokens.at(0).size() - 2);
    ssa_instruction.instr = l_instr;
}

void parse_cond(ssa& ssa_instruction, virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens, std::ifstream& infile, uint32_t bb_number){
    clean_tokens(tokens);
    //fill jump destinations with next three lines of infile
    //replace true_dest
    std::vector<std::string> true_dest_tokens;
    std::string line;
    std::getline(infile, line);
    split_line_into_tokens(true_dest_tokens, line);
    while(true_dest_tokens.back().rfind(";", true_dest_tokens.back().length()-1) == std::string::npos){
        true_dest_tokens.erase(true_dest_tokens.begin() + true_dest_tokens.size()-1);
    }
    std::string true_dest_bb = true_dest_tokens.back().substr(0, true_dest_tokens.back().find(">"));
    tokens.at(3) = true_dest_bb;

    std::getline(infile, line); //skip single line else

    //replace false_dest
    std::vector<std::string> false_dest_tokens;
    std::getline(infile, line);
    split_line_into_tokens(false_dest_tokens, line);
    while(false_dest_tokens.back().rfind(";", false_dest_tokens.back().length()-1) == std::string::npos){
        false_dest_tokens.erase(false_dest_tokens.begin() + false_dest_tokens.size()-1);
    }
    std::string false_dest_bb = false_dest_tokens.back().substr(0, false_dest_tokens.back().find(">"));
    tokens.at(4) = false_dest_bb;

    parse_expression(ssa_instruction, virtual_variables, tokens, bb_number);
}


void parse_function_call(const std::vector<internal_function_calls_struct>& internal_function_calls, ssa& ssa_instruction, virtual_variables_vec& virtual_variables, const std::vector<std::string>& tokens, const INSTRUCTION_TYPE instruction_type){
    ssa_instruction.instr_type = instruction_type;
    

    std::string func_name = tokens.at(0);

    std::string func_name_upper = boost::to_upper_copy(func_name);
    auto it_internal_function_call = std::find_if(internal_function_calls.begin(), internal_function_calls.end(), [func_name_upper](const internal_function_calls_struct& t){return t.function_name == func_name_upper;});
    if(it_internal_function_call == internal_function_calls.end()){
        throw std::runtime_error("in parse_function_call_with_return: can not find name of internal function call");
    }

    std::vector<param_struct> parameters;
    //check if function contains no parameter -> ()
    if(tokens.size() == 2){

    }
    //check if function contains at least one parameter
    else if(tokens.size() >= 3){
        for(uint32_t i = 2; i < tokens.size(); ++i){
            std::string param = tokens.at(i);
            //TODO: parameter can be given by reference or dereferenced  value (with leading & or leading *)
            param_struct op_param;
            try{
                op_param.sensitivity = it_internal_function_call->parameter_sensitivity.at(i - 2);
            }
            catch(...){
                throw std::runtime_error("in parse_function_call_without_return: amount of internal function call sensitivities is less than number of parameters. please assign every parameter a sensitivity level (SEC or PUB). ");
            }
            

            if(param.rfind("&", 0) != std::string::npos){
                param = param.substr(1, param.length() - 1);
                uint32_t param_idx = get_index_of_virtual_variable(virtual_variables, param, "&");
                op_param.op.op_type = virtual_variables.at(param_idx).data_type;
                op_param.op.op_data = param_idx;
                parameters.emplace_back(op_param);
            }
            else if(param.rfind("*", 0) != std::string::npos){
                param = param.substr(1, param.length() - 1);
                uint32_t param_idx = get_index_of_virtual_variable(virtual_variables, param, "*");
                op_param.op.op_type = virtual_variables.at(param_idx).data_type;
                op_param.op.op_data = param_idx;
                parameters.emplace_back(op_param);
            }
            else if(is_number(param)){ //TODO: could also be float
                op_param.op.op_type = DATA_TYPE::NUM;
                op_param.op.op_data = stoi(param);
                parameters.emplace_back(op_param);
                
            }
            else{
                uint32_t param_idx = get_index_of_virtual_variable(virtual_variables, param, "");
                op_param.op.op_type = virtual_variables.at(param_idx).data_type;
                op_param.op.op_data = param_idx;
                parameters.emplace_back(op_param);
            }
        }
    }
    else{
        throw std::runtime_error("error while parsing function call without return");
    }

    if(instruction_type == INSTRUCTION_TYPE::INSTR_FUNC_RETURN){
        function_call_with_return_instruction func_instr;
        func_instr.func_name = func_name;
        func_instr.parameters = parameters;

        //parse result variable
        if(is_variable(tokens.at(1))){
            func_instr.dest.index_to_variable = get_index_of_virtual_variable(virtual_variables, tokens.at(1), "");
            func_instr.dest_sensitivity = it_internal_function_call->return_sensitivity;
        }
        else{
            throw std::runtime_error("in parse_func_call_with_return: return parameter is not normal variable. Can not handle yet");
        }
        ssa_instruction.instr = func_instr;
    }
    else{
        function_call_without_return_instruction func_instr;
        func_instr.func_name = func_name;
        func_instr.parameters = parameters;
        ssa_instruction.instr = func_instr;
    }

    ssa_instruction.sensitivity_level = 0; //TODO: for now 
}


void parse_call(const std::vector<internal_function_calls_struct>& internal_function_calls, ssa& ssa_instruction,virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens){
    clean_tokens(tokens);
    if(tokens.at(1) == "NULL"){ //no return
        parse_function_call(internal_function_calls, ssa_instruction, virtual_variables, tokens, INSTRUCTION_TYPE::INSTR_FUNC_NO_RETURN);
    }
    else{ //return
        parse_function_call(internal_function_calls, ssa_instruction, virtual_variables, tokens, INSTRUCTION_TYPE::INSTR_FUNC_RETURN);
    }
}

void parse_goto(ssa& ssa_instruction,std::vector<std::string>& tokens){

    while(tokens.back().rfind(";", tokens.back().length()-1) == std::string::npos){
        tokens.erase(tokens.begin() + tokens.size()-1);
    }
    ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_BRANCH;

    branch_instruction b_instr;
    b_instr.dest_bb = static_cast<uint32_t>(stoi(tokens.back().substr(0, tokens.back().find(">"))));
    ssa_instruction.instr = b_instr;
}

/**
 * @brief 
 * 
 * @param func 
 * @param line 
 * @param virtual_variables 
 * @param infile 
 */
void parse_raw_ssa(function& func, const std::string& line, virtual_variables_vec& virtual_variables, std::ifstream& infile){
    ssa ssa_instruction;

    default_instruction d_instr;
    d_instr.err = "This is a default type";
    ssa_instruction.instr = d_instr;
    ssa_instruction.instr_type = INSTRUCTION_TYPE::INSTR_DEFAULT;

    if(line.find("gimple_assign <string_cst") != std::string::npos){
        parse_string_cst(func, ssa_instruction, virtual_variables, line);
    }
    else{
        std::vector<std::string> tokens;
        split_line_into_tokens(tokens, line); 


        if(tokens.at(0) == "gimple_assign"){
            parse_assign(ssa_instruction, virtual_variables, tokens, func.bb.back().bb_number);
        }
        else if(tokens.at(0) == "goto"){
            parse_goto(ssa_instruction, tokens);
        }
        else if(tokens.at(0) == "gimple_call"){
            parse_call(func.internal_function_calls, ssa_instruction, virtual_variables, tokens);
        }
        else if(tokens.at(0) == "gimple_cond"){
            parse_cond(ssa_instruction, virtual_variables, tokens, infile, func.bb.back().bb_number);
        }
        else if((tokens.at(0) == "#") &&  (tokens.at(1) == "gimple_phi")){
            parse_phi(ssa_instruction, virtual_variables, tokens );
        }
        else if(tokens.at(0) == "gimple_return"){
            parse_return(ssa_instruction, virtual_variables, tokens);
        }
        else if(tokens.at(0) == "gimple_label"){
            parse_label(ssa_instruction, tokens);
        }
        else if(tokens.at(0) == "gimple_resx"){
        }
        else{
            throw std::runtime_error(tokens.at(0) +  " is unkown/not a gimple statement");
        }
    }



    func.bb.back().ssa_instruction.push_back(ssa_instruction);
}




/**
 * @brief extracts all defines and searches for matching PoMMES defines to parse the security level of the function parameters
 * 
 * @param func [IN-OUT] internal data-structure of function, gets information of security level of arguments
 * @param virtual_variables [IN-OUT] assigns sensitivity to arguments
 */
void parse_security_level_of_parameter(function& func, virtual_variables_vec& virtual_variables){
    
    std::array<char, 128> buffer;
    std::string result;

    #if defined(REFRESH_CASE_STUDY) 
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/locality_refresh/pommes_locality_refresh_d2.c", "r"), pclose);
    #elif defined(ISW_AND_CASE_STUDY)
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/isw_and/pommes_isw_and_d1.c", "r"), pclose);
    #elif defined(PINI_MULT_CASE_STUDY)
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/pini_mult/pommes_pini_mult_d1.c", "r"), pclose);
    #elif defined(SUBBYTE_RP)
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/rp_sbox_subbytes_func/pommes_subbytes_rp_func_d1.c", "r"), pclose);
    #elif defined(MULTSHARE)
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/rp_sbox_multshare/pommes_multshare_d1.c", "r"), pclose);
    #elif defined(SQUARE_SHARE)
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/rp_sbox_squareshare/pommes_square_share_d1.c", "r"), pclose);
    #elif defined(SQUARE)
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/rp_sbox_square/pommes_square_d1.c", "r"), pclose);
    #elif defined(MULTTABLE)
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/rp_sbox_multtable/pommes_multtable_d1.c", "r"), pclose);
    #endif


    std::string sensitivity_level_macro = "POMMES_INPUT_" +  boost::to_upper_copy<std::string>(func.function_name);
    if (!pipe) {
        throw std::runtime_error("in parse_security_level_of_parameter: popen() failed!");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    char* pch = strtok((char*)result.c_str(), "#define");
    while(pch != NULL){

        std::string tmp(pch);

        if(tmp.find(sensitivity_level_macro) != std::string::npos){
            char* pch2 = strtok((char*)tmp.c_str(), " "); // looks now something like this #define POMMES_INPUT_XY Z
            pch2 = strtok(NULL, " ");   //advance pointer, points to POMMES_INPUT_XY Z
            
            uint32_t parameter_ctr = 0;

            while(pch2 != NULL){
                std::string tmp2(pch2);
                tmp2 = boost::to_upper_copy<std::string>(tmp2);
                if(tmp2.find("RETURNSEC") != std::string::npos){
                    func.return_sensitivity = 1;
                }
                else if(tmp2.find("RETURNPUB") != std::string::npos){
                    func.return_sensitivity = 0;
                }
                else if(tmp2.find("SEC") != std::string::npos){
                    func.arguments.at(parameter_ctr).sensitivity = 1;
                    std::string id = func.arguments.at(parameter_ctr).id;
                    auto it = std::find_if(virtual_variables.begin(), virtual_variables.end(), [id](const virtual_variable_information t){return t.ssa_name == id;});
                    
                    if(it == virtual_variables.end()){
                        std::cout << "identifier is: " << id << std::endl;
                        throw std::runtime_error("in parse_security_level_of_parameter: can not find identifier of parameter");
                    }

                    //addressess of pointer and arrays are not sensitive -> everytime this element will be decremented we decrement the sensitivity -> if 1 reached -> marks sensitive instruction
                    if(it->data_type == DATA_TYPE::INPUT_ARR){
                        array_information a_info = boost::get<array_information>(it->data_information);
                        it->sensitivity = 1 + a_info.dimension;
                    }
                    else if(it->data_type == DATA_TYPE::INPUT_VAR_PTR){
                        pointer_information p_info = boost::get<pointer_information>(it->data_information);
                        it->sensitivity = 1 + p_info.dimension;
                    }
                    else if(it->data_type == DATA_TYPE::INPUT_ARR_PTR){
                        array_information a_info = boost::get<array_information>(it->data_information);
                        it->sensitivity = 1 + a_info.dimension;
                    }
                    else{
                        it->sensitivity = 1;
                    }
                    parameter_ctr += 1;
                    
                }
                else if(tmp2.find("PUB") != std::string::npos){
                    func.arguments.at(parameter_ctr).sensitivity = 0;
                    std::string id = func.arguments.at(parameter_ctr).id;
                    auto it = std::find_if(virtual_variables.begin(), virtual_variables.end(), [id](const virtual_variable_information t){return t.ssa_name == id;});
                    it->sensitivity = 0;
                    parameter_ctr += 1;
                }
                else{
                    throw std::runtime_error("unkown security level in " + sensitivity_level_macro + " macro. Allowed: SEC and PUB");
                }
                pch2 = strtok(NULL, " ");

            }

            if(parameter_ctr != func.arguments.size()){
                throw std::runtime_error("not all inputs were annotated in " + sensitivity_level_macro);
            }

            break;
        }
        pch = strtok(NULL, "#define"); //advance token finding to next define
    }
}

//TODO: input parameter: file name, for now it is enough
void parse_security_level_function_call(function& func){
    
    std::array<char, 128> buffer;
    std::string result;
    #if defined(REFRESH_CASE_STUDY) 
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/locality_refresh/pommes_locality_refresh_d1.c", "r"), pclose);
    #elif defined(ISW_AND_CASE_STUDY)
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/isw_and/pommes_isw_and_d1.c", "r"), pclose);
    #elif defined(PINI_MULT_CASE_STUDY)
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/pini_mult/pommes_pini_mult_d1.c", "r"), pclose);
    #elif defined(SUBBYTE_RP)
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/rp_sbox_subbytes_func/pommes_subbytes_rp_func_d1.c", "r"), pclose);
    #elif defined(MULTSHARE)
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/rp_sbox_multshare/pommes_multshare_d1.c", "r"), pclose);
    #elif defined(SQUARE_SHARE)
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/rp_sbox_squareshare/pommes_square_share_d1.c", "r"), pclose);
    #elif defined(SQUARE)
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/rp_sbox_square/pommes_square_d1.c", "r"), pclose);
    #elif defined(MULTTABLE)
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("echo | arm-none-eabi-gcc -dM -E ./examples/rp_sbox_multtable/pommes_multtable_d1.c", "r"), pclose);
    #endif


    std::string sensitivity_level_macro = "POMMES_FUNCTION_CALL_";
    if (!pipe) {
        throw std::runtime_error("in parse_security_level_of_parameter: popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    char* end_pch;
    char* pch = strtok_r((char*)result.c_str(), "#define", &end_pch);
    
    while(pch != NULL){
        std::string tmp(pch);
        if(tmp.find(sensitivity_level_macro) != std::string::npos){

            //get function name
            func.internal_function_calls.resize(func.internal_function_calls.size() + 1);

            std::string identifier = " POMMES_FUNCTION_CALL_";
            std::string func_call_name = tmp.substr(identifier.length(), std::string::npos - 1);
            uint32_t pos_delimiter = func_call_name.find(" ");
            func_call_name = func_call_name.substr(0, pos_delimiter);
            func.internal_function_calls.back().function_name = func_call_name;
            //parse sensitivity of function parameters
            char* end_pch2;
            char* pch2 = strtok_r((char*)tmp.c_str(), " ", &end_pch2);
            pch2 = strtok_r(NULL, " ", &end_pch2);
            pch2[strcspn(pch2, "\n")] = 0;
            uint32_t parameter_ctr = 0;
            if(pch2[0] != '\0'){
                while(pch2 != NULL){
                    std::string tmp2(pch2);
                    tmp2 = boost::to_upper_copy<std::string>(tmp2);
            
                    if(tmp2.find("RETURNSEC") != std::string::npos){
                        func.internal_function_calls.back().return_sensitivity = 1;
                    }
                    else if(tmp2.find("RETURNPUB") != std::string::npos){
                        func.internal_function_calls.back().return_sensitivity = 0;
                    }
                    else if(tmp2.find("SEC") != std::string::npos){
                        func.internal_function_calls.back().parameter_sensitivity.emplace_back(1);
                                            }
                    else if(tmp2.find("PUB") != std::string::npos){
                        func.internal_function_calls.back().parameter_sensitivity.emplace_back(0);
                    }
                    else if(tmp2.empty()){

                    }
                    else{
                        throw std::runtime_error("unkown security level in " + sensitivity_level_macro + " macro. Allowed: SEC and PUB");
                    }
                    pch2 = strtok_r(NULL, " ", &end_pch2);
                    parameter_ctr += 1;
                }
            }

        }
        pch = strtok_r(NULL, "#define", &end_pch);
    }
}

uint8_t parse_data_type_size(std::string data_type){
    if(data_type == "unsigned int"){
        return 4;
    }
    else if(data_type == "unsigned char"){
        return 1;
    }
    else if(data_type == "unsigned short"){
        return 2;
    }
    else if(data_type == "unsigned short int"){
        return 2;
    }
    else if(data_type == "unsigned"){
        return 4;
    }
    else if(data_type == "unsigned int"){
        return 4;
    }
    else if(data_type == "unsigned long int"){
        return 4;
    }
    else if(data_type == "long unsigned int"){
        return 4;
    }
    else if(data_type == "unsigned long"){
        return 4;
    }
    else if(data_type == "unsigned long long"){
        return 4;
    }
    else if(data_type == "unsigned long long int"){
        return 4;
    }
    else if(data_type == "uint8_t"){
        return 1;
    }
    else if(data_type == "uint16_t"){
        return 2;
    }
    else if(data_type == "uint32_t"){
        return 4;
    }
    else if(data_type == "uint64_t"){
        throw std::runtime_error("in parse_data_type_size: can currently only handle datatypes from 1 byte to 4 byte");
    }
    else if(data_type == "size_t"){
        return 4;
    }
    else if(data_type == "signed char"){
        return 1;
    }
    else if(data_type == "short"){
        return 2;
    }
    else if(data_type == "short int"){
        return 2;
    }
    else if(data_type == "signed short"){
        return 2;
    }
    else if(data_type == "signed short int"){
        return 2;
    }
    else if(data_type == "int"){
        return 4;
    }
    else if(data_type == "signed"){
        return 4;
    }
    else if(data_type == "signed int"){
        return 4;
    }
    else if(data_type == "long"){
        return 4;
    }
    else if(data_type == "long int"){
        return 4;
    }
    else if(data_type == "signed long"){
        return 4;
    }
    else if(data_type == "signed long int"){
        return 4;
    }
    else if(data_type == "long long"){
        return 4;
    }
    else if(data_type == "long long int"){
        return 4;
    }
    else if(data_type == "signed long long"){
        return 4;
    }
    else if(data_type == "signed long long int"){
        return 4;
    }
    else if(data_type == "int8_t"){
        return 1;
    }
    else if(data_type == "int16_t"){
        return 2;
    }
    else if(data_type == "int32_t"){
        return 4;
    }
    else if(data_type == "int32_t"){
        throw std::runtime_error("in parse_data_type_size: can currently only handle datatypes from 1 byte to 4 byte");
    }
    else{
        throw std::runtime_error("can not identify data type size for: " + data_type);
    }
    
}

SIGNED_TYPE parse_signedness(std::string signedness){
    if(signedness == "unsigned int"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "unsigned char"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "unsigned short"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "unsigned short int"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "unsigned"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "unsigned int"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "unsigned long int"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "long unsigned int"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "unsigned long"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "unsigned long long"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "unsigned long long int"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "uint8_t"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "uint16_t"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "uint32_t"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "uint64_t"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "size_t"){
        return SIGNED_TYPE::UNSIGNED;
    }
    else if(signedness == "signed char"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "short"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "short int"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "signed short"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "signed short int"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "int"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "signed"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "signed int"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "long"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "long int"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "signed long"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "signed long int"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "long long"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "long long int"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "signed long long"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "signed long long int"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "int8_t"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "int16_t"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "int32_t"){
        return SIGNED_TYPE::SIGNED;
    }
    else if(signedness == "int64_t"){
        return SIGNED_TYPE::SIGNED;
    }
    else{
        throw std::runtime_error("can not identify data type signedness for: " + signedness);
    }
    
}

/**
 * @brief Get the Data Type of the input in the internal data-structure,
 * 
 * @param var_decl [IN]
 * @return DATA_TYPE [OUT]
 */
DATA_TYPE getInputDataType(const std::vector<std::string>& var_decl){

    //parameter is either INPUT_ARR_PTR or INPUT_VAR_PTR
    uint32_t pos_of_first_asterisk = std::find(var_decl.begin(), var_decl.end(), "*") - var_decl.begin();
    if(pos_of_first_asterisk != var_decl.size()){ //found asterisk 

        if(var_decl.at(pos_of_first_asterisk - 1).find("[") != std::string::npos){ //a type is always in front of an asterisk
            return DATA_TYPE::INPUT_ARR_PTR;
        }
        else{
            return DATA_TYPE::INPUT_VAR_PTR;
        }
    }
    else{
        return DATA_TYPE::INPUT_VAR;
    }

}


/**
 * @brief Get the Data Type of the local and virtual variables,
 * 
 * @param tokens [IN] tokenized input parameter string
 * @return DATA_TYPE [OUT] data type of tokens
 */
DATA_TYPE getDataType(const std::vector<std::string>& tokens){
    std::string variable_name = tokens.back().substr(0, tokens.back().find(";"));
    uint32_t pos_of_first_asterisk = std::find(tokens.begin(), tokens.end(), "*") - tokens.begin();

    if(is_virtual_variable(variable_name)){

        if(pos_of_first_asterisk != tokens.size()){ //found asterisk
            if(tokens.at(pos_of_first_asterisk - 1).find("[") != std::string::npos){      //e.g. uint32_t[2][2] * _9;
                return DATA_TYPE::VIRTUAL_ARR_PTR;
            }
            else{                                                                         //e.g. uint32_t * _1;
                return DATA_TYPE::VIRTUAL_VAR_PTR;
            }
        }
        else if(tokens.at(pos_of_first_asterisk - 1).find("[") != std::string::npos){
            throw std::runtime_error("in getDataType: can not infere data type of virtual variable " + variable_name);
        }
        else{                                                                             //e.g. uint32_t _1;
            return DATA_TYPE::VIRTUAL_VAR;
        }
        
    }
    else{
        if(pos_of_first_asterisk != tokens.size()){ //found asterisk
            if(variable_name.find("[") != std::string::npos){
                return DATA_TYPE::LOCAL_ARR_PTR;
            }
            else{
                return DATA_TYPE::LOCAL_VAR_PTR;
            }
        }
        else if(variable_name.find("[") != std::string::npos){                             //e.g. uint32_t r[2][2];
            return DATA_TYPE::LOCAL_ARR;
        }
        else{                                                                              //e.g. int i;
            return DATA_TYPE::LOCAL_VAR;
        }
    }

}




/**
 * @brief parse input of form 'type [dim][dim] * id'
 * 
 * @param virtual_variables [IN-OUT] internal datastructure for variables
 * @param tokens [IN] tokenized input parameter string
 */
void ParseInputArrPtr(virtual_variables_vec& virtual_variables, const std::vector<std::string>& tokens){
    std::string parameter_name = tokens.back();
    array_information a_info;
    uint32_t pos_first_asterisk = std::find(tokens.begin(), tokens.end(), "*") - tokens.begin();
    uint32_t pos_before_first_asterisk = pos_first_asterisk - 1;

    std::string array_dimensions =  tokens.at(pos_before_first_asterisk).substr(tokens.at(pos_before_first_asterisk).find("["), std::string::npos - 1);
    std::string signedness = std::accumulate(tokens.begin()+1, tokens.begin() + pos_first_asterisk, tokens.at(0), [](std::string x, std::string y) {return x + " " + y;});            
    signedness = signedness.substr(0, signedness.find("["));
    SIGNED_TYPE sign = parse_signedness(signedness);
    uint8_t data_type_size = parse_data_type_size(signedness);

    a_info.dimension = std::count(array_dimensions.begin(), array_dimensions.end(), '[');
    uint32_t num_asterisk = std::count(tokens.begin(), tokens.end(), "*");
    a_info.dimension += num_asterisk;
    a_info.dimension_without_size = num_asterisk;

    std::string name_copy = array_dimensions;
    std::string depth = name_copy.substr(name_copy.find("[")+1,name_copy.find("]") - name_copy.find("[") - 1);
    a_info.depth_per_dimension.resize(a_info.dimension);

    for(uint32_t i = 0; i < num_asterisk; ++i){
        a_info.depth_per_dimension.at(i) = {DATA_TYPE::NONE, UINT32_MAX}; 
    }

    uint32_t ctr = 0;
    while(!name_copy.empty()){
        name_copy = name_copy.substr(name_copy.find("]")+1, std::string::npos);
        a_info.depth_per_dimension.at(num_asterisk + ctr) = {DATA_TYPE::NUM, static_cast<uint32_t>(stoi(depth))};
        depth = name_copy.substr(name_copy.find("[")+1,name_copy.find("]") - name_copy.find("[") - 1);
        ctr++;
    }

    make_virtual_variable_information(virtual_variables, parameter_name, -1, DATA_TYPE::INPUT_ARR_PTR, a_info, sign, parameter_name, data_type_size, -1, SSA_ADD_ON::NO_ADD_ON);

}

/**
 * @brief parse input that is of form 'type * id'
 * 
 * @param virtual_variables [IN-OUT] internal datastructure for variables
 * @param tokens [IN] tokenized input parameter string
 */
void ParseInputVarPtr(virtual_variables_vec& virtual_variables, const std::vector<std::string>& tokens){
    std::string parameter_name = tokens.back();
    pointer_information p_info;
    p_info.dimension = std::count(tokens.begin(), tokens.end(), "*");
    auto pos_first_asterisk = std::find(tokens.begin(), tokens.end(), "*") - tokens.begin();
    
    //check signedness, can contain multiple strings (e.g. unsigned int, long long int, ...)
    std::string signedness = std::accumulate(tokens.begin()+1, tokens.begin() + pos_first_asterisk, tokens.at(0), [](std::string x, std::string y) {return x + " " + y;});
    SIGNED_TYPE sign = parse_signedness(signedness);
    uint8_t data_type_size = parse_data_type_size(signedness);
    
    make_virtual_variable_information(virtual_variables, parameter_name, -1, DATA_TYPE::INPUT_VAR_PTR, p_info, sign, parameter_name, data_type_size, -1, SSA_ADD_ON::NO_ADD_ON);
}

/**
 * @brief parse input that is of form 'type id'
 * 
 * @param virtual_variables [IN-OUT] internal datastructure for variables encountered in function
 * @param tokens [IN] tokenized input parameter string
 */
void ParseInputVar(virtual_variables_vec& virtual_variables, const std::vector<std::string>& tokens){
    default_information d_info;
    std::string parameter_name = tokens.back();
    std::string signedness = std::accumulate(tokens.begin()+1, tokens.end() - 1, tokens.at(0), [](std::string x, std::string y) {return x + " " + y;});
    SIGNED_TYPE sign = parse_signedness(signedness);
    uint8_t data_type_size = parse_data_type_size(signedness);
    make_virtual_variable_information(virtual_variables, parameter_name, -1, DATA_TYPE::INPUT_VAR, d_info, sign, parameter_name, data_type_size, -1, SSA_ADD_ON::NO_ADD_ON);
}

/**
 * @brief Parse function parameters, parameters can in .ssa file be of form 'type id', 'type * id', or 'type[dim][dim][...] * id'. Distinguish these three cases, collect information and parse accordingly
 * 
 * @param virtual_variables [IN-OUT] internal datastructure for variables encountered in function
 * @param tokens [IN] tokenized input parameter string
 */
void parse_parameter(virtual_variables_vec& virtual_variables, const std::vector<std::string>& tokens){
    DATA_TYPE input_data_type = getInputDataType(tokens);

    switch(input_data_type){
        case(DATA_TYPE::INPUT_ARR_PTR): ParseInputArrPtr(virtual_variables, tokens); break;
        case(DATA_TYPE::INPUT_VAR_PTR): ParseInputVarPtr(virtual_variables, tokens); break;
        case(DATA_TYPE::INPUT_VAR): ParseInputVar(virtual_variables, tokens); break;
        default: throw std::runtime_error("in parse_parameter: input parameter is unkown");
    }

}

/**
 * @brief 
 * 
 * @param virtual_variables [IN-OUT] internal datastructure for variables encountered in function
 * @param tokens [IN] tokenized input parameter string
 * @param data_type [IN] either LOCAL_VAR or VIRTUAL_VAR
 */
void parseVAR(virtual_variables_vec& virtual_variables, const std::vector<std::string>& tokens, const DATA_TYPE data_type){
    std::string variable_name = tokens.back().substr(0, tokens.back().find(";"));
    default_information d_info;
    std::string signedness = std::accumulate(tokens.begin()+1, tokens.end() - 1, tokens.at(0), [](std::string x, std::string y) {return x + " " + y;});
    SIGNED_TYPE sign = parse_signedness(signedness);
    uint8_t data_type_size = parse_data_type_size(signedness);
    std::string original_name;
    int32_t idx_to_original_var;
    std::tie(original_name, idx_to_original_var) = get_original_name_and_idx_of_ssa_name(variable_name, virtual_variables);
    make_virtual_variable_information(virtual_variables, variable_name, 0, data_type, d_info, sign, original_name, data_type_size, idx_to_original_var, SSA_ADD_ON::NO_ADD_ON);
}

/**
 * @brief 
 * 
 * @param virtual_variables [IN-OUT] internal datastructure for variables encountered in function
 * @param tokens [IN] tokenized input parameter string
 * @param data_type [IN] either LOCAL_VAR_PTR or VIRTUAL_VAR_PTR
 */
void parseVARPTR(virtual_variables_vec& virtual_variables, const std::vector<std::string>& tokens, const DATA_TYPE data_type){
    std::string variable_name = tokens.back().substr(0, tokens.back().find(";"));
    pointer_information p_info;
    p_info.dimension = std::count(tokens.begin(), tokens.end(), "*");
    auto pos_first_asterisk = std::find(tokens.begin(), tokens.end(), "*") - tokens.begin();
    std::string signedness = std::accumulate(tokens.begin()+1, tokens.begin() + pos_first_asterisk, tokens.at(0), [](std::string x, std::string y) {return x + " " + y;});
    SIGNED_TYPE sign = parse_signedness(signedness);
    uint8_t data_type_size = parse_data_type_size(signedness);
    std::string original_name;
    int32_t idx_to_original_var;
    std::tie(original_name, idx_to_original_var) = get_original_name_and_idx_of_ssa_name(variable_name, virtual_variables);
    make_virtual_variable_information(virtual_variables, variable_name, 0, data_type, p_info, sign, original_name, data_type_size, idx_to_original_var, SSA_ADD_ON::NO_ADD_ON);
}

//TODO: the LOCAL_ARR_PTR case did not occur in our test-cases
/**
 * @brief 
 * 
 * @param virtual_variables [IN-OUT] internal datastructure for variables encountered in function
 * @param tokens [IN] tokenized input parameter string
 * @param data_type [IN] either LOCAL_ARR_PTR or VIRTUAL_ARR_PTR
 */
void parseARRPTR(virtual_variables_vec& virtual_variables, const std::vector<std::string>& tokens, const DATA_TYPE data_type){
    std::string variable_name = tokens.back().substr(0, tokens.back().find(";"));
    array_information a_info;
    auto pos_first_asterisk = std::find(tokens.begin(), tokens.end(), "*") - tokens.begin();
    
    std::string array_dimensions;
    std::string signedness;
    if(data_type == DATA_TYPE::LOCAL_ARR_PTR){
        array_dimensions =  variable_name.substr(variable_name.find("["), std::string::npos - 1);
        variable_name = variable_name.substr(0, variable_name.find("[")); //e.g. uint32_t r[3][3] -> r
        signedness = std::accumulate(tokens.begin()+1, tokens.begin() + pos_first_asterisk, tokens.at(0), [](std::string x, std::string y) {return x + " " + y;}); 
    }
    else if(data_type == DATA_TYPE::VIRTUAL_ARR_PTR){
        array_dimensions = tokens.at(pos_first_asterisk - 1).substr(tokens.at(pos_first_asterisk - 1).find("["), std::string::npos - 1); //e.g. uint32_t[2][2] * _9;
        signedness = std::accumulate(tokens.begin()+1, tokens.begin() + pos_first_asterisk, tokens.at(0), [](std::string x, std::string y) {return x + " " + y;});    
        signedness = signedness.substr(0, signedness.find("["));
    }
    else{
        throw std::runtime_error("in parseARRPTR: unkown data_type");
    }

            
    SIGNED_TYPE sign = parse_signedness(signedness);
    uint8_t data_type_size = parse_data_type_size(signedness);

    a_info.dimension = std::count(array_dimensions.begin(), array_dimensions.end(), '[');
    uint32_t num_asterisk = std::count(tokens.begin(), tokens.end(), "*");
    a_info.dimension += num_asterisk;
    a_info.dimension_without_size = num_asterisk;

    std::string name_copy = array_dimensions;
    std::string depth = name_copy.substr(name_copy.find("[")+1,name_copy.find("]") - name_copy.find("[") - 1);
    a_info.depth_per_dimension.resize(a_info.dimension);

    for(uint32_t i = 0; i < num_asterisk; ++i){
        a_info.depth_per_dimension.at(i) = {DATA_TYPE::NONE, UINT32_MAX}; 
    }

    uint32_t ctr = 0;
    while(!name_copy.empty()){
        operand op;
        name_copy = name_copy.substr(name_copy.find("]")+1, std::string::npos);
        a_info.depth_per_dimension.at(num_asterisk + ctr) = {DATA_TYPE::NUM, static_cast<uint32_t>(stoi(depth))};
        depth = name_copy.substr(name_copy.find("[")+1,name_copy.find("]") - name_copy.find("[") - 1);
        ctr++;
    }

    std::string original_name;
    int32_t idx_to_original_var;
    std::tie(original_name, idx_to_original_var) = get_original_name_and_idx_of_ssa_name(variable_name, virtual_variables);
    make_virtual_variable_information(virtual_variables, variable_name, 0, data_type, a_info, sign, original_name, data_type_size, idx_to_original_var, SSA_ADD_ON::NO_ADD_ON);

}

void parseARR(virtual_variables_vec& virtual_variables, const std::vector<std::string>& tokens, const DATA_TYPE data_type){
    std::string variable_name = tokens.back().substr(0, tokens.back().find(";"));
    array_information a_info;

    std::string array_dimensions =  variable_name.substr(variable_name.find("["), std::string::npos - 1);
    variable_name = variable_name.substr(0, variable_name.find("["));
    std::string signedness = std::accumulate(tokens.begin()+1, tokens.end() -1, tokens.at(0), [](std::string x, std::string y) {return x + " " + y;});            
    SIGNED_TYPE sign = parse_signedness(signedness);
    uint8_t data_type_size = parse_data_type_size(signedness);

    a_info.dimension = std::count(array_dimensions.begin(), array_dimensions.end(), '[');

    std::string name_copy = array_dimensions;
    std::string depth = name_copy.substr(name_copy.find("[")+1,name_copy.find("]") - name_copy.find("[") - 1);
    a_info.depth_per_dimension.resize(a_info.dimension);

    uint32_t ctr = 0;
    while(!name_copy.empty()){
        name_copy = name_copy.substr(name_copy.find("]")+1, std::string::npos);

        a_info.depth_per_dimension.at(ctr) = {DATA_TYPE::NUM, static_cast<uint32_t>(stoi(depth))};
        depth = name_copy.substr(name_copy.find("[")+1,name_copy.find("]") - name_copy.find("[") - 1);
        ctr++;
    }

    std::string original_name;
    int32_t idx_to_original_var;
    std::tie(original_name, idx_to_original_var) = get_original_name_and_idx_of_ssa_name(variable_name, virtual_variables);
    make_virtual_variable_information(virtual_variables, variable_name, 0, data_type, a_info, sign, original_name, data_type_size, idx_to_original_var, SSA_ADD_ON::NO_ADD_ON);
}


/**
 * @brief collect information of variables (local and virtual) that are enumerated at start of function
 * 
 * @param virtual_variables 
 * @param tokens 
 */
void parse_single_variable(virtual_variables_vec& virtual_variables, const std::vector<std::string>& tokens){
    DATA_TYPE data_type = getDataType(tokens);
    switch(data_type){
        case(DATA_TYPE::LOCAL_VAR): 
        case(DATA_TYPE::VIRTUAL_VAR): parseVAR(virtual_variables, tokens, data_type); break;
        case(DATA_TYPE::LOCAL_VAR_PTR): 
        case(DATA_TYPE::VIRTUAL_VAR_PTR): parseVARPTR(virtual_variables, tokens, data_type); break;
        case(DATA_TYPE::LOCAL_ARR_PTR): 
        case(DATA_TYPE::VIRTUAL_ARR_PTR): parseARRPTR(virtual_variables, tokens, data_type); break;
        case(DATA_TYPE::LOCAL_ARR): parseARR(virtual_variables, tokens, data_type); break;
        default: throw std::runtime_error("in parse_single_variable: data type of variable is unkown");
    }

}



/**
 * @brief parse single parameter, construct argument struct with type (int, float, ...) and id (name of parameter)
 * 
 * @param func [IN-OUT] internal data-structure of function, gets information of function arguments
 * @param virtual_variables [IN-OUT] gets filled with function arguments
 * @param parameters [IN] parameters of function as string
 */
void parse_single_parameter(function& func, virtual_variables_vec& virtual_variables, const std::string& param){

    //split by delemiter
    std::vector<std::string> tokens;
    boost::algorithm::split_regex(tokens, param, boost::regex(" "));

    parse_parameter(virtual_variables, tokens);

    // FIXME: handle optinal array boundaries
    std::string type = tokens.at(0).substr(0, tokens.at(0).find("["));
    func.arguments.emplace_back(parameter{type, tokens.back(), 0});

}


/**
 * @brief parse function parameters, split parameter list into separat parameters and add them to function data-structure
 * 
 * @param func [IN-OUT] internal data-structure of function, gets information of function arguments
 * @param virtual_variables [IN-OUT] gets filled with function arguments
 * @param parameters [IN] parameters of function as string
 */
void parse_parameter_list(function& func, virtual_variables_vec& virtual_variables, const std::string& parameters){


    //case: no parameters
    if(parameters.size() == 0){
        std::cout << "[+] no function argument\n";
    }

    //case: one parameters
    else if(parameters.find(",") == std::string::npos){
        std::cout << "[+] one function argument\n";
        func.arguments.reserve(1);
        parse_single_parameter(func, virtual_variables, parameters);
    }

    //case: two or more parameters
    else{
        std::cout << "[+] multiple function arguments\n";
        std::vector<std::string> result;

        // parameters are comma seperated
        boost::algorithm::split_regex(result, parameters, boost::regex(", "));
        func.arguments.reserve(result.size());
        
        for(std::string& token: result){
            parse_single_parameter(func, virtual_variables, token);
        }
    }
}


/**
 * @brief parse function header, gets name of function and arguments
 * 
 * @param func [IN-OUT] internal data-structure of function, gets information of header
 * @param virtual_variables [IN-OUT] gets filled with function arguments
 * @param line [IN-OUT]
 */
void parse_function_header(function& func, virtual_variables_vec& virtual_variables, const std::string& line){
    std::string func_name = line.substr(0, line.find(" "));
    func.function_name = func_name;
    std::string parameter_list = line.substr(line.find("(")+1, line.find(")") - line.find("(") - 1);

    parse_parameter_list(func, virtual_variables, parameter_list);

    //bind security level to inputs
    std::cout << "[+] parse security level of parameter... ";
    parse_security_level_of_parameter(func, virtual_variables);
    std::cout << "done!" << std::endl;
    std::cout << "[+] parse security level of internal function calls... ";
    parse_security_level_function_call(func);
    std::cout << "done!" << std::endl;
}


/**
 * @brief parse basic block. Within basic block read each ssa instruction line by line and parse in internal datastructure
 * 
 * @param func 
 * @param virtual_variables 
 * @param infile 
 */
void ParseBasicBlocks(function& func, virtual_variables_vec& virtual_variables, std::ifstream& infile){
    std::cout << "[+] start parsing basic blocks... " << std::endl;

    std::string line;
    std::getline(infile, line);

    while(!(is_end_of_function(line))){
        parse_bb_start(func, line);    
        std::getline(infile, line); //first ssa of bb;

        //parse basic block
        while(!is_end_of_bb(line)){
            parse_raw_ssa(func, line, virtual_variables, infile);
            std::getline(infile, line);
        }
        std::getline(infile, line); 

    }
    std::cout << "done!" << std::endl;
}

/**
 * @brief parse variable declaration. Variables declared prior to basic blocks will be registered sequentially and inserted into internal data structure
 * 
 * @param virtual_variables 
 * @param infile 
 */
void ParseVariableDeclaration(virtual_variables_vec& virtual_variables, std::ifstream& infile){
    std::string line = "";
    std::vector<std::string> tokens;
    
    auto file_pos = infile.tellg();

    while(std::getline(infile, line)){
        if(is_start_of_bb(line)){
            infile.seekg(file_pos);
            break;
        }

        if(line.size() != 0){
            split_line_into_tokens(tokens, line);
            parse_single_variable(virtual_variables, tokens);
        }
        file_pos = infile.tellg();
    }
}

/**
 * @brief parses function body, contains three main steps: 1. parse variable declarations at start of function, 2. go over all basic blocks and parse instruction sequence
 * 
 * @param func [IN-OUT] internal data-structure to represent function, gets filled with information
 * @param virtual_variables list of variables encounterd in the function
 * @param infile [IN-OUT] GIMPLE file, gets digested
 */
void parse_function_body(function& func, virtual_variables_vec& virtual_variables, std::ifstream& infile){
    std::cout << "[+] start parsing function body... ";
    std::string line;
    std::string tmp_line = "";
    std::vector<std::string> tokens;

    //dummy read get to actual body
    std::getline(infile, line);

    //parse local variables and virtual registers until first bb starts
    ParseVariableDeclaration(virtual_variables, infile);

    //necessary for unrolling
    std::string last_virt_elem = std::find_if(virtual_variables.rbegin(), virtual_variables.rend(), [](virtual_variable_information& t){return (t.data_type == DATA_TYPE::VIRTUAL_ARR) | (t.data_type == DATA_TYPE::VIRTUAL_ARR_ADDR) | (t.data_type == DATA_TYPE::VIRTUAL_ARR_PTR) | (t.data_type == DATA_TYPE::VIRTUAL_VAR) | (t.data_type == DATA_TYPE::VIRTUAL_VAR_ADDR) | (t.data_type == DATA_TYPE::VIRTUAL_VAR_PTR);})->ssa_name;
    func.biggest_virtual_variable_number = static_cast<uint32_t>(stoi(last_virt_elem.substr(1, last_virt_elem.size() - 1)));


    auto file_pos = infile.tellg();
    while(!is_end_of_function(tmp_line)){
        //look out for phi functions as they can introduce new variables
        
        std::getline(infile, tmp_line);
        
        split_line_into_tokens(tokens, tmp_line);
        
        if(is_phi_ssa(tokens)){
        
            //extract all variables, check if the already exist in virtual_variables, if not add them
            get_variables_from_phi(virtual_variables, tokens);
        }
    }

    infile.seekg(file_pos);

    
    //parse function
    ParseBasicBlocks(func, virtual_variables, infile);

    std::cout << "done!" << std::endl;
}

/**
 * @brief make new space in func for upcoming basic block, converts start of basic block which is noted as <bb X> into basic block number X and stores in func
 * 
 * @param func [IN-OUT] internal data-structure to represent function, gets filled with information
 * @param line [IN] line of ssa file, not tokenized, of form <bb X>
 */
void parse_bb_start(function& func, const std::string& line){
    std::string bb_start = line.substr(line.find("<")+1, line.find(">") - line.find("<") - 1);
    
    //make space for new basic block
    func.bb.resize(func.bb.size() + 1);

    std::string bb_number = bb_start.substr(bb_start.find(" ")+1, std::string::npos - 1);
    func.bb.back().bb_number = static_cast<uint32_t>(stoi(bb_number));
}
