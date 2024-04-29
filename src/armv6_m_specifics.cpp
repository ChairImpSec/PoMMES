#include "armv6_m_specifics.hpp"

extern uint32_t v_register_ctr;

void insert_epilog_armv6m(std::vector<TAC_type>& TAC, std::set<uint32_t>& non_scratch_register_used, uint32_t bytes_to_reserve, std::vector<stack_local_variable_tracking_struct>& stack_mapping_tracker){
    //3.
    non_scratch_register_used.erase(11); //remove r11 as this can not be used in combination with push
    non_scratch_register_used.erase(9); //remove r11 as this can not be used in combination with push
    uint32_t low_stack_address_register;
    uint32_t register_used_for_stack_clear = *non_scratch_register_used.begin();

    TAC_type tmp_tac;
    tmp_tac.op = Operation::MOV;
    tmp_tac.sensitivity = 0;
    
    tmp_tac.dest_sensitivity = 0;
    tmp_tac.dest_type = TAC_OPERAND::R_REG;
    tmp_tac.dest_v_reg_idx = register_used_for_stack_clear;
    low_stack_address_register = tmp_tac.dest_v_reg_idx;

    tmp_tac.src1_sensitivity = 0;
    tmp_tac.src1_type = TAC_OPERAND::R_REG;
    tmp_tac.src1_v = 13;

    tmp_tac.src2_sensitivity = 0;
    tmp_tac.src2_type = TAC_OPERAND::NONE;
    tmp_tac.src2_v = UINT32_MAX;

    TAC.push_back(tmp_tac);
    

    uint32_t stack_elem_idx = 0;
    uint32_t curr_stack_position = 0;
    
    for(auto& stack_elem: stack_mapping_tracker){ //check each 4 byte word of stack
        if((stack_elem.sensitivity == 1) && (stack_elem.position != 0)){

            if((stack_elem.position <= 124) && (stack_elem.position % 4 == 0)){
                tmp_tac.op = Operation::STOREWORD_WITH_NUMBER_OFFSET;
                tmp_tac.sensitivity = 0;
                
                tmp_tac.dest_sensitivity = 0;
                tmp_tac.dest_type = TAC_OPERAND::NUM;
                tmp_tac.dest_v_reg_idx = stack_elem.position;


                tmp_tac.src1_sensitivity = 0;
                tmp_tac.src1_type = TAC_OPERAND::R_REG;
                tmp_tac.src1_v = register_used_for_stack_clear;

                tmp_tac.src2_sensitivity = 0;
                tmp_tac.src2_type = TAC_OPERAND::R_REG;
                tmp_tac.src2_v = register_used_for_stack_clear;
                tmp_tac.psr_status_flag = false;

                TAC.push_back(tmp_tac);
            }
            else{
                if(stack_elem_idx == 0){
                    //add initial offset to
                    if(stack_elem.position < 256){
                        tmp_tac.op = Operation::MOV;
                        tmp_tac.sensitivity = 0;
                        
                        tmp_tac.dest_sensitivity = 0;
                        tmp_tac.dest_type = TAC_OPERAND::R_REG;
                        tmp_tac.dest_v_reg_idx = register_used_for_stack_clear;
                        low_stack_address_register = tmp_tac.dest_v_reg_idx;

                        tmp_tac.src1_sensitivity = 0;
                        tmp_tac.src1_type = TAC_OPERAND::NUM;
                        tmp_tac.src1_v = stack_elem.position;

                        tmp_tac.src2_sensitivity = 0;
                        tmp_tac.src2_type = TAC_OPERAND::NONE;
                        tmp_tac.src2_v = UINT32_MAX;
                        tmp_tac.psr_status_flag = true;

                        TAC.push_back(tmp_tac);
                    } 
                    else{
                        tmp_tac.op = Operation::LOADWORD;
                        tmp_tac.sensitivity = 0;
                        
                        tmp_tac.dest_sensitivity = 0;
                        tmp_tac.dest_type = TAC_OPERAND::R_REG;
                        tmp_tac.dest_v_reg_idx = register_used_for_stack_clear;
                        low_stack_address_register = tmp_tac.dest_v_reg_idx;

                        tmp_tac.src1_sensitivity = 0;
                        tmp_tac.src1_type = TAC_OPERAND::NUM;
                        tmp_tac.src1_v = stack_elem.position;

                        tmp_tac.src2_sensitivity = 0;
                        tmp_tac.src2_type = TAC_OPERAND::NONE;
                        tmp_tac.src2_v = UINT32_MAX;
                        tmp_tac.psr_status_flag = false;

                        TAC.push_back(tmp_tac);
                    }

                    tmp_tac.op = Operation::ADD;
                    tmp_tac.sensitivity = 0;
                        
                    tmp_tac.dest_sensitivity = 0;
                    tmp_tac.dest_type = TAC_OPERAND::R_REG;
                    tmp_tac.dest_v_reg_idx = TAC.back().dest_v_reg_idx;

                    tmp_tac.src1_sensitivity = 0;
                    tmp_tac.src1_type = TAC_OPERAND::R_REG;
                    tmp_tac.src1_v = TAC.back().dest_v_reg_idx;

                    tmp_tac.src2_sensitivity = 0;
                    tmp_tac.src2_type = TAC_OPERAND::R_REG;
                    tmp_tac.src2_v = 13;
                    tmp_tac.psr_status_flag = false;

                    TAC.push_back(tmp_tac);
                
                    curr_stack_position = stack_elem.position;
                }
                else{ //add offset between current and next position to register
                    uint32_t offset_between_stack_positions = stack_elem.position - curr_stack_position;
                    if(offset_between_stack_positions < 256){
                        tmp_tac.op = Operation::ADD;
                        tmp_tac.sensitivity = 0;
                            
                        tmp_tac.dest_sensitivity = 0;
                        tmp_tac.dest_type = TAC_OPERAND::R_REG;
                        tmp_tac.dest_v_reg_idx = register_used_for_stack_clear;

                        tmp_tac.src1_sensitivity = 0;
                        tmp_tac.src1_type = TAC_OPERAND::R_REG;
                        tmp_tac.src1_v = register_used_for_stack_clear;

                        tmp_tac.src2_sensitivity = 0;
                        tmp_tac.src2_type = TAC_OPERAND::NUM;
                        tmp_tac.src2_v = offset_between_stack_positions;
                        tmp_tac.psr_status_flag = true;

                        TAC.push_back(tmp_tac);
                    }
                    else{
                        tmp_tac.op = Operation::LOADWORD;
                        tmp_tac.sensitivity = 0;
                        
                        tmp_tac.dest_sensitivity = 0;
                        tmp_tac.dest_type = TAC_OPERAND::R_REG;
                        tmp_tac.dest_v_reg_idx = register_used_for_stack_clear;
                        low_stack_address_register = tmp_tac.dest_v_reg_idx;

                        tmp_tac.src1_sensitivity = 0;
                        tmp_tac.src1_type = TAC_OPERAND::NUM;
                        tmp_tac.src1_v = stack_elem.position;

                        tmp_tac.src2_sensitivity = 0;
                        tmp_tac.src2_type = TAC_OPERAND::NONE;
                        tmp_tac.src2_v = UINT32_MAX;
                        tmp_tac.psr_status_flag = false;

                        TAC.push_back(tmp_tac); 

                        tmp_tac.op = Operation::ADD;
                        tmp_tac.sensitivity = 0;
                            
                        tmp_tac.dest_sensitivity = 0;
                        tmp_tac.dest_type = TAC_OPERAND::R_REG;
                        tmp_tac.dest_v_reg_idx = TAC.back().dest_v_reg_idx;

                        tmp_tac.src1_sensitivity = 0;
                        tmp_tac.src1_type = TAC_OPERAND::R_REG;
                        tmp_tac.src1_v = TAC.back().dest_v_reg_idx;

                        tmp_tac.src2_sensitivity = 0;
                        tmp_tac.src2_type = TAC_OPERAND::R_REG;
                        tmp_tac.src2_v = 13;
                        tmp_tac.psr_status_flag = false;

                        TAC.push_back(tmp_tac);

                    }
                    curr_stack_position = stack_elem.position;


                }

                TAC_type store_tac;
                store_tac.op = Operation::STOREWORD;
                store_tac.sensitivity = 0;

                store_tac.src1_sensitivity = 0;
                store_tac.src1_type = TAC_OPERAND::R_REG;
                store_tac.src1_v = TAC.back().dest_v_reg_idx;

                store_tac.src2_sensitivity = 0;
                store_tac.src2_type = TAC_OPERAND::R_REG;
                store_tac.src2_v = TAC.back().dest_v_reg_idx;

                store_tac.psr_status_flag = false;

                TAC.push_back(store_tac);
            }

        }
        stack_elem_idx++;
    }


    if(bytes_to_reserve > 127){

        //move r11 to register that will contain the new value
        Operation op = Operation::MOV;
        uint8_t sens = 0; 
        TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
        uint32_t src2_v = UINT32_MAX;
        uint8_t src2_sens = 0; 
        TAC_OPERAND src1_type = TAC_OPERAND::NUM; 
        uint32_t src1_v = bytes_to_reserve; 
        uint8_t src1_sens = 0; 
        TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
        uint32_t dest_v = low_stack_address_register;
        uint8_t dest_sens = 0;

        if(bytes_to_reserve > 255){
            op = Operation::LOADWORD;
            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
            TAC.push_back(tmp_tac);
        }
        else{
            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, true);
            TAC.push_back(tmp_tac);
        }



        //move r11 to register that will contain the new value
        op = Operation::ADD;
        sens = 0; 
        src2_type = TAC_OPERAND::R_REG; 
        src2_v = low_stack_address_register;
        src2_sens = 0; 
        src1_type = TAC_OPERAND::R_REG; 
        src1_v = 13; 
        src1_sens = 0; 
        dest_type = TAC_OPERAND::R_REG; 
        dest_v = 13;
        dest_sens = 0;

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        TAC.push_back(tmp_tac);

    }
    else{

        //move r11 to register that will contain the new value
        Operation op = Operation::ADD;
        uint8_t sens = 0; 
        TAC_OPERAND src2_type = TAC_OPERAND::NUM; 
        uint32_t src2_v = bytes_to_reserve;
        uint8_t src2_sens = 0; 
        TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
        uint32_t src1_v = 13; 
        uint8_t src1_sens = 0; 
        TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
        uint32_t dest_v = 13;
        uint8_t dest_sens = 0;

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);
        TAC.push_back(tmp_tac);
    }

    TAC_type restore_fp_tac;
    restore_fp_tac.sensitivity = 0;
    restore_fp_tac.op = Operation::POP;

    restore_fp_tac.src1_sensitivity = 0;
    restore_fp_tac.src1_type = TAC_OPERAND::R_REG;
    restore_fp_tac.src1_v = 4; 

    restore_fp_tac.src2_sensitivity = 0;
    restore_fp_tac.src2_type = TAC_OPERAND::NONE;
    restore_fp_tac.src2_v = UINT32_MAX;

    restore_fp_tac.dest_sensitivity = 0;
    restore_fp_tac.dest_type = TAC_OPERAND::NONE;
    restore_fp_tac.dest_v_reg_idx = UINT32_MAX;

    TAC.push_back(restore_fp_tac);

    restore_fp_tac.sensitivity = 0;
    restore_fp_tac.op = Operation::MOV;

    restore_fp_tac.src1_sensitivity = 0;
    restore_fp_tac.src1_type = TAC_OPERAND::R_REG;
    restore_fp_tac.src1_v = 4; 

    restore_fp_tac.src2_sensitivity = 0;
    restore_fp_tac.src2_type = TAC_OPERAND::NONE;
    restore_fp_tac.src2_v = UINT32_MAX;

    restore_fp_tac.dest_sensitivity = 0;
    restore_fp_tac.dest_type = TAC_OPERAND::R_REG;
    restore_fp_tac.dest_v_reg_idx = 11;

    TAC.push_back(restore_fp_tac);

    //2.
    TAC_type restore_tac;
    restore_tac.op = Operation::EPILOG_POP;
    restore_tac.sensitivity = 0;
    std::copy(non_scratch_register_used.begin(), non_scratch_register_used.end(), std::back_inserter(restore_tac.epilog_register_list));
    // restore_tac.epilog_register_list.emplace_back(11);
    restore_tac.epilog_register_list.emplace_back(15); //save additionally link register
    restore_tac.src1_type = TAC_OPERAND::NONE;
    restore_tac.src2_type = TAC_OPERAND::NONE;
    restore_tac.dest_type = TAC_OPERAND::NONE;
    TAC.push_back(restore_tac);
}


bool armv6_m_xor_specification_is_empty(const std::vector<std::tuple<uint32_t, uint32_t>>& armv6_m_specification){
    return armv6_m_specification.empty();
}

bool virtual_register_at_src_TAC_instruction_has_armv6_m_xor_specification(TAC_type& TAC_instr, uint32_t instruction_idx, uint32_t& virtual_register, const std::vector<std::tuple<uint32_t, uint32_t>>& armv6_m_specification){

    if((TAC_instr.src1_v == virtual_register) && (std::find_if(armv6_m_specification.begin(), armv6_m_specification.end(), [instruction_idx](const std::tuple<uint32_t, uint32_t>& t){return instruction_idx == std::get<1>(t);}) != armv6_m_specification.end())){
        return true;
    }
    return false;

}

bool armv6_m_xor_specification_contains_instruction(const std::vector<std::tuple<uint32_t, uint32_t>>& armv6_m_specification, uint32_t intersection_point){
    if(std::find_if(armv6_m_specification.begin(), armv6_m_specification.end(), [intersection_point](const std::tuple<uint32_t, uint32_t>& t){return std::get<1>(t) == intersection_point;}) != armv6_m_specification.end()){
        return true;
    }
    return false;
}


void insert_prolog_armv6m(std::vector<TAC_type>& TAC, uint32_t& bytes_to_reserve, std::set<uint32_t>& non_scratch_register_used){
    //1. 
    TAC_type save_tac;
    save_tac.op = Operation::PROLOG_PUSH;
    save_tac.sensitivity = 0;
    non_scratch_register_used.erase(11); //remove r11 as this can not be used in combination with push
    non_scratch_register_used.erase(9); //remove r9 as this can not be used in combination with push
    non_scratch_register_used.insert(4);
    std::copy(non_scratch_register_used.begin(), non_scratch_register_used.end(), std::back_inserter(save_tac.prolog_register_list));
    save_tac.prolog_register_list.emplace_back(14); //save additionally link register
    save_tac.src1_type = TAC_OPERAND::NONE;
    save_tac.src2_type = TAC_OPERAND::NONE;
    save_tac.dest_type = TAC_OPERAND::NONE;
    TAC.insert(TAC.begin(), save_tac);


    TAC_type save_fp_tac;
    save_fp_tac.sensitivity = 0;
    save_fp_tac.op = Operation::MOV;
    save_fp_tac.src1_type = TAC_OPERAND::R_REG;
    save_fp_tac.src1_sensitivity = 0;
    save_fp_tac.src1_v = 11;

    save_fp_tac.src2_type = TAC_OPERAND::NONE;
    save_fp_tac.src2_sensitivity = 0;
    save_fp_tac.src2_v = UINT32_MAX;

    save_fp_tac.dest_sensitivity = 0;
    save_fp_tac.dest_type = TAC_OPERAND::R_REG;
    save_fp_tac.dest_v_reg_idx = 4;
    TAC.insert(TAC.begin() + 1, save_fp_tac);

    //push register on stack
    save_fp_tac.sensitivity = 0;
    save_fp_tac.op = Operation::PUSH;
    save_fp_tac.src1_type = TAC_OPERAND::R_REG;
    save_fp_tac.src1_sensitivity = 0;
    save_fp_tac.src1_v = 4;

    save_fp_tac.src2_type = TAC_OPERAND::NONE;
    save_fp_tac.src2_sensitivity = 0;
    save_fp_tac.src2_v = UINT32_MAX;

    save_fp_tac.dest_sensitivity = 0;
    save_fp_tac.dest_type = TAC_OPERAND::NONE;
    save_fp_tac.dest_v_reg_idx = UINT32_MAX;
    TAC.insert(TAC.begin() + 2, save_fp_tac);

    uint32_t number_of_non_scratch_register_used = save_tac.prolog_register_list.size();
    //2.
    uint32_t m = 4 * (number_of_non_scratch_register_used);

    //3.
    uint32_t n = bytes_to_reserve + 4 + 4; //extra four byte reservation for a place on the stack that does not contain sensitive information -> used for memory_shadow resolve; extra four bytes for additional push of old 11
    if(((m + n + 4) % 8) != 0){
        n += ((m + n + 4) % 8);
        
    }
    bytes_to_reserve = n;

    if(n > 508){
        TAC_type move_sp_tac;
        move_sp_tac.sensitivity = 0;
        move_sp_tac.op = Operation::MOV;
        move_sp_tac.src1_type = TAC_OPERAND::R_REG;
        move_sp_tac.src1_sensitivity = 0;
        move_sp_tac.src1_v = 13;

        move_sp_tac.src2_type = TAC_OPERAND::NONE;
        move_sp_tac.src2_sensitivity = 0;
        move_sp_tac.src2_v = UINT32_MAX;

        move_sp_tac.dest_sensitivity = 0;
        move_sp_tac.dest_type = TAC_OPERAND::R_REG;
        move_sp_tac.dest_v_reg_idx = 4;
        move_sp_tac.psr_status_flag = false;

        TAC.insert(TAC.begin() + 3, move_sp_tac);

        TAC_type load_imm_tac;
        load_imm_tac.sensitivity = 0;
        load_imm_tac.op = Operation::LOADWORD;
        load_imm_tac.src1_type = TAC_OPERAND::NUM;
        load_imm_tac.src1_sensitivity = 0;
        load_imm_tac.src1_v = n;

        load_imm_tac.src2_type = TAC_OPERAND::NONE;
        load_imm_tac.src2_sensitivity = 0;
        load_imm_tac.src2_v = UINT32_MAX;

        load_imm_tac.dest_sensitivity = 0;
        load_imm_tac.dest_type = TAC_OPERAND::R_REG;
        load_imm_tac.dest_v_reg_idx = 5;
        load_imm_tac.psr_status_flag = false;

        TAC.insert(TAC.begin() + 4, load_imm_tac);

        TAC_type sub_tac;
        sub_tac.sensitivity = 0;
        sub_tac.op = Operation::SUB;
        sub_tac.src1_type = TAC_OPERAND::R_REG;
        sub_tac.src1_sensitivity = 0;
        sub_tac.src1_v = 4;

        sub_tac.src2_type = TAC_OPERAND::R_REG;
        sub_tac.src2_sensitivity = 0;
        sub_tac.src2_v = 5;

        sub_tac.dest_sensitivity = 0;
        sub_tac.dest_type = TAC_OPERAND::R_REG;
        sub_tac.dest_v_reg_idx = 4;
        sub_tac.psr_status_flag = true;

        TAC.insert(TAC.begin() + 5, sub_tac);


        move_sp_tac.sensitivity = 0;
        move_sp_tac.op = Operation::MOV;
        move_sp_tac.src1_type = TAC_OPERAND::R_REG;
        move_sp_tac.src1_sensitivity = 0;
        move_sp_tac.src1_v = 4;

        move_sp_tac.src2_type = TAC_OPERAND::NONE;
        move_sp_tac.src2_sensitivity = 0;
        move_sp_tac.src2_v = UINT32_MAX;

        move_sp_tac.dest_sensitivity = 0;
        move_sp_tac.dest_type = TAC_OPERAND::R_REG;
        move_sp_tac.dest_v_reg_idx = 13;
        move_sp_tac.psr_status_flag = false;

        TAC.insert(TAC.begin() + 6, move_sp_tac);

        move_sp_tac.sensitivity = 0;
        move_sp_tac.op = Operation::MOV;
        move_sp_tac.src1_type = TAC_OPERAND::R_REG;
        move_sp_tac.src1_sensitivity = 0;
        move_sp_tac.src1_v = 13;

        move_sp_tac.src2_type = TAC_OPERAND::NONE;
        move_sp_tac.src2_sensitivity = 0;
        move_sp_tac.src2_v = UINT32_MAX;

        move_sp_tac.dest_sensitivity = 0;
        move_sp_tac.dest_type = TAC_OPERAND::R_REG;
        move_sp_tac.dest_v_reg_idx = 11;
        move_sp_tac.psr_status_flag = false;

        TAC.insert(TAC.begin() + 7, move_sp_tac);
    }
    else{
        TAC_type sp_tac;
        sp_tac.sensitivity = 0;
        sp_tac.op = Operation::SUB;
        sp_tac.src1_type = TAC_OPERAND::R_REG;
        sp_tac.src1_sensitivity = 0;
        sp_tac.src1_v = 13;

        sp_tac.src2_type = TAC_OPERAND::NUM;
        sp_tac.src2_sensitivity = 0;
        sp_tac.src2_v = n;

        sp_tac.dest_sensitivity = 0;
        sp_tac.dest_type = TAC_OPERAND::R_REG;
        sp_tac.dest_v_reg_idx = 13;
        sp_tac.psr_status_flag = false;

        TAC.insert(TAC.begin() + 3, sp_tac);

        TAC_type fp_tac;
        fp_tac.sensitivity = 0;
        fp_tac.op = Operation::MOV;
        fp_tac.src1_type = TAC_OPERAND::R_REG;
        fp_tac.src1_sensitivity = 0;
        fp_tac.src1_v = 13;

        fp_tac.src2_type = TAC_OPERAND::NONE;
        fp_tac.src2_sensitivity = 0;
        fp_tac.src2_v = UINT32_MAX;

        fp_tac.dest_sensitivity = 0;
        fp_tac.dest_type = TAC_OPERAND::R_REG;
        fp_tac.dest_v_reg_idx = 11;
        TAC.insert(TAC.begin() + 4, fp_tac);
    }



}

void and_specification_armv6m(std::vector<TAC_type>& TAC, TAC_OPERAND& src2_type, uint32_t& src2_v){
    
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
        bool load_psr_status_flag = false;

        generate_generic_tmp_tac(load_tac, load_op, load_sens, load_dest_type, load_dest_v, load_dest_sens, load_src1_type, load_src1_v, load_src1_sens, load_src2_type, load_src2_v, load_src2_sens, load_psr_status_flag);
        TAC.push_back(load_tac);

        src2_v = load_dest_v;
        src2_type = TAC_OPERAND::V_REG;
    }
    else{
        TAC_type mov_tac;

        //move r11 to register that will contain the new value
        Operation mov_op = Operation::MOV;
        uint8_t mov_sens = 0; 
        TAC_OPERAND mov_src2_type = TAC_OPERAND::NONE; 
        uint32_t mov_src2_v = UINT32_MAX;
        uint8_t mov_src2_sens = 0; 
        TAC_OPERAND mov_src1_type = TAC_OPERAND::NUM; 
        uint32_t mov_src1_v = src2_v; 
        uint8_t mov_src1_sens = 0; 
        TAC_OPERAND mov_dest_type = TAC_OPERAND::V_REG; 
        uint32_t mov_dest_v = v_register_ctr++;
        uint8_t mov_dest_sens = 0;
        bool mov_psr_status_flag = true;

        generate_generic_tmp_tac(mov_tac, mov_op, mov_sens, mov_dest_type, mov_dest_v, mov_dest_sens, mov_src1_type, mov_src1_v, mov_src1_sens, mov_src2_type, mov_src2_v, mov_src2_sens, mov_psr_status_flag);
        TAC.push_back(mov_tac);

        src2_v = mov_dest_v;
        src2_type = TAC_OPERAND::V_REG;
    }

}

void move_specification_armv6m(Operation& op, uint32_t src, bool& psr_status_flag){
    if(src > 255){
        op = Operation::LOADWORD;
        psr_status_flag = false;
    }
    else{
        psr_status_flag = true;
    }
}

uint8_t add_specification_armv6m(uint32_t src2, bool& add_psr_status_flag, TAC_type& load_number_to_reg_tac){
    

    
    if(src2 > 255){


        Operation load_number_to_reg_op = Operation::LOADWORD;
        uint8_t load_number_to_reg_sens = 0; 
        TAC_OPERAND load_number_to_reg_src2_type = TAC_OPERAND::NONE; 
        uint32_t load_number_to_reg_src2_v = UINT32_MAX;
        uint8_t load_number_to_reg_src2_sens = 0; 
        TAC_OPERAND load_number_to_reg_src1_type = TAC_OPERAND::NUM; 
        uint32_t load_number_to_reg_src1_v = src2; //the real register that will be spilled has to be stored in memory
        uint8_t load_number_to_reg_src1_sens = 0; 
        TAC_OPERAND load_number_to_reg_dest_type = TAC_OPERAND::V_REG; 
        uint32_t load_number_to_reg_dest_v = v_register_ctr++;
        uint8_t load_number_to_reg_dest_sens = 0;
        bool load_number_to_reg_psr_status_flag = false;
        add_psr_status_flag = false;

        generate_generic_tmp_tac(load_number_to_reg_tac, load_number_to_reg_op, load_number_to_reg_sens, load_number_to_reg_dest_type, load_number_to_reg_dest_v, load_number_to_reg_dest_sens, load_number_to_reg_src1_type, load_number_to_reg_src1_v, load_number_to_reg_src1_sens, load_number_to_reg_src2_type, load_number_to_reg_src2_v, load_number_to_reg_src2_sens, load_number_to_reg_psr_status_flag);


        return 1;

    }
    else{
        add_psr_status_flag = true;
    
        return 0;
    }
}

void add_t1_encoding_specification_armv6m(std::vector<TAC_type>& TAC, uint32_t dest, uint32_t src1, uint32_t src2){
    std::cout << "add_t1_encoding_specification_armv6m start" << std::endl;
    if(dest == src1){
        return;
    }

    if(src2 <= 7){
        return;
    }

    std::cout << "[add_t1_encoding_specification_armv6m] src2 = " << src2 << std::endl;
    uint32_t number_to_add = src2;
    TAC.back().src2_v = 4;
    number_to_add -= 4;

    //divide into chunks of 4
    uint32_t add_iterations = static_cast<uint32_t>(std::ceil(src2/4.0f)) - 1;
    std::cout << "[add_t1_encoding_specification_armv6m] add_iterations = " << add_iterations << std::endl;

    
    for(uint32_t i = 0; i < add_iterations; ++i){

        TAC_type tmp_tac;
        generate_generic_tmp_tac(tmp_tac, Operation::ADD, 0, TAC_OPERAND::V_REG, TAC.back().dest_v_reg_idx, TAC.back().dest_sensitivity, TAC.back().dest_type, TAC.back().dest_v_reg_idx, TAC.back().dest_sensitivity, TAC_OPERAND::NUM, number_to_add, 0, true);
        TAC.emplace_back(tmp_tac);

        if(number_to_add >= 4){
            number_to_add -= 4;
        }
    }

}



uint8_t insert_load_armv6m(uint32_t in_dest, uint8_t in_dest_sens, uint8_t in_overall_sens, uint32_t in_src2, std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC, uint32_t instruction_idx_to_insert, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers,  std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m,  std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion){

    //1. load number/offset into register
    if(in_src2 > 255){
        TAC_type tmp_tac;

        //move r11 to register that will contain the new value
        Operation op = Operation::LOADWORD;
        uint8_t sens = 0; 
        TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
        uint32_t src2_v = UINT32_MAX;
        uint8_t src2_sens = 0; 
        TAC_OPERAND src1_type = TAC_OPERAND::NUM; 
        uint32_t src1_v = in_src2; 
        uint8_t src1_sens = 0; 
        TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
        uint32_t dest_v = in_dest;
        uint8_t dest_sens = 0;

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

        TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
        insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

        synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);
    }
    else{
        TAC_type tmp_tac;

        //move r11 to register that will contain the new value
        Operation op = Operation::MOV;
        uint8_t sens = 0; 
        TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
        uint32_t src2_v = UINT32_MAX;
        uint8_t src2_sens = 0; 
        TAC_OPERAND src1_type = TAC_OPERAND::NUM; 
        uint32_t src1_v = in_src2; 
        uint8_t src1_sens = 0; 
        TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
        uint32_t dest_v = in_dest;
        uint8_t dest_sens = 0;

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, true);

        TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
        insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

        synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);
    }

    //2. add r11 to it

    TAC_type tmp_tac;

    //move r11 to register that will contain the new value
    Operation op = Operation::ADD;
    uint8_t sens = 0; 
    TAC_OPERAND src2_type = TAC_OPERAND::R_REG; 
    uint32_t src2_v = 11;
    uint8_t src2_sens = 0; 
    TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
    uint32_t src1_v = in_dest; 
    uint8_t src1_sens = 0; 
    TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
    uint32_t dest_v = in_dest;
    uint8_t dest_sens = 0;

    generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

    TAC.insert(TAC.begin() + instruction_idx_to_insert + 1, tmp_tac);
    insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert + 1, tmp_tac);

    synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert + 1, track_xor_specification_armv6m, postponed_store_insertion);

    //load the actual value
    op = Operation::LOADWORD;
    sens = in_overall_sens; 
    src2_type = TAC_OPERAND::NONE; 
    src2_v = UINT32_MAX;
    src2_sens = 0; 
    src1_type = TAC_OPERAND::R_REG; 
    src1_v = in_dest;
    src1_sens = 0; 
    dest_type = TAC_OPERAND::R_REG; 
    dest_v = in_dest;
    dest_sens = in_dest_sens;

    generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

    TAC.insert(TAC.begin() + instruction_idx_to_insert + 2, tmp_tac);
    insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert + 2, tmp_tac);

    synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert + 2, track_xor_specification_armv6m, postponed_store_insertion);


    return 2;

}


void insert_store_armv6m(uint32_t in_dest, uint8_t in_dest_sens, uint8_t in_overall_sens, uint32_t in_src1, uint8_t in_src1_sens, std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC, uint32_t instruction_idx_to_insert, uint32_t start_range_of_latest_interval, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers, std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m, uint32_t index_of_interval_to_store, std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion, const std::map<uint32_t, std::vector<uint32_t>>& predecessors){

    // uint8_t load_number_into_register_overhead = 0;
    //check if initial position of store is less than instruction index of current interval -> if yes we can directly assign a register to it
    if(instruction_idx_to_insert < start_range_of_latest_interval){

        //get interval that starts just before store instruction insertion
        auto rev_it_last_interval_before_store = std::find_if(liveness_intervals.rbegin(), liveness_intervals.rend(), [instruction_idx_to_insert](const liveness_interval_struct& t){return std::get<0>(t.ranges.back()) <= instruction_idx_to_insert;});
        if(rev_it_last_interval_before_store == liveness_intervals.rend()){
            throw std::runtime_error("in insert_store_armv6m: can not find interval that starts before store insertion");
        }
        auto it_last_interval_before_store = (rev_it_last_interval_before_store+1).base();


        //we can take any available register from this interval as the next interval in liveness intervals comes by construction after store instruction index and thus can not interfere with the store instructions
        if((!it_last_interval_before_store->available_registers_after_own_allocation.empty()) && (!((it_last_interval_before_store->available_registers_after_own_allocation.size() == 1) && (it_last_interval_before_store->available_registers_after_own_allocation.at(0) == in_src1)))){



            uint32_t available_register_during_store = *(std::find_if(it_last_interval_before_store->available_registers_after_own_allocation.begin(), it_last_interval_before_store->available_registers_after_own_allocation.end(), [in_src1](const uint32_t& t){return t != in_src1;}));//it_last_interval_before_store->available_registers_after_own_allocation.at(0);

            if(in_dest > 255){
                TAC_type tmp_tac;

                //move r11 to register that will contain the new value
                Operation op = Operation::LOADWORD;
                uint8_t sens = 0; 
                TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
                uint32_t src2_v = UINT32_MAX;
                uint8_t src2_sens = 0; 
                TAC_OPERAND src1_type = TAC_OPERAND::NUM; 
                uint32_t src1_v = in_dest; 
                uint8_t src1_sens = 0; 
                TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
                uint32_t dest_v = available_register_during_store;
                uint8_t dest_sens = 0;

                generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

                TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
                insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

                synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);
            }
            else{
                TAC_type tmp_tac;

                //move r11 to register that will contain the new value
                Operation op = Operation::MOV;
                uint8_t sens = 0; 
                TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
                uint32_t src2_v = UINT32_MAX;
                uint8_t src2_sens = 0; 
                TAC_OPERAND src1_type = TAC_OPERAND::NUM; 
                uint32_t src1_v = in_dest; 
                uint8_t src1_sens = 0; 
                TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
                uint32_t dest_v = available_register_during_store;
                uint8_t dest_sens = 0;

                generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, true);

                TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
                insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

                synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);
            }


            instruction_idx_to_insert += 1;

            TAC_type tmp_tac;

            //move r11 to register that will contain the new value
            Operation op = Operation::ADD;
            uint8_t sens = 0; 
            TAC_OPERAND src2_type = TAC_OPERAND::R_REG; 
            uint32_t src2_v = 11;
            uint8_t src2_sens = 0; 
            TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
            uint32_t src1_v = available_register_during_store; 
            uint8_t src1_sens = 0; 
            TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
            uint32_t dest_v = available_register_during_store;
            uint8_t dest_sens = 0;

            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

            TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
            insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

            synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);


            // synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);
            instruction_idx_to_insert += 1;

            //perform actual store
            op = Operation::STOREWORD;
            sens = in_overall_sens; 
            src2_type = TAC_OPERAND::R_REG; 
            src2_v = available_register_during_store;
            src2_sens = in_dest_sens; 
            src1_type = TAC_OPERAND::R_REG; 
            src1_v = in_src1;
            src1_sens = in_src1_sens; 
            dest_type = TAC_OPERAND::NONE; 
            dest_v = UINT32_MAX;
            dest_sens = 0;

            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

            TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
            insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

            synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);

            // instruction_idx_to_insert += 1;


            //change sensitivity of available if last interval before store is directly before interval that was assigned a real register last
            auto rev_it_last_interval_with_real_register = std::find_if(liveness_intervals.rbegin(), liveness_intervals.rend(), [](const liveness_interval_struct& t){return t.real_register_number != UINT32_MAX;});
            if((rev_it_last_interval_before_store - liveness_intervals.rbegin()) == ((rev_it_last_interval_with_real_register - liveness_intervals.rbegin()) - 1)){
                auto it_available_register = std::find_if(available_registers.begin(), available_registers.end(), [available_register_during_store](const std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>& t){return std::get<0>(t) == available_register_during_store;});
                if(it_available_register != available_registers.end()){
                    std::get<1>(*it_available_register) = 0;
                }
            }


        }
        else{ //pick register from non-sensitive interval in active during time when to insert store instruction
            auto it_active_non_sensitive_interval_during_store = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [instruction_idx_to_insert, in_src1](const liveness_interval_struct& t){return (t.sensitivity == 0) && (std::get<0>(t.ranges.back()) <= instruction_idx_to_insert) && (std::get<1>(t.ranges.back()) >= instruction_idx_to_insert) && (t.real_register_number != in_src1);});
            uint32_t non_sensitive_register_during_store = it_active_non_sensitive_interval_during_store->real_register_number;
        
            //if no register is available -> move a non-sensitive register from the current active register to the high register r9

            TAC_type tmp_tac;

            Operation op = Operation::MOV;
            uint8_t sens = 0; 
            TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
            uint32_t src2_v = UINT32_MAX;
            uint8_t src2_sens = 0; 
            TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
            uint32_t src1_v = non_sensitive_register_during_store; //the real register that will be spilled has to be stored in memory
            uint8_t src1_sens = 0; 
            TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
            uint32_t dest_v = 9;
            uint8_t dest_sens = 0;

            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);


            TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
            insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

            synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);

            instruction_idx_to_insert += 1;



            if(in_dest > 255){

                //move r11 to register that will contain the new value
                op = Operation::LOADWORD;
                sens = 0; 
                src2_type = TAC_OPERAND::NONE; 
                src2_v = UINT32_MAX;
                src2_sens = 0; 
                src1_type = TAC_OPERAND::NUM; 
                src1_v = in_dest; 
                src1_sens = 0; 
                dest_type = TAC_OPERAND::R_REG; 
                dest_v = non_sensitive_register_during_store;
                dest_sens = 0;

                generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

                TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
                insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

                synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);
            }
            else{

                //move r11 to register that will contain the new value
                op = Operation::MOV;
                sens = 0; 
                src2_type = TAC_OPERAND::NONE; 
                src2_v = UINT32_MAX;
                src2_sens = 0; 
                src1_type = TAC_OPERAND::NUM; 
                src1_v = in_dest; 
                src1_sens = 0; 
                dest_type = TAC_OPERAND::R_REG; 
                dest_v = non_sensitive_register_during_store;
                dest_sens = 0;

                generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, true);

                TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
                insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

                synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);
            }

            instruction_idx_to_insert += 1;

            //subtract the offset
            op = Operation::ADD;
            sens = 0; 
            src2_type = TAC_OPERAND::R_REG; 
            src2_v = 11;
            src2_sens = 0; 
            src1_type = TAC_OPERAND::R_REG; 
            src1_v = non_sensitive_register_during_store; //the real register that will be spilled has to be stored in memory
            src1_sens = 0; 
            dest_type = TAC_OPERAND::R_REG; 
            dest_v = non_sensitive_register_during_store;
            dest_sens = 0;
            bool add_psr_status_flag = false;


            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, add_psr_status_flag);

            TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
            insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

            synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);
            instruction_idx_to_insert += 1;

            //perform actual store
            op = Operation::STOREWORD;
            sens = in_overall_sens; 
            src2_type = TAC_OPERAND::R_REG; 
            src2_v = non_sensitive_register_during_store;
            src2_sens = in_dest_sens; 
            src1_type = TAC_OPERAND::R_REG; 
            src1_v = in_src1;
            src1_sens = in_src1_sens; 
            dest_type = TAC_OPERAND::NONE; 
            dest_v = UINT32_MAX;
            dest_sens = 0;

            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

            TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
            insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

            synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);

            instruction_idx_to_insert += 1;
            

            //after store move value from higher register back to lower register 

            op = Operation::MOV;
            sens = 0; 
            src2_type = TAC_OPERAND::NONE; 
            src2_v = UINT32_MAX;
            src2_sens = 0; 
            src1_type = TAC_OPERAND::R_REG; 
            src1_v = 9; //the real register that will be spilled has to be stored in memory
            src1_sens = 0; 
            dest_type = TAC_OPERAND::R_REG; 
            dest_v = non_sensitive_register_during_store;
            dest_sens = 0;

            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

            TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
            insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

            synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);

            // instruction_idx_to_insert += 1;

        }


        liveness_intervals.at(index_of_interval_to_store).liveness_used.emplace_back(instruction_idx_to_insert);

        std::sort(liveness_intervals.at(index_of_interval_to_store).liveness_used.begin(), liveness_intervals.at(index_of_interval_to_store).liveness_used.end());
        liveness_intervals.at(index_of_interval_to_store).liveness_used.erase(std::unique(liveness_intervals.at(index_of_interval_to_store).liveness_used.begin(), liveness_intervals.at(index_of_interval_to_store).liveness_used.end()), liveness_intervals.at(index_of_interval_to_store).liveness_used.end());
        std::get<0>(liveness_intervals.at(index_of_interval_to_store).ranges.back()) = liveness_intervals.at(index_of_interval_to_store).liveness_used.at(0);
        std::get<1>(liveness_intervals.at(index_of_interval_to_store).ranges.back()) = liveness_intervals.at(index_of_interval_to_store).liveness_used.back();

        split_into_bb_ranges_of_intervals(control_flow_TAC, liveness_intervals.at(index_of_interval_to_store), predecessors);
        liveness_intervals.at(index_of_interval_to_store).postponed_store = false;

    }   
    else{ //in this case there could be an interval later that would require the register we use for the address computation -> can not yet assign register to it
        liveness_intervals.at(index_of_interval_to_store).postponed_store = true;
    } 

}


//The destination and first source register of xor operations have to be the same
//We need to ensure that the two intervals during this instruction that are responsible for destination and first source have the same real register
//We check whe two intervals have a same use point if this use point has an xor operations
//we split those intervals just like in break_vertical and mark the intervals, s.t. we create a link between those two to assign both the same real register
void get_intervals_in_line_with_xor_specification(const std::vector<TAC_type>& TAC, std::vector<liveness_interval_struct>& liveness_intervals, memory_security_struct& memory_security){




    uint32_t original_intervals_size = liveness_intervals.size();
    std::vector<uint32_t> indices_of_original_sensitive_intervals;
    std::vector<liveness_interval_struct> all_xor_intervals;

    //get all sensitive intervals in one vector
    for(uint32_t index_of_current_interval = 0; index_of_current_interval < original_intervals_size; ++index_of_current_interval){
        for(uint32_t pair_index_of_current_interval = index_of_current_interval + 1; pair_index_of_current_interval < original_intervals_size; ++pair_index_of_current_interval){
            
            //intervals overlap
            if(std::get<0>(liveness_intervals.at(pair_index_of_current_interval).ranges.back()) <= std::get<1>(liveness_intervals.at(index_of_current_interval).ranges.back())){
                auto use_points_of_curr_interval = liveness_intervals.at(index_of_current_interval).liveness_used;
                auto use_points_of_pair_interval = liveness_intervals.at(pair_index_of_current_interval).liveness_used;

                //check if intervals have common use point -> get intersection
                std::vector<uint32_t> common_use_points;
                std::set_intersection(use_points_of_curr_interval.begin(),use_points_of_curr_interval.end(), use_points_of_pair_interval.begin(),use_points_of_pair_interval.end(), std::back_inserter(common_use_points));

                if(!common_use_points.empty()){
                    for(const auto& common_point: common_use_points){
                        if((TAC.at(common_point).op == Operation::XOR) || (TAC.at(common_point).op == Operation::LOGICAL_AND) || (TAC.at(common_point).op == Operation::MUL) || (TAC.at(common_point).op == Operation::LOGICAL_OR)){
                            if(((static_cast<uint32_t>(TAC.at(common_point).dest_v_reg_idx) == liveness_intervals.at(pair_index_of_current_interval).virtual_register_number) && (TAC.at(common_point).src1_v == liveness_intervals.at(index_of_current_interval).virtual_register_number)) || ((static_cast<uint32_t>(TAC.at(common_point).dest_v_reg_idx) == liveness_intervals.at(index_of_current_interval).virtual_register_number) && (TAC.at(common_point).src1_v == liveness_intervals.at(pair_index_of_current_interval).virtual_register_number))){
                                liveness_interval_struct first_interval = liveness_intervals.at(index_of_current_interval);
                                liveness_interval_struct second_interval = liveness_intervals.at(pair_index_of_current_interval);
                                if(std::find_if(all_xor_intervals.begin(), all_xor_intervals.end(), [first_interval](const liveness_interval_struct& t){return t == first_interval;}) == all_xor_intervals.end()){
                                    //check if interval is already in specification, if yes do not add it again

                                    all_xor_intervals.push_back(first_interval);
                                    indices_of_original_sensitive_intervals.emplace_back(index_of_current_interval);


                                }
                                if(std::find_if(all_xor_intervals.begin(), all_xor_intervals.end(), [second_interval](const liveness_interval_struct& t){return t == second_interval;}) == all_xor_intervals.end()){
                                    //check if interval is already in specification, if yes do not add it again

                                    all_xor_intervals.push_back(second_interval);
                                    indices_of_original_sensitive_intervals.emplace_back(pair_index_of_current_interval);

                                }
                            }
                        }
                    }
                }
            }

        }
    }

    std::vector<liveness_interval_struct> all_xor_intervals_after_splitting;

    //process every sensitive interval
    while(!all_xor_intervals.empty()){
    
        //get one sensitive interval to compare
        liveness_interval_struct first_sensitive_interval = all_xor_intervals.at(0);
        bool no_interference_of_current_interval = true;

        //go over all other intervals to compare with
        for(uint32_t interval_idx = 1; interval_idx < all_xor_intervals.size(); ++interval_idx){
            liveness_interval_struct later_sensivite_interval = all_xor_intervals.at(interval_idx);

            //if the use points of the two intervals to compare overlap
            if((!(first_sensitive_interval.liveness_used.at(0) > later_sensivite_interval.liveness_used.back())) && (!(first_sensitive_interval.liveness_used.back() < later_sensivite_interval.liveness_used.at(0)))){
                auto use_points_of_curr_interval = first_sensitive_interval.liveness_used;
                auto use_points_of_pair_interval = later_sensivite_interval.liveness_used;

                //check if intervals have common use point -> get intersection
                std::vector<uint32_t> common_use_points;
                std::set_intersection(use_points_of_curr_interval.begin(),use_points_of_curr_interval.end(), use_points_of_pair_interval.begin(),use_points_of_pair_interval.end(), std::back_inserter(common_use_points));

                if(!common_use_points.empty()){
                    int32_t xor_intersection_point = -1;
                    for(const auto& common_point: common_use_points){

                        if(((TAC.at(common_point).op == Operation::XOR) || (TAC.at(common_point).op == Operation::LOGICAL_AND) || (TAC.at(common_point).op == Operation::MUL) || (TAC.at(common_point).op == Operation::LOGICAL_OR)) && (((static_cast<uint32_t>(TAC.at(common_point).dest_v_reg_idx) == later_sensivite_interval.virtual_register_number) && (TAC.at(common_point).src1_v == first_sensitive_interval.virtual_register_number)) || ((static_cast<uint32_t>(TAC.at(common_point).dest_v_reg_idx) == first_sensitive_interval.virtual_register_number) && (TAC.at(common_point).src1_v == later_sensivite_interval.virtual_register_number)))){
                            xor_intersection_point = common_point;
                            break;
                        }
                    }

                    if((xor_intersection_point != -1) && (!armv6_m_xor_specification_contains_instruction(first_sensitive_interval.arm6_m_xor_specification, xor_intersection_point)) && (!armv6_m_xor_specification_contains_instruction(later_sensivite_interval.arm6_m_xor_specification, xor_intersection_point))){

                        liveness_interval_struct X;
                        liveness_interval_struct Y;
                        uint32_t reference_value;
                        
                        //bring the intervals in the right order, interval with bigger first use point is X other one is Y
                        if(first_sensitive_interval.liveness_used.at(0) > later_sensivite_interval.liveness_used.at(0)){
                            X = first_sensitive_interval;
                            Y = later_sensivite_interval;
                            reference_value = X.liveness_used.at(0);
                        }
                        else if(first_sensitive_interval.liveness_used.at(0) < later_sensivite_interval.liveness_used.at(0)){
                            Y = first_sensitive_interval;
                            X = later_sensivite_interval;
                            reference_value = X.liveness_used.at(0);
                        }
                        else{
                            //if first use point of both intervals is equal, X will be the interval with the smaller use point that differs
                            std::vector<uint32_t> diff;
                            std::set_difference(first_sensitive_interval.liveness_used.begin(), first_sensitive_interval.liveness_used.end(), later_sensivite_interval.liveness_used.begin(), later_sensivite_interval.liveness_used.end(), std::inserter(diff, diff.begin()));
                            if(diff.empty()){
                            //continue; 
                                reference_value = 0xffffffff;
                            }
                            else{
                                reference_value = diff.at(0);
                            }
                            
                            if(std::find(first_sensitive_interval.liveness_used.begin(), first_sensitive_interval.liveness_used.end(), reference_value) == first_sensitive_interval.liveness_used.end()){
                                X = later_sensivite_interval;
                                Y = first_sensitive_interval;
                            }
                            else{
                                X = first_sensitive_interval;
                                Y = later_sensivite_interval;
                            }
                        }


                        //split the intervals everytime they overlap s.t. only one interval is sensitive at a time
                        while(!(X.liveness_used.empty() && Y.liveness_used.empty())){

                            //find the use point that is bigger than the reference point (i.e. the last processed use point of X)
                            auto it_sub_interval_y_end = std::find_if(Y.liveness_used.begin(), Y.liveness_used.end(), [reference_value](uint32_t& use_point){return use_point > reference_value;});
                            
                            if(it_sub_interval_y_end == Y.liveness_used.end()){
                                reference_value = 0xffffffff;
                            }
                            else{
                                reference_value = *it_sub_interval_y_end;
                            }
                            if(!Y.liveness_used.empty()){
                                //we spill a sensitive interval if it is split, this is the case for Y and it is not already spilled
                                if((Y.is_spilled == false) && (it_sub_interval_y_end != Y.liveness_used.end())){
                                    assign_memory_position(Y, memory_security);
                                }

                                liveness_interval_struct sub_interval_y;
                                sub_interval_y.sensitivity = Y.sensitivity;
                                sub_interval_y.is_spilled = Y.is_spilled;
                                sub_interval_y.virtual_register_number = Y.virtual_register_number;
                                sub_interval_y.real_register_number = UINT32_MAX;
                                sub_interval_y.position_on_stack = Y.position_on_stack;
                                std::copy(Y.liveness_used.begin(), it_sub_interval_y_end, std::back_inserter(sub_interval_y.liveness_used));
                                sub_interval_y.ranges.push_back(std::make_tuple(sub_interval_y.liveness_used.at(0), sub_interval_y.liveness_used.back()));  
                                sub_interval_y.arm6_m_xor_specification = Y.arm6_m_xor_specification;
                                //split_into_bb_ranges_of_intervals(control_flow_TAC, sub_interval_y, predecessors);

                                if(std::find(sub_interval_y.liveness_used.begin(), sub_interval_y.liveness_used.end(), xor_intersection_point) != sub_interval_y.liveness_used.end()){

                                    sub_interval_y.arm6_m_xor_specification.push_back(std::make_tuple(X.virtual_register_number, xor_intersection_point));

                                }  

                                all_xor_intervals.push_back(sub_interval_y);

                                Y.liveness_used.erase(Y.liveness_used.begin(), it_sub_interval_y_end);
                            }

                            
                            auto it_sub_interval_x_end = std::find_if(X.liveness_used.begin(), X.liveness_used.end(), [reference_value](uint32_t& use_point){return use_point > reference_value;});
                            
                            if(it_sub_interval_x_end == X.liveness_used.end()){
                                reference_value = 0xffffffff;
                            }
                            else{
                                reference_value = *it_sub_interval_x_end;
                            }
                            if(!X.liveness_used.empty()){
                                if((X.is_spilled == false) && (it_sub_interval_x_end != X.liveness_used.end())){
                                    assign_memory_position(X, memory_security);
                                }
                                liveness_interval_struct sub_interval_x;
                                sub_interval_x.sensitivity = X.sensitivity;
                                sub_interval_x.is_spilled = X.is_spilled;
                                sub_interval_x.virtual_register_number = X.virtual_register_number;
                                sub_interval_x.real_register_number = UINT32_MAX;
                                sub_interval_x.position_on_stack = X.position_on_stack;
                                std::copy(X.liveness_used.begin(), it_sub_interval_x_end, std::back_inserter(sub_interval_x.liveness_used));
                                sub_interval_x.ranges.push_back(std::make_tuple(sub_interval_x.liveness_used.at(0), sub_interval_x.liveness_used.back()));   
                                sub_interval_x.arm6_m_xor_specification = X.arm6_m_xor_specification;

                                if(std::find(sub_interval_x.liveness_used.begin(), sub_interval_x.liveness_used.end(), xor_intersection_point) != sub_interval_x.liveness_used.end()){

                                    sub_interval_x.arm6_m_xor_specification.push_back(std::make_tuple(Y.virtual_register_number, xor_intersection_point));

                                }  

                                //split_into_bb_ranges_of_intervals(control_flow_TAC, sub_interval_x, predecessors);

                                all_xor_intervals.push_back(sub_interval_x);

                                X.liveness_used.erase(X.liveness_used.begin(), it_sub_interval_x_end);
                            }


                        }

                        all_xor_intervals.erase(all_xor_intervals.begin() + interval_idx);  //remove later_sensivite_interval as it was processed by while loop above
                        all_xor_intervals.erase(all_xor_intervals.begin());                 //remove first_sensitive_interval as it was processed by while loop above
                        

                        no_interference_of_current_interval = false;
                        break;

                    }
                }

            }

        }

        if(no_interference_of_current_interval){
            all_xor_intervals_after_splitting.push_back(first_sensitive_interval);
            all_xor_intervals.erase(all_xor_intervals.begin());
        }
    }


    liveness_intervals.insert(liveness_intervals.end(), all_xor_intervals_after_splitting.begin(), all_xor_intervals_after_splitting.end());

    std::sort(indices_of_original_sensitive_intervals.begin(), indices_of_original_sensitive_intervals.end());

    for(int32_t i = indices_of_original_sensitive_intervals.size() - 1; i >= 0; --i){
        liveness_intervals.erase(liveness_intervals.begin() + indices_of_original_sensitive_intervals.at(i));
    }

}


void available_register_assignment_armv6m(std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC, const std::vector<liveness_interval_struct>::iterator& first_interval_without_real_register_iterator, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers, std::vector<std::tuple<uint32_t, uint32_t, uint8_t>>& allocated_registers, memory_security_struct& memory_security, const std::map<uint32_t, std::vector<uint32_t>>& predecessors, std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m, std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion){
    uint32_t index_of_interval_without_real_register = std::distance(liveness_intervals.begin(), first_interval_without_real_register_iterator);
    uint32_t virt_reg = liveness_intervals.at(index_of_interval_without_real_register).virtual_register_number;




    bool assigned_register_from_active_for_xor_specification = false;
    if(!armv6_m_xor_specification_is_empty(liveness_intervals.at(index_of_interval_without_real_register).arm6_m_xor_specification)){
        //if we encounter an interval that requires the same real register due to to the xor specification restrictions check if it the other interval is in active and if yes assign it the register
        
        for(auto& interval_without_real_register_arm_spec: liveness_intervals.at(index_of_interval_without_real_register).arm6_m_xor_specification){
            uint32_t partner_virtual_register = std::get<0>(interval_without_real_register_arm_spec);
            uint32_t xor_instruction_used = std::get<1>(interval_without_real_register_arm_spec);

            auto it_partner_interval_in_active = std::find_if(active.begin(), active.end(), [partner_virtual_register, xor_instruction_used](const liveness_interval_struct& t){return (t.virtual_register_number == partner_virtual_register) && (std::find(t.liveness_used.begin(), t.liveness_used.end(), xor_instruction_used) != t.liveness_used.end());});
            if(it_partner_interval_in_active != active.end()){
                
                //allocated_registers.push_back(std::make_tuple(liveness_intervals.at(index_of_interval_without_real_register).virtual_register_number, it_partner_interval_in_active->real_register_number, liveness_intervals.at(index_of_interval_without_real_register).sensitivity));
                uint32_t partner_real_register = it_partner_interval_in_active->real_register_number;
                auto it_alloc = std::find_if(allocated_registers.begin(), allocated_registers.end(), [partner_real_register](const std::tuple<uint32_t, uint32_t, uint8_t>& t){return std::get<1>(t) == partner_real_register;});
                std::get<0>(*it_alloc) = liveness_intervals.at(index_of_interval_without_real_register).virtual_register_number;
                std::get<2>(*it_alloc) = liveness_intervals.at(index_of_interval_without_real_register).sensitivity;
                liveness_intervals.at(index_of_interval_without_real_register).real_register_number = it_partner_interval_in_active->real_register_number;
                active.erase(it_partner_interval_in_active);
                assigned_register_from_active_for_xor_specification = true;
                break;
            }
        }
    }

    if(!assigned_register_from_active_for_xor_specification){

        liveness_interval_struct curr_interval = *first_interval_without_real_register_iterator;

        //check if there is a sensitive register available 
        auto sens_real_reg = std::find_if(available_registers.begin(), available_registers.end(), [curr_interval, track_xor_specification_armv6m](const std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>& t){return (std::get<1>(t) == 1) && (!assignment_breaks_xor_specifications(t, curr_interval, track_xor_specification_armv6m));});

        //sens interval 
        if(curr_interval.sensitivity == 1){

            if(virt_reg_at_use_point_is_destination(TAC.at(std::get<1>(curr_interval.split_bb_ranges.at(0)).at(0)), virt_reg) && (!virt_reg_at_use_point_is_source(TAC.at(std::get<1>(curr_interval.split_bb_ranges.at(0)).at(0)),virt_reg))){
                //check if there is an sensitive interval in active -> bc. sensitive intervals are disjoint means the intervals overlap at same instruction -> combined leakage anyway
                auto it_sens_in_active = std::find_if(active.begin(), active.end(), [](const liveness_interval_struct& t){return t.sensitivity == 1;});
                if(it_sens_in_active != active.end()){
                    liveness_intervals.at(index_of_interval_without_real_register).real_register_number = it_sens_in_active->real_register_number;
                    //find interval from active in allocated

                    auto interval_from_active_in_liveness = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [it_sens_in_active](liveness_interval_struct& t){return ((t.virtual_register_number == it_sens_in_active->virtual_register_number) && (t.sensitivity == it_sens_in_active->sensitivity) && (t.real_register_number == it_sens_in_active->real_register_number) && (t.ranges == it_sens_in_active->ranges) && (t.position_on_stack == it_sens_in_active->position_on_stack) && (t.liveness_used == it_sens_in_active->liveness_used) && (t.is_spilled == it_sens_in_active->is_spilled));});
                    interval_from_active_in_liveness->contains_break_transition = true;

                    auto it_sens_in_allocated = std::find_if(allocated_registers.begin(), allocated_registers.end(), [it_sens_in_active](const std::tuple<uint32_t, uint32_t, uint8_t>& t){return (std::get<0>(t) == it_sens_in_active->virtual_register_number) && (std::get<1>(t) == it_sens_in_active->real_register_number);});
                    std::get<0>(*it_sens_in_allocated) = liveness_intervals.at(index_of_interval_without_real_register).virtual_register_number;
                    active.erase(it_sens_in_active);
                    // liveness_intervals.at(index_of_interval_without_real_register).contains_break_transition = true;
                }
                else if(sens_real_reg != available_registers.end()){
                    allocated_registers.push_back(std::make_tuple(virt_reg, std::get<0>(available_registers.at(sens_real_reg - available_registers.begin())), liveness_intervals.at(index_of_interval_without_real_register).sensitivity));
                    liveness_intervals.at(index_of_interval_without_real_register).real_register_number = std::get<0>(available_registers.at(sens_real_reg - available_registers.begin()));
                    available_registers.erase(sens_real_reg);
                }
                else{
                    allocated_registers.push_back(std::make_tuple(virt_reg, std::get<0>(available_registers.at(0)), liveness_intervals.at(index_of_interval_without_real_register).sensitivity));
                    liveness_intervals.at(index_of_interval_without_real_register).real_register_number = std::get<0>(available_registers.at(0));
                    available_registers.erase(available_registers.begin());
                }

            }
            else{
                if(sens_real_reg != available_registers.end()){
                    allocated_registers.push_back(std::make_tuple(virt_reg, std::get<0>(available_registers.at(sens_real_reg - available_registers.begin())), liveness_intervals.at(index_of_interval_without_real_register).sensitivity));
                    liveness_intervals.at(index_of_interval_without_real_register).real_register_number = std::get<0>(available_registers.at(sens_real_reg - available_registers.begin()));
                    available_registers.erase(sens_real_reg);
                }
                else{
                    allocated_registers.push_back(std::make_tuple(virt_reg, std::get<0>(available_registers.at(0)), liveness_intervals.at(index_of_interval_without_real_register).sensitivity));
                    liveness_intervals.at(index_of_interval_without_real_register).real_register_number = std::get<0>(available_registers.at(0));
                    available_registers.erase(available_registers.begin());
                }
            }


        }
        //non-sens interval
        else{
            if(sens_real_reg != available_registers.end()){
                allocated_registers.push_back(std::make_tuple(virt_reg, std::get<0>(available_registers.at(sens_real_reg - available_registers.begin())), liveness_intervals.at(index_of_interval_without_real_register).sensitivity));
                liveness_intervals.at(index_of_interval_without_real_register).real_register_number = std::get<0>(available_registers.at(sens_real_reg - available_registers.begin()));
                available_registers.erase(sens_real_reg);
            }
            else{
                allocated_registers.push_back(std::make_tuple(virt_reg, std::get<0>(available_registers.at(0)), liveness_intervals.at(index_of_interval_without_real_register).sensitivity));
                liveness_intervals.at(index_of_interval_without_real_register).real_register_number = std::get<0>(available_registers.at(0));
                available_registers.erase(available_registers.begin());
            }

        }

    }


    

    if(liveness_intervals.at(index_of_interval_without_real_register).is_spilled == true){
        for(auto use_point_in_split_range: liveness_intervals.at(index_of_interval_without_real_register).split_bb_ranges){
            uint32_t instruction_idx = std::get<1>(use_point_in_split_range).at(0); //std::get<1>(liveness_intervals.at(index_of_interval_without_real_register).split_bb_ranges.at(0)).at(0);
            if(virt_reg_at_use_point_is_source(TAC.at(instruction_idx), virt_reg)){
                if(TAC.at(instruction_idx).op == Operation::FUNC_MOVE){
 
                    uint32_t position_of_inserted_load = instruction_idx - TAC.at(instruction_idx).func_move_idx;
                    load_spill_from_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, position_of_inserted_load, liveness_intervals.at(index_of_interval_without_real_register).real_register_number, index_of_interval_without_real_register, memory_security, track_xor_specification_armv6m, postponed_store_insertion);
      

                
                    liveness_intervals.at(index_of_interval_without_real_register).liveness_used.emplace_back(position_of_inserted_load);
                    std::sort(liveness_intervals.at(index_of_interval_without_real_register).liveness_used.begin(), liveness_intervals.at(index_of_interval_without_real_register).liveness_used.end());
                    liveness_intervals.at(index_of_interval_without_real_register).liveness_used.erase(std::unique(liveness_intervals.at(index_of_interval_without_real_register).liveness_used.begin(), liveness_intervals.at(index_of_interval_without_real_register).liveness_used.end()), liveness_intervals.at(index_of_interval_without_real_register).liveness_used.end());
                    std::get<0>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()) = liveness_intervals.at(index_of_interval_without_real_register).liveness_used.at(0);
                    std::get<1>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()) = liveness_intervals.at(index_of_interval_without_real_register).liveness_used.back();
                    split_into_bb_ranges_of_intervals(control_flow_TAC, liveness_intervals.at(index_of_interval_without_real_register), predecessors);

                }
                else{

                    uint32_t position_of_inserted_load = instruction_idx;
                    load_spill_from_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, position_of_inserted_load, liveness_intervals.at(index_of_interval_without_real_register).real_register_number, index_of_interval_without_real_register, memory_security, track_xor_specification_armv6m, postponed_store_insertion);
       

                
                    liveness_intervals.at(index_of_interval_without_real_register).liveness_used.emplace_back(position_of_inserted_load);
                    std::sort(liveness_intervals.at(index_of_interval_without_real_register).liveness_used.begin(), liveness_intervals.at(index_of_interval_without_real_register).liveness_used.end());
                    liveness_intervals.at(index_of_interval_without_real_register).liveness_used.erase(std::unique(liveness_intervals.at(index_of_interval_without_real_register).liveness_used.begin(), liveness_intervals.at(index_of_interval_without_real_register).liveness_used.end()), liveness_intervals.at(index_of_interval_without_real_register).liveness_used.end());
                    std::get<0>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()) = liveness_intervals.at(index_of_interval_without_real_register).liveness_used.at(0);
                    std::get<1>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()) = liveness_intervals.at(index_of_interval_without_real_register).liveness_used.back();
                    split_into_bb_ranges_of_intervals(control_flow_TAC, liveness_intervals.at(index_of_interval_without_real_register), predecessors);

                }
            }
        }



        for(auto split_range: liveness_intervals.at(index_of_interval_without_real_register).split_bb_ranges){
            if(!remove_str_optimization_possible(control_flow_TAC, liveness_intervals, liveness_intervals.at(index_of_interval_without_real_register), split_range)){
                for(auto use_point_in_split_range: std::get<1>(split_range)){
                    uint32_t instruction_idx = use_point_in_split_range;
                    if(virt_reg_at_use_point_is_destination(TAC.at(instruction_idx), virt_reg)){

                        auto curr_interval = liveness_intervals.at(index_of_interval_without_real_register);
                        //fix specification store
                        //if interval has specification and is first register of specification -> can not store after last use point because at this point the other interval already has the register -> we would store a wrong value -> thus store one instruction earlier
                        if(TAC.at(std::get<1>(split_range).back()).op == Operation::FUNC_MOVE){
                            instruction_idx = std::get<1>(split_range).back() - TAC.at(std::get<1>(split_range).back()).func_move_idx - 1; // -1 bc in store function call the instruction will be increased by 1
                            store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, liveness_intervals.at(index_of_interval_without_real_register).real_register_number, index_of_interval_without_real_register, std::get<0>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                        }
                        else if(liveness_intervals.at(index_of_interval_without_real_register).contains_break_transition){
                            instruction_idx = std::get<1>(split_range).back() - 1;//liveness_intervals.at(index_of_interval_without_real_register).liveness_used.back() - 1;
                            store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, liveness_intervals.at(index_of_interval_without_real_register).real_register_number, index_of_interval_without_real_register, std::get<0>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                        }
                        else if((!armv6_m_xor_specification_is_empty(liveness_intervals.at(index_of_interval_without_real_register).arm6_m_xor_specification)) && (virtual_register_at_src_TAC_instruction_has_armv6_m_xor_specification(TAC.at(std::get<1>(split_range).back()), std::get<1>(split_range).back(), liveness_intervals.at(index_of_interval_without_real_register).virtual_register_number, liveness_intervals.at(index_of_interval_without_real_register).arm6_m_xor_specification))){
                            instruction_idx = std::get<1>(split_range).back() - 1;//liveness_intervals.at(index_of_interval_without_real_register).liveness_used.back() - 1;
                            store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, liveness_intervals.at(index_of_interval_without_real_register).real_register_number, index_of_interval_without_real_register, std::get<0>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                        }
                        else{
                            instruction_idx = std::get<1>(split_range).back();//liveness_intervals.at(index_of_interval_without_real_register).liveness_used.back();
                            store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, liveness_intervals.at(index_of_interval_without_real_register).real_register_number, index_of_interval_without_real_register, std::get<0>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                        }

                        
                        break;
                    }
                }
            }
        }

    }

}


bool assignment_breaks_xor_specifications(const std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>& available_register, const liveness_interval_struct& liveness_interval, const std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m){

    //check if real register in datastructure, if not good case
    auto it_xor_specification = std::find_if(track_xor_specification_armv6m.begin(), track_xor_specification_armv6m.end(), [available_register](const std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>& t){return std::get<0>(t) == std::get<0>(available_register);});
    if(it_xor_specification == track_xor_specification_armv6m.end()){
        return false;
    }
    else{
        uint32_t virt_reg1 = std::get<2>(*it_xor_specification);
        uint32_t virt_reg2 = std::get<3>(*it_xor_specification);
        uint32_t xor_use_point = std::get<1>(*it_xor_specification);

        if(((liveness_interval.virtual_register_number == virt_reg1) || (liveness_interval.virtual_register_number == virt_reg2)) && ((liveness_interval.liveness_used.at(0) <= xor_use_point) && (liveness_interval.liveness_used.back() >= xor_use_point))){
            return false;
        }
        else if(((liveness_interval.virtual_register_number == virt_reg1) || (liveness_interval.virtual_register_number == virt_reg2)) && ((liveness_interval.liveness_used.at(0) > xor_use_point) || (liveness_interval.liveness_used.back() < xor_use_point))){
            return false;
        }
        else if(((liveness_interval.virtual_register_number != virt_reg1) && (liveness_interval.virtual_register_number != virt_reg2)) && ((liveness_interval.liveness_used.at(0) > xor_use_point) || (liveness_interval.liveness_used.back() < xor_use_point))){
            return false;
        }
        else if(((liveness_interval.virtual_register_number != virt_reg1) && (liveness_interval.virtual_register_number != virt_reg2)) && ((liveness_interval.liveness_used.at(0) <= xor_use_point) && (liveness_interval.liveness_used.back() >= xor_use_point))){
            return true;
        }
        else{
            throw std::runtime_error("in assignment_breaks_xor_specification: can not decide if available register is suited for interval");
        }
    }

}