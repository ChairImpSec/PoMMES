#include "graph.hpp"

//create a mapping between position of bb in function and bb number
//e.g. first basic block in function has basic block number 2 => mapping will be 2: 0
void map_bb_number_to_graph_node(std::map<uint32_t, uint32_t>& bb_number_to_node, uint32_t bb_number, uint32_t node_ctr){
    bb_number_to_node.insert(std::pair<uint32_t, uint32_t>(bb_number, node_ctr));
}

void build_graph(function& func, std::vector<std::vector<uint8_t>>& graph, std::map<uint32_t, uint32_t>& node_to_bb_number){
    
    std::map<uint32_t, uint32_t> bb_number_to_node;
    uint32_t node_ctr = 0;
    for(const auto& bb: func.bb){
        map_bb_number_to_graph_node(bb_number_to_node, bb.bb_number, node_ctr);
        node_to_bb_number.insert(std::pair<uint32_t, uint32_t>(node_ctr, bb.bb_number)); //generate the inverse mappping of bb_number_to_node ,e.g. first basic block in function has bb number 2 => mapping will be 0: 2
        ++node_ctr;
    }

    for(const auto& bb: func.bb){
        uint8_t contains_branch = 0;
        for(const auto& instr: bb.ssa_instruction){
            if(instr.instr_type == INSTRUCTION_TYPE::INSTR_BRANCH){ // a basic block is connected with another basic block if we have a jump instruction at the end of the basic block
                branch_instruction b_instr = boost::get<branch_instruction>(instr.instr);
                contains_branch = 1;
                graph.at(bb_number_to_node.find(bb.bb_number)->second).at(bb_number_to_node.find(b_instr.dest_bb)->second) = 1;
            }
            if(instr.instr_type == INSTRUCTION_TYPE::INSTR_CONDITON){ // a basic block is connected with another basic block if we have a conditional jump 
                condition_instruction c_instr = boost::get<condition_instruction>(instr.instr);
                contains_branch = 1;
                graph.at(bb_number_to_node.find(bb.bb_number)->second).at(bb_number_to_node.find(c_instr.false_dest)->second) = 1;
                graph.at(bb_number_to_node.find(bb.bb_number)->second).at(bb_number_to_node.find(c_instr.true_dest)->second) = 1;
            }
        }
        if(!contains_branch){ //a basic block is connected to the following basic block (fallthrough) if we have no jump
            //check if elements is last bb in function, as the last bb has no successor
            if(bb.bb_number != func.bb.back().bb_number){
                std::vector<basic_block>::iterator it = (func.bb.begin() + (std::find_if(func.bb.begin(), func.bb.end(), [&curr_bb = bb](basic_block& func_bb){return curr_bb.bb_number == func_bb.bb_number;}) - func.bb.begin()) + 1);
                graph.at(bb_number_to_node.find(bb.bb_number)->second).at(bb_number_to_node.find((it)->bb_number)->second) = 1;
            }
        }
    }

}


//predecessor will contain the bb_number of the current bb as key and the bb_number of all predecessors as value in a list
void compute_predecessors(function& func, std::vector<std::vector<uint8_t>>& adjacency_matrix, std::map<uint32_t, std::vector<uint32_t>>& predecessors){

    for(uint32_t row = 0; row < adjacency_matrix.size(); ++row){
        for(uint32_t column = 0; column < adjacency_matrix.size(); ++column){
            if(adjacency_matrix.at(row).at(column) == 1){
                uint32_t key = func.bb.at(column).bb_number;
                uint32_t value = func.bb.at(row).bb_number;
                if(predecessors.find(key) == predecessors.end()){
                    predecessors.insert(std::pair<uint32_t, std::vector<uint32_t>>(key, std::vector<uint32_t>()));
                }    
                predecessors[key].emplace_back(value);
            }
        }
    }


}


//successor will contain the bb_number of the current bb as key and the bb_number of all successors as value in a list
void compute_successors(function& func, std::vector<std::vector<uint8_t>>& adjacency_matrix, std::map<uint32_t, std::vector<uint32_t>>& successors){

    for(uint32_t row = 0; row < adjacency_matrix.size(); ++row){
        for(uint32_t column = 0; column < adjacency_matrix.size(); ++column){
            if(adjacency_matrix.at(row).at(column) == 1){
                uint32_t key = func.bb.at(row).bb_number;
                uint32_t value = func.bb.at(column).bb_number;
                if(successors.find(key) == successors.end()){
                    successors.insert(std::pair<uint32_t, std::vector<uint32_t>>(key, std::vector<uint32_t>()));
                }    
                successors[key].emplace_back(value);
            }
        }
    }


}