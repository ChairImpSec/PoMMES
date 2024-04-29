#include "security.hpp"


void assign_sensitivity(int32_t& dest_sens, int32_t sens1, int32_t sens2, bool& no_sensitivity_changed){


    if(sens1 != -1){
        if(dest_sens == -1){
            dest_sens = sens1;
            no_sensitivity_changed = false;
        }
        else if((sens1 == 1) && (dest_sens != 1)){
            dest_sens = 1;
            no_sensitivity_changed = false;
        }
        else if((sens1 == 0) && (dest_sens == -1)){
            dest_sens = 0;
            no_sensitivity_changed = false;
        }
        else if((sens1 > 0) && (sens1 < dest_sens)){
            dest_sens = sens1;
            no_sensitivity_changed = false;
        }
        else if((sens1 > 0) && (dest_sens == 0)){
            dest_sens = sens1;
            no_sensitivity_changed = false;
        }
    }
    if(sens2 != -1){
        if(dest_sens == -1){
            dest_sens = sens2;
            no_sensitivity_changed = false;
        }
        else if((sens2 == 1) && (dest_sens != 1)){
            dest_sens = 1;
            no_sensitivity_changed = false;
        }
        else if((sens2 == 0) && (dest_sens == -1)){
            dest_sens = 0;
            no_sensitivity_changed = false;
        }
        else if((sens2 > 0) && (sens2 < dest_sens)){
            dest_sens = sens2;
            no_sensitivity_changed = false;
        }
        else if((sens2 > 0) && (dest_sens == 0)){
            dest_sens = sens2;
            no_sensitivity_changed = false;
        }
    }

}



void union_out_of_predecessors_to_curr_in(std::vector<std::tuple<uint32_t, int32_t>>& curr_in, std::vector<std::tuple<uint32_t, int32_t>>& predec_out){
    for(auto& elem: predec_out){
        uint32_t virt_var = std::get<0>(elem);
        auto it = std::find_if(curr_in.begin(), curr_in.end(), [virt_var](const std::tuple<uint32_t, int32_t>& t){return std::get<0>(t) == virt_var;});
        if(it == curr_in.end()){
            curr_in.emplace_back(elem);
        }
        else{ //there is already the virtual variable present -> pass to curr_in the
            if((std::get<1>(*it) == -1) && (std::get<1>(elem) != -1)){
                std::get<1>(*it) = std::get<1>(elem);
            }
            else if((std::get<1>(*it) == 0) && (std::get<1>(elem) > 0)){
                std::get<1>(*it) = std::get<1>(elem);
            }
            else if((std::get<1>(*it) == 1)){

            }
            else if((std::get<1>(*it) > 1) && (std::get<1>(elem) > 0)){
                if(std::get<1>(elem) < std::get<1>(*it)){
                    std::get<1>(*it) = std::get<1>(elem);
                }
            }
        }
    }
}

//"in" of first bb is filled with all inputs to their respective sensitivity and all local variables set to zero
void set_in_of_first_bb(function& func, virtual_variables_vec& virtual_variables){
    for(uint32_t virt_var_idx = 0; virt_var_idx < virtual_variables.size(); ++virt_var_idx){
        //if variable is input -> add to in 
        if((1 <= static_cast<uint32_t>(virtual_variables.at(virt_var_idx).data_type)) && (static_cast<uint32_t>(virtual_variables.at(virt_var_idx).data_type) <= 6)){
            func.bb.at(0).in.push_back(std::make_tuple(virt_var_idx, virtual_variables.at(virt_var_idx).sensitivity));
        }
        //if variable is local -> add to in
        else if((7 <= static_cast<uint32_t>(virtual_variables.at(virt_var_idx).data_type)) && (static_cast<uint32_t>(virtual_variables.at(virt_var_idx).data_type) <= 12)){
            func.bb.at(0).in.push_back(std::make_tuple(virt_var_idx, 0));
            virtual_variables.at(virt_var_idx).sensitivity = 0;
        }
    }
}

void make_dependencies_of_curr_bb(basic_block& bb, virtual_variables_vec& virtual_variables, std::vector<std::tuple<uint32_t, std::vector<uint32_t>>>& virtual_variable_dependency){
    for(uint32_t ssa_idx = 0; ssa_idx < bb.ssa_instruction.size(); ++ssa_idx){
        switch(bb.ssa_instruction.at(ssa_idx).instr_type){
            case(INSTRUCTION_TYPE::INSTR_NORMAL):
            {
                bool src1_is_variable = false;
                bool src2_is_variable = false;
                normal_instruction instr = boost::get<normal_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                if(instr.src1.op_type != DATA_TYPE::NUM){
                    src1_is_variable = true;
                    uint32_t idx_to_variable_that_influences_destination = instr.src1.op_data;
                    auto it = std::find_if(virtual_variable_dependency.begin(), virtual_variable_dependency.end(), [idx_to_variable_that_influences_destination](const std::tuple<uint32_t, std::vector<uint32_t>>& t){return std::get<0>(t) == idx_to_variable_that_influences_destination;});
                    if(it == virtual_variable_dependency.end()){
                        std::vector<uint32_t> tmp;
                        tmp.emplace_back(instr.dest.index_to_variable);
                        virtual_variable_dependency.emplace_back(std::make_tuple(idx_to_variable_that_influences_destination, tmp));
                    }
                    else{
                        std::get<1>(*it).emplace_back(instr.dest.index_to_variable);
                        std::get<1>(*it).erase(std::unique(std::get<1>(*it).begin(), std::get<1>(*it).end()), std::get<1>(*it).end());
                        std::sort(std::get<1>(*it).begin(), std::get<1>(*it).end());
                    }
                    

                }
                if((instr.src2.op_type != DATA_TYPE::NUM) && (instr.src2.op_type != DATA_TYPE::NONE)){
                    src2_is_variable = true;
                    uint32_t idx_to_variable_that_influences_destination = instr.src2.op_data;
                    auto it = std::find_if(virtual_variable_dependency.begin(), virtual_variable_dependency.end(), [idx_to_variable_that_influences_destination](const std::tuple<uint32_t, std::vector<uint32_t>>& t){return std::get<0>(t) == idx_to_variable_that_influences_destination;});
                    if(it == virtual_variable_dependency.end()){
                        std::vector<uint32_t> tmp;
                        tmp.emplace_back(instr.dest.index_to_variable);
                        virtual_variable_dependency.emplace_back(std::make_tuple(idx_to_variable_that_influences_destination, tmp));

                    }
                    else{
                        std::get<1>(*it).emplace_back(instr.dest.index_to_variable);
                        std::get<1>(*it).erase(std::unique(std::get<1>(*it).begin(), std::get<1>(*it).end()), std::get<1>(*it).end());
                        std::sort(std::get<1>(*it).begin(), std::get<1>(*it).end());

                    }

                }   
                if((!src1_is_variable) && (!src2_is_variable)){
                    if(virtual_variables.at(instr.dest.index_to_variable).sensitivity == -1){
                        virtual_variables.at(instr.dest.index_to_variable).sensitivity = 0;
                    }
                }
                break;
            }
            case(INSTRUCTION_TYPE::INSTR_ASSIGN):
            {
                assignment_instruction instr = boost::get<assignment_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                if(instr.src.op_type != DATA_TYPE::NUM){
                    uint32_t idx_to_variable_that_influences_destination = instr.src.op_data;
                    auto it = std::find_if(virtual_variable_dependency.begin(), virtual_variable_dependency.end(), [idx_to_variable_that_influences_destination](const std::tuple<uint32_t, std::vector<uint32_t>>& t){return std::get<0>(t) == idx_to_variable_that_influences_destination;});
                    if(it == virtual_variable_dependency.end()){
                        std::vector<uint32_t> tmp;
                        tmp.emplace_back(instr.dest.index_to_variable);
                        virtual_variable_dependency.emplace_back(std::make_tuple(idx_to_variable_that_influences_destination, tmp));

                    }
                    else{
                        std::get<1>(*it).emplace_back(instr.dest.index_to_variable);
                        std::get<1>(*it).erase(std::unique(std::get<1>(*it).begin(), std::get<1>(*it).end()), std::get<1>(*it).end());
                        std::sort(std::get<1>(*it).begin(), std::get<1>(*it).end());

                    }
                }
                else{
                    if(virtual_variables.at(instr.dest.index_to_variable).sensitivity == -1){
                        virtual_variables.at(instr.dest.index_to_variable).sensitivity = 0;
                    }
                }
                break;
            }
            case(INSTRUCTION_TYPE::INSTR_REF):
            {
                reference_instruction instr = boost::get<reference_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                uint32_t idx_to_src = instr.src.index_to_variable;
                uint32_t idx_to_dest = instr.dest.index_to_variable;

                auto it = std::find_if(virtual_variable_dependency.begin(), virtual_variable_dependency.end(), [idx_to_src](const std::tuple<uint32_t, std::vector<uint32_t>>& t){return std::get<0>(t) == idx_to_src;});
                if(it == virtual_variable_dependency.end()){
                    std::vector<uint32_t> tmp;
                    tmp.emplace_back(instr.dest.index_to_variable);
                    virtual_variable_dependency.emplace_back(std::make_tuple(idx_to_src, tmp));

                }
                else{
                    std::get<1>(*it).emplace_back(instr.dest.index_to_variable);
                    std::get<1>(*it).erase(std::unique(std::get<1>(*it).begin(), std::get<1>(*it).end()), std::get<1>(*it).end());
                    std::sort(std::get<1>(*it).begin(), std::get<1>(*it).end());

                }


                it = std::find_if(virtual_variable_dependency.begin(), virtual_variable_dependency.end(), [idx_to_dest](const std::tuple<uint32_t, std::vector<uint32_t>>& t){return std::get<0>(t) == idx_to_dest;});
                if(it == virtual_variable_dependency.end()){
                    std::vector<uint32_t> tmp;
                    tmp.emplace_back(instr.src.index_to_variable);
                    virtual_variable_dependency.emplace_back(std::make_tuple(idx_to_dest, tmp));

                }
                else{
                    std::get<1>(*it).emplace_back(instr.dest.index_to_variable);
                    std::get<1>(*it).erase(std::unique(std::get<1>(*it).begin(), std::get<1>(*it).end()), std::get<1>(*it).end());
                    std::sort(std::get<1>(*it).begin(), std::get<1>(*it).end());

                }
                break;
            }
            case(INSTRUCTION_TYPE::INSTR_LOAD):
            {
                mem_load_instruction instr = boost::get<mem_load_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                uint32_t idx_to_src = instr.base_address.index_to_variable;
        
                auto it = std::find_if(virtual_variable_dependency.begin(), virtual_variable_dependency.end(), [idx_to_src](const std::tuple<uint32_t, std::vector<uint32_t>>& t){return std::get<0>(t) == idx_to_src;});
                if(it == virtual_variable_dependency.end()){
                    std::vector<uint32_t> tmp;
                    tmp.emplace_back(instr.dest.index_to_variable);
                    virtual_variable_dependency.emplace_back(std::make_tuple(idx_to_src, tmp));

                }
                else{
                    std::get<1>(*it).emplace_back(instr.dest.index_to_variable);
                    std::get<1>(*it).erase(std::unique(std::get<1>(*it).begin(), std::get<1>(*it).end()), std::get<1>(*it).end());
                    std::sort(std::get<1>(*it).begin(), std::get<1>(*it).end());

                }

                break;
            }
            case(INSTRUCTION_TYPE::INSTR_STORE):
            {
                mem_store_instruction instr = boost::get<mem_store_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                if(instr.value.op_type != DATA_TYPE::NUM){
                    uint32_t idx_to_src = instr.value.op_data;
            
                    auto it = std::find_if(virtual_variable_dependency.begin(), virtual_variable_dependency.end(), [idx_to_src](const std::tuple<uint32_t, std::vector<uint32_t>>& t){return std::get<0>(t) == idx_to_src;});
                    if(it == virtual_variable_dependency.end()){
                        std::vector<uint32_t> tmp;
                        tmp.emplace_back(instr.dest.index_to_variable);
                        virtual_variable_dependency.emplace_back(std::make_tuple(idx_to_src, tmp));

                    }
                    else{
                        std::get<1>(*it).emplace_back(instr.dest.index_to_variable);
                        std::get<1>(*it).erase(std::unique(std::get<1>(*it).begin(), std::get<1>(*it).end()), std::get<1>(*it).end());
                        std::sort(std::get<1>(*it).begin(), std::get<1>(*it).end());

                    }
                }
                break;
            }

            case(INSTRUCTION_TYPE::INSTR_PHI):
            {
                phi_instruction instr = boost::get<phi_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                for(uint32_t phi_src_idx = 0; phi_src_idx < instr.srcs.size(); ++phi_src_idx){
                    uint32_t idx_to_src = std::get<0>(instr.srcs.at(phi_src_idx));
            
                    auto it = std::find_if(virtual_variable_dependency.begin(), virtual_variable_dependency.end(), [idx_to_src](const std::tuple<uint32_t, std::vector<uint32_t>>& t){return std::get<0>(t) == idx_to_src;});
                    if(it == virtual_variable_dependency.end()){
                        std::vector<uint32_t> tmp;
                        tmp.emplace_back(instr.dest.index_to_variable);
                        virtual_variable_dependency.emplace_back(std::make_tuple(idx_to_src, tmp));

                    }
                    else{
                        std::get<1>(*it).emplace_back(instr.dest.index_to_variable);
                        std::get<1>(*it).erase(std::unique(std::get<1>(*it).begin(), std::get<1>(*it).end()), std::get<1>(*it).end());
                        std::sort(std::get<1>(*it).begin(), std::get<1>(*it).end());

                    }
                }
                break;
            }
            default: break;
        }
    }
}

void assign_new_sensitivity(int32_t& old_sensitivity, int32_t new_sensitivity, bool& no_sensitivity_changed){
    if(old_sensitivity == 1){

    }
    else if((old_sensitivity == 0) & (new_sensitivity > 0)){
        old_sensitivity = new_sensitivity;
        no_sensitivity_changed = false;
    }
    else if((old_sensitivity > 1) & (new_sensitivity > 0)){
        if(old_sensitivity > new_sensitivity){
            old_sensitivity = new_sensitivity;
            no_sensitivity_changed = false;
        }
    }
    else if(old_sensitivity == -1){
        old_sensitivity = new_sensitivity;
        no_sensitivity_changed = false;
    }
}


void resolve_collection_dependency(std::vector<uint32_t> collection_of_virtual_variable, uint32_t idx_of_dependent_variable, virtual_variables_vec& virtual_variables, bool& no_sensitivity_changed){
    virtual_variable_information starting_point_variable = virtual_variables.at(idx_of_dependent_variable);
    for(const auto& elem: collection_of_virtual_variable){ //modify each element in collection except the one that is the reference 
        if(elem != idx_of_dependent_variable){
            //variable that initially changed sensitivity has no add on:
            // - all elements with no add on get same sensitivity
            // - all elements with array add on get the sensitivity minus the number of dimensions -> dereferencing (having arr_add_on) will get variable closer to sensitive information
            // - all elements with asterisk add on get the sensitivity minus 1 -> dereferencing (having asterisk_add_on) will get variable closer to sensitive information
            // - all elements with amp add on get the sensitivity plus 1 -> holding the address (having amp_add_on) will get variable further away from sensitive information
            // - all elements with asterisk and arr  add on get the sensitivity minus the number of dimensions -> dereferencing (having arr_add_on) will get variable closer to sensitive information
            if(starting_point_variable.ssa_add_on == SSA_ADD_ON::NO_ADD_ON){
                if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::NO_ADD_ON){
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, starting_point_variable.sensitivity, no_sensitivity_changed);
                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ARR_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        array_information a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                        modified_sensitivity = starting_point_variable.sensitivity - a_info.dimension;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity - 1;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::AMP_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity + 1;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        array_information a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                        modified_sensitivity = starting_point_variable.sensitivity - a_info.dimension;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::AMP_ARR_ADD_ON){
                    throw std::runtime_error("in resolve_collection_dependency: can not handle yet no_add_on + amp_arr_add_on case");
                }
                
            }


            //variable that initially changed sensitivity has arr add on:
            // - all elements with no add on get sensitivity plus dimensions -> no dereferencing will get variable further away from sensitive information
            // - all elements with array add on get the sensitivity plus (dimension of initial variable - dimension of dependent variable)
            // - all elements with asterisk add on get the sensitivity plus dimension of initial variable - 1
            // - all elements with amp add on get the sensitivity plus 1 plus dimension of initial variable
            // - all elements with asterisk and arr  add on get the sensitivity plus (dimension of initial variable - dimension of dependent variable)
            // - all elements with amp and arr add on get sensitivity + 1 + (dimension of initial variable - dimension of dependent variable)
            else if(starting_point_variable.ssa_add_on == SSA_ADD_ON::ARR_ADD_ON){
                array_information a_info = boost::get<array_information>(starting_point_variable.data_information);
                std::vector<uint32_t> known_indices_in_variable_array(a_info.dimension, UINT32_MAX);
                for(uint32_t arr_idx = 0; arr_idx < a_info.dimension; ++arr_idx){
                    if(arr_idx >= a_info.dimension_without_size){
                        if(a_info.depth_per_dimension.at(arr_idx).op_type == DATA_TYPE::NUM){
                            known_indices_in_variable_array.at(arr_idx) = a_info.depth_per_dimension.at(arr_idx).op_data;
                        }
                    }
                }


                if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::NO_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity + a_info.dimension;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ARR_ADD_ON){
                    int32_t modified_sensitivity;
                    array_information dependent_a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity + (a_info.dimension - dependent_a_info.dimension);
                    }

                    bool adjust_vector = true;
                    for(uint32_t arr_idx = 0; arr_idx < dependent_a_info.dimension; ++arr_idx){
                        if(arr_idx >= dependent_a_info.dimension_without_size){
                            if((dependent_a_info.depth_per_dimension.at(arr_idx).op_type == DATA_TYPE::NUM) && (arr_idx < known_indices_in_variable_array.size()) && (known_indices_in_variable_array.at(arr_idx) != UINT32_MAX)){
                                if(dependent_a_info.depth_per_dimension.at(arr_idx).op_data != known_indices_in_variable_array.at(arr_idx)){
                                    adjust_vector = false;
                                }
                            }
                        }
                    }

                    if(adjust_vector){
                        assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                    }
                    

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        //array_information a_info = boost::get<array_information>(starting_point_variable.data_information);
                        modified_sensitivity = starting_point_variable.sensitivity + (a_info.dimension - 1);
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::AMP_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity + a_info.dimension + 1;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON){
                    array_information dependent_a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity + (a_info.dimension - dependent_a_info.dimension);
                    }
                    bool adjust_vector = true;
                    for(uint32_t arr_idx = 0; arr_idx < dependent_a_info.dimension; ++arr_idx){
                        if(arr_idx >= dependent_a_info.dimension_without_size){
                            if((dependent_a_info.depth_per_dimension.at(arr_idx).op_type == DATA_TYPE::NUM) && (arr_idx < known_indices_in_variable_array.size()) && (known_indices_in_variable_array.at(arr_idx) != UINT32_MAX)){
                                if(dependent_a_info.depth_per_dimension.at(arr_idx).op_data != known_indices_in_variable_array.at(arr_idx)){
                                    adjust_vector = false;
                                }
                            }
                        }
                    }

                    if(adjust_vector){
                        assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                    }
                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::AMP_ARR_ADD_ON){
                    array_information dependent_a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity  + 1 + (a_info.dimension - dependent_a_info.dimension);
                    }
                    bool adjust_vector = true;
                    for(uint32_t arr_idx = 0; arr_idx < dependent_a_info.dimension; ++arr_idx){
                        if(arr_idx >= dependent_a_info.dimension_without_size){
                            if((dependent_a_info.depth_per_dimension.at(arr_idx).op_type == DATA_TYPE::NUM) && (arr_idx < known_indices_in_variable_array.size()) && (known_indices_in_variable_array.at(arr_idx) != UINT32_MAX)){
                                if(dependent_a_info.depth_per_dimension.at(arr_idx).op_data != known_indices_in_variable_array.at(arr_idx)){
                                    adjust_vector = false;
                                }
                            }
                        }
                    }

                    if(adjust_vector){
                        assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                    }
                }
            }

            //variable that initially changed sensitivity has asterisk add on:
            // - all elements with no add on get sensitivity plus 1 -> asterisk is dereference -> no asterisk is just a pointer -> will get variable further away from sensitive information
            // - all elements with array add on get the sensitivity minus (the number of dimensions - 1) -> dereferencing (having arr_add_on) will get variable closer to sensitive information, if the number of dimensions is greater than 1 (the dereference)
            // - all elements with asterisk add on get the same sensitivity
            // - all elements with asterisk and arr  add on get the sensitivity minus (the number of dimensions - 1) -> dereferencing (having arr_add_on) will get variable closer to sensitive information
            else if(starting_point_variable.ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON){
                if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::NO_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity + 1;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ARR_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        array_information a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                        modified_sensitivity = starting_point_variable.sensitivity - (a_info.dimension - 1);
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::AMP_ADD_ON){
                    throw std::runtime_error("in resolve_collection_dependency: can not handle yet asteristk_add_on + amp_add_on case");

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        array_information a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                        modified_sensitivity = starting_point_variable.sensitivity - (a_info.dimension - 1);
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::AMP_ARR_ADD_ON){
                    throw std::runtime_error("in resolve_collection_dependency: can not handle yet asteristk_add_on + amp_arr_add_on case");
                }
            }

            //variable that initially changed sensitivity has asterisk and arr add on:
            // - all elements with no add on get sensitivity plus number of dimensions of initial variable -> no dereferencing will get variable further away from sensitivity
            // - all elements with array add on get the sensitivity plus (the number of dimensions of initial - number of dimensions of add on) -> if the new add on has more dimensions the variable will get closer to sensitive information
            // - all elements with asterisk add on get the  sensitivity plus (the number of dimensions of initial - number of dimensions of add on) -> if the new add on has less dimensions the variable will get further away from sensitive information
            // - all elements with asterisk and arr  add on get the sensitivity plus (the number of dimensions of initial - number of dimensions of add on) -> if the new add on has more dimensions the variable will get closer to sensitive information
            else if(starting_point_variable.ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON){
                array_information a_info = boost::get<array_information>(starting_point_variable.data_information);
                std::vector<uint32_t> known_indices_in_variable_array(a_info.dimension, UINT32_MAX);
                for(uint32_t arr_idx = 0; arr_idx < a_info.dimension; ++arr_idx){
                    if(arr_idx >= a_info.dimension_without_size){
                        if(a_info.depth_per_dimension.at(arr_idx).op_type == DATA_TYPE::NUM){
                            known_indices_in_variable_array.at(arr_idx) = a_info.depth_per_dimension.at(arr_idx).op_data;
                        }
                    }
                }


                if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::NO_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity + a_info.dimension;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ARR_ADD_ON){
                    int32_t modified_sensitivity;
                    array_information dependent_a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity + (a_info.dimension - dependent_a_info.dimension);
                    }

                    bool adjust_vector = true;
                    for(uint32_t arr_idx = 0; arr_idx < dependent_a_info.dimension; ++arr_idx){
                        if(arr_idx >= dependent_a_info.dimension_without_size){
                            if((dependent_a_info.depth_per_dimension.at(arr_idx).op_type == DATA_TYPE::NUM) && (arr_idx < known_indices_in_variable_array.size()) && (known_indices_in_variable_array.at(arr_idx) != UINT32_MAX)){
                                if(dependent_a_info.depth_per_dimension.at(arr_idx).op_data != known_indices_in_variable_array.at(arr_idx)){
                                    adjust_vector = false;
                                }
                            }
                        }
                    }

                    if(adjust_vector){
                        assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                    }
                    

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        //array_information a_info = boost::get<array_information>(starting_point_variable.data_information);
                        modified_sensitivity = starting_point_variable.sensitivity + (a_info.dimension - 1);
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::AMP_ADD_ON){
                    throw std::runtime_error("in resolve_collection_dependency: can not handle yet arr_add_on + amp_add_on case");

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON){
                    array_information dependent_a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity + (a_info.dimension - dependent_a_info.dimension);
                    }
                    bool adjust_vector = true;
                    for(uint32_t arr_idx = 0; arr_idx < dependent_a_info.dimension; ++arr_idx){
                        if(arr_idx >= dependent_a_info.dimension_without_size){
                            if((dependent_a_info.depth_per_dimension.at(arr_idx).op_type == DATA_TYPE::NUM) && (arr_idx < known_indices_in_variable_array.size()) && (known_indices_in_variable_array.at(arr_idx) != UINT32_MAX)){
                                if(dependent_a_info.depth_per_dimension.at(arr_idx).op_data != known_indices_in_variable_array.at(arr_idx)){
                                    adjust_vector = false;
                                }
                            }
                        }
                    }

                    if(adjust_vector){
                        assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                    }
                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::AMP_ARR_ADD_ON){
                    array_information dependent_a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity  + 1 + (a_info.dimension - dependent_a_info.dimension);
                    }
                    bool adjust_vector = true;
                    for(uint32_t arr_idx = 0; arr_idx < dependent_a_info.dimension; ++arr_idx){
                        if(arr_idx >= dependent_a_info.dimension_without_size){
                            if((dependent_a_info.depth_per_dimension.at(arr_idx).op_type == DATA_TYPE::NUM) && (arr_idx < known_indices_in_variable_array.size()) && (known_indices_in_variable_array.at(arr_idx) != UINT32_MAX)){
                                if(dependent_a_info.depth_per_dimension.at(arr_idx).op_data != known_indices_in_variable_array.at(arr_idx)){
                                    adjust_vector = false;
                                }
                            }
                        }
                    }

                    if(adjust_vector){
                        assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                    }
                }
            }

            //variable that initially changed sensitivity has amp add on:
            // - all elements with no add on get sensitivity minus 1 -> removing the reference will get variable closer to sensitive information
            // - all elements with asterisk add on get the same sensitivity minus two -> removing reference and adding dereference will get variable closer to sensitive information
            // - all elements with asterisk and arr  add on get the sensitivity minus (the number of dimensions - 1) -> dereferencing (having arr_add_on) will get variable closer to sensitive information
            else if(starting_point_variable.ssa_add_on == SSA_ADD_ON::AMP_ADD_ON){

                array_information a_info_variable_is_array;
                if(starting_point_variable.index_to_original != -1){
                    if((virtual_variables.at(starting_point_variable.index_to_original).data_type == DATA_TYPE::INPUT_ARR) || (virtual_variables.at(starting_point_variable.index_to_original).data_type == DATA_TYPE::LOCAL_ARR) || (virtual_variables.at(starting_point_variable.index_to_original).data_type == DATA_TYPE::VIRTUAL_ARR)){
                        a_info_variable_is_array = boost::get<array_information>(virtual_variables.at(starting_point_variable.index_to_original).data_information);
                    }

                }
                else{
                    if((starting_point_variable.data_type == DATA_TYPE::INPUT_ARR) || (starting_point_variable.data_type == DATA_TYPE::LOCAL_ARR) || (starting_point_variable.data_type == DATA_TYPE::VIRTUAL_ARR)){
                        a_info_variable_is_array = boost::get<array_information>(starting_point_variable.data_information);

                    }
                }
                
                
                
                
                if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::NO_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity - 1;

                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ARR_ADD_ON){
                    array_information dependent_a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity - 1 - dependent_a_info.dimension;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity - 2;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::AMP_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON){
                    array_information dependent_a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity - 1 - dependent_a_info.dimension;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::AMP_ARR_ADD_ON){
                    throw std::runtime_error("in resolve_collection_dependency: can not handle yet amp_add_on + amp_arr_add_on case");
                }
            }

            //variable that initially changed sensitivity has amp and arr add on:
            // - all elements with no add on get sensitivity minus 1 plus dimension of array -> removing the reference will get variable closer to sensitive information and removing dereference will get variable further away from sensitive information
            // - all elements with arr add on get sensitivity minus 1 + (the number of dimensions of initial - number of dimensions of add on) -> removing reference will get variable closer to sensitive information and depending if dependent add on has more or less dimension accesses the sensitivity will be changed
            // - all elements with asterisk add on get the same sensitivity minus 1 plus dimension of initial - 1 
            // - all elements with asterisk and arr  add on get the sensitivity minus 1 + (the number of dimensions of initial - number of dimensions of add on) -> removing reference will get variable closer to sensitive information and depending if dependent add on has more or less dimension accesses the sensitivity will be changed
            // - all elements with amp and arr and on get sensitivitiy + (the number of dimensions of initial - number of dimensions of add on) -> removing reference will get variable closer to sensitive information and depending if dependent add on has more or less dimension accesses the sensitivity will be changed
            else if(starting_point_variable.ssa_add_on == SSA_ADD_ON::AMP_ARR_ADD_ON){
                array_information a_info = boost::get<array_information>(starting_point_variable.data_information);
                std::vector<uint32_t> known_indices_in_variable_array(a_info.dimension, UINT32_MAX);
                for(uint32_t arr_idx = 0; arr_idx < a_info.dimension; ++arr_idx){
                    if(arr_idx >= a_info.dimension_without_size){
                        if(a_info.depth_per_dimension.at(arr_idx).op_type == DATA_TYPE::NUM){
                            known_indices_in_variable_array.at(arr_idx) = a_info.depth_per_dimension.at(arr_idx).op_data;
                        }
                    }
                }


                if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::NO_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity + a_info.dimension - 1;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ARR_ADD_ON){
                    int32_t modified_sensitivity;
                    array_information dependent_a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity - 1 + (a_info.dimension - dependent_a_info.dimension);
                    }

                    bool adjust_vector = true;
                    for(uint32_t arr_idx = 0; arr_idx < dependent_a_info.dimension; ++arr_idx){
                        if(arr_idx >= dependent_a_info.dimension_without_size){
                            if((dependent_a_info.depth_per_dimension.at(arr_idx).op_type == DATA_TYPE::NUM) && (arr_idx < known_indices_in_variable_array.size()) && (known_indices_in_variable_array.at(arr_idx) != UINT32_MAX)){
                                if(dependent_a_info.depth_per_dimension.at(arr_idx).op_data != known_indices_in_variable_array.at(arr_idx)){
                                    adjust_vector = false;
                                }
                            }
                        }
                    }

                    if(adjust_vector){
                        assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                    }
                    

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ASTERISK_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        //array_information a_info = boost::get<array_information>(starting_point_variable.data_information);
                        modified_sensitivity = starting_point_variable.sensitivity - 1 + (a_info.dimension - 1);
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::AMP_ADD_ON){
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        //array_information a_info = boost::get<array_information>(starting_point_variable.data_information);
                        modified_sensitivity = starting_point_variable.sensitivity + a_info.dimension ;
                    }
                    assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);

                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::ASTERISK_ARR_ADD_ON){
                    array_information dependent_a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                    int32_t modified_sensitivity;
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity - 1 + (a_info.dimension - dependent_a_info.dimension);
                    }
                    bool adjust_vector = true;
                    for(uint32_t arr_idx = 0; arr_idx < dependent_a_info.dimension; ++arr_idx){
                        if(arr_idx >= dependent_a_info.dimension_without_size){
                            if((dependent_a_info.depth_per_dimension.at(arr_idx).op_type == DATA_TYPE::NUM) && (arr_idx < known_indices_in_variable_array.size()) && (known_indices_in_variable_array.at(arr_idx) != UINT32_MAX)){
                                if(dependent_a_info.depth_per_dimension.at(arr_idx).op_data != known_indices_in_variable_array.at(arr_idx)){
                                    adjust_vector = false;
                                }
                            }
                        }
                    }

                    if(adjust_vector){
                        assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                    }
                }
                else if(virtual_variables.at(elem).ssa_add_on == SSA_ADD_ON::AMP_ARR_ADD_ON){
                    int32_t modified_sensitivity;
                    array_information dependent_a_info = boost::get<array_information>(virtual_variables.at(elem).data_information);
                    if(starting_point_variable.sensitivity == 0){
                        modified_sensitivity = 0;
                    }
                    else{
                        modified_sensitivity = starting_point_variable.sensitivity + (a_info.dimension - dependent_a_info.dimension);
                    }

                    bool adjust_vector = true;
                    for(uint32_t arr_idx = 0; arr_idx < dependent_a_info.dimension; ++arr_idx){
                        if(arr_idx >= dependent_a_info.dimension_without_size){
                            if((dependent_a_info.depth_per_dimension.at(arr_idx).op_type == DATA_TYPE::NUM) && (arr_idx < known_indices_in_variable_array.size()) && (known_indices_in_variable_array.at(arr_idx) != UINT32_MAX)){
                                if(dependent_a_info.depth_per_dimension.at(arr_idx).op_data != known_indices_in_variable_array.at(arr_idx)){
                                    adjust_vector = false;
                                }
                            }
                        }
                    }

                    if(adjust_vector){
                        assign_new_sensitivity(virtual_variables.at(elem).sensitivity, modified_sensitivity, no_sensitivity_changed);
                    }
                }
            }

            else{
                throw std::runtime_error("in resolve_initial_dependency: can not identify ssa_add_on of root variable");
            }
        }
    }
}


void propagate_influence_of_current_dependency(uint32_t idx_of_changed_variable, const std::vector<std::vector<uint32_t>>& collection_of_variables_with_same_original_name, virtual_variables_vec& virtual_variables, bool& no_sensitivity_changed){
    

    uint32_t idx_of_original_dependent_variable;
    if(virtual_variables.at(idx_of_changed_variable).index_to_original == -1){
        idx_of_original_dependent_variable = idx_of_changed_variable;
    }
    else{
        idx_of_original_dependent_variable = virtual_variables.at(idx_of_changed_variable).index_to_original;
    }
    auto it = std::find_if(collection_of_variables_with_same_original_name.begin(), collection_of_variables_with_same_original_name.end(), [idx_of_original_dependent_variable](const std::vector<uint32_t>& t){return t.at(0) == idx_of_original_dependent_variable;});
    if(it == collection_of_variables_with_same_original_name.end()){
        throw std::runtime_error("in propagate_influence_of_current_dependency: error in finding original idx for virtual variable");
    }
    resolve_collection_dependency(*it, idx_of_changed_variable, virtual_variables, no_sensitivity_changed);

}

//construct the "out" datatype by adjusting all sensitivities of "in" and adding new variables that were covered in current bb
void make_out_of_curr_bb(basic_block& bb, virtual_variables_vec& virtual_variables, std::vector<std::tuple<uint32_t, std::vector<uint32_t>>> virtual_variable_dependency){
    bb.out.clear();
    for(auto& bb_in: bb.in){
        std::tuple<uint32_t, int32_t> tmp = std::make_tuple(std::get<0>(bb_in), virtual_variables.at(std::get<0>(bb_in)).sensitivity);
        bb.out.emplace_back(tmp);
    }

    for(auto& dependency: virtual_variable_dependency){
        for(auto& new_elem: std::get<1>(dependency)){
            auto it = std::find_if(bb.out.begin(), bb.out.end(), [new_elem](const std::tuple<uint32_t, int32_t>& t){return std::get<0>(t) == new_elem;});
            if(it == bb.out.end()){
                bb.out.emplace_back(std::make_tuple(new_elem, virtual_variables.at(new_elem).sensitivity));
            }
            else{ //there is already the virtual variable present -> pass to curr_in the
                if((std::get<1>(*it) == -1) && (virtual_variables.at(new_elem).sensitivity != -1)){
                    std::get<1>(*it) = virtual_variables.at(new_elem).sensitivity;
                }
                else if((std::get<1>(*it) == 0) && (virtual_variables.at(new_elem).sensitivity > 0)){
                    std::get<1>(*it) = virtual_variables.at(new_elem).sensitivity;
                }
                else if((std::get<1>(*it) == 1)){

                }
                else if((std::get<1>(*it) > 1) && (virtual_variables.at(new_elem).sensitivity > 0)){
                    if(virtual_variables.at(new_elem).sensitivity < std::get<1>(*it)){
                        std::get<1>(*it) = virtual_variables.at(new_elem).sensitivity;
                    }
                }
            }
        }
    }

}

//use the information from "in" of current bb together with the variables and their relation within the current bb and update the required sensitivity of the variables
void taint_propagation_of_bb(basic_block& bb, virtual_variables_vec& virtual_variables, const std::vector<std::vector<uint32_t>>& collection_of_variables_with_same_original_name){
    
    std::vector<std::tuple<uint32_t, std::vector<uint32_t>>> virtual_variable_dependency; //first element in tuple influences all elements in vector

    bool no_sensitivity_changed = false;
    while(!no_sensitivity_changed){ //update the sensitivity of the variables within the bb until no sensitivity changes anymore

        no_sensitivity_changed = true;

        for(uint32_t ssa_idx = 0; ssa_idx < bb.ssa_instruction.size(); ++ssa_idx){ //go through every ssa instruction of the basic block and update the sensitivity based on the relation of the variables in their instruction
            switch(bb.ssa_instruction.at(ssa_idx).instr_type){
                //the sensitivity of the destination is the conjunction of the source sensitivities
                case(INSTRUCTION_TYPE::INSTR_NORMAL):{
                    normal_instruction instr = boost::get<normal_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                    int32_t src1_sens = -1;
                    int32_t src2_sens = -1;
                    if(instr.src1.op_type != DATA_TYPE::NUM){
                        src1_sens = virtual_variables.at(instr.src1.op_data).sensitivity;
                    }
                    else{
                        src1_sens = 0;
                    }
                    if((instr.src2.op_type != DATA_TYPE::NUM) && (instr.src2.op_type != DATA_TYPE::NONE)){
                        src2_sens = virtual_variables.at(instr.src2.op_data).sensitivity;
                    }
                    else{
                        src2_sens = 0;
                    }
                    int32_t old_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;

                    assign_sensitivity(virtual_variables.at(instr.dest.index_to_variable).sensitivity, src1_sens, src2_sens, no_sensitivity_changed);
                    int32_t new_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;
                    
                    //if the sensitivity of the destination change we can apply sensitivity modifications to the same variable but with different add ons -> they can be found in collection_of_variables_with_same_original_name
                    if(old_sens_of_var != new_sens_of_var){
                        propagate_influence_of_current_dependency(instr.dest.index_to_variable, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                    }
                    break;
                }
                //the sensitivity of the destination is the source sensitivity
                case(INSTRUCTION_TYPE::INSTR_ASSIGN):{
                    assignment_instruction instr = boost::get<assignment_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                    int32_t src_sens = -1;
                    if(instr.src.op_type != DATA_TYPE::NUM){
                        src_sens = virtual_variables.at(instr.src.op_data).sensitivity;
                    }
                    int32_t old_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;
                    assign_sensitivity(virtual_variables.at(instr.dest.index_to_variable).sensitivity, src_sens, -1, no_sensitivity_changed);
                    int32_t new_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;

                    if(old_sens_of_var != new_sens_of_var){
                        propagate_influence_of_current_dependency(instr.dest.index_to_variable, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                    }
                    break;
                }
                case(INSTRUCTION_TYPE::INSTR_NEGATE):{
                    normal_instruction instr = boost::get<normal_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                    int32_t src_sens = -1;
                    if(instr.src1.op_type != DATA_TYPE::NUM){
                        src_sens = virtual_variables.at(instr.src1.op_data).sensitivity;
                    }
                    int32_t old_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;
                    assign_sensitivity(virtual_variables.at(instr.dest.index_to_variable).sensitivity, src_sens, -1, no_sensitivity_changed);
                    int32_t new_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;

                    if(old_sens_of_var != new_sens_of_var){
                        propagate_influence_of_current_dependency(instr.dest.index_to_variable, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                    }
                    break;
                }
                //the sensitivity of the destination is the conjunction of the source sensitivities
                case(INSTRUCTION_TYPE::INSTR_PHI):{
                    phi_instruction instr = boost::get<phi_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                    for(uint32_t phi_src_idx = 0; phi_src_idx < instr.srcs.size(); ++phi_src_idx){
                        uint32_t idx_to_src = std::get<0>(instr.srcs.at(phi_src_idx));
                        int32_t src_sens = virtual_variables.at(idx_to_src).sensitivity;

                        int32_t old_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;
                        assign_sensitivity(virtual_variables.at(instr.dest.index_to_variable).sensitivity, src_sens, -1, no_sensitivity_changed);
                        int32_t new_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;

                        if(old_sens_of_var != new_sens_of_var){
                            propagate_influence_of_current_dependency(instr.dest.index_to_variable, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                        }
                    }
                    break;
                }
                //the sensitivity of the destination is the conjunction of the source sensitivities
                case(INSTRUCTION_TYPE::INSTR_REF):{
                    reference_instruction instr = boost::get<reference_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                    int32_t src_sens = -1;
                    src_sens = virtual_variables.at(instr.src.index_to_variable).sensitivity;

                    int32_t old_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;
                    assign_sensitivity(virtual_variables.at(instr.dest.index_to_variable).sensitivity, src_sens, -1, no_sensitivity_changed);
                    int32_t new_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;

                    if(old_sens_of_var != new_sens_of_var){
                        propagate_influence_of_current_dependency(instr.dest.index_to_variable, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                    }

                    int32_t dest_sens = -1;
                    dest_sens = virtual_variables.at(instr.dest.index_to_variable).sensitivity;

                    old_sens_of_var = virtual_variables.at(instr.src.index_to_variable).sensitivity;
                    assign_sensitivity(virtual_variables.at(instr.src.index_to_variable).sensitivity, dest_sens, -1, no_sensitivity_changed);
                    new_sens_of_var = virtual_variables.at(instr.src.index_to_variable).sensitivity;

                    if(old_sens_of_var != new_sens_of_var){
                        propagate_influence_of_current_dependency(instr.src.index_to_variable, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                    }
                    break;
                }
                case(INSTRUCTION_TYPE::INSTR_LOAD):{
                    mem_load_instruction instr = boost::get<mem_load_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                    int32_t src_sens = -1;
                    src_sens = virtual_variables.at(instr.base_address.index_to_variable).sensitivity;
                    if(src_sens > 0){
                        src_sens = 1; //TODO: this might be too over conservative
                    }

                    if(virtual_variables.at(instr.base_address.index_to_variable).ssa_add_on == SSA_ADD_ON::ARR_ADD_ON){
                        auto arr_info = boost::get<array_information>(virtual_variables.at(instr.base_address.index_to_variable).data_information);
                        for(uint32_t dim_idx = 0; dim_idx < arr_info.dimension; ++dim_idx){
                            if(dim_idx >= arr_info.dimension_without_size){
                                if(arr_info.depth_per_dimension.at(dim_idx).op_type != DATA_TYPE::NUM){
                                    if(virtual_variables.at(arr_info.depth_per_dimension.at(dim_idx).op_data).sensitivity == 1){
                                        src_sens = 1;
                                    }
                                }
                            }
                        }
                    }


                    int32_t old_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;
                    assign_sensitivity(virtual_variables.at(instr.dest.index_to_variable).sensitivity, src_sens, -1, no_sensitivity_changed);
                    int32_t new_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;

                    if(old_sens_of_var != new_sens_of_var){
                        propagate_influence_of_current_dependency(instr.dest.index_to_variable, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                    }
                    break;

                }
                case(INSTRUCTION_TYPE::INSTR_STORE):{
                    mem_store_instruction instr = boost::get<mem_store_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                    int32_t src_sens = -1;
                    if(instr.value.op_type != DATA_TYPE::NUM){
                        src_sens = virtual_variables.at(instr.value.op_data).sensitivity;
                    }

                    int32_t old_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;
                    assign_sensitivity(virtual_variables.at(instr.dest.index_to_variable).sensitivity, src_sens, -1, no_sensitivity_changed);
                    int32_t new_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;

                    if(old_sens_of_var != new_sens_of_var){
                        propagate_influence_of_current_dependency(instr.dest.index_to_variable, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                    }
                    break;

                }
                //the sensitivity of the input parameter is defined by the macro set in the c file -> if the parameter is not a number assign the security to the variable depending on the add on and the real data type of the original variable
                case(INSTRUCTION_TYPE::INSTR_FUNC_NO_RETURN):{
                    function_call_without_return_instruction instr = boost::get<function_call_without_return_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                    for(uint32_t param_idx = 0; param_idx < instr.parameters.size(); ++param_idx){
                        if(instr.parameters.at(param_idx).op.op_type != DATA_TYPE::NUM){
                            uint32_t idx_of_original;
                            if(virtual_variables.at(instr.parameters.at(param_idx).op.op_data).index_to_original == -1){
                                idx_of_original = instr.parameters.at(param_idx).op.op_data;
                            }
                            else{
                                idx_of_original = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).index_to_original;
                            }

                            uint32_t sens_of_param = instr.parameters.at(param_idx).sensitivity;

                            switch(virtual_variables.at(instr.parameters.at(param_idx).op.op_data).ssa_add_on){
                                //input parameter has amp add on
                                case(SSA_ADD_ON::AMP_ADD_ON):{
                                    switch(virtual_variables.at(idx_of_original).data_type){
                                        //data type of input paramerter is plain variable -> increase sensitivity by 1 as we pass an address
                                        case(DATA_TYPE::INPUT_VAR):
                                        case(DATA_TYPE::LOCAL_VAR):{
                                            if(sens_of_param == 1){
                                                sens_of_param += 1;
                                            }
                                            int32_t old_sens_of_var = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity;
                                            assign_sensitivity(virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity, sens_of_param, -1, no_sensitivity_changed);
                                            int32_t new_sens_of_var = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity;

                                            if(old_sens_of_var != new_sens_of_var){
                                                propagate_influence_of_current_dependency(instr.parameters.at(param_idx).op.op_data, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                                            }
                                            break;
                                        }
                                        case(DATA_TYPE::INPUT_ARR):
                                        case(DATA_TYPE::LOCAL_ARR):{
                                            if(sens_of_param == 1){
                                                array_information original_a_info = boost::get<array_information>(virtual_variables.at(idx_of_original).data_information);
                                                sens_of_param = 1 + original_a_info.dimension + 1; //secret + dimension + amp
                                            }
                                            int32_t old_sens_of_var = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity;
                                            assign_sensitivity(virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity, sens_of_param, -1, no_sensitivity_changed);
                                            int32_t new_sens_of_var = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity;

                                            if(old_sens_of_var != new_sens_of_var){
                                                propagate_influence_of_current_dependency(instr.parameters.at(param_idx).op.op_data, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                                            }
                                            break;
                                        }
                                        default: std::cout << virtual_variables.at(idx_of_original).ssa_name << " has data type" << static_cast<uint32_t>(virtual_variables.at(idx_of_original).data_type) << std::endl; throw std::runtime_error("11 in taint_propagation_of_bb: data type of parameter of function call can not be handled yet.");
                                    }
                                    break;
                                }
                                //input parameter has no add on
                                case(SSA_ADD_ON::NO_ADD_ON):{
                                    switch(virtual_variables.at(idx_of_original).data_type){
                                        //data type of input parameter is pointer type -> increase sensitivity by 1 as we pass an address
                                        case(DATA_TYPE::INPUT_VAR_PTR):
                                        case(DATA_TYPE::VIRTUAL_VAR_PTR):{
                                            if(sens_of_param == 1){
                                                sens_of_param += 1;
                                            }
                                            int32_t old_sens_of_var = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity;
                                            assign_sensitivity(virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity, sens_of_param, -1, no_sensitivity_changed);
                                            int32_t new_sens_of_var = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity;

                                            if(old_sens_of_var != new_sens_of_var){
                                                propagate_influence_of_current_dependency(instr.parameters.at(param_idx).op.op_data, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                                            }
                                            break;
                                        }
                                        case(DATA_TYPE::VIRTUAL_VAR):
                                        case(DATA_TYPE::INPUT_VAR):
                                        case(DATA_TYPE::LOCAL_VAR):{
                                            int32_t old_sens_of_var = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity;
                                            assign_sensitivity(virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity, sens_of_param, -1, no_sensitivity_changed);
                                            int32_t new_sens_of_var = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity;

                                            if(old_sens_of_var != new_sens_of_var){
                                                propagate_influence_of_current_dependency(instr.parameters.at(param_idx).op.op_data, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                                            }
                                            break;
                                        }
                                        default: {
                                            throw std::runtime_error("0 in taint_propagation_of_bb: data type of parameter of function call can not be handled yet.");
                                        }
                                    }
                                    break;  
                                }
                                default: throw std::runtime_error("in taint_propagation_of_bb: ssa add on of parameter of function call can not be handled yet.");
                            }
                        }
                    }
                    break;
                }
                case(INSTRUCTION_TYPE::INSTR_FUNC_RETURN):{
                    //the sensitivity of the input parameter is defined by the macro set in the c file -> if the parameter is not a number assign the security to the variable depending on the add on and the real data type of the original variable
                    function_call_with_return_instruction instr = boost::get<function_call_with_return_instruction>(bb.ssa_instruction.at(ssa_idx).instr);
                    for(uint32_t param_idx = 0; param_idx < instr.parameters.size(); ++param_idx){
                        if(instr.parameters.at(param_idx).op.op_type != DATA_TYPE::NUM){
                            uint32_t idx_of_original;
                            if(virtual_variables.at(instr.parameters.at(param_idx).op.op_data).index_to_original == -1){
                                idx_of_original = instr.parameters.at(param_idx).op.op_data;
                            }
                            else{
                                idx_of_original = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).index_to_original;
                            }

                            uint32_t sens_of_param = instr.parameters.at(param_idx).sensitivity;

                            switch(virtual_variables.at(instr.parameters.at(param_idx).op.op_data).ssa_add_on){
                                //input parameter has amp add on
                                case(SSA_ADD_ON::AMP_ADD_ON):{
                                    switch(virtual_variables.at(idx_of_original).data_type){
                                        //data type of input paramerter is plain variable -> increase sensitivity by 1 as we pass an address
                                        case(DATA_TYPE::LOCAL_VAR):{
                                            if(sens_of_param == 1){
                                                sens_of_param += 1;
                                            }
                                            int32_t old_sens_of_var = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity;
                                            assign_sensitivity(virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity, sens_of_param, -1, no_sensitivity_changed);
                                            int32_t new_sens_of_var = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity;

                                            if(old_sens_of_var != new_sens_of_var){
                                                propagate_influence_of_current_dependency(instr.parameters.at(param_idx).op.op_data, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                                            }
                                            break;
                                        }
                                        default: std::cout << virtual_variables.at(idx_of_original).ssa_name << " has data type" << static_cast<uint32_t>(virtual_variables.at(idx_of_original).data_type) << std::endl; throw std::runtime_error("12 in taint_propagation_of_bb: data type of parameter of function call can not be handled yet.");
                                    }
                                    break;
                                }
                                //input parameter has no add on
                                case(SSA_ADD_ON::NO_ADD_ON):{
                                    switch(virtual_variables.at(idx_of_original).data_type){
                                        //data type of input parameter is pointer type -> increase sensitivity by 1 as we pass an address
                                        case(DATA_TYPE::INPUT_VAR_PTR):
                                        case(DATA_TYPE::VIRTUAL_VAR_PTR):{
                                            if(sens_of_param == 1){
                                                sens_of_param += 1;
                                            }
                                            int32_t old_sens_of_var = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity;
                                            assign_sensitivity(virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity, sens_of_param, -1, no_sensitivity_changed);
                                            int32_t new_sens_of_var = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity;

                                            if(old_sens_of_var != new_sens_of_var){
                                                propagate_influence_of_current_dependency(instr.parameters.at(param_idx).op.op_data, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                                            }
                                            break;
                                        }
                                        case(DATA_TYPE::VIRTUAL_VAR):
                                        case(DATA_TYPE::INPUT_VAR):
                                        case(DATA_TYPE::LOCAL_VAR):{
                                            int32_t old_sens_of_var = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity;
                                            assign_sensitivity(virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity, sens_of_param, -1, no_sensitivity_changed);
                                            int32_t new_sens_of_var = virtual_variables.at(instr.parameters.at(param_idx).op.op_data).sensitivity;

                                            if(old_sens_of_var != new_sens_of_var){
                                                propagate_influence_of_current_dependency(instr.parameters.at(param_idx).op.op_data, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                                            }
                                            break;
                                        }
                                        default: {
                                            throw std::runtime_error("in taint_propagation_of_bb: data type of parameter of function call can not be handled yet.");
                                        }
                                    }
                                    break;  
                                }
                                default: throw std::runtime_error("in taint_propagation_of_bb: ssa add on of parameter of function call can not be handled yet.");
                            }
                        }
                    }

                    //sensitivity tracking for destination variable

                    uint32_t dest_idx_of_original;
                    if(virtual_variables.at(instr.dest.index_to_variable).index_to_original == -1){
                        dest_idx_of_original = instr.dest.index_to_variable;
                    }
                    else{
                        dest_idx_of_original = virtual_variables.at(instr.dest.index_to_variable).index_to_original;
                    }
                    uint32_t sens_of_dest = instr.dest_sensitivity;

                    switch(virtual_variables.at(dest_idx_of_original).ssa_add_on){
                        case(SSA_ADD_ON::NO_ADD_ON):{
                            switch(virtual_variables.at(dest_idx_of_original).data_type){
                                case(DATA_TYPE::INPUT_VAR):
                                case(DATA_TYPE::VIRTUAL_VAR):
                                case(DATA_TYPE::LOCAL_VAR):{
                                    int32_t old_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;
                                    assign_sensitivity(virtual_variables.at(instr.dest.index_to_variable).sensitivity, sens_of_dest, -1, no_sensitivity_changed);
                                    int32_t new_sens_of_var = virtual_variables.at(instr.dest.index_to_variable).sensitivity;

                                    if(old_sens_of_var != new_sens_of_var){
                                        propagate_influence_of_current_dependency(instr.dest.index_to_variable, collection_of_variables_with_same_original_name, virtual_variables, no_sensitivity_changed);
                                    }
                                    break;
                                }
                                default: {
                                    std::cout << virtual_variables.at(dest_idx_of_original).ssa_name << " has data type " << static_cast<uint32_t>(virtual_variables.at(dest_idx_of_original).data_type) << std::endl; 
                                    throw std::runtime_error("2 in taint_propagation_of_bb: data type of parameter of function call can not be handled yet.");
                                }
                            }
                            break;
                        }
                        default: throw std::runtime_error("in taint_propagation_of_bb: destination of function call has add on. Can not track sensitivity yet.");
                    }

                    break;
                }
                default: break;
            }
        }
    }

    make_out_of_curr_bb(bb, virtual_variables, virtual_variable_dependency);
}

//all variables that have the same original index get the same entry -> every entry consists of the different add ons that one variable migth have (e.g. _15, *_15, &_15, _15[1])
void generate_collection_of_variables(virtual_variables_vec& virtual_variables, std::vector<std::vector<uint32_t>>& collection_of_variables_with_same_original_name){
    for(uint32_t virt_var_idx = 0; virt_var_idx < virtual_variables.size(); ++virt_var_idx){
        if(virtual_variables.at(virt_var_idx).index_to_original == -1){
            collection_of_variables_with_same_original_name.resize(collection_of_variables_with_same_original_name.size() + 1);
            collection_of_variables_with_same_original_name.back().emplace_back(virt_var_idx);
        }
        else{
            uint32_t idx_to_original = virtual_variables.at(virt_var_idx).index_to_original;
            auto it = std::find_if(collection_of_variables_with_same_original_name.begin(), collection_of_variables_with_same_original_name.end(), [idx_to_original](const std::vector<uint32_t>& t){return t.at(0) == idx_to_original;});
            if(it == collection_of_variables_with_same_original_name.end()){
                throw std::runtime_error("in generate_collection_of_variables: can not find index to original of current virtua variable");
            }
            it->emplace_back(virt_var_idx);
        }
    }
}


void compute_overall_sensitivity_of_instructions(function& func, virtual_variables_vec& virtual_variables){
    for(auto& bb: func.bb)
    {
        for(auto& ssa_instr: bb.ssa_instruction){
            switch(ssa_instr.instr_type){
                case(INSTRUCTION_TYPE::INSTR_ASSIGN):{
                    assignment_instruction a_instr = boost::get<assignment_instruction>(ssa_instr.instr);
                    ssa_instr.sensitivity_level = virtual_variables.at(a_instr.dest.index_to_variable).sensitivity;
                    break;
                }
                case(INSTRUCTION_TYPE::INSTR_NEGATE):{
                    normal_instruction a_instr = boost::get<normal_instruction>(ssa_instr.instr);
                    ssa_instr.sensitivity_level = virtual_variables.at(a_instr.dest.index_to_variable).sensitivity;
                    break;
                }
                case(INSTRUCTION_TYPE::INSTR_NORMAL):{
                    normal_instruction n_instr = boost::get<normal_instruction>(ssa_instr.instr);
                    ssa_instr.sensitivity_level = virtual_variables.at(n_instr.dest.index_to_variable).sensitivity;
                    break;
                }
                case(INSTRUCTION_TYPE::INSTR_FUNC_NO_RETURN):{
                    ssa_instr.sensitivity_level = 0;
                    break;
                }
                case(INSTRUCTION_TYPE::INSTR_FUNC_RETURN):{
                    function_call_with_return_instruction f_instr = boost::get<function_call_with_return_instruction>(ssa_instr.instr);
                    ssa_instr.sensitivity_level = f_instr.dest_sensitivity;
                    break;
                }
                case(INSTRUCTION_TYPE::INSTR_REF):{
                    ssa_instr.sensitivity_level = 0;
                    break;
                }
                case(INSTRUCTION_TYPE::INSTR_PHI):{
                    phi_instruction p_instr = boost::get<phi_instruction>(ssa_instr.instr);
                    ssa_instr.sensitivity_level = virtual_variables.at(p_instr.dest.index_to_variable).sensitivity;

                    break;
                }
                case(INSTRUCTION_TYPE::INSTR_LOAD):{
                     mem_load_instruction  m_instr = boost::get<mem_load_instruction>(ssa_instr.instr);
                    if((virtual_variables.at(m_instr.dest.index_to_variable).sensitivity) == 1){
                        ssa_instr.sensitivity_level = 1;
                    }
                    else{
                        ssa_instr.sensitivity_level = 0;
                    }

                    break;
                }
                case(INSTRUCTION_TYPE::INSTR_STORE):{
                    mem_store_instruction m_instr = boost::get<mem_store_instruction>(ssa_instr.instr);
                    if(((virtual_variables.at(m_instr.value.op_data).sensitivity) == 1)){
                        ssa_instr.sensitivity_level = 1;
                    }
                    else{
                        ssa_instr.sensitivity_level = 0;
                    }
                    break;
                }
                case(INSTRUCTION_TYPE::INSTR_RETURN):{
                    return_instruction r_instr = boost::get<return_instruction>(ssa_instr.instr);
                    if(r_instr.return_value.op_type == DATA_TYPE::NONE){
                        ssa_instr.sensitivity_level = 0;
                    }
                    else{
                        if(r_instr.return_value.op_type == DATA_TYPE::NUM){
                            ssa_instr.sensitivity_level = 0;
                        }
                        else{
                            if(virtual_variables.at(r_instr.return_value.op_data).sensitivity != -1){
                                ssa_instr.sensitivity_level = virtual_variables.at(r_instr.return_value.op_data).sensitivity;
                            }
                        }
                    }
                    break;
                }
                case(INSTRUCTION_TYPE::INSTR_CONDITON):{
                    condition_instruction c_instr = boost::get<condition_instruction>(ssa_instr.instr);

                    if((c_instr.op1.op_type == DATA_TYPE::NUM)){
                        if(c_instr.op2.op_type == DATA_TYPE::NUM){
                            ssa_instr.sensitivity_level = 0;
                        }
                        else if(virtual_variables.at(c_instr.op2.op_data).sensitivity != -1){
                            ssa_instr.sensitivity_level = virtual_variables.at(c_instr.op2.op_data).sensitivity;
                        }
                    }
                    else{
                        if(c_instr.op2.op_type == DATA_TYPE::NUM){
                            ssa_instr.sensitivity_level = virtual_variables.at(c_instr.op1.op_data).sensitivity;
                        } 
                        else{
                            if((virtual_variables.at(c_instr.op2.op_data).sensitivity == 1) || (virtual_variables.at(c_instr.op1.op_data).sensitivity == 1)){
                                ssa_instr.sensitivity_level = 1;
                            }
                            else{
                                ssa_instr.sensitivity_level = 0;
                            }

                        }
                    }
                    break;
                }
                case(INSTRUCTION_TYPE::INSTR_BRANCH):{
                    ssa_instr.sensitivity_level = 0;
                    break;
                }
                default: break;
            }
        }
    }
}

void static_taint_tracking(function& func, std::vector<std::vector<uint8_t>>& adjacency_matrix, virtual_variables_vec& virtual_variables){
    
    std::vector<std::vector<uint32_t>> collection_of_variables_with_same_original_name; //e.g. *_15, _15 would be in same collection
    generate_collection_of_variables(virtual_variables, collection_of_variables_with_same_original_name);
    
    //get successors and predecessors for forward data flow analysis
    std::map<uint32_t, std::vector<uint32_t>> successors;
    compute_successors(func, adjacency_matrix, successors);
    std::map<uint32_t, std::vector<uint32_t>> predecessors;
    compute_predecessors(func, adjacency_matrix, predecessors);

    //fill worklist
    std::vector<uint32_t> worklist; //contains bb_number of basic blocks that needs to be processed
    for(uint32_t bb_idx = 1; bb_idx < func.bb.size(); ++bb_idx){
        worklist.emplace_back(func.bb.at(bb_idx).bb_number);
        func.bb.at(bb_idx).out.clear();
    }

    set_in_of_first_bb(func, virtual_variables);

    taint_propagation_of_bb(func.bb.at(0), virtual_variables, collection_of_variables_with_same_original_name);

    while(!worklist.empty()){
        //remove node from worklist
        uint32_t curr_data_flow_bb_number = worklist.at(0);
        worklist.erase(worklist.begin());
        //get actual bb from func
        auto it_curr_data_flow_bb = std::find_if(func.bb.begin(), func.bb.end(), [curr_data_flow_bb_number](const basic_block& bb){return bb.bb_number == curr_data_flow_bb_number;});
        if(it_curr_data_flow_bb == func.bb.end()){
            throw std::runtime_error("in static_taint_tracking: can not find basic block in function with current basic block number");
        }

        //in of current basic block is union of all
        for(const auto& predec: predecessors.at(curr_data_flow_bb_number)){
            auto it_predec = std::find_if(func.bb.begin(), func.bb.end(), [predec](const basic_block& bb){return bb.bb_number == predec;});
            union_out_of_predecessors_to_curr_in(it_curr_data_flow_bb->in, it_predec->out);
        }

        
        auto old_out = it_curr_data_flow_bb->out;

        taint_propagation_of_bb(*it_curr_data_flow_bb, virtual_variables, collection_of_variables_with_same_original_name);

        if(it_curr_data_flow_bb->out != old_out){
            // worklist.emplace_back(successors.at(curr_data_flow_bb_number));
            if(successors.find(curr_data_flow_bb_number) != successors.end()){
                std::copy(successors.at(curr_data_flow_bb_number).begin(), successors.at(curr_data_flow_bb_number).end(), std::back_inserter(worklist));
                worklist.erase(std::unique(worklist.begin(), worklist.end()), worklist.end());
                std::sort(worklist.begin(), worklist.end());
            }

        }

    }

    compute_overall_sensitivity_of_instructions(func, virtual_variables);
}