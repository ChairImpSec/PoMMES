#include "gimple_parser.hpp"
#include "graph.hpp"
#include "check_functions.hpp"
#include "parse_functions.hpp"






/**
 * @brief gimple statements are contained in < >, the data inside the statement is either a single expression <expr> or comma separated <expr, expr, expr, ...>, return only (list of) expr
 * 
 * @param tokens [IN-OUT] tokenized input parameter string, gets sanitized
 */
void clean_tokens(std::vector<std::string>& tokens){
    std::vector<std::string> tmp_tokens;
    
    if(tokens.size() == 2){
        std::string tmp = tokens.at(1).substr(1,tokens.at(1).size() - 2);//remove first < and last >
        tmp_tokens.emplace_back(tmp);
    }
    else{
        tokens.at(1) = tokens.at(1).substr(1,tokens.at(1).size());
        tokens.back() = tokens.back().substr(0,tokens.back().size()-1);
        for(uint32_t i = 1; i < tokens.size()-1; ++i){
            tokens.at(i).erase(std::remove(tokens.at(i).begin(), tokens.at(i).end(), ','), tokens.at(i).end());
            tmp_tokens.emplace_back(tokens.at(i));
        }
        tmp_tokens.emplace_back(tokens.back());
    }
    tokens = tmp_tokens;
}

void split_line_into_tokens(std::vector<std::string>& tokens, const std::string& line){
    boost::algorithm::split_regex(tokens, line, boost::regex(" "));
    tokens.erase(std::remove_if(tokens.begin(), tokens.end(), is_empty_or_blank), tokens.end());
}

void generate_array_information(virtual_variables_vec& virtual_variables, const std::string& var, array_information& var_a_info){
        std::string name_copy = var;
        std::string depth_var = name_copy.substr(name_copy.find("[")+1,name_copy.find("]") - name_copy.find("[") - 1);
        while(!name_copy.empty()){
            operand op;
            name_copy = name_copy.substr(name_copy.find("]")+1, std::string::npos);
            var_a_info.dimension++;
            if(is_number(depth_var)){
                op.op_type = DATA_TYPE::NUM;
                op.op_data = stoi(depth_var);
            }
            else{
                auto tmp = std::find_if(virtual_variables.begin(), virtual_variables.end(), [depth_var](virtual_variable_information& t){return t.ssa_name == depth_var;});
                if(tmp == virtual_variables.end()){
                    //throw std::runtime_error("0 in get_virtual_variable_index: can not find offset to array");
                
                    //it might be possible that is uses a variable that has not yet been seen (e.g result of phi node) if so we add it to 
                    uint32_t idx_of_new_variable = get_index_of_virtual_variable(virtual_variables, depth_var, "");
                    op.op_type = virtual_variables.at(idx_of_new_variable).data_type;
                    op.op_data = idx_of_new_variable;
                }
                else{
                    op.op_type = tmp->data_type;
                    op.op_data = tmp - virtual_variables.begin();
                }

            }
            var_a_info.depth_per_dimension.push_back(op);


            depth_var = name_copy.substr(name_copy.find("[")+1,name_copy.find("]") - name_copy.find("[") - 1);
        }   
}



/**
 * @brief Get the original name and idx of ssa name object
 * 
 * @param ssa_name ssa_name as encountered in file, e.g. *_33[i_46], &s, _18, *_1, j_45, i.6_31, ...
 * @param virtual_variables 
 * @return std::tuple<std::string, int32_t> 
 */
std::tuple<std::string, int32_t> get_original_name_and_idx_of_ssa_name(const std::string ssa_name, const virtual_variables_vec& virtual_variables){

    std::string leading_sanitized_ssa_name = ssa_name;

    if((leading_sanitized_ssa_name.at(0) == '&') || (leading_sanitized_ssa_name.at(0) == '*')){
        leading_sanitized_ssa_name = leading_sanitized_ssa_name.substr(1, leading_sanitized_ssa_name.length() - 1);
    }

    if(is_virtual_variable(leading_sanitized_ssa_name)){ //virtual variables have no original name, i.e. their ssa name is their original name
        std::string ssa_name_without_array_bound = leading_sanitized_ssa_name.substr(0, leading_sanitized_ssa_name.find("[")); //e.g. _25[i_46][i_46] array brackets can never be original name
        auto it = std::find_if(virtual_variables.begin(), virtual_variables.end(), [ssa_name_without_array_bound](const virtual_variable_information& t){return (t.ssa_name == ssa_name_without_array_bound) && (t.index_to_original == -1);});
        if((it != virtual_variables.end()) && (leading_sanitized_ssa_name != ssa_name)){
            return std::make_tuple(it->ssa_name, it - virtual_variables.begin());
        }
        else{

            return std::make_tuple(ssa_name, -1);

        }
    }

    //check if ssa_name can be found in virtual variables directly
    auto it = std::find_if(virtual_variables.begin(), virtual_variables.end(), [leading_sanitized_ssa_name](const virtual_variable_information& t){return (t.ssa_name == leading_sanitized_ssa_name) && (t.index_to_original == -1);});
    if((it != virtual_variables.end()) && (leading_sanitized_ssa_name != ssa_name)){
        return std::make_tuple(it->ssa_name, it - virtual_variables.begin()); 
    }
    else if((it != virtual_variables.end()) && (leading_sanitized_ssa_name == ssa_name)){
        return std::make_tuple(ssa_name, -1); 
    }

    //check if ssa_name contains array bounds
    std::string ssa_name_without_array_bound = leading_sanitized_ssa_name.substr(0, leading_sanitized_ssa_name.find("["));

    //check if ssa_name contains '.' if yes remove and search again
    std::string modified_ssa_name = ssa_name_without_array_bound.substr(0, ssa_name_without_array_bound.find('.'));
    if(modified_ssa_name != leading_sanitized_ssa_name){ //have found '.', e.g. s.4_5
        it = std::find_if(virtual_variables.begin(), virtual_variables.end(), [modified_ssa_name](const virtual_variable_information& t){return (t.ssa_name == modified_ssa_name) && (t.index_to_original == -1);});
        if(it != virtual_variables.end()){
            return std::make_tuple(modified_ssa_name, it - virtual_variables.begin());  
        }
    }

    modified_ssa_name = ssa_name_without_array_bound.substr(0, ssa_name_without_array_bound.rfind('_'));
    if(modified_ssa_name != leading_sanitized_ssa_name){ //have found '_', e.g. j_5
        it = std::find_if(virtual_variables.begin(), virtual_variables.end(), [modified_ssa_name](const virtual_variable_information& t){return (t.ssa_name == modified_ssa_name) && (t.index_to_original == -1);});
        if(it != virtual_variables.end()){
            return std::make_tuple(modified_ssa_name, it - virtual_variables.begin());  
        }
    }

    // if we can not find the variable inside the vector of variables we know 1. it was not a virtual variable 2. it was not a modified local variable 3. the variable is not yet in virtual variables 
    // -> has to be new original local variable
    return std::make_tuple(ssa_name, -1);

}

SSA_ADD_ON resolve_ssa_add_on(const std::string modification, bool has_array_index){
    SSA_ADD_ON return_ssa_add_on;

    if(has_array_index){
        if(modification == ""){
            return_ssa_add_on = SSA_ADD_ON::ARR_ADD_ON;
        }
        else if(modification == "&"){
            return_ssa_add_on = SSA_ADD_ON::AMP_ARR_ADD_ON;
        }
        else if(modification == "*"){
            return_ssa_add_on = SSA_ADD_ON::ASTERISK_ARR_ADD_ON;
        }
        else{
            throw std::runtime_error("in resolve_ssa_add_on: can not identifiy the add on with array index");
        }
    }
    else{
        if(modification == ""){
            return_ssa_add_on = SSA_ADD_ON::NO_ADD_ON;
        }
        else if(modification == "&"){
            return_ssa_add_on = SSA_ADD_ON::AMP_ADD_ON;
        }
        else if(modification == "*"){
            return_ssa_add_on = SSA_ADD_ON::ASTERISK_ADD_ON;
        }
        else{
            throw std::runtime_error("in resolve_ssa_add_on: can not identifiy the add on without array index");
        }
    }

    return return_ssa_add_on;
}

DATA_TYPE resolve_modification(const DATA_TYPE data_type, const std::string modification){
    if(modification == ""){
        return data_type;
    }
    else if(modification == "*"){
        if(data_type == DATA_TYPE::INPUT_ARR_PTR){
            return DATA_TYPE::INPUT_ARR;
        }
        else if(data_type == DATA_TYPE::INPUT_VAR_PTR){
            return DATA_TYPE::INPUT_VAR;
        }
        else if(data_type == DATA_TYPE::LOCAL_ARR_PTR){
            return DATA_TYPE::LOCAL_ARR;
        }
        else if(data_type == DATA_TYPE::LOCAL_VAR_PTR){
            return DATA_TYPE::LOCAL_VAR;
        }
        else if(data_type == DATA_TYPE::VIRTUAL_ARR_PTR){
            return DATA_TYPE::VIRTUAL_ARR;
        }
        else if(data_type == DATA_TYPE::VIRTUAL_VAR_PTR){
            return DATA_TYPE::VIRTUAL_VAR;
        }
        else{
            throw std::runtime_error("can not resolve data type for dereference modification");
        }
    }
    else if(modification == "&"){
        if(data_type == DATA_TYPE::INPUT_VAR){
            return DATA_TYPE::INPUT_VAR_ADDR;
        }
        else if(data_type == DATA_TYPE::INPUT_ARR){
            return DATA_TYPE::INPUT_ARR_ADDR;
        }
        else if(data_type == DATA_TYPE::LOCAL_ARR){
            return DATA_TYPE::LOCAL_ARR_ADDR;
        }
        else if(data_type == DATA_TYPE::LOCAL_VAR){
            return DATA_TYPE::LOCAL_VAR_ADDR;
        }
        else if(data_type == DATA_TYPE::VIRTUAL_ARR){
            return DATA_TYPE::VIRTUAL_ARR_ADDR;
        }
        else if(data_type == DATA_TYPE::VIRTUAL_VAR){
            return DATA_TYPE::VIRTUAL_VAR_ADDR;
        }
        else{
            throw std::runtime_error("can not resolve data type for dereference modification");
        }
    }
    else{
        throw std::runtime_error("can not resolve modification, modification unkown");
    }
}


/**
 * @brief Get the index of virtual variable object; if the ssa name of the variable already exists in virtual_variables return its index (we do not need to add the variable as it is already present). 
 * If the ssa name of the variable does not exist we look for the
 * 
 * @param virtual_variables 
 * @param var 
 * @param modification 
 * @return uint32_t 
 */
uint32_t get_index_of_virtual_variable( virtual_variables_vec& virtual_variables, const std::string& var, const std::string modification){
    
    //modifications were removed from variable before function call, needed for search
    std::string tmp_var = modification + var;

    //check if ssa name is already in virtual variables, do not need to continue
    auto it = std::find_if(virtual_variables.begin(), virtual_variables.end(), [tmp_var](virtual_variable_information& t){return t.ssa_name == tmp_var;});

    if(it != virtual_variables.end()){
        return it - virtual_variables.begin();
    }

    std::string original_name;
    int32_t idx_to_original;

    std::tie(original_name, idx_to_original) = get_original_name_and_idx_of_ssa_name(tmp_var, virtual_variables);


    if(tmp_var.find("[") != std::string::npos){ //generate array information
        array_information a_info;
        generate_array_information(virtual_variables, tmp_var, a_info);
        DATA_TYPE data_type = resolve_modification(virtual_variables.at(idx_to_original).data_type, modification);
        SSA_ADD_ON ssa_add_on = resolve_ssa_add_on(modification, true);
        make_virtual_variable_information(virtual_variables, tmp_var, virtual_variables.at(idx_to_original).sensitivity, data_type, a_info, virtual_variables.at(idx_to_original).signedness, original_name, virtual_variables.at(idx_to_original).data_type_size, idx_to_original, ssa_add_on);
        return virtual_variables.size() - 1;
    }
    else{
        DATA_TYPE data_type = resolve_modification(virtual_variables.at(idx_to_original).data_type, modification);
        SSA_ADD_ON ssa_add_on = resolve_ssa_add_on(modification, false);
        make_virtual_variable_information(virtual_variables, tmp_var, virtual_variables.at(idx_to_original).sensitivity, data_type, virtual_variables.at(idx_to_original).data_information, virtual_variables.at(idx_to_original).signedness, original_name, virtual_variables.at(idx_to_original).data_type_size, idx_to_original, ssa_add_on);
        return virtual_variables.size() - 1;
    }

}


/**
 * @brief Get the operand type object
 * 
 * @param op 
 * @param virtual_variables 
 * @param token 
 */
void get_operand_type(operand& op, virtual_variables_vec& virtual_variables, const std::string& token){

    if(is_number(token)){
        op.op_type = DATA_TYPE::NUM;
        op.op_data = stoul(token);   //TODO: if token is_number -> can also be float
    } 
    else{
        op.op_data = get_index_of_virtual_variable(virtual_variables, token, "");
        op.op_type = virtual_variables.at(op.op_data).data_type;
    }

}


void get_variables_from_phi(virtual_variables_vec& virtual_variables, std::vector<std::string>& tokens){
    if(tokens.at(2).rfind("<",0) == std::string::npos){
        throw std::runtime_error("error while extracting variables from phi");
    }

    //in order to call clean_tokens function we have to remove the first #
    tokens.erase(tokens.begin());

    clean_tokens(tokens); //have now only the variables inside <>

    // std::string element;
    std::string origin_name = tokens.at(0);

    std::string original_name;
    int32_t idx_to_original;
    std::tie(original_name, idx_to_original) = get_original_name_and_idx_of_ssa_name(origin_name, virtual_variables);


    make_virtual_variable_information(virtual_variables, origin_name, -1, virtual_variables.at(idx_to_original).data_type, virtual_variables.at(idx_to_original).data_information, virtual_variables.at(idx_to_original).signedness, original_name, virtual_variables.at(idx_to_original).data_type_size, idx_to_original, SSA_ADD_ON::NO_ADD_ON);


    for(uint32_t i = 1; i < tokens.size(); ++i){
        origin_name = tokens.at(i).substr(0, tokens.at(i).find("("));
        make_virtual_variable_information(virtual_variables, origin_name, -1, virtual_variables.at(idx_to_original).data_type, virtual_variables.at(idx_to_original).data_information, virtual_variables.at(idx_to_original).signedness, original_name, virtual_variables.at(idx_to_original).data_type_size, idx_to_original, SSA_ADD_ON::NO_ADD_ON);

    }
    
}



/**
 * @brief function parses whole file in GIMPLE format, two main parts are function header parsing and function body parsing
 * 
 * @param func [IN-OUT] internal data-structure to represent function, gets filled with information
 * @param virtual_variables list of variables encounterd in the function
 * @param infile [IN-OUT] GIMPLE file, gets digested
 */
void gimple_parser(function& func, virtual_variables_vec& virtual_variables, std::ifstream& infile){

    std::cout << "[-] start gimple ssa parsing... " << std::endl;

    std::string line;

    while(std::getline(infile, line)){
        if((line.length() != 0) && (!is_comment(line))){
            break;
        }
    }

    parse_function_header(func, virtual_variables, line);

    parse_function_body(func, virtual_variables, infile);


    std::cout << "done!\n";
}

