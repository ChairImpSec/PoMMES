#include "removing_security_hazard.hpp"

extern uint32_t v_register_ctr;


void break_transition_armv7m(std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers, uint32_t idx_of_current_interval, uint32_t& use_point, std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m, std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion){
    if(!((TAC.at(use_point).op == Operation::FUNC_RES_MOV) && (liveness_intervals.at(idx_of_current_interval).real_register_number == 0))){
        uint32_t instruction_idx_to_insert = use_point;
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
        uint32_t dest_v = liveness_intervals.at(idx_of_current_interval).real_register_number;
        uint8_t dest_sens = 0;

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

        TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
        insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

        synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);

        use_point++;
    }

}




void get_bb_idx_and_cfg_instr_idx_from_use_point( std::vector<TAC_bb>& control_flow_TAC, uint32_t& bb_idx, uint32_t& cfg_instr_idx, uint32_t use_point){
    bb_idx = get_bb_idx_of_instruction_idx(control_flow_TAC, use_point);
    cfg_instr_idx = 0;
    uint32_t ctr = 0;
    uint32_t instr_sum_of_prev_bb = 0;
    while(ctr < bb_idx){
        instr_sum_of_prev_bb += control_flow_TAC.at(ctr).TAC.size();
        ctr++;
    }
    cfg_instr_idx = use_point - instr_sum_of_prev_bb;
}

//we perform a dfs through all the predecessors of the initial bb and check for the required property
void check_all_predecessor_paths(std::vector<TAC_bb>& control_flow_TAC, uint32_t case_flag, std::vector<bool>& visited, uint32_t predec, std::map<uint32_t, std::vector<uint32_t>>& predecessors, uint8_t& local_mem_sens, bool& found_mem_in_previous_bb){
    auto it = std::find_if(control_flow_TAC.begin(), control_flow_TAC.end(), [predec](const TAC_bb& t){return t.TAC_bb_number == predec;});
    if(it == control_flow_TAC.end()){
        throw std::runtime_error("in check_all_predecessor_paths: can not find predecessor in control flow TAC");
    }
    uint32_t idx_of_predec = it - control_flow_TAC.begin();

    visited.at(idx_of_predec) = true;
    bool found_mem = false;
    //go over all instructions of the current basic block
    for(int32_t instr_idx_of_bb = (control_flow_TAC.at(idx_of_predec).TAC.size() - 1); instr_idx_of_bb >= 0; --instr_idx_of_bb){
        if(case_flag == 1){ //load case

            if(((control_flow_TAC.at(idx_of_predec).TAC.at(instr_idx_of_bb).op == Operation::LOADBYTE) ||  (control_flow_TAC.at(idx_of_predec).TAC.at(instr_idx_of_bb).op == Operation::LOADHALFWORD) ||  (control_flow_TAC.at(idx_of_predec).TAC.at(instr_idx_of_bb).op == Operation::LOADWORD) ||  (control_flow_TAC.at(idx_of_predec).TAC.at(instr_idx_of_bb).op == Operation::LOADWORD_WITH_NUMBER_OFFSET))){
                if(control_flow_TAC.at(idx_of_predec).TAC.at(instr_idx_of_bb).sensitivity == 1){
                    local_mem_sens = 1;
                }
                else{
                    local_mem_sens = 0;
                }
                found_mem = true;
                found_mem_in_previous_bb = true;
                break;
            }
        }
        else if(case_flag == 2){ //store case
            if(((control_flow_TAC.at(idx_of_predec).TAC.at(instr_idx_of_bb).op == Operation::STOREBYTE) ||  (control_flow_TAC.at(idx_of_predec).TAC.at(instr_idx_of_bb).op == Operation::STOREHALFWORD) ||  (control_flow_TAC.at(idx_of_predec).TAC.at(instr_idx_of_bb).op == Operation::STOREWORD) ||  (control_flow_TAC.at(idx_of_predec).TAC.at(instr_idx_of_bb).op == Operation::STOREWORD_WITH_NUMBER_OFFSET))){
                if(control_flow_TAC.at(idx_of_predec).TAC.at(instr_idx_of_bb).sensitivity == 1){
                    local_mem_sens = 1;
                }
                else{
                    local_mem_sens = 0;
                }
                found_mem = true;
                found_mem_in_previous_bb = true;
                break;
            }
        }

    }

    if(!found_mem){
        if(predecessors.find(predec) != predecessors.end()){
            for(auto& p: predecessors.at(predec)){
                uint8_t intern_local_mem_sens = 0;
                auto tmp = std::find_if(control_flow_TAC.begin(), control_flow_TAC.end(), [p](const TAC_bb& t){return t.TAC_bb_number == p;});
                if(tmp == control_flow_TAC.end()){
                    throw std::runtime_error("in check_all_predecessor_paths: can not find predecessor in control flow TAC");
                }
                uint32_t idx_of_next_predec = tmp - control_flow_TAC.begin();
                if((tmp != control_flow_TAC.end()) && (visited.at(idx_of_next_predec) != true)){
                    check_all_predecessor_paths(control_flow_TAC, case_flag, visited, p, predecessors, intern_local_mem_sens, found_mem_in_previous_bb);
                    local_mem_sens |= intern_local_mem_sens;
                    if(local_mem_sens == 1){
                        break;
                    }
                }
            }
        }
    }


}

bool find_pure_destination_register_in_bb_to_break_mem(TAC_bb& tac_bb, std::vector<liveness_interval_struct>& liveness_intervals, uint32_t instr_offset_of_prev_bbs, uint32_t instr_idx, uint32_t case_flag){

    //seach for destination register that is not a xor operation beween the two sensitive instructions
    uint32_t found_dest_reg = UINT32_MAX;
    uint32_t idx_to_insert_mem_instr = UINT32_MAX;

    for(int32_t instr_idx_with_destination_register_between_sensitive_mem_instr = instr_idx - 1; instr_idx_with_destination_register_between_sensitive_mem_instr > 0; --instr_idx_with_destination_register_between_sensitive_mem_instr){
        std::set<uint32_t> instr_in;
        std::set<uint32_t> instr_out;


        if((tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).op == Operation::STOREBYTE) || (tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).op == Operation::STOREHALFWORD) || (tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).op == Operation::STOREWORD) || (tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).op == Operation::STOREWORD_WITH_NUMBER_OFFSET)){
            if(tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src1_type == TAC_OPERAND::V_REG){
                uint32_t virt_src1_reg = tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src1_v;
                uint32_t liveness_use_point = instr_idx_with_destination_register_between_sensitive_mem_instr + instr_offset_of_prev_bbs;
                auto it_src1_real_reg = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [virt_src1_reg, liveness_use_point](const liveness_interval_struct& t){return (t.virtual_register_number == virt_src1_reg) && (std::find(t.liveness_used.begin(), t.liveness_used.end(), liveness_use_point) != t.liveness_used.end());});
                instr_in.insert(it_src1_real_reg->real_register_number);
            }
            else if(tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src1_type == TAC_OPERAND::R_REG){
                instr_in.insert(tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src1_v);
            }

            if(tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src2_type == TAC_OPERAND::V_REG){
                uint32_t virt_src2_reg = tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src2_v;
                uint32_t liveness_use_point = instr_idx_with_destination_register_between_sensitive_mem_instr + instr_offset_of_prev_bbs;
                auto it_src2_real_reg = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [virt_src2_reg, liveness_use_point](const liveness_interval_struct& t){return (t.virtual_register_number == virt_src2_reg) && (std::find(t.liveness_used.begin(), t.liveness_used.end(), liveness_use_point) != t.liveness_used.end());});
                instr_in.insert(it_src2_real_reg->real_register_number);
            }
            else if(tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src2_type == TAC_OPERAND::R_REG){
                instr_in.insert(tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src2_v);
            }
        }
        else{
            if(tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).dest_type == TAC_OPERAND::V_REG){
                uint32_t virt_dest_reg = tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).dest_v_reg_idx;
                uint32_t liveness_use_point = instr_idx_with_destination_register_between_sensitive_mem_instr + instr_offset_of_prev_bbs;
                auto it_dest_real_reg = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [virt_dest_reg, liveness_use_point](const liveness_interval_struct& t){return (t.virtual_register_number == virt_dest_reg) && (std::find(t.liveness_used.begin(), t.liveness_used.end(), liveness_use_point) != t.liveness_used.end());});
                instr_out.insert(it_dest_real_reg->real_register_number);
            }
            else if (tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).dest_type == TAC_OPERAND::R_REG){
                instr_out.insert(tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).dest_v_reg_idx);
            }

            if(tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src1_type == TAC_OPERAND::V_REG){
                uint32_t virt_src1_reg = tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src1_v;
                uint32_t liveness_use_point = instr_idx_with_destination_register_between_sensitive_mem_instr + instr_offset_of_prev_bbs;
                auto it_src1_real_reg = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [virt_src1_reg, liveness_use_point](const liveness_interval_struct& t){return (t.virtual_register_number == virt_src1_reg) && (std::find(t.liveness_used.begin(), t.liveness_used.end(), liveness_use_point) != t.liveness_used.end());});
                instr_in.insert(it_src1_real_reg->real_register_number);
            }
            else if(tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src1_type == TAC_OPERAND::R_REG){
                instr_in.insert(tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src1_v);
            }

            if(tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src2_type == TAC_OPERAND::V_REG){
                uint32_t virt_src2_reg = tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src2_v;
                uint32_t liveness_use_point = instr_idx_with_destination_register_between_sensitive_mem_instr + instr_offset_of_prev_bbs;
                auto it_src2_real_reg = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [virt_src2_reg, liveness_use_point](const liveness_interval_struct& t){return (t.virtual_register_number == virt_src2_reg) && (std::find(t.liveness_used.begin(), t.liveness_used.end(), liveness_use_point) != t.liveness_used.end());});
                instr_in.insert(it_src2_real_reg->real_register_number);
            }
            else if(tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src2_type == TAC_OPERAND::R_REG){
                instr_in.insert(tac_bb.TAC.at(instr_idx_with_destination_register_between_sensitive_mem_instr).src2_v);
            }
        }



        std::set<uint32_t> result;
        std::set_difference(instr_out.begin(), instr_out.end(), instr_in.begin(), instr_in.end(), std::inserter(result, result.end()));
        result.erase(13);
        result.erase(11);
        #ifdef ARMV6_M
        result.erase(9);
        #endif
        if(!result.empty()){
            found_dest_reg = *(result.begin());
            idx_to_insert_mem_instr = instr_offset_of_prev_bbs + instr_idx_with_destination_register_between_sensitive_mem_instr;

            break;
        }
    }

    if(found_dest_reg != UINT32_MAX){

        TAC_type tmp_tac;

        Operation op = Operation::MOV;
        uint8_t sens = 0; 
        TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
        uint32_t dest_v = found_dest_reg;
        uint8_t dest_sens = 0; 
        TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
        uint32_t src1_v = 11;
        uint8_t src1_sens = 0; 
        TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
        uint32_t src2_v = UINT32_MAX;
        uint8_t src2_sens = 0;
        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

        tac_bb.TAC.insert(tac_bb.TAC.begin() + idx_to_insert_mem_instr - instr_offset_of_prev_bbs, tmp_tac);
        synchronize_instruction_index_of_liveness_intervals(liveness_intervals, idx_to_insert_mem_instr);


        if(case_flag == 1){
            op = Operation::LOADWORD;
            sens = 0; 
            dest_type = TAC_OPERAND::R_REG; 
            dest_v = found_dest_reg;
            dest_sens = 0; 
            src1_type = TAC_OPERAND::R_REG; 
            src1_v = found_dest_reg;
            src1_sens = 0; 
            src2_type = TAC_OPERAND::NONE; 
            src2_v = UINT32_MAX;
            src2_sens = 0;
        }
        else if(case_flag == 2){
            op = Operation::STOREWORD;
            sens = 0; 
            dest_type = TAC_OPERAND::NONE; 
            dest_v = UINT32_MAX;
            dest_sens = 0; 
            src1_type = TAC_OPERAND::R_REG; 
            src1_v = found_dest_reg;
            src1_sens = 0; 
            src2_type = TAC_OPERAND::R_REG; 
            src2_v = found_dest_reg;
            src2_sens = 0;
        }

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

        tac_bb.TAC.insert(tac_bb.TAC.begin() + idx_to_insert_mem_instr - instr_offset_of_prev_bbs + 1, tmp_tac);
        synchronize_instruction_index_of_liveness_intervals(liveness_intervals, idx_to_insert_mem_instr + 1);


        tac_bb.local_shadow_memory_tracker.insert(tac_bb.local_shadow_memory_tracker.rbegin().base(), 0);
        return true;
    }

    return false;


}

bool find_non_sensitive_interval_during_bb_to_break_mem(TAC_bb& tac_bb, std::vector<liveness_interval_struct>& liveness_intervals, uint32_t instr_offset_of_prev_bbs, uint32_t instr_pos_of_curr_sensitive_memory_op, uint32_t case_flag){
    //try to get non-sensitive interval that exist during this instruction range (between prev sens. instr and curr sens instr.)
    auto it_non_sen_reg_in_sens_mem_range = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [instr_pos_of_curr_sensitive_memory_op](const liveness_interval_struct& t){return (t.sensitivity == 0) && (std::get<1>(t.ranges.back()) >= instr_pos_of_curr_sensitive_memory_op) && (std::get<0>(t.ranges.back()) < instr_pos_of_curr_sensitive_memory_op);});

    if(it_non_sen_reg_in_sens_mem_range != liveness_intervals.end()){


        TAC_type tmp_tac;

        Operation op = Operation::MOV;
        uint8_t sens = 0; 
        TAC_OPERAND dest_type = TAC_OPERAND::R_REG; 
        uint32_t dest_v = 9;
        uint8_t dest_sens = 0; 
        TAC_OPERAND src1_type = TAC_OPERAND::R_REG; 
        uint32_t src1_v = it_non_sen_reg_in_sens_mem_range->real_register_number;
        uint8_t src1_sens = 0; 
        TAC_OPERAND src2_type = TAC_OPERAND::NONE; 
        uint32_t src2_v = UINT32_MAX;
        uint8_t src2_sens = 0;
        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

        uint32_t idx_to_insert_mem_instr = instr_pos_of_curr_sensitive_memory_op - 1;

        tac_bb.TAC.insert(tac_bb.TAC.begin() + idx_to_insert_mem_instr - instr_offset_of_prev_bbs, tmp_tac);
        synchronize_instruction_index_of_liveness_intervals(liveness_intervals, idx_to_insert_mem_instr);

        op = Operation::MOV;
        sens = 0; 
        dest_type = TAC_OPERAND::R_REG; 
        dest_v = it_non_sen_reg_in_sens_mem_range->real_register_number;
        dest_sens = 0; 
        src1_type = TAC_OPERAND::R_REG; 
        src1_v = 11;
        src1_sens = 0; 
        src2_type = TAC_OPERAND::NONE; 
        src2_v = UINT32_MAX;
        src2_sens = 0;
        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

        tac_bb.TAC.insert(tac_bb.TAC.begin() + idx_to_insert_mem_instr - instr_offset_of_prev_bbs + 1, tmp_tac);
        synchronize_instruction_index_of_liveness_intervals(liveness_intervals, idx_to_insert_mem_instr + 1);

        if(case_flag == 1){
            op = Operation::LOADWORD;
            sens = 0; 
            dest_type = TAC_OPERAND::R_REG; 
            dest_v = it_non_sen_reg_in_sens_mem_range->real_register_number;
            dest_sens = 0; 
            src1_type = TAC_OPERAND::R_REG; 
            src1_v = it_non_sen_reg_in_sens_mem_range->real_register_number;
            src1_sens = 0; 
            src2_type = TAC_OPERAND::NONE; 
            src2_v = UINT32_MAX;
            src2_sens = 0;
        }
        else if(case_flag == 2){
            op = Operation::STOREWORD;
            sens = 0; 
            dest_type = TAC_OPERAND::NONE; 
            dest_v = UINT32_MAX;
            dest_sens = 0; 
            src1_type = TAC_OPERAND::R_REG; 
            src1_v = it_non_sen_reg_in_sens_mem_range->real_register_number;
            src1_sens = 0; 
            src2_type = TAC_OPERAND::R_REG; 
            src2_v = it_non_sen_reg_in_sens_mem_range->real_register_number;
            src2_sens = 0;
        }

        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

        tac_bb.TAC.insert(tac_bb.TAC.begin() + idx_to_insert_mem_instr - instr_offset_of_prev_bbs + 2, tmp_tac);
        synchronize_instruction_index_of_liveness_intervals(liveness_intervals, idx_to_insert_mem_instr + 2);


        op = Operation::MOV;
        sens = 0; 
        dest_type = TAC_OPERAND::R_REG; 
        dest_v = it_non_sen_reg_in_sens_mem_range->real_register_number;
        dest_sens = 0; 
        src1_type = TAC_OPERAND::R_REG; 
        src1_v = 9;
        src1_sens = 0; 
        src2_type = TAC_OPERAND::NONE; 
        src2_v = UINT32_MAX;
        src2_sens = 0;
        generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

        tac_bb.TAC.insert(tac_bb.TAC.begin() + idx_to_insert_mem_instr - instr_offset_of_prev_bbs + 3, tmp_tac);
        synchronize_instruction_index_of_liveness_intervals(liveness_intervals, idx_to_insert_mem_instr + 3);

        tac_bb.local_shadow_memory_tracker.insert(tac_bb.local_shadow_memory_tracker.rbegin().base(), 0);


        return true;
    }
    else{    

        return false;
    }
    
}

void break_load(TAC_bb& tac_bb, std::vector<liveness_interval_struct>& liveness_intervals, uint32_t instr_offset_of_prev_bbs, uint32_t instr_idx, uint32_t instr_pos_of_curr_sensitive_memory_op){

    bool success = find_pure_destination_register_in_bb_to_break_mem(tac_bb, liveness_intervals, instr_offset_of_prev_bbs, instr_idx, 1);

    if(!success){
        //check for non-sensitive interval whose range is in bb until load instruction, use the register assigned to it to break load by temporarily storing the value of the register in a higher register
        success = find_non_sensitive_interval_during_bb_to_break_mem(tac_bb, liveness_intervals, instr_offset_of_prev_bbs, instr_pos_of_curr_sensitive_memory_op, 1);

        if(!success){
            throw std::runtime_error("to implement");
            //force_register_from_other_interval_for_shadow_memory(control_flow_TAC, predecessors, liveness_intervals, instr_offset_of_prev_bbs, instr_idx, instr_pos_of_curr_sensitive_memory_op, 1);
        }
    }

}

void break_store(TAC_bb& tac_bb, std::vector<liveness_interval_struct>& liveness_intervals, uint32_t instr_offset_of_prev_bbs, uint32_t instr_idx, uint32_t instr_pos_of_curr_sensitive_memory_op){
    bool success = find_pure_destination_register_in_bb_to_break_mem(tac_bb, liveness_intervals, instr_offset_of_prev_bbs, instr_idx, 2);
    if(!success){
        //check for non-sensitive interval whose range is in bb until load instruction, use the register assigned to it to break load by temporarily storing the value of the register in a higher register
        success = find_non_sensitive_interval_during_bb_to_break_mem(tac_bb, liveness_intervals, instr_offset_of_prev_bbs, instr_pos_of_curr_sensitive_memory_op, 2);

        if(!success){
            throw std::runtime_error("to implement");
            //force_register_from_other_interval_for_shadow_memory(control_flow_TAC, predecessors, liveness_intervals, instr_offset_of_prev_bbs, instr_idx, instr_pos_of_curr_sensitive_memory_op, 2);
        }
    }
}



uint32_t get_first_use_point_of_interval_in_bb(liveness_interval_struct& liveness_interval, std::vector<TAC_bb>& control_flow_TAC, uint32_t dest_bb_idx){
    uint32_t cfg_instr_idx;
    uint32_t bb_idx;
    for(const auto& use_point: liveness_interval.liveness_used){

        get_bb_idx_and_cfg_instr_idx_from_use_point(control_flow_TAC, bb_idx, cfg_instr_idx, use_point);
        if(bb_idx == dest_bb_idx){
            break;
        }
    }

    return cfg_instr_idx;
}


void break_memory_overwrite(std::vector<TAC_type>& TAC, std::vector<liveness_interval_struct>& liveness_intervals, memory_security_struct& memory_security){
    for(uint32_t tac_idx = 0; tac_idx < TAC.size(); ++tac_idx){
        if((TAC.at(tac_idx).op == Operation::STOREWORD) || (TAC.at(tac_idx).op == Operation::STOREHALFWORD) || (TAC.at(tac_idx).op == Operation::STOREBYTE) || (TAC.at(tac_idx).op == Operation::STOREWORD_WITH_NUMBER_OFFSET)){
            uint32_t dest_value = TAC.at(tac_idx).dest_v_reg_idx;
            uint8_t store_offset_is_sensitive = 0;
            auto it_memory_offset_of_store = std::find_if(memory_security.stack_mapping_tracker.begin(), memory_security.stack_mapping_tracker.end(), [dest_value](const stack_local_variable_tracking_struct& t){return t.position == dest_value;});
            if(it_memory_offset_of_store != memory_security.stack_mapping_tracker.end()){
                if(it_memory_offset_of_store->sensitivity == 1){
                    store_offset_is_sensitive = 1;
                }
            }


            if((TAC.at(tac_idx).src1_sensitivity == 1) && (TAC.at(tac_idx).src2_sensitivity == 1)){
                //insert memory overwrite
                TAC_type tmp_tac;

                Operation op = TAC.at(tac_idx).op;
                uint8_t sens = 0; 
                TAC_OPERAND dest_type = TAC_OPERAND::NONE; 
                uint32_t dest_v = UINT32_MAX;
                uint8_t dest_sens = 0;
                if(TAC.at(tac_idx).op == Operation::STOREWORD_WITH_NUMBER_OFFSET){
                    dest_type = TAC_OPERAND::NUM;
                    dest_v = TAC.at(tac_idx).dest_v_reg_idx;
                } 
                TAC_OPERAND src1_type = TAC.at(tac_idx).src2_type; 
                uint32_t src1_v = TAC.at(tac_idx).src2_v;
                uint8_t src1_sens = 0; 
                TAC_OPERAND src2_type = TAC.at(tac_idx).src2_type; 
                uint32_t src2_v = TAC.at(tac_idx).src2_v;
                uint8_t src2_sens = 0;

                generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

                TAC.at(tac_idx).src2_sensitivity = 0;

                TAC.insert(TAC.begin() + tac_idx, tmp_tac);
                synchronize_instruction_index_of_liveness_intervals(liveness_intervals, tac_idx);


            }
            else if((TAC.at(tac_idx).src1_sensitivity == 1) && (TAC.at(tac_idx).dest_type == TAC_OPERAND::NUM) && (store_offset_is_sensitive)){
                //insert memory overwrite
                TAC_type tmp_tac;

                Operation op = TAC.at(tac_idx).op;
                uint8_t sens = 0; 
                TAC_OPERAND dest_type = TAC_OPERAND::NONE; 
                uint32_t dest_v = UINT32_MAX;
                uint8_t dest_sens = 0;
                if(TAC.at(tac_idx).op == Operation::STOREWORD_WITH_NUMBER_OFFSET){
                    dest_type = TAC_OPERAND::NUM;
                    dest_v = TAC.at(tac_idx).dest_v_reg_idx;
                } 
                TAC_OPERAND src1_type = TAC.at(tac_idx).src2_type; 
                uint32_t src1_v = TAC.at(tac_idx).src2_v;
                uint8_t src1_sens = 0; 
                TAC_OPERAND src2_type = TAC.at(tac_idx).src2_type; 
                uint32_t src2_v = TAC.at(tac_idx).src2_v;
                uint8_t src2_sens = 0;

                generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

                TAC.at(tac_idx).src2_sensitivity = 0;
                TAC.at(tac_idx).src1_sensitivity = 0;

                TAC.insert(TAC.begin() + tac_idx, tmp_tac);
                synchronize_instruction_index_of_liveness_intervals(liveness_intervals, tac_idx);

                
            }
        }
    }
}

void break_shadow_register_overwrite_armv6m(std::vector<TAC_bb>& control_flow_TAC, std::map<uint32_t, std::vector<uint32_t>>& predecessors, std::vector<liveness_interval_struct>& liveness_intervals){
    //go over every sensitive interval
    for(uint32_t interval_idx = 0; interval_idx < liveness_intervals.size(); ++interval_idx){
        if(liveness_intervals.at(interval_idx).sensitivity == 1){
            //go over all split ranges
            for(uint32_t split_range_idx = 0; split_range_idx < liveness_intervals.at(interval_idx).split_bb_ranges.size(); ++split_range_idx){
                //go over all use points of current range
                auto use_points_of_range = std::get<1>(liveness_intervals.at(interval_idx).split_bb_ranges.at(split_range_idx));
                for(uint32_t use_point_idx = 0; use_point_idx < use_points_of_range.size(); ++use_point_idx){
                    

                    uint32_t bb_idx;
                    uint32_t cfg_instr_idx;
                    
                    get_bb_idx_and_cfg_instr_idx_from_use_point(control_flow_TAC, bb_idx, cfg_instr_idx, use_points_of_range.at(use_point_idx));

                    uint32_t bb_number = get_bb_number_of_instruction_idx(control_flow_TAC, use_points_of_range.at(use_point_idx));
                    
                    
                    //if use point is sensitive load
                    if( (control_flow_TAC.at(bb_idx).TAC.at(cfg_instr_idx).sensitivity == 1) && ((control_flow_TAC.at(bb_idx).TAC.at(cfg_instr_idx).op == Operation::LOADBYTE) ||  (control_flow_TAC.at(bb_idx).TAC.at(cfg_instr_idx).op == Operation::LOADHALFWORD) ||  (control_flow_TAC.at(bb_idx).TAC.at(cfg_instr_idx).op == Operation::LOADWORD) ||  (control_flow_TAC.at(bb_idx).TAC.at(cfg_instr_idx).op == Operation::LOADWORD_WITH_NUMBER_OFFSET))){
                        //go from the use point to the beginning of the bb and check if we find a load instruction
                        bool found_load_in_original_bb = false;
                        for(int32_t instr_idx_of_bb = cfg_instr_idx - 1; instr_idx_of_bb >= 0; --instr_idx_of_bb){
                            if(((control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::LOADBYTE) ||  (control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::LOADHALFWORD) ||  (control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::LOADWORD) ||  (control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::LOADWORD_WITH_NUMBER_OFFSET))){
                                found_load_in_original_bb = true;
                                if(control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).sensitivity == 1){
                                    uint32_t instr_offset_of_prev_bbs = get_idx_of_bb_start(control_flow_TAC, bb_idx);
                                    #ifdef ARMV6_M
                                    break_load(control_flow_TAC.at(bb_idx), liveness_intervals, instr_offset_of_prev_bbs, cfg_instr_idx, cfg_instr_idx + instr_offset_of_prev_bbs);
                                    #endif
                                }
                                break;
                            }
                        }

                        if(!found_load_in_original_bb){
                            bool found_load_in_previous_bb = false;
                            //if we did not find and load instruction
                            if(predecessors.find(bb_number) != predecessors.end()){
                                uint8_t global_load_sens = 0;
                                for(auto& predec: predecessors.at(bb_number)){
                                    uint8_t local_load_sens = 0;
                                    std::vector<bool> visited(control_flow_TAC.size(), false);
                                    check_all_predecessor_paths(control_flow_TAC, 1, visited, predec, predecessors, local_load_sens, found_load_in_previous_bb ); 
                                    global_load_sens |= local_load_sens;
                                    if(global_load_sens == 1){
                                        break;
                                    }
                                }

                                if(global_load_sens == 1){
                                    uint32_t instr_offset_of_prev_bbs = get_idx_of_bb_start(control_flow_TAC, bb_idx);
                                    #ifdef ARMV6_M
                                    break_load(control_flow_TAC.at(bb_idx), liveness_intervals, instr_offset_of_prev_bbs, cfg_instr_idx, cfg_instr_idx + instr_offset_of_prev_bbs);
                                    #endif
                                }
                            }

                            //we have found no load instruction at all so no instruction that loads non-sensitive value was added, but there might be a sensitive store instruction that could also could cause a violation thus we have to check it in this case too.
                            if(!found_load_in_previous_bb){

                                //go from the use point to the beginning of the bb and check if we find a load instruction
                                bool found_store_in_original_bb = false;
                                uint32_t first_use_point_of_interval_in_bb = get_first_use_point_of_interval_in_bb(liveness_intervals.at(interval_idx), control_flow_TAC, bb_idx);
                                for(int32_t instr_idx_of_bb = first_use_point_of_interval_in_bb - 1; instr_idx_of_bb >= 0; --instr_idx_of_bb){
                                    if(((control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::STOREBYTE) ||  (control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::STOREHALFWORD) ||  (control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::STOREWORD) ||  (control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::STOREWORD_WITH_NUMBER_OFFSET))){
                                        found_store_in_original_bb = true;
                                        if(control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).sensitivity == 1){
                                            uint32_t instr_offset_of_prev_bbs = get_idx_of_bb_start(control_flow_TAC, bb_idx);
                                            #ifdef ARMV6_M
                                            break_store(control_flow_TAC.at(bb_idx), liveness_intervals, instr_offset_of_prev_bbs, cfg_instr_idx, cfg_instr_idx + instr_offset_of_prev_bbs);
                                            #endif
                                        }
                                        break;
                                    }
                                }

                                if(!found_store_in_original_bb){
                                    bool found_store_in_previous_bb = false;
                                    //if we did not find and load instruction
                                    if(predecessors.find(bb_number) != predecessors.end()){
                                        uint8_t global_store_sens = 0;
                                        for(auto& predec: predecessors.at(bb_number)){
                                            uint8_t local_store_sens = 0;
                                            std::vector<bool> visited(control_flow_TAC.size(), false);
                                            check_all_predecessor_paths(control_flow_TAC, 2, visited, predec, predecessors, local_store_sens, found_store_in_previous_bb ); 
                                            global_store_sens |= local_store_sens;
                                            if(global_store_sens == 1){
                                                break;
                                            }
                                        }

                                        if(global_store_sens == 1){
                                            uint32_t instr_offset_of_prev_bbs = get_idx_of_bb_start(control_flow_TAC, bb_idx);
                                            #ifdef ARMV6_M
                                            break_store(control_flow_TAC.at(bb_idx), liveness_intervals, instr_offset_of_prev_bbs, cfg_instr_idx, cfg_instr_idx + instr_offset_of_prev_bbs);
                                            #endif
                                        }
                                    }

                                }

                            }
                        }

 
                    }

                    //if use point is sensitive store
                    if( (control_flow_TAC.at(bb_idx).TAC.at(cfg_instr_idx).sensitivity == 1) && ((control_flow_TAC.at(bb_idx).TAC.at(cfg_instr_idx).op == Operation::STOREBYTE) ||  (control_flow_TAC.at(bb_idx).TAC.at(cfg_instr_idx).op == Operation::STOREHALFWORD) ||  (control_flow_TAC.at(bb_idx).TAC.at(cfg_instr_idx).op == Operation::STOREWORD) ||  (control_flow_TAC.at(bb_idx).TAC.at(cfg_instr_idx).op == Operation::STOREWORD_WITH_NUMBER_OFFSET))){
   
                        //go from the use point to the beginning of the bb and check if we find a load instruction
                        bool found_store_in_original_bb = false;
                        for(int32_t instr_idx_of_bb = cfg_instr_idx - 1; instr_idx_of_bb >= 0; --instr_idx_of_bb){
                            if(((control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::STOREBYTE) ||  (control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::STOREHALFWORD) ||  (control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::STOREWORD) ||  (control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::STOREWORD_WITH_NUMBER_OFFSET))){
                                found_store_in_original_bb = true;
                                if(control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).sensitivity == 1){
                                    uint32_t instr_offset_of_prev_bbs = get_idx_of_bb_start(control_flow_TAC, bb_idx);
                                    #ifdef ARMV6_M
                                    break_store(control_flow_TAC.at(bb_idx), liveness_intervals, instr_offset_of_prev_bbs, cfg_instr_idx, cfg_instr_idx + instr_offset_of_prev_bbs);

                                    #endif
                                }
                                break;
                            }
                        }

                        if(!found_store_in_original_bb){
                            bool found_store_in_previous_bb = false;
                            //if we did not find and load instruction
                            if(predecessors.find(bb_number) != predecessors.end()){
                                uint8_t global_store_sens = 0;
                                for(auto& predec: predecessors.at(bb_number)){
                                    uint8_t local_store_sens = 0;
                                    std::vector<bool> visited(control_flow_TAC.size(), false);
                                    check_all_predecessor_paths(control_flow_TAC, 2, visited, predec, predecessors, local_store_sens, found_store_in_previous_bb ); 
                                    global_store_sens |= local_store_sens;
                                    if(global_store_sens == 1){
                                        break;
                                    }
                                }

                                if(global_store_sens == 1){
                                    uint32_t instr_offset_of_prev_bbs = get_idx_of_bb_start(control_flow_TAC, bb_idx);
                                    #ifdef ARMV6_M
                                    break_store(control_flow_TAC.at(bb_idx), liveness_intervals, instr_offset_of_prev_bbs, cfg_instr_idx, cfg_instr_idx + instr_offset_of_prev_bbs);
                                    #endif
                                }
                            }

                            //we have found no store instruction at all so no instruction that stoers non-sensitive value was added, but there might be a sensitive load instruction that could also could cause a violation thus we have to check it in this case too.
                            if(!found_store_in_previous_bb){
                                //go from the use point to the beginning of the bb and check if we find a load instruction
                                bool found_load_in_original_bb = false;
                                //the starting point of the search is the beginning of the interval as the interval might contain a sensitive load at the beginning but this is no problem as it is part of the same interval
                                //get from split range the first use point of the current bb
                                uint32_t first_use_point_of_interval_in_bb = get_first_use_point_of_interval_in_bb(liveness_intervals.at(interval_idx), control_flow_TAC, bb_idx);

                                for(int32_t instr_idx_of_bb = first_use_point_of_interval_in_bb - 1; instr_idx_of_bb >= 0; --instr_idx_of_bb){
                                    if(((control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::LOADBYTE) ||  (control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::LOADHALFWORD) ||  (control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::LOADWORD) ||  (control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).op == Operation::LOADWORD_WITH_NUMBER_OFFSET))){
                                        found_load_in_original_bb = true;
                                        if(control_flow_TAC.at(bb_idx).TAC.at(instr_idx_of_bb).sensitivity == 1){
                                            uint32_t instr_offset_of_prev_bbs = get_idx_of_bb_start(control_flow_TAC, bb_idx);
                                            #ifdef ARMV6_M
                                            break_load(control_flow_TAC.at(bb_idx), liveness_intervals, instr_offset_of_prev_bbs, cfg_instr_idx, cfg_instr_idx + instr_offset_of_prev_bbs);
                                            #endif
                                        }
                                        break;
                                    }
                                }

                                if(!found_load_in_original_bb){
                                    bool found_load_in_previous_bb = false;
                                    //if we did not find and load instruction
                                    if(predecessors.find(bb_number) != predecessors.end()){
                                        uint8_t global_load_sens = 0;
                                        for(auto& predec: predecessors.at(bb_number)){
                                            uint8_t local_load_sens = 0;
                                            std::vector<bool> visited(control_flow_TAC.size(), false);
                                            check_all_predecessor_paths(control_flow_TAC, 1, visited, predec, predecessors, local_load_sens, found_load_in_previous_bb ); 
                                            global_load_sens |= local_load_sens;
                                            if(global_load_sens == 1){
                                                break;
                                            }
                                        }

                                        if(global_load_sens == 1){
                                            uint32_t instr_offset_of_prev_bbs = get_idx_of_bb_start(control_flow_TAC, bb_idx);
                                            #ifdef ARMV6_M
                                            break_load(control_flow_TAC.at(bb_idx), liveness_intervals, instr_offset_of_prev_bbs, cfg_instr_idx, cfg_instr_idx + instr_offset_of_prev_bbs);
                                            #endif
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}


//if there exist an interval that uses the available register before any sensitive register in every path we are secure, otherwise we have to add an instruction to break the dependency
//uses dfs to check the paths
void necessity_to_remove_vertical_dependency(std::vector<TAC_bb>& control_flow_TAC, uint32_t bb_idx_of_available_sens_reg, std::vector<liveness_interval_struct>& liveness_intervals, uint32_t succ, const std::map<uint32_t, std::vector<uint32_t>>& successor_map, bool& need_to_clear_register, std::vector<bool>& visited, uint32_t available_sensitive_real_register){

    visited.at(succ) = true;

    if(bb_idx_of_available_sens_reg >= succ){
        uint32_t instr_idx_start_of_bb = get_idx_of_bb_start(control_flow_TAC, succ);
        uint32_t instr_idx_end_of_bb = get_idx_of_bb_end(control_flow_TAC, succ);

        //get (if possible) the earlist sensitive register in this basic block -> start of range of liveness interval has to be <= start of bb and end of range has to be >= end of bb
        auto it_first_sensitive_register_in_bb = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [instr_idx_start_of_bb](const liveness_interval_struct& t){return (std::get<0>(t.ranges.back()) <= instr_idx_start_of_bb) && (std::get<1>(t.ranges.back()) >= instr_idx_start_of_bb) && (t.sensitivity == 1);});

        //if there exist no interval that is sensitive at the beginning of the bb look for a interval that becomes sensitivie during the interval
        if(it_first_sensitive_register_in_bb == liveness_intervals.end()){
            it_first_sensitive_register_in_bb = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [instr_idx_start_of_bb, instr_idx_end_of_bb](const liveness_interval_struct& t){return (std::get<0>(t.ranges.back()) >= instr_idx_start_of_bb) && (std::get<1>(t.ranges.back()) <= instr_idx_end_of_bb) && (t.sensitivity == 1);});
        }

        //get (if possible) the first interval that is non-sensitive and uses the available register
        auto it_first_non_sensitive_interval_that_uses_available_register = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [instr_idx_start_of_bb, available_sensitive_real_register](const liveness_interval_struct& t){return (std::get<0>(t.ranges.back()) <= instr_idx_start_of_bb) && (std::get<1>(t.ranges.back()) >= instr_idx_start_of_bb) && (t.real_register_number == available_sensitive_real_register);});
        if(it_first_non_sensitive_interval_that_uses_available_register == liveness_intervals.end()){
            it_first_non_sensitive_interval_that_uses_available_register = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [instr_idx_start_of_bb, instr_idx_end_of_bb, available_sensitive_real_register](const liveness_interval_struct& t){return (std::get<0>(t.ranges.back()) >= instr_idx_start_of_bb) && (std::get<1>(t.ranges.back()) <= instr_idx_end_of_bb) && (t.real_register_number == available_sensitive_real_register);});
        }
        //if both of those intervals do not exist we go to the next depth (i.e. there is no sensitive interval during this bb and there is no non-sensitive register that uses the available register)
        if((it_first_sensitive_register_in_bb == liveness_intervals.end()) && (it_first_non_sensitive_interval_that_uses_available_register == liveness_intervals.end())){

        }
        else{ // we can make a statement about the necessity to insert an instruction

            if((it_first_sensitive_register_in_bb == liveness_intervals.end())){
                need_to_clear_register = (need_to_clear_register | false);
            }
            else if(it_first_non_sensitive_interval_that_uses_available_register == liveness_intervals.end()){
                need_to_clear_register = (need_to_clear_register | true);

            }
            else{
                if(it_first_non_sensitive_interval_that_uses_available_register->liveness_used.at(0) < it_first_sensitive_register_in_bb->liveness_used.at(0)){
                    need_to_clear_register = (need_to_clear_register | false);

                }
                else{
                    need_to_clear_register = (need_to_clear_register | true);

                }
            }
        }
    }

    if(successor_map.find(succ) != successor_map.end()){
        auto successors = successor_map.at(succ);

        for(uint32_t new_succ: successors){
            if(visited.at(new_succ) != true){
                necessity_to_remove_vertical_dependency(control_flow_TAC, bb_idx_of_available_sens_reg, liveness_intervals, new_succ, successor_map, need_to_clear_register, visited, available_sensitive_real_register);
            }
        }
    }

}

void break_vertical_in_loops(std::vector<TAC_bb>& control_flow_TAC, std::vector<TAC_type>& TAC, const std::map<uint32_t, std::vector<uint32_t>>& successor_map,  std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m, std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion){
    //go over every available sensitive registers as potentially all of them could interfere
    for(uint32_t available_idx = 0; available_idx < available_registers.size(); ++available_idx){
        if(std::get<1>(available_registers.at(available_idx)) == 1){    //found sensitive register
            uint32_t instr_idx_where_sens_available_last_used = std::get<2>(available_registers.at(available_idx));
            uint32_t bb_idx_of_available_sens_reg = get_bb_idx_of_instruction_idx(control_flow_TAC, instr_idx_where_sens_available_last_used);

            //critical if there exist a path from bb where sensitive register was freed to a bb that has a sensitive interval before making the real register non-sensitive
            if(successor_map.find(bb_idx_of_available_sens_reg) != successor_map.end()){
                for(auto& succ: successor_map.at(bb_idx_of_available_sens_reg)){ //go through all successors of bb
                        bool need_to_clear_register = false;
                        std::vector<bool> visited(control_flow_TAC.size(), false);
                        necessity_to_remove_vertical_dependency(control_flow_TAC, bb_idx_of_available_sens_reg, liveness_intervals, succ, successor_map, need_to_clear_register, visited, std::get<0>(available_registers.at(available_idx)));
                        if(need_to_clear_register){

                            //break sensitivity of all available register
                            uint32_t instruction_idx_to_insert = instr_idx_where_sens_available_last_used + 1;
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
                            uint32_t dest_v = std::get<0>(available_registers.at(available_idx));
                            uint8_t dest_sens = 0;
                            
                            std::get<1>(available_registers.at(available_idx)) = 0;
                            generate_generic_tmp_tac(tmp_tac, op, sens, dest_type, dest_v, dest_sens, src1_type, src1_v, src1_sens, src2_type, src2_v, src2_sens, false);

                            TAC.insert(TAC.begin() + instruction_idx_to_insert, tmp_tac);
                            insert_new_instruction_in_CFG_TAC(control_flow_TAC, instruction_idx_to_insert, tmp_tac);

                            synchronize_instruction_index(liveness_intervals, active, available_registers, instruction_idx_to_insert, track_xor_specification_armv6m, postponed_store_insertion);

                        }
                }
            }

        }
    }
}




//to break vertical dependency any two sensitive intervals must not overlap during the whole execution
//if we identify two sensitive intervals that intersect (i.e. their ranges overlap) they will be split in such a way that they do not overlap anymore
//if a interval is split into two parts the interval will be spilled to memory
void break_vertical(std::vector<liveness_interval_struct>& liveness_intervals, memory_security_struct& memory_security){



    uint32_t original_intervals_size = liveness_intervals.size();
    std::vector<uint32_t> indices_of_original_sensitive_intervals;
    std::vector<liveness_interval_struct> all_sensitive_intervals;

    //get all sensitive intervals in one vector
    for(uint32_t index_of_current_interval = 0; index_of_current_interval < original_intervals_size; ++index_of_current_interval){
        if(liveness_intervals.at(index_of_current_interval).sensitivity == 1){

            // assign_memory_position(liveness_intervals.at(index_of_current_interval), memory_security);
            all_sensitive_intervals.push_back(liveness_intervals.at(index_of_current_interval));
            indices_of_original_sensitive_intervals.emplace_back(index_of_current_interval);

        }
    }

    std::vector<liveness_interval_struct> all_sensitive_intervals_after_splitting;

    //process every sensitive interval
    while(!all_sensitive_intervals.empty()){
    
        //get one sensitive interval to compare
        liveness_interval_struct first_sensitive_interval = all_sensitive_intervals.at(0);
        bool no_interference_of_current_interval = true;

        //go over all other intervals to compare with
        for(uint32_t interval_idx = 1; interval_idx < all_sensitive_intervals.size(); ++interval_idx){
            liveness_interval_struct later_sensivite_interval = all_sensitive_intervals.at(interval_idx);

            //vertical dependency if the use points of the two intervals to compare overlap
            if((!(first_sensitive_interval.liveness_used.at(0) >= later_sensivite_interval.liveness_used.back())) && (!(first_sensitive_interval.liveness_used.back() <= later_sensivite_interval.liveness_used.at(0)))){
                
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
                       continue; 
                    }
                    reference_value = diff.at(0);
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

                        //split_into_bb_ranges_of_intervals(control_flow_TAC, sub_interval_y, predecessors);

                        all_sensitive_intervals.push_back(sub_interval_y);

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


                        all_sensitive_intervals.push_back(sub_interval_x);

                        X.liveness_used.erase(X.liveness_used.begin(), it_sub_interval_x_end);
                    }


                }
    


                all_sensitive_intervals.erase(all_sensitive_intervals.begin() + interval_idx);  //remove later_sensivite_interval as it was processed by while loop above
                all_sensitive_intervals.erase(all_sensitive_intervals.begin());                 //remove first_sensitive_interval as it was processed by while loop above
                


                no_interference_of_current_interval = false;
                break;
            }

        }

        if(no_interference_of_current_interval){
            all_sensitive_intervals_after_splitting.push_back(first_sensitive_interval); // if the first interval does not overlap with any other later interval we move it to the finished data structure
            all_sensitive_intervals.erase(all_sensitive_intervals.begin());
        }
    }


    liveness_intervals.insert(liveness_intervals.end(), all_sensitive_intervals_after_splitting.begin(), all_sensitive_intervals_after_splitting.end());

    //erase the old active intervals
    for(int32_t i = indices_of_original_sensitive_intervals.size() - 1; i >= 0; --i){
        liveness_intervals.erase(liveness_intervals.begin() + indices_of_original_sensitive_intervals.at(i));
    }

}