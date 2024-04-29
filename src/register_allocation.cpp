#include "register_allocation.hpp"

void add_range(liveness_interval_struct& liveness_interval, uint32_t block_from, uint32_t instruction_idx){

    //if beginning of range not set, assign block_from
    if(std::get<0>(liveness_interval.ranges.at(0)) == UINT32_MAX){
        std::get<0>(liveness_interval.ranges.at(0)) = block_from;
    }
    else if(std::get<0>(liveness_interval.ranges.at(0)) > block_from){ //if block_from is smaller than beginning assign block_from to beginning of range
        std::get<0>(liveness_interval.ranges.at(0)) = block_from;
    }

    //if end of range not set, assign instruction_idx
    if(std::get<1>(liveness_interval.ranges.at(0)) == UINT32_MAX){
        std::get<1>(liveness_interval.ranges.at(0)) = instruction_idx;
    }
    else if(std::get<1>(liveness_interval.ranges.at(0)) < instruction_idx){ //if instruction_idx is greater than end assing instruction_idx to end of range
        std::get<1>(liveness_interval.ranges.at(0)) = instruction_idx;
    }
}

void compute_liveness_intervals(std::vector<uint32_t>& reverse_block_order, std::vector<uint32_t>& start_points_of_blocks, std::vector<uint32_t>& end_points_of_blocks, std::vector<TAC_type>& TAC, std::vector<global_live_set>& global_live_sets, std::vector<liveness_interval_struct>& liveness_intervals, uint32_t number_of_virtual_registers){
    //there are as many intervals as virtual registers
    liveness_intervals.resize(number_of_virtual_registers + v_function_borders); // +1 because we have a symbolic register that marks the border between pre and post function call
    for(uint32_t interval_idx = 0; interval_idx < number_of_virtual_registers; ++interval_idx){
        liveness_intervals.at(interval_idx).is_spilled = false;
        liveness_intervals.at(interval_idx).position_on_stack = UINT32_MAX;
        liveness_intervals.at(interval_idx).virtual_register_number = interval_idx;
        liveness_intervals.at(interval_idx).real_register_number = UINT32_MAX;
        liveness_intervals.at(interval_idx).sensitivity = 0;
        liveness_intervals.at(interval_idx).liveness_used.shrink_to_fit();
        liveness_intervals.at(interval_idx).ranges.clear();
        liveness_intervals.at(interval_idx).ranges.emplace_back(std::make_tuple(UINT32_MAX, UINT32_MAX));
    }

    for(uint32_t func_border_idx = 0; func_border_idx < v_function_borders; ++func_border_idx){
        liveness_intervals.at(number_of_virtual_registers + func_border_idx).is_spilled = false;
        liveness_intervals.at(number_of_virtual_registers + func_border_idx).position_on_stack = UINT32_MAX;
        liveness_intervals.at(number_of_virtual_registers + func_border_idx).virtual_register_number = UINT32_MAX - func_border_idx - 1;
        liveness_intervals.at(number_of_virtual_registers + func_border_idx).real_register_number = UINT32_MAX;
        liveness_intervals.at(number_of_virtual_registers + func_border_idx).sensitivity = 0;
        liveness_intervals.at(number_of_virtual_registers + func_border_idx).liveness_used.shrink_to_fit();
        liveness_intervals.at(number_of_virtual_registers + func_border_idx).ranges.clear();
        liveness_intervals.at(number_of_virtual_registers + func_border_idx).ranges.emplace_back(std::make_tuple(UINT32_MAX, UINT32_MAX));
    }

    
    for(uint32_t i = 0; i < reverse_block_order.size(); ++i){
        uint32_t start_point = start_points_of_blocks.at(reverse_block_order.at(i));
        uint32_t end_point = end_points_of_blocks.at(reverse_block_order.at(i));

        //get idx of start and idx of end of block
        uint32_t block_from = start_point; 
        uint32_t block_to = end_point;

        for(auto virt_register: global_live_sets.at(reverse_block_order.at(i)).live_out){
            add_range(liveness_intervals.at(virt_register), block_from, block_to);

        }

        //operations in reverse order
        for(uint32_t instruction_idx = block_to; instruction_idx >= block_from ; --instruction_idx){
            switch(TAC.at(instruction_idx).op){
                case Operation::NONE: break;
                case Operation::NEG:
                case Operation::FUNC_MOVE:
                case Operation::FUNC_RES_MOV:
                case(Operation::MOV): {
                    if(static_cast<uint32_t>(TAC.at(instruction_idx).dest_v_reg_idx) >= (UINT32_MAX - v_function_borders)){ //this defines the liveness of the symbolic register that marks the border between pre- and post-function cal
                        liveness_intervals.at(number_of_virtual_registers + (UINT32_MAX - TAC.at(instruction_idx).dest_v_reg_idx - 1)).liveness_used.emplace_back(instruction_idx);
                        liveness_intervals.at(number_of_virtual_registers + (UINT32_MAX - TAC.at(instruction_idx).dest_v_reg_idx - 1)).sensitivity = TAC.at(instruction_idx).dest_sensitivity;
                        std::get<0>(liveness_intervals.at(number_of_virtual_registers + (UINT32_MAX - TAC.at(instruction_idx).dest_v_reg_idx - 1)).ranges.at(0)) = instruction_idx;
                        std::get<1>(liveness_intervals.at(number_of_virtual_registers + (UINT32_MAX - TAC.at(instruction_idx).dest_v_reg_idx - 1)).ranges.at(0)) = instruction_idx;
                    }
                    else{
                        if(TAC.at(instruction_idx).dest_type == TAC_OPERAND::V_REG){
                            liveness_intervals.at(TAC.at(instruction_idx).dest_v_reg_idx).liveness_used.emplace_back(instruction_idx);
                            liveness_intervals.at(TAC.at(instruction_idx).dest_v_reg_idx).sensitivity |= return_sensitivity(TAC.at(instruction_idx).dest_sensitivity);
                            std::get<0>(liveness_intervals.at(TAC.at(instruction_idx).dest_v_reg_idx).ranges.at(0)) = instruction_idx;

                        }
                        if(TAC.at(instruction_idx).src1_type == TAC_OPERAND::V_REG){
                            liveness_intervals.at(TAC.at(instruction_idx).src1_v).liveness_used.emplace_back(instruction_idx);
                            liveness_intervals.at(TAC.at(instruction_idx).src1_v).sensitivity |= return_sensitivity(TAC.at(instruction_idx).src1_sensitivity);
                            add_range(liveness_intervals.at(TAC.at(instruction_idx).src1_v), block_from, instruction_idx);
                        }
                    }


                    break;
                }
                case Operation::ADD :
                case Operation::ADD_INPUT_STACK : 
                case Operation::SUB :
                case Operation::MUL :
                case Operation::DIV :
                case Operation::XOR :
                case Operation::LSL :
                case Operation::LSR :
                case Operation::LOGICAL_AND:
                case Operation::LOGICAL_OR: {
                    if(TAC.at(instruction_idx).dest_type == TAC_OPERAND::V_REG){
                        liveness_intervals.at(TAC.at(instruction_idx).dest_v_reg_idx).liveness_used.emplace_back(instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).dest_v_reg_idx).sensitivity |= return_sensitivity(TAC.at(instruction_idx).dest_sensitivity);
                        std::get<0>(liveness_intervals.at(TAC.at(instruction_idx).dest_v_reg_idx).ranges.at(0)) = instruction_idx;
                    }

                    if(TAC.at(instruction_idx).src1_type == TAC_OPERAND::V_REG){
                        liveness_intervals.at(TAC.at(instruction_idx).src1_v).liveness_used.emplace_back(instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).src1_v).sensitivity |= return_sensitivity(TAC.at(instruction_idx).src1_sensitivity);
                        add_range(liveness_intervals.at(TAC.at(instruction_idx).src1_v), block_from, instruction_idx);

                    }

                    if(TAC.at(instruction_idx).src2_type == TAC_OPERAND::V_REG){
                        liveness_intervals.at(TAC.at(instruction_idx).src2_v).liveness_used.emplace_back(instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).src2_v).sensitivity |= return_sensitivity(TAC.at(instruction_idx).src2_sensitivity);
                        add_range(liveness_intervals.at(TAC.at(instruction_idx).src2_v), block_from, instruction_idx);

                    }


                    break;
                }
                case Operation::LOADBYTE : 
                case Operation::LOADHALFWORD : 
                case Operation::LOADWORD_FROM_INPUT_STACK : 
                case Operation::LOADWORD_WITH_LABELNAME:
                case Operation::LOADWORD_NUMBER_INTO_REGISTER:
                case Operation::LOADWORD : 
                {

                    if(TAC.at(instruction_idx).dest_type == TAC_OPERAND::V_REG){
                        liveness_intervals.at(TAC.at(instruction_idx).dest_v_reg_idx).liveness_used.emplace_back(instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).dest_v_reg_idx).sensitivity |= return_sensitivity(TAC.at(instruction_idx).dest_sensitivity);
                        std::get<0>(liveness_intervals.at(TAC.at(instruction_idx).dest_v_reg_idx).ranges.at(0)) = instruction_idx;
                    }
                    if(TAC.at(instruction_idx).src1_type == TAC_OPERAND::V_REG){
                        liveness_intervals.at(TAC.at(instruction_idx).src1_v).liveness_used.emplace_back(instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).src1_v).sensitivity |= return_sensitivity(TAC.at(instruction_idx).src1_sensitivity);
                        add_range(liveness_intervals.at(TAC.at(instruction_idx).src1_v), block_from, instruction_idx);

                    }

                    break;
                }
                case Operation::STOREBYTE : 
                case Operation::STOREHALFWORD : 
                case Operation::STOREWORD : 
                {
                    if(TAC.at(instruction_idx).src1_type == TAC_OPERAND::V_REG){
                        // add_range(liveness_intervals.at(TAC.at(instruction_idx).src1_v), block_from, instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).src1_v).liveness_used.emplace_back(instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).src1_v).sensitivity |= return_sensitivity(TAC.at(instruction_idx).src1_sensitivity);
                        add_range(liveness_intervals.at(TAC.at(instruction_idx).src1_v), block_from, instruction_idx);

                    }

                    if(TAC.at(instruction_idx).src2_type == TAC_OPERAND::V_REG){
                        // add_range(liveness_intervals.at(TAC.at(instruction_idx).src2_v), block_from, instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).src2_v).liveness_used.emplace_back(instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).src2_v).sensitivity |= return_sensitivity(TAC.at(instruction_idx).src2_sensitivity);
                        add_range(liveness_intervals.at(TAC.at(instruction_idx).src2_v), block_from, instruction_idx);

                    }
                    break;
                }
                case Operation::PUSH:
                case Operation::POP: 
                {
                    if(TAC.at(instruction_idx).src1_type == TAC_OPERAND::V_REG){
                        // add_range(liveness_intervals.at(TAC.at(instruction_idx).src1_v), block_from, instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).src1_v).liveness_used.emplace_back(instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).src1_v).sensitivity |= return_sensitivity(TAC.at(instruction_idx).src1_sensitivity);
                        add_range(liveness_intervals.at(TAC.at(instruction_idx).src1_v), block_from, instruction_idx);

                    }
                    break;
                }
                case Operation::BRANCH:
                case Operation::LESS_EQUAL:
                case Operation::LESS:
                case Operation::GREATER:
                case Operation::GREATER_EQUAL:
                case Operation::EQUAL:
                case Operation::NOT_EQUAL:
                
                case Operation::FUNC_CALL: break;
                case Operation::LABEL: break;

                case Operation::COMPARE: {
                    if(TAC.at(instruction_idx).src1_type == TAC_OPERAND::V_REG){
                        // add_range(liveness_intervals.at(TAC.at(instruction_idx).src1_v), block_from, instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).src1_v).liveness_used.emplace_back(instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).src1_v).sensitivity |= return_sensitivity(TAC.at(instruction_idx).src1_sensitivity);
                        add_range(liveness_intervals.at(TAC.at(instruction_idx).src1_v), block_from, instruction_idx);

                    }

                    if(TAC.at(instruction_idx).src2_type == TAC_OPERAND::V_REG){
                        // add_range(liveness_intervals.at(TAC.at(instruction_idx).src2_v), block_from, instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).src2_v).liveness_used.emplace_back(instruction_idx);
                        liveness_intervals.at(TAC.at(instruction_idx).src2_v).sensitivity |= return_sensitivity(TAC.at(instruction_idx).src2_sensitivity);
                        add_range(liveness_intervals.at(TAC.at(instruction_idx).src2_v), block_from, instruction_idx);


                    }
                    break;
                }

                default: std::cout << static_cast<uint32_t>(TAC.at(instruction_idx).op) << std::endl; throw std::runtime_error("3 in compute_local_live_sets: operation unknown");
            }

        }
    }

    //merge ranges that are adjacent
    for(uint32_t interval_idx = 0; interval_idx < number_of_virtual_registers; ++interval_idx){
        std::sort(liveness_intervals.at(interval_idx).liveness_used.begin(), liveness_intervals.at(interval_idx).liveness_used.end());
        liveness_intervals.at(interval_idx).liveness_used.erase(std::unique(liveness_intervals.at(interval_idx).liveness_used.begin(), liveness_intervals.at(interval_idx).liveness_used.end()), liveness_intervals.at(interval_idx).liveness_used.end());
    }

}


bool any_live_set_changed(std::vector<global_live_set>& old_global_live_sets,  std::vector<global_live_set> updated_global_live_sets){
    for(uint32_t idx = 0; idx < updated_global_live_sets.size(); ++idx){
        if(updated_global_live_sets.at(idx).live_out != old_global_live_sets.at(idx).live_out){
            return true;
        }
        if(updated_global_live_sets.at(idx).live_in != old_global_live_sets.at(idx).live_in){
            return true;
        }
    }
    return false;
}


//propagate the local liveness through the CFG to build global live sets
//each global live set has two attributes
// live_in : a variable is live at the beginning of the bb
// live_out : a variable is live at the end of the bb
void compute_global_live_sets(std::vector<uint32_t>& reverse_block_order, std::map<uint32_t, std::vector<uint32_t>>& successor_map, std::vector<local_live_set>& local_live_sets, std::vector<global_live_set>& global_live_sets){
    global_live_sets.resize(reverse_block_order.size());
    for(auto elem: global_live_sets){
        elem.live_in.clear();
        elem.live_out.clear();
    }

    std::vector<global_live_set> old_global_live_sets;
    do{
        old_global_live_sets = global_live_sets;
        for(auto& reverse_order_element: reverse_block_order){
            global_live_sets.at(reverse_order_element).live_out.clear();
            auto successors = successor_map.find(reverse_order_element)->second;
            for(auto succ: successors){
                global_live_sets.at(reverse_order_element).live_out.insert(global_live_sets.at(succ).live_in.begin(), global_live_sets.at(succ).live_in.end());
            }

            std::set<uint32_t> set_difference;

            std::set_difference(global_live_sets.at(reverse_order_element).live_out.begin(), global_live_sets.at(reverse_order_element).live_out.end(), local_live_sets.at(reverse_order_element).live_kill.begin(), local_live_sets.at(reverse_order_element).live_kill.end(), std::inserter(set_difference, set_difference.end()));

            set_difference.insert(local_live_sets.at(reverse_order_element).live_gen.begin(), local_live_sets.at(reverse_order_element).live_gen.end());

            global_live_sets.at(reverse_order_element).live_in = set_difference;
        }
    } while(any_live_set_changed(old_global_live_sets, global_live_sets));

}

//compute local live sets
//each local live set has two attributes
// -live_gen aka liveUse: a variable was used (aka source register) before it is defined (aka destination register)
void compute_local_live_sets(std::vector<local_live_set>& local_live_sets, uint32_t local_live_sets_start_index, std::vector<TAC_type>& TAC){
    local_live_set new_live_set;
    new_live_set.live_gen.clear(); //liveUse
    new_live_set.live_kill.clear(); //liveDef
    
    for(uint32_t tac_idx = local_live_sets_start_index; tac_idx < TAC.size(); ++tac_idx){
        
        switch(TAC.at(tac_idx).op){
            case Operation::NONE: break;
            case (Operation::NEG):
            case (Operation::FUNC_MOVE):
            case (Operation::FUNC_RES_MOV):
            case(Operation::MOV): {


                if(TAC.at(tac_idx).src1_type == TAC_OPERAND::V_REG){
                    //if register is not yet in live_ill insert into live_gen
                    if(new_live_set.live_kill.find(TAC.at(tac_idx).src1_v) == new_live_set.live_kill.end()){
                        new_live_set.live_gen.insert(TAC.at(tac_idx).src1_v);
                    }
                }

                if(TAC.at(tac_idx).dest_type == TAC_OPERAND::V_REG){
                    new_live_set.live_kill.insert(TAC.at(tac_idx).dest_v_reg_idx);
                }

                break;
            }
            case Operation::ADD :
            case Operation::ADD_INPUT_STACK : 
            case Operation::SUB :
            case Operation::MUL :
            case Operation::DIV :
            case Operation::XOR :
            case Operation::LSL :
            case Operation::LSR : 
            case Operation::LOGICAL_AND:
            case Operation::LOGICAL_OR: {


                if(TAC.at(tac_idx).src1_type == TAC_OPERAND::V_REG){
                    if(new_live_set.live_kill.find(TAC.at(tac_idx).src1_v) == new_live_set.live_kill.end()){
                        new_live_set.live_gen.insert(TAC.at(tac_idx).src1_v);
                    }
                }

                if(TAC.at(tac_idx).src2_type == TAC_OPERAND::V_REG){
                    if(new_live_set.live_kill.find(TAC.at(tac_idx).src2_v) == new_live_set.live_kill.end()){
                        new_live_set.live_gen.insert(TAC.at(tac_idx).src2_v);
                    }
                }

                if(TAC.at(tac_idx).dest_type == TAC_OPERAND::V_REG){
                    new_live_set.live_kill.insert(TAC.at(tac_idx).dest_v_reg_idx);
                }
                break;
            }
            case Operation::LOADBYTE : 
            case Operation::LOADHALFWORD : 
            case Operation::LOADWORD_FROM_INPUT_STACK :
            case Operation::LOADWORD_WITH_LABELNAME:
            case Operation::LOADWORD_NUMBER_INTO_REGISTER: 
            case Operation::LOADWORD : 
            {


                if(TAC.at(tac_idx).src1_type == TAC_OPERAND::V_REG){
                    if(new_live_set.live_kill.find(TAC.at(tac_idx).src1_v) == new_live_set.live_kill.end()){
                        new_live_set.live_gen.insert(TAC.at(tac_idx).src1_v);
                    }
                }

                if(TAC.at(tac_idx).dest_type == TAC_OPERAND::V_REG){
                    new_live_set.live_kill.insert(TAC.at(tac_idx).dest_v_reg_idx);
                }
                break;
            }
            case Operation::STOREBYTE : 
            case Operation::STOREHALFWORD : 
            case Operation::STOREWORD : 
            {
                if(TAC.at(tac_idx).src1_type == TAC_OPERAND::V_REG){
                    if(new_live_set.live_kill.find(TAC.at(tac_idx).src1_v) == new_live_set.live_kill.end()){
                        new_live_set.live_gen.insert(TAC.at(tac_idx).src1_v);
                    }
                }

                if(TAC.at(tac_idx).src2_type == TAC_OPERAND::V_REG){
                    if(new_live_set.live_kill.find(TAC.at(tac_idx).src2_v) == new_live_set.live_kill.end()){
                        new_live_set.live_gen.insert(TAC.at(tac_idx).src2_v);
                    }
                }
                break;
            }
            case Operation::PUSH:
            case Operation::POP: 
            {
                if(TAC.at(tac_idx).src1_type == TAC_OPERAND::V_REG){
                    if(new_live_set.live_kill.find(TAC.at(tac_idx).src1_v) == new_live_set.live_kill.end()){
                        new_live_set.live_gen.insert(TAC.at(tac_idx).src1_v);
                    }
                }
                break;
            }
            case Operation::BRANCH:
            case Operation::LESS_EQUAL:
            case Operation::LESS:
            case Operation::GREATER:
            case Operation::GREATER_EQUAL:
            case Operation::EQUAL:
            case Operation::NOT_EQUAL:
            
            case Operation::FUNC_CALL: break;
            case Operation::LABEL: break;

            case Operation::COMPARE: {
                if(TAC.at(tac_idx).src1_type == TAC_OPERAND::V_REG){
                    if(new_live_set.live_kill.find(TAC.at(tac_idx).src1_v) == new_live_set.live_kill.end()){
                        new_live_set.live_gen.insert(TAC.at(tac_idx).src1_v);
                    }
                }

                if(TAC.at(tac_idx).src2_type == TAC_OPERAND::V_REG){
                    if(new_live_set.live_kill.find(TAC.at(tac_idx).src2_v) == new_live_set.live_kill.end()){
                        new_live_set.live_gen.insert(TAC.at(tac_idx).src2_v);
                    }
                }
                break;
            }

            default: std::cout << static_cast<uint32_t>(TAC.at(tac_idx).op) << std::endl; throw std::runtime_error("2 in compute_local_live_sets: operation unknown");
        }
    }


    local_live_sets.push_back( new_live_set);
}


void expire_old_intervals_rolled(liveness_interval_struct& liveness_interval, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint32_t, uint8_t>>& allocated_registers, std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers){

    auto it_active = active.begin();
    while(it_active != active.end()){
        if(std::get<1>(it_active->ranges.back()) >= std::get<0>(liveness_interval.ranges.back())){
            break;
        }
        //remove j from active and add the register that was assigned to j to pool of free registers
        uint32_t virt_reg = it_active->virtual_register_number; //get virtual register of current interval
        uint8_t virt_reg_sens = it_active->sensitivity;
        auto alloc_it = std::find_if(allocated_registers.begin(), allocated_registers.end(), [virt_reg](std::tuple<uint32_t, uint32_t, uint8_t>& t){return std::get<0>(t) == virt_reg;}); //get the mapping of virtual register and real register
        
        
        if(alloc_it == allocated_registers.end()){
            print_interval(*it_active);
            throw std::runtime_error("in expire_old_intervals_rolled: can not find the register that is allocated for this interval");
        }




        //in xor restriction case we have two intervals in active that have the same real register -> if we free one of them and make the register available we will get a wrong result as the other interval still possesses this register
        //therefore we only place the register in available when the register is not anymore in track_xor_specification
        #ifdef ARMV6_M
        uint32_t real_register_of_expired_interval = std::get<1>(*alloc_it);
        if(std::find_if(active.begin(), active.end(), [real_register_of_expired_interval, virt_reg](const liveness_interval_struct& t){return (t.real_register_number == real_register_of_expired_interval) && (t.virtual_register_number != virt_reg);}) == active.end()){

            available_registers.push_back(std::make_tuple(std::get<1>(*alloc_it), virt_reg_sens, it_active->liveness_used.back(), it_active->virtual_register_number, false)); //remove mapping from allocated registers
            //available_registers.push_back(std::make_tuple(std::get<1>(*alloc_it), virt_reg_sens, it_active->liveness_used.back(), it_active->virtual_register_number)); //remove mapping from allocated registers
            
            
        }
        else{

        }
        #endif
        
        it_active = active.erase(it_active);
        allocated_registers.erase(alloc_it); //add real register back to available real registers
        
    }

    
}

void split_and_insert(std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC, std::vector<liveness_interval_struct>& liveness_intervals, std::vector<liveness_interval_struct>& active,std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers, uint32_t& instruction_idx_to_split, uint32_t index_of_interval_to_spill, uint32_t index_of_current_interval, memory_security_struct& memory_security, const std::map<uint32_t, std::vector<uint32_t>>& predecessors, std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m, std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion){
    //the register assigment of this interval is valid until instruction_idx, therefore we create one interval which is valid until this index
    liveness_interval_struct valid_interval_up_to_instruction_idx;

    //it will get all attributes from liveness_interval but with different ranges
    valid_interval_up_to_instruction_idx.sensitivity = liveness_intervals.at(index_of_interval_to_spill).sensitivity;
    valid_interval_up_to_instruction_idx.is_spilled = liveness_intervals.at(index_of_interval_to_spill).is_spilled;
    valid_interval_up_to_instruction_idx.virtual_register_number = liveness_intervals.at(index_of_interval_to_spill).virtual_register_number;
    valid_interval_up_to_instruction_idx.real_register_number = liveness_intervals.at(index_of_interval_to_spill).real_register_number;
    valid_interval_up_to_instruction_idx.position_on_stack = liveness_intervals.at(index_of_interval_to_spill).position_on_stack;
    valid_interval_up_to_instruction_idx.available_registers_after_own_allocation = liveness_intervals.at(index_of_interval_to_spill).available_registers_after_own_allocation;

    for(auto liveness_used_elem: liveness_intervals.at(index_of_interval_to_spill).liveness_used){
        if(liveness_used_elem < instruction_idx_to_split){
            valid_interval_up_to_instruction_idx.liveness_used.emplace_back(liveness_used_elem);
        }
        else{
            break;
        }
    }



    for(auto range_elem: liveness_intervals.at(index_of_interval_to_spill).ranges){
        if(std::get<1>(range_elem) < instruction_idx_to_split){ //end of range is smaller than instruction to strip -> full range can be inserted into new container
            valid_interval_up_to_instruction_idx.ranges.push_back(range_elem);
        }
        else if((std::get<0>(range_elem) < instruction_idx_to_split) && (std::get<1>(range_elem) >= instruction_idx_to_split)){
            // valid_interval_up_to_instruction_idx.ranges.push_back(std::make_tuple(std::get<0>(range_elem), instruction_idx)); //TODO: range might be wrong
            valid_interval_up_to_instruction_idx.ranges.push_back(std::make_tuple(std::get<0>(range_elem), instruction_idx_to_split-1)); //TODO: range might be wrong
        }
        else{
            break;
        }
    }

    //insert loads and stores at positions where we have divergend control flow
    split_into_bb_ranges_of_intervals(control_flow_TAC, valid_interval_up_to_instruction_idx, predecessors);


    liveness_intervals.push_back(valid_interval_up_to_instruction_idx);

    uint32_t idx_of_old_interval = liveness_intervals.size() - 1;


    //all use points >= instruction_idx have not been seen and can get a *new* register, therefore we need to split them into atomic intervals 
    for(uint32_t use_point_idx = 0; use_point_idx < liveness_intervals.at(index_of_interval_to_spill).liveness_used.size(); ++use_point_idx){
        if(liveness_intervals.at(index_of_interval_to_spill).liveness_used.at(use_point_idx) >= instruction_idx_to_split){
            liveness_interval_struct atomic_interval;
            atomic_interval.sensitivity = liveness_intervals.at(index_of_interval_to_spill).sensitivity;
            atomic_interval.is_spilled = liveness_intervals.at(index_of_interval_to_spill).is_spilled;
            atomic_interval.virtual_register_number = liveness_intervals.at(index_of_interval_to_spill).virtual_register_number;
            atomic_interval.real_register_number = UINT32_MAX;
            atomic_interval.position_on_stack = liveness_intervals.at(index_of_interval_to_spill).position_on_stack;
            if(TAC.at(liveness_intervals.at(index_of_interval_to_spill).liveness_used.at(use_point_idx)).op == Operation::FUNC_MOVE){
                atomic_interval.liveness_used.emplace_back(liveness_intervals.at(index_of_interval_to_spill).liveness_used.at(use_point_idx));
                atomic_interval.ranges.push_back(std::make_tuple(liveness_intervals.at(index_of_interval_to_spill).liveness_used.at(use_point_idx) - TAC.at(liveness_intervals.at(index_of_interval_to_spill).liveness_used.at(use_point_idx)).func_move_idx, atomic_interval.liveness_used.back()));
                
            }
            else{
                atomic_interval.ranges.push_back(std::make_tuple(liveness_intervals.at(index_of_interval_to_spill).liveness_used.at(use_point_idx), liveness_intervals.at(index_of_interval_to_spill).liveness_used.at(use_point_idx)));
                atomic_interval.liveness_used.emplace_back(liveness_intervals.at(index_of_interval_to_spill).liveness_used.at(use_point_idx));
            }

            split_into_bb_ranges_of_intervals(control_flow_TAC, atomic_interval, predecessors);

            liveness_intervals.push_back(atomic_interval);
        }

    }


    //clear liveness used and the ranges of original spilled to allow remove_ldr_optimization
    liveness_intervals.at(index_of_interval_to_spill).liveness_used.clear();
    liveness_intervals.at(index_of_interval_to_spill).ranges.clear();



    //all use points of old interval have already occurred, but they are now spilled -> thus we have to insert loads and stores at the appropriate places
    //loads are inserted when the register is used as source
    //store is inserted when the register is used as destination
    for(auto& split_range: liveness_intervals.at(idx_of_old_interval).split_bb_ranges){
        if(!liveness_intervals.at(idx_of_old_interval).liveness_used.empty()){
            uint32_t first_use_point_of_old_interval = std::get<1>(split_range).at(0);//liveness_intervals.back().liveness_used.at(0);//std::get<1>(split_range).at(0);
            if(virt_reg_at_use_point_is_source(TAC.at(first_use_point_of_old_interval), liveness_intervals.at(idx_of_old_interval).virtual_register_number)){
                if(TAC.at(first_use_point_of_old_interval).op == Operation::FUNC_MOVE){ //load values into registers before assigning r0-r3 to parameters, this way it ensures that the virtual variable does not get one of the parameter register numbers and overwrites it (would lead to wrong execution)
                    //get instruction of index
                    uint32_t position_of_inserted_load = first_use_point_of_old_interval - TAC.at(first_use_point_of_old_interval).func_move_idx;
                    load_spill_from_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, position_of_inserted_load, liveness_intervals.at(idx_of_old_interval).real_register_number, idx_of_old_interval, memory_security, track_xor_specification_armv6m, postponed_store_insertion);
                                        

                    liveness_intervals.at(idx_of_old_interval).liveness_used.emplace_back(position_of_inserted_load);
                    std::sort(liveness_intervals.at(idx_of_old_interval).liveness_used.begin(), liveness_intervals.at(idx_of_old_interval).liveness_used.end());
                    liveness_intervals.at(idx_of_old_interval).liveness_used.erase(std::unique(liveness_intervals.at(idx_of_old_interval).liveness_used.begin(), liveness_intervals.at(idx_of_old_interval).liveness_used.end()), liveness_intervals.at(idx_of_old_interval).liveness_used.end());
                    std::get<0>(liveness_intervals.at(idx_of_old_interval).ranges.back()) = liveness_intervals.at(idx_of_old_interval).liveness_used.at(0);
                    std::get<1>(liveness_intervals.at(idx_of_old_interval).ranges.back()) = liveness_intervals.at(idx_of_old_interval).liveness_used.back();
                    split_into_bb_ranges_of_intervals(control_flow_TAC, liveness_intervals.at(idx_of_old_interval), predecessors);

                    
                    instruction_idx_to_split++;
                }
                else{
                    uint32_t position_of_inserted_load = first_use_point_of_old_interval;
                    load_spill_from_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, position_of_inserted_load, liveness_intervals.at(idx_of_old_interval).real_register_number, idx_of_old_interval, memory_security, track_xor_specification_armv6m, postponed_store_insertion);
                                        

                    liveness_intervals.at(idx_of_old_interval).liveness_used.emplace_back(position_of_inserted_load);
                    std::sort(liveness_intervals.at(idx_of_old_interval).liveness_used.begin(), liveness_intervals.at(idx_of_old_interval).liveness_used.end());
                    liveness_intervals.at(idx_of_old_interval).liveness_used.erase(std::unique(liveness_intervals.at(idx_of_old_interval).liveness_used.begin(), liveness_intervals.at(idx_of_old_interval).liveness_used.end()), liveness_intervals.at(idx_of_old_interval).liveness_used.end());
                    std::get<0>(liveness_intervals.at(idx_of_old_interval).ranges.back()) = liveness_intervals.at(idx_of_old_interval).liveness_used.at(0);
                    std::get<1>(liveness_intervals.at(idx_of_old_interval).ranges.back()) = liveness_intervals.at(idx_of_old_interval).liveness_used.back();
                    split_into_bb_ranges_of_intervals(control_flow_TAC, liveness_intervals.at(idx_of_old_interval), predecessors);

                    
                    instruction_idx_to_split++;
                }

            }
            //if there is any destination instruction we place a store at the end of the interval and all memory operations in this interval except the first becomes non-sensitive

                for(auto used_point: std::get<1>(split_range)){
                    if(virt_reg_at_use_point_is_destination(TAC.at(used_point), liveness_intervals.at(idx_of_old_interval).virtual_register_number)){
                        


                        auto curr_interval = liveness_intervals.at(idx_of_old_interval);
                        //fix specification store
                        //if interval has specification and is first register of specification -> can not store after last use point because at this point the other interval already has the register -> we would store a wrong value -> thus store one instruction earlier
                        if(TAC.at(std::get<1>(split_range).back()).op == Operation::FUNC_MOVE){
                            uint32_t instruction_idx = std::get<1>(split_range).back() - TAC.at(std::get<1>(split_range).back()).func_move_idx - 1; // -1 bc in store function call the instruction will be increased by 1
                            store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, liveness_intervals.at(idx_of_old_interval).real_register_number, idx_of_old_interval, std::get<0>(liveness_intervals.at(index_of_current_interval).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                        }
                        else if((!armv6_m_xor_specification_is_empty(liveness_intervals.back().arm6_m_xor_specification)) && (std::find_if(track_xor_specification_armv6m.begin(), track_xor_specification_armv6m.end(), [curr_interval](const std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>& t){return (std::get<0>(t) == curr_interval.real_register_number) && (armv6_m_xor_specification_contains_instruction(curr_interval.arm6_m_xor_specification, std::get<1>(t))) && ((std::get<2>(t) == curr_interval.virtual_register_number) || (std::get<3>(t) == curr_interval.virtual_register_number));}) == track_xor_specification_armv6m.end())){
                            uint32_t instruction_idx = std::get<1>(split_range).back() - 1;//liveness_intervals.back().liveness_used.back() - 1;
                            store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, liveness_intervals.at(idx_of_old_interval).real_register_number, idx_of_old_interval, std::get<0>(liveness_intervals.at(index_of_current_interval).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                        }
                        else{
                            uint32_t instruction_idx = std::get<1>(split_range).back();//liveness_intervals.back().liveness_used.back();
                            store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, liveness_intervals.at(idx_of_old_interval).real_register_number, idx_of_old_interval, std::get<0>(liveness_intervals.at(index_of_current_interval).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                        }


                        break;
                    }
                }
        
        }
    }


    std::sort(liveness_intervals.at(idx_of_old_interval).liveness_used.begin(), liveness_intervals.at(idx_of_old_interval).liveness_used.end());
    

    split_into_bb_ranges_of_intervals(control_flow_TAC, liveness_intervals.at(idx_of_old_interval), predecessors);


    if(liveness_intervals.at(idx_of_old_interval).ranges.empty()){
        liveness_intervals.erase(liveness_intervals.begin() + idx_of_old_interval);
    }


}




void spill_at_interval_rolled_armv6m(std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC, std::vector<liveness_interval_struct>& liveness_intervals, uint32_t index_of_current_liveness_interval, std::vector<liveness_interval_struct>& active, std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>>& available_registers, std::vector<std::tuple<uint32_t, uint32_t, uint8_t>>& allocated_registers, memory_security_struct& memory_security, const std::map<uint32_t, std::vector<uint32_t>>& predecessors, std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>& track_xor_specification_armv6m, std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>>& postponed_store_insertion){


    //check if new interval is interval that has xor specification -> check if pair interval is in active, if yes assign the register and avoid the other spilling part
    bool assigned_register_from_active_for_xor_specification = false;
    if(!armv6_m_xor_specification_is_empty(liveness_intervals.at(index_of_current_liveness_interval).arm6_m_xor_specification)){
        //if we encounter an interval that requires the same real register due to to the xor specification restrictions check if it the other interval is in active and if yes assign it the register
        for(auto& arm_spec: liveness_intervals.at(index_of_current_liveness_interval).arm6_m_xor_specification){
            uint32_t partner_virtual_register = std::get<0>(arm_spec);
            uint32_t xor_instruction_used = std::get<1>(arm_spec);

            auto it_partner_interval_in_active = std::find_if(active.begin(), active.end(), [partner_virtual_register, xor_instruction_used](const liveness_interval_struct& t){return (t.virtual_register_number == partner_virtual_register) && (std::find(t.liveness_used.begin(), t.liveness_used.end(), xor_instruction_used) != t.liveness_used.end());});
            if(it_partner_interval_in_active != active.end()){
                
                //allocated_registers.push_back(std::make_tuple(liveness_intervals.at(index_of_current_liveness_interval).virtual_register_number, it_partner_interval_in_active->real_register_number, liveness_intervals.at(index_of_current_liveness_interval).sensitivity));
                uint32_t partner_real_register = it_partner_interval_in_active->real_register_number;
                auto it_alloc = std::find_if(allocated_registers.begin(), allocated_registers.end(), [partner_real_register](const std::tuple<uint32_t, uint32_t, uint8_t>& t){return std::get<1>(t) == partner_real_register;});
                std::get<0>(*it_alloc) = liveness_intervals.at(index_of_current_liveness_interval).virtual_register_number;
                std::get<2>(*it_alloc) = liveness_intervals.at(index_of_current_liveness_interval).sensitivity;
                
                liveness_intervals.at(index_of_current_liveness_interval).real_register_number = it_partner_interval_in_active->real_register_number;

                uint32_t index_of_last_non_sensitive_in_active = std::distance(std::begin(active), it_partner_interval_in_active);
                remove_from_active(active, index_of_last_non_sensitive_in_active);
                active.push_back(liveness_intervals.at(index_of_current_liveness_interval));
                std::sort(active.begin(), active.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){return std::get<1>(rhs.ranges.back()) < std::get<1>(lhs.ranges.back()) ;});
                assigned_register_from_active_for_xor_specification = true;
                break;
            }
        }




    }

    if(!assigned_register_from_active_for_xor_specification){

        if(liveness_intervals.at(index_of_current_liveness_interval).sensitivity == 1){
            if(virt_reg_at_use_point_is_destination(TAC.at(std::get<1>(liveness_intervals.at(index_of_current_liveness_interval).split_bb_ranges.at(0)).at(0)), liveness_intervals.at(index_of_current_liveness_interval).virtual_register_number) && (!virt_reg_at_use_point_is_source(TAC.at(std::get<1>(liveness_intervals.at(index_of_current_liveness_interval).split_bb_ranges.at(0)).at(0)), liveness_intervals.at(index_of_current_liveness_interval).virtual_register_number))){
                //check if there is an sensitive interval in active -> bc. sensitive intervals are disjoint means the intervals overlap at same instruction -> combined leakage anyway
                auto it_sens_in_active = std::find_if(active.begin(), active.end(), [](const liveness_interval_struct& t){return t.sensitivity == 1;});
                if(it_sens_in_active != active.end()){
                    liveness_intervals.at(index_of_current_liveness_interval).real_register_number = it_sens_in_active->real_register_number;
                    //find interval from active in allocated

                    auto interval_from_active_in_liveness = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [it_sens_in_active](liveness_interval_struct& t){return ((t.virtual_register_number == it_sens_in_active->virtual_register_number) && (t.sensitivity == it_sens_in_active->sensitivity) && (t.real_register_number == it_sens_in_active->real_register_number) && (t.ranges == it_sens_in_active->ranges) && (t.position_on_stack == it_sens_in_active->position_on_stack) && (t.liveness_used == it_sens_in_active->liveness_used) && (t.is_spilled == it_sens_in_active->is_spilled));});
                    interval_from_active_in_liveness->contains_break_transition = true;

                    auto it_sens_in_allocated = std::find_if(allocated_registers.begin(), allocated_registers.end(), [it_sens_in_active](const std::tuple<uint32_t, uint32_t, uint8_t>& t){return (std::get<0>(t) == it_sens_in_active->virtual_register_number) && (std::get<1>(t) == it_sens_in_active->real_register_number);});
                    std::get<0>(*it_sens_in_allocated) = liveness_intervals.at(index_of_current_liveness_interval).virtual_register_number;
                }
                else{
                    auto last_non_sensitive_in_active = std::find_if(active.rbegin(), active.rend(), [](liveness_interval_struct& t){return (t.sensitivity == 0);});

                    uint32_t index_of_last_non_sensitive_in_active = std::distance(std::begin(active), last_non_sensitive_in_active.base()-1);
                    
                    //find interval from active in liveness
                    auto interval_from_active_in_liveness = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [last_non_sensitive_in_active](liveness_interval_struct& t){return ((t.virtual_register_number == last_non_sensitive_in_active->virtual_register_number) && (t.sensitivity == last_non_sensitive_in_active->sensitivity) && (t.real_register_number == last_non_sensitive_in_active->real_register_number) && (t.ranges == last_non_sensitive_in_active->ranges) && (t.position_on_stack == last_non_sensitive_in_active->position_on_stack) && (t.liveness_used == last_non_sensitive_in_active->liveness_used) && (t.is_spilled == last_non_sensitive_in_active->is_spilled));});
                    uint32_t index_of_spilled_active_in_liveness_interval = std::distance(liveness_intervals.begin(), interval_from_active_in_liveness);

                    if(liveness_intervals.at(index_of_spilled_active_in_liveness_interval).is_spilled == false){
                        assign_memory_position(liveness_intervals.at(index_of_spilled_active_in_liveness_interval), memory_security);
                    }
                    

                    split_and_insert(TAC, control_flow_TAC, liveness_intervals, active, available_registers, std::get<0>(liveness_intervals.at(index_of_current_liveness_interval).ranges.at(0)), index_of_spilled_active_in_liveness_interval, index_of_current_liveness_interval, memory_security, predecessors, track_xor_specification_armv6m, postponed_store_insertion);
                    
                    uint32_t virt_reg_spill = liveness_intervals.at(index_of_spilled_active_in_liveness_interval).virtual_register_number;
                    auto alloc_spill = std::find_if(allocated_registers.begin(), allocated_registers.end(), [virt_reg_spill](std::tuple<uint32_t, uint32_t, uint8_t>& t){return std::get<0>(t) == virt_reg_spill;});
                    std::get<0>(*alloc_spill) = liveness_intervals.at(index_of_current_liveness_interval).virtual_register_number; //the virtual register i has now possesion of real register from spill
                    std::get<2>(*alloc_spill) = liveness_intervals.at(index_of_current_liveness_interval).sensitivity;
                    liveness_intervals.at(index_of_current_liveness_interval).real_register_number = liveness_intervals.at(index_of_spilled_active_in_liveness_interval).real_register_number;


                    remove_from_active(active, index_of_last_non_sensitive_in_active);
                    active.push_back(liveness_intervals.at(index_of_current_liveness_interval));
                    std::sort(active.begin(), active.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){return std::get<1>(rhs.ranges.back()) < std::get<1>(lhs.ranges.back()) ;});


                    liveness_intervals.erase(liveness_intervals.begin() + index_of_spilled_active_in_liveness_interval);
                    std::sort(liveness_intervals.begin(), liveness_intervals.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){if(std::get<0>(rhs.ranges.at(0)) == std::get<0>(lhs.ranges.at(0))){return rhs.virtual_register_number < lhs.virtual_register_number;}else{return  std::get<0>(rhs.ranges.at(0)) < std::get<0>(lhs.ranges.at(0));};});


                }
            }
            else{
                auto last_non_sensitive_in_active = std::find_if(active.rbegin(), active.rend(), [](liveness_interval_struct& t){return (t.sensitivity == 0);});

                uint32_t index_of_last_non_sensitive_in_active = std::distance(std::begin(active), last_non_sensitive_in_active.base()-1);
                
                //find interval from active in liveness
                auto interval_from_active_in_liveness = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [last_non_sensitive_in_active](liveness_interval_struct& t){return ((t.virtual_register_number == last_non_sensitive_in_active->virtual_register_number) && (t.sensitivity == last_non_sensitive_in_active->sensitivity) && (t.real_register_number == last_non_sensitive_in_active->real_register_number) && (t.ranges == last_non_sensitive_in_active->ranges) && (t.position_on_stack == last_non_sensitive_in_active->position_on_stack) && (t.liveness_used == last_non_sensitive_in_active->liveness_used) && (t.is_spilled == last_non_sensitive_in_active->is_spilled));});
                uint32_t index_of_spilled_active_in_liveness_interval = std::distance(liveness_intervals.begin(), interval_from_active_in_liveness);

                if(liveness_intervals.at(index_of_spilled_active_in_liveness_interval).is_spilled == false){
                    assign_memory_position(liveness_intervals.at(index_of_spilled_active_in_liveness_interval), memory_security);
                }
                

                split_and_insert(TAC, control_flow_TAC, liveness_intervals, active, available_registers, std::get<0>(liveness_intervals.at(index_of_current_liveness_interval).ranges.at(0)), index_of_spilled_active_in_liveness_interval, index_of_current_liveness_interval, memory_security, predecessors, track_xor_specification_armv6m, postponed_store_insertion);

                uint32_t virt_reg_spill = liveness_intervals.at(index_of_spilled_active_in_liveness_interval).virtual_register_number;
                auto alloc_spill = std::find_if(allocated_registers.begin(), allocated_registers.end(), [virt_reg_spill](std::tuple<uint32_t, uint32_t, uint8_t>& t){return std::get<0>(t) == virt_reg_spill;});
                std::get<0>(*alloc_spill) = liveness_intervals.at(index_of_current_liveness_interval).virtual_register_number; //the virtual register i has now possesion of real register from spill
                std::get<2>(*alloc_spill) = liveness_intervals.at(index_of_current_liveness_interval).sensitivity;
                liveness_intervals.at(index_of_current_liveness_interval).real_register_number = liveness_intervals.at(index_of_spilled_active_in_liveness_interval).real_register_number;


                remove_from_active(active, index_of_last_non_sensitive_in_active);
                active.push_back(liveness_intervals.at(index_of_current_liveness_interval));
                std::sort(active.begin(), active.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){return std::get<1>(rhs.ranges.back()) < std::get<1>(lhs.ranges.back()) ;});


                liveness_intervals.erase(liveness_intervals.begin() + index_of_spilled_active_in_liveness_interval);
                std::sort(liveness_intervals.begin(), liveness_intervals.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){if(std::get<0>(rhs.ranges.at(0)) == std::get<0>(lhs.ranges.at(0))){return rhs.virtual_register_number < lhs.virtual_register_number;}else{return  std::get<0>(rhs.ranges.at(0)) < std::get<0>(lhs.ranges.at(0));};});

            }
        }
        else{
            auto it_sens_in_active = std::find_if(active.begin(), active.end(), [](const liveness_interval_struct& t){return t.sensitivity == 1;});
            if(it_sens_in_active != active.end()){
                if(it_sens_in_active->liveness_used.back() == liveness_intervals.at(index_of_current_liveness_interval).liveness_used.at(0)){
                    liveness_intervals.at(index_of_current_liveness_interval).real_register_number = it_sens_in_active->real_register_number;
                    auto it_sens_in_allocated = std::find_if(allocated_registers.begin(), allocated_registers.end(), [it_sens_in_active](const std::tuple<uint32_t, uint32_t, uint8_t>& t){return (std::get<0>(t) == it_sens_in_active->virtual_register_number) && (std::get<1>(t) == it_sens_in_active->real_register_number);});
                    std::get<0>(*it_sens_in_allocated) = liveness_intervals.at(index_of_current_liveness_interval).virtual_register_number;
                }
                else{
                    auto last_non_sensitive_in_active = std::find_if(active.rbegin(), active.rend(), [](liveness_interval_struct& t){return (t.sensitivity == 0);});

                    uint32_t index_of_last_non_sensitive_in_active = std::distance(std::begin(active), last_non_sensitive_in_active.base()-1);
                    
                    //find interval from active in liveness
                    auto interval_from_active_in_liveness = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [last_non_sensitive_in_active](liveness_interval_struct& t){return ((t.virtual_register_number == last_non_sensitive_in_active->virtual_register_number) && (t.sensitivity == last_non_sensitive_in_active->sensitivity) && (t.real_register_number == last_non_sensitive_in_active->real_register_number) && (t.ranges == last_non_sensitive_in_active->ranges) && (t.position_on_stack == last_non_sensitive_in_active->position_on_stack) && (t.liveness_used == last_non_sensitive_in_active->liveness_used) && (t.is_spilled == last_non_sensitive_in_active->is_spilled));});
                    uint32_t index_of_spilled_active_in_liveness_interval = std::distance(liveness_intervals.begin(), interval_from_active_in_liveness);

                    if(liveness_intervals.at(index_of_spilled_active_in_liveness_interval).is_spilled == false){
                        assign_memory_position(liveness_intervals.at(index_of_spilled_active_in_liveness_interval), memory_security);
                    }
                    

                    split_and_insert(TAC, control_flow_TAC, liveness_intervals, active, available_registers, std::get<0>(liveness_intervals.at(index_of_current_liveness_interval).ranges.at(0)), index_of_spilled_active_in_liveness_interval, index_of_current_liveness_interval, memory_security, predecessors, track_xor_specification_armv6m, postponed_store_insertion);
                
                    uint32_t virt_reg_spill = liveness_intervals.at(index_of_spilled_active_in_liveness_interval).virtual_register_number;
                    auto alloc_spill = std::find_if(allocated_registers.begin(), allocated_registers.end(), [virt_reg_spill](std::tuple<uint32_t, uint32_t, uint8_t>& t){return std::get<0>(t) == virt_reg_spill;});
                    std::get<0>(*alloc_spill) = liveness_intervals.at(index_of_current_liveness_interval).virtual_register_number; //the virtual register i has now possesion of real register from spill
                    std::get<2>(*alloc_spill) = liveness_intervals.at(index_of_current_liveness_interval).sensitivity;
                    liveness_intervals.at(index_of_current_liveness_interval).real_register_number = liveness_intervals.at(index_of_spilled_active_in_liveness_interval).real_register_number;

                    remove_from_active(active, index_of_last_non_sensitive_in_active);
                    active.push_back(liveness_intervals.at(index_of_current_liveness_interval));
                    std::sort(active.begin(), active.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){return std::get<1>(rhs.ranges.back()) < std::get<1>(lhs.ranges.back()) ;});


                    liveness_intervals.erase(liveness_intervals.begin() + index_of_spilled_active_in_liveness_interval);
                    std::sort(liveness_intervals.begin(), liveness_intervals.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){if(std::get<0>(rhs.ranges.at(0)) == std::get<0>(lhs.ranges.at(0))){return rhs.virtual_register_number < lhs.virtual_register_number;}else{return  std::get<0>(rhs.ranges.at(0)) < std::get<0>(lhs.ranges.at(0));};});

                }

            }
            else{
                auto last_non_sensitive_in_active = std::find_if(active.rbegin(), active.rend(), [](liveness_interval_struct& t){return (t.sensitivity == 0);});

                uint32_t index_of_last_non_sensitive_in_active = std::distance(std::begin(active), last_non_sensitive_in_active.base()-1);
                
                //find interval from active in liveness
                auto interval_from_active_in_liveness = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [last_non_sensitive_in_active](liveness_interval_struct& t){return ((t.virtual_register_number == last_non_sensitive_in_active->virtual_register_number) && (t.sensitivity == last_non_sensitive_in_active->sensitivity) && (t.real_register_number == last_non_sensitive_in_active->real_register_number) && (t.ranges == last_non_sensitive_in_active->ranges) && (t.position_on_stack == last_non_sensitive_in_active->position_on_stack) && (t.liveness_used == last_non_sensitive_in_active->liveness_used) && (t.is_spilled == last_non_sensitive_in_active->is_spilled));});
                uint32_t index_of_spilled_active_in_liveness_interval = std::distance(liveness_intervals.begin(), interval_from_active_in_liveness);

                if(liveness_intervals.at(index_of_spilled_active_in_liveness_interval).is_spilled == false){
                    assign_memory_position(liveness_intervals.at(index_of_spilled_active_in_liveness_interval), memory_security);
                }
                

                split_and_insert(TAC, control_flow_TAC, liveness_intervals, active, available_registers, std::get<0>(liveness_intervals.at(index_of_current_liveness_interval).ranges.at(0)), index_of_spilled_active_in_liveness_interval, index_of_current_liveness_interval, memory_security, predecessors, track_xor_specification_armv6m, postponed_store_insertion);
            
                uint32_t virt_reg_spill = liveness_intervals.at(index_of_spilled_active_in_liveness_interval).virtual_register_number;
                auto alloc_spill = std::find_if(allocated_registers.begin(), allocated_registers.end(), [virt_reg_spill](std::tuple<uint32_t, uint32_t, uint8_t>& t){return std::get<0>(t) == virt_reg_spill;});
                std::get<0>(*alloc_spill) = liveness_intervals.at(index_of_current_liveness_interval).virtual_register_number; //the virtual register i has now possesion of real register from spill
                std::get<2>(*alloc_spill) = liveness_intervals.at(index_of_current_liveness_interval).sensitivity;
                liveness_intervals.at(index_of_current_liveness_interval).real_register_number = liveness_intervals.at(index_of_spilled_active_in_liveness_interval).real_register_number;

                remove_from_active(active, index_of_last_non_sensitive_in_active);
                active.push_back(liveness_intervals.at(index_of_current_liveness_interval));
                std::sort(active.begin(), active.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){return std::get<1>(rhs.ranges.back()) < std::get<1>(lhs.ranges.back()) ;});

                liveness_intervals.erase(liveness_intervals.begin() + index_of_spilled_active_in_liveness_interval);
                std::sort(liveness_intervals.begin(), liveness_intervals.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){if(std::get<0>(rhs.ranges.at(0)) == std::get<0>(lhs.ranges.at(0))){return rhs.virtual_register_number < lhs.virtual_register_number;}else{return  std::get<0>(rhs.ranges.at(0)) < std::get<0>(lhs.ranges.at(0));};});

            }

        }




        if(liveness_intervals.at(index_of_current_liveness_interval).is_spilled){
            for(auto& split_range: liveness_intervals.at(index_of_current_liveness_interval).split_bb_ranges){
                uint32_t instruction_idx = std::get<1>(split_range).at(0);
                if(virt_reg_at_use_point_is_source(TAC.at(instruction_idx), liveness_intervals.at(index_of_current_liveness_interval).virtual_register_number)){
                    if(TAC.at(instruction_idx).op == Operation::FUNC_MOVE){

                        uint32_t position_of_inserted_load = instruction_idx - TAC.at(instruction_idx).func_move_idx;
                        load_spill_from_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, position_of_inserted_load, liveness_intervals.at(index_of_current_liveness_interval).real_register_number, index_of_current_liveness_interval, memory_security, track_xor_specification_armv6m, postponed_store_insertion);
        

                    
                        liveness_intervals.at(index_of_current_liveness_interval).liveness_used.emplace_back(position_of_inserted_load);
                        std::sort(liveness_intervals.at(index_of_current_liveness_interval).liveness_used.begin(), liveness_intervals.at(index_of_current_liveness_interval).liveness_used.end());
                        liveness_intervals.at(index_of_current_liveness_interval).liveness_used.erase(std::unique(liveness_intervals.at(index_of_current_liveness_interval).liveness_used.begin(), liveness_intervals.at(index_of_current_liveness_interval).liveness_used.end()), liveness_intervals.at(index_of_current_liveness_interval).liveness_used.end());
                        std::get<0>(liveness_intervals.at(index_of_current_liveness_interval).ranges.back()) = liveness_intervals.at(index_of_current_liveness_interval).liveness_used.at(0);
                        std::get<1>(liveness_intervals.at(index_of_current_liveness_interval).ranges.back()) = liveness_intervals.at(index_of_current_liveness_interval).liveness_used.back();
                        split_into_bb_ranges_of_intervals(control_flow_TAC, liveness_intervals.at(index_of_current_liveness_interval), predecessors);

                    }
                    else{

                        uint32_t position_of_inserted_load = instruction_idx;
                        load_spill_from_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, position_of_inserted_load, liveness_intervals.at(index_of_current_liveness_interval).real_register_number, index_of_current_liveness_interval, memory_security, track_xor_specification_armv6m, postponed_store_insertion);
      

                        liveness_intervals.at(index_of_current_liveness_interval).liveness_used.emplace_back(position_of_inserted_load);
                        std::sort(liveness_intervals.at(index_of_current_liveness_interval).liveness_used.begin(), liveness_intervals.at(index_of_current_liveness_interval).liveness_used.end());
                        liveness_intervals.at(index_of_current_liveness_interval).liveness_used.erase(std::unique(liveness_intervals.at(index_of_current_liveness_interval).liveness_used.begin(), liveness_intervals.at(index_of_current_liveness_interval).liveness_used.end()), liveness_intervals.at(index_of_current_liveness_interval).liveness_used.end());
                        std::get<0>(liveness_intervals.at(index_of_current_liveness_interval).ranges.back()) = liveness_intervals.at(index_of_current_liveness_interval).liveness_used.at(0);
                        std::get<1>(liveness_intervals.at(index_of_current_liveness_interval).ranges.back()) = liveness_intervals.at(index_of_current_liveness_interval).liveness_used.back();
                        split_into_bb_ranges_of_intervals(control_flow_TAC, liveness_intervals.at(index_of_current_liveness_interval), predecessors);

                    }
                }
            }

            for(auto split_range: liveness_intervals.at(index_of_current_liveness_interval).split_bb_ranges){
                if(!remove_str_optimization_possible(control_flow_TAC, liveness_intervals, liveness_intervals.at(index_of_current_liveness_interval), liveness_intervals.at(index_of_current_liveness_interval).split_bb_ranges.at(0))){
                    for(auto use_point_in_split_range: std::get<1>(split_range)){
                        uint32_t instruction_idx = use_point_in_split_range;
                        if(virt_reg_at_use_point_is_destination(TAC.at(instruction_idx), liveness_intervals.at(index_of_current_liveness_interval).virtual_register_number)){

                            auto curr_interval = liveness_intervals.at(index_of_current_liveness_interval);
                            //fix specification store
                            //if interval has specification and is first register of specification -> can not store after last use point because at this point the other interval already has the register -> we would store a wrong value -> thus store one instruction earlier
                            if(TAC.at(std::get<1>(split_range).back()).op == Operation::FUNC_MOVE){
                                instruction_idx = std::get<1>(split_range).back() - TAC.at(std::get<1>(split_range).back()).func_move_idx - 1; // -1 bc in store function call the instruction will be increased by 1
                                store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, liveness_intervals.at(index_of_current_liveness_interval).real_register_number, index_of_current_liveness_interval, std::get<0>(liveness_intervals.at(index_of_current_liveness_interval).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                            }
                            else if(liveness_intervals.at(index_of_current_liveness_interval).contains_break_transition){
                                instruction_idx = std::get<1>(split_range).back() - 1;//liveness_intervals.at(index_of_interval_without_real_register).liveness_used.back() - 1;
                                store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, liveness_intervals.at(index_of_current_liveness_interval).real_register_number, index_of_current_liveness_interval, std::get<0>(liveness_intervals.at(index_of_current_liveness_interval).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                            }
                            else if((!armv6_m_xor_specification_is_empty(liveness_intervals.at(index_of_current_liveness_interval).arm6_m_xor_specification)) && (virtual_register_at_src_TAC_instruction_has_armv6_m_xor_specification(TAC.at(std::get<1>(split_range).back()), std::get<1>(split_range).back(), liveness_intervals.at(index_of_current_liveness_interval).virtual_register_number, liveness_intervals.at(index_of_current_liveness_interval).arm6_m_xor_specification))){
                                instruction_idx = std::get<1>(split_range).back() - 1;//liveness_intervals.at(index_of_interval_without_real_register).liveness_used.back() - 1;
                                store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, liveness_intervals.at(index_of_current_liveness_interval).real_register_number, index_of_current_liveness_interval, std::get<0>(liveness_intervals.at(index_of_current_liveness_interval).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                            }
                            else{
                                instruction_idx = std::get<1>(split_range).back();//liveness_intervals.at(index_of_interval_without_real_register).liveness_used.back();
                                store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, liveness_intervals.at(index_of_current_liveness_interval).real_register_number, index_of_current_liveness_interval, std::get<0>(liveness_intervals.at(index_of_current_liveness_interval).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                            }


                            break;
                        }
                    }
                }
            }

        }
    }
}

void linear_scan_algorithm_rolled(std::vector<TAC_type>& TAC, std::vector<TAC_bb>& control_flow_TAC, std::vector<liveness_interval_struct>& liveness_intervals, memory_security_struct& memory_security, const std::map<uint32_t, std::vector<uint32_t>>& predecessors, uint32_t number_of_input_registers){
    //list of active intervals ordere by increasing end points
    std::vector<liveness_interval_struct> active;
    active.shrink_to_fit();

    //set of available registers
    std::vector<std::tuple<uint32_t, uint8_t, uint32_t, uint32_t, bool>> available_registers(NUM_PHYSICAL_REGISTERS); //(real register index, sensitivity of real register, instruction idx where real register was used last, virtual variable number that last used this real register, register does not contain input for function)
    

    if(number_of_input_registers > 4){
        number_of_input_registers = 4;
    }
    for(uint32_t reg_idx = 0; reg_idx < NUM_PHYSICAL_REGISTERS; ++reg_idx){
        available_registers.at(reg_idx) = std::make_tuple(reg_idx, 0, UINT32_MAX, UINT32_MAX, true); //in the beginning every register is available and has no security level
        if(reg_idx < number_of_input_registers){
            std::get<4>(available_registers.at(reg_idx)) = false;
        }
    }

    if(NUM_PHYSICAL_REGISTERS >= 12){
        available_registers.erase(available_registers.begin() + 11);
    }


    std::vector<std::tuple<uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint8_t, uint32_t>> postponed_store_insertion; // (idx to insert store, in_dest, in_dest_sens, in_overall_sens, in_src1, in_src1_sens, index of liveness interval)

    std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> track_xor_specification_armv6m; //(real register, use point, virtual_register1, virtual_register2)

    // set of allocated registers
    std::vector<std::tuple<uint32_t, uint32_t, uint8_t>> allocated_registers; //(virtual_register_number, real_register_number, sensitivity of real register)
    allocated_registers.shrink_to_fit();


    auto first_interval_without_real_register_iterator = liveness_intervals.begin();
    while(first_interval_without_real_register_iterator != liveness_intervals.end()){


        uint32_t index_of_interval_without_real_register = std::distance(liveness_intervals.begin(), first_interval_without_real_register_iterator);


        if(liveness_intervals.at(index_of_interval_without_real_register).virtual_register_number >= (UINT32_MAX - v_function_borders)){ //this interval will not be handled specifically, rather marks this register the border between pre- and post-function call, here we have to check if there is any sensitive register in available or active before we make the funciton call

            //get number of operands in register for func call
            uint32_t idx_of_border_var = liveness_intervals.at(index_of_interval_without_real_register).liveness_used.at(0);
            TAC_type tac_func_info;
            while(TAC.at(idx_of_border_var).op != Operation::FUNC_CALL){
                idx_of_border_var++;
                tac_func_info = TAC.at(idx_of_border_var);
            }

            for(auto& avail_reg: available_registers){
                if((std::get<1>(avail_reg) == 1) && (std::get<0>(avail_reg) >= tac_func_info.function_arg_num)){
                    auto it = std::find_if(liveness_intervals.rbegin(), liveness_intervals.rend(), [avail_reg](const liveness_interval_struct& t){return t.real_register_number == std::get<0>(avail_reg);});
                    uint32_t position_to_insert_break_transition = it->liveness_used.back() + 1;
                    uint32_t idx_of_break_trans_reg = (it+1).base() - liveness_intervals.begin();

                    break_transition_armv7m(TAC, control_flow_TAC, liveness_intervals, active, available_registers, idx_of_break_trans_reg, position_to_insert_break_transition, track_xor_specification_armv6m, postponed_store_insertion);
                    std::get<1>(avail_reg) = 0;

                }
            }

            for(auto& active_reg: active){
                if((active_reg.sensitivity == 1) && (active_reg.real_register_number >= tac_func_info.function_arg_num)){
                    auto it = std::find_if(liveness_intervals.rbegin(), liveness_intervals.rend(), [active_reg](const liveness_interval_struct& t){return t.real_register_number == active_reg.real_register_number;});
                    uint32_t position_to_insert_break_transition = it->liveness_used.back() + 1;
                    uint32_t idx_of_break_trans_reg = (it+1).base() - liveness_intervals.begin();

                    break_transition_armv7m(TAC, control_flow_TAC, liveness_intervals, active, available_registers, idx_of_break_trans_reg, position_to_insert_break_transition, track_xor_specification_armv6m, postponed_store_insertion);
                    active_reg.sensitivity = 0;

                }
            }
            liveness_intervals.at(index_of_interval_without_real_register).real_register_number = UINT32_MAX - 1;
            first_interval_without_real_register_iterator = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [](liveness_interval_struct& t){return t.real_register_number == UINT32_MAX;});
        }
        else{


            expire_old_intervals_rolled(liveness_intervals.at(index_of_interval_without_real_register), active, allocated_registers, available_registers);


            if((active.size() == NUM_PHYSICAL_REGISTERS)){
                #ifdef ARMV6_M
                spill_at_interval_rolled_armv6m(TAC, control_flow_TAC, liveness_intervals, index_of_interval_without_real_register, active, available_registers, allocated_registers, memory_security, predecessors, track_xor_specification_armv6m, postponed_store_insertion);
                #endif
            }
            else{

                /**
                 * @brief get register from available registers and generate mapping for virtual register to real registers
                 *        if virtual register is sensitive we are only allowed to assign a free non-sensitive register
                 *        if virtual register is non-sensitive we assign a free sensitive register -> thus it looses its sensitivity
                 * 
                 * 
                 */

                #ifdef ARMV6_M
                available_register_assignment_armv6m(TAC, control_flow_TAC, first_interval_without_real_register_iterator, liveness_intervals, active, available_registers, allocated_registers, memory_security, predecessors, track_xor_specification_armv6m, postponed_store_insertion);
                #endif

                //add to active
                active.push_back(liveness_intervals.at(index_of_interval_without_real_register));

                //sort 
                std::sort(active.begin(), active.end(), [](liveness_interval_struct& rhs, liveness_interval_struct& lhs){return std::get<1>(rhs.ranges.back()) < std::get<1>(lhs.ranges.back()) ;});



            }
            




            first_interval_without_real_register_iterator = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [](liveness_interval_struct& t){return t.real_register_number == UINT32_MAX;});


            //save all available registers after register allocation of current interval to the datastructure -> important for spill stores to know which registers are allowed to be used at this point
            for(const auto& available_reg: available_registers){
                if(std::get<0>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()) <= 3){
                    if(!((std::get<0>(available_reg) == 0) || (std::get<0>(available_reg) == 1) || (std::get<0>(available_reg) == 2) || (std::get<0>(available_reg) == 3))){
                        liveness_intervals.at(index_of_interval_without_real_register).available_registers_after_own_allocation.emplace_back(std::get<0>(available_reg));
                    }
                }
                else{
                    liveness_intervals.at(index_of_interval_without_real_register).available_registers_after_own_allocation.emplace_back(std::get<0>(available_reg));
                }

            }



            auto postponed_store_insertion_elem = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [](const liveness_interval_struct& t){return t.postponed_store == true;});
            while(postponed_store_insertion_elem != liveness_intervals.end()){

                if(!(std::get<1>(postponed_store_insertion_elem->ranges.back()) < std::get<0>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()))){
                    break;
                }


                auto curr_interval = *postponed_store_insertion_elem;
                    
                for(auto& split_range: postponed_store_insertion_elem->split_bb_ranges){
                    if(!remove_str_optimization_possible(control_flow_TAC, liveness_intervals, *postponed_store_insertion_elem, split_range)){
                        if(TAC.at(std::get<1>(split_range).back()).op == Operation::FUNC_MOVE){
                            uint32_t instruction_idx = std::get<1>(split_range).back() - TAC.at(std::get<1>(split_range).back()).func_move_idx - 2; // -1 bc in store function call the instruction will be increased by 1
                            store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, postponed_store_insertion_elem->real_register_number, (postponed_store_insertion_elem - liveness_intervals.begin()), std::get<0>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                        }
                        else if(postponed_store_insertion_elem->contains_break_transition){
                            uint32_t instruction_idx = std::get<1>(split_range).back() - 1;//liveness_intervals.at(index_of_interval_without_real_register).liveness_used.back() - 1;
                            store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, postponed_store_insertion_elem->real_register_number, (postponed_store_insertion_elem - liveness_intervals.begin()), std::get<0>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                        }
                        else if((!armv6_m_xor_specification_is_empty(postponed_store_insertion_elem->arm6_m_xor_specification)) && (virtual_register_at_src_TAC_instruction_has_armv6_m_xor_specification(TAC.at(std::get<1>(split_range).back()), std::get<1>(split_range).back(), postponed_store_insertion_elem->virtual_register_number, postponed_store_insertion_elem->arm6_m_xor_specification)) && (std::find(std::get<1>(split_range).begin(), std::get<1>(split_range).end(), std::get<1>(split_range).back())) != std::get<1>(split_range).end()){
                            uint32_t instruction_idx = std::get<1>(split_range).back() - 1;
                            store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, postponed_store_insertion_elem->real_register_number, (postponed_store_insertion_elem - liveness_intervals.begin()), std::get<0>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                        }
                        else{
                            uint32_t instruction_idx = std::get<1>(split_range).back();
                            store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, postponed_store_insertion_elem->real_register_number, (postponed_store_insertion_elem - liveness_intervals.begin()), std::get<0>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                        }
                    }
                }

                // store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, postponed_store_insertion_elem->liveness_used.back(), postponed_store_insertion_elem->real_register_number, (postponed_store_insertion_elem - liveness_intervals.begin()) , std::get<0>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                postponed_store_insertion_elem++;
                postponed_store_insertion_elem = std::find_if(postponed_store_insertion_elem, liveness_intervals.end(), [](const liveness_interval_struct& t){return t.postponed_store == true;});
            }

            #ifdef ARMV6_M
            //if an interval that has an xor constraint gets assigned a register two things can happen
            //1. It was the first of the two intervals -> the second interval has not a register yet -> save information about the real register, use point, and both intervals in datastructure, this information has not yet been in the datastructure
            //2. It was the second interval -> the information has been in the datastructure and is not needed anymore
            if(!armv6_m_xor_specification_is_empty(liveness_intervals.at(index_of_interval_without_real_register).arm6_m_xor_specification)){ //check if interval has xor specification constraints
                for(auto& arm_spec: liveness_intervals.at(index_of_interval_without_real_register).arm6_m_xor_specification){
                    auto curr_interval = liveness_intervals.at(index_of_interval_without_real_register);
                    auto it = std::find_if(track_xor_specification_armv6m.begin(), track_xor_specification_armv6m.end(), [curr_interval](const std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>& t){return (std::get<0>(t) == curr_interval.real_register_number) && (armv6_m_xor_specification_contains_instruction(curr_interval.arm6_m_xor_specification, std::get<1>(t))) && ((std::get<2>(t) == curr_interval.virtual_register_number) || (std::get<3>(t) == curr_interval.virtual_register_number));});
                    if(it != track_xor_specification_armv6m.end()){ //second case -> there already exists an entry -> just remove the information
                        track_xor_specification_armv6m.erase(it);
                    }
                    else{   // first case -> need to save information for second interval
                        track_xor_specification_armv6m.push_back(std::make_tuple(liveness_intervals.at(index_of_interval_without_real_register).real_register_number, std::get<1>(arm_spec), std::get<0>(arm_spec), liveness_intervals.at(index_of_interval_without_real_register).virtual_register_number));
                    }
                }

            }
            #endif

        }


    }

    auto postponed_store_insertion_elem = std::find_if(liveness_intervals.begin(), liveness_intervals.end(), [](const liveness_interval_struct& t){return t.postponed_store == true;});
    while(postponed_store_insertion_elem != liveness_intervals.end()){

        auto curr_interval = *postponed_store_insertion_elem;
        for(auto& split_range: postponed_store_insertion_elem->split_bb_ranges){
            if(!remove_str_optimization_possible(control_flow_TAC, liveness_intervals, *postponed_store_insertion_elem, split_range)){
                if(TAC.at(std::get<1>(split_range).back()).op == Operation::FUNC_MOVE){
                    uint32_t instruction_idx = std::get<1>(split_range).back() - TAC.at(std::get<1>(split_range).back()).func_move_idx - 1; // -1 bc in store function call the instruction will be increased by 1
                    store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, postponed_store_insertion_elem->real_register_number, (postponed_store_insertion_elem - liveness_intervals.begin()), TAC.size(), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                }
                else if(postponed_store_insertion_elem->contains_break_transition){
                    uint32_t instruction_idx = std::get<1>(split_range).back() - 1;//liveness_intervals.at(index_of_interval_without_real_register).liveness_used.back() - 1;
                    store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, postponed_store_insertion_elem->real_register_number, (postponed_store_insertion_elem - liveness_intervals.begin()), TAC.size(), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                }
                else if((!armv6_m_xor_specification_is_empty(postponed_store_insertion_elem->arm6_m_xor_specification)) && (virtual_register_at_src_TAC_instruction_has_armv6_m_xor_specification(TAC.at(std::get<1>(split_range).back()), std::get<1>(split_range).back(), postponed_store_insertion_elem->virtual_register_number, postponed_store_insertion_elem->arm6_m_xor_specification)) && (std::find(std::get<1>(split_range).begin(), std::get<1>(split_range).end(), std::get<1>(split_range).back()) != std::get<1>(split_range).end())){
                    uint32_t instruction_idx = std::get<1>(split_range).back() - 1;
                    store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, postponed_store_insertion_elem->real_register_number, (postponed_store_insertion_elem - liveness_intervals.begin()), TAC.size(), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                }
                else{
                    uint32_t instruction_idx = std::get<1>(split_range).back();
                    store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, instruction_idx, postponed_store_insertion_elem->real_register_number, (postponed_store_insertion_elem - liveness_intervals.begin()), TAC.size(), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
                }
            }
        }

        // store_spill_to_memory_address(TAC, control_flow_TAC, liveness_intervals, active, available_registers, postponed_store_insertion_elem->liveness_used.back(), postponed_store_insertion_elem->real_register_number, (postponed_store_insertion_elem - liveness_intervals.begin()) , std::get<0>(liveness_intervals.at(index_of_interval_without_real_register).ranges.back()), memory_security, track_xor_specification_armv6m, postponed_store_insertion, predecessors);
        postponed_store_insertion_elem++;
        postponed_store_insertion_elem = std::find_if(postponed_store_insertion_elem, liveness_intervals.end(), [](const liveness_interval_struct& t){return t.postponed_store == true;});
    }


}
