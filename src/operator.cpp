#include "operator.hpp"

bool operator!=(const array_information& lhs, const array_information& rhs){
    if(lhs.dimension != rhs.dimension){
        return true;
    }
    for(uint32_t i = 0; i < lhs.dimension; ++i){
        if((lhs.depth_per_dimension.at(i).op_data != rhs.depth_per_dimension.at(i).op_data) || (lhs.depth_per_dimension.at(i).op_type != rhs.depth_per_dimension.at(i).op_type)){
            return true;
        }
    }
    return false;
}
bool operator==(const array_information& lhs, const array_information& rhs){
    if(lhs.dimension != rhs.dimension){

        return false;
    }
    for(uint32_t i = 0; i < lhs.dimension; ++i){
        if((lhs.depth_per_dimension.at(i).op_data != rhs.depth_per_dimension.at(i).op_data) || (lhs.depth_per_dimension.at(i).op_type != rhs.depth_per_dimension.at(i).op_type)){
            return false;
        }
    }
    return true;
}

bool operator==(const liveness_interval_struct& lhs, const liveness_interval_struct& rhs){
    if(lhs.virtual_register_number != rhs.virtual_register_number){
        return false;
    }
    if(lhs.liveness_used != rhs.liveness_used){
        return false;
    }
    if(lhs.ranges.back() != rhs.ranges.back()){
        return false;
    }
    if(lhs.is_spilled != rhs.is_spilled){
        return false;
    }
    if(lhs.sensitivity != rhs.sensitivity){
        return false;
    }
    if(lhs.position_on_stack != rhs.position_on_stack){
        return false;
    }
    return true;
}

bool operator==(const TAC_type& lhs, const TAC_type& rhs){
    if(lhs.dest_v_reg_idx != rhs.dest_v_reg_idx){
        return false;
    }
    if(lhs.dest_type != rhs.dest_type){
        return false;
    }
    if(lhs.dest_sensitivity != rhs.dest_sensitivity){
        return false;
    }
    if(lhs.src1_v != rhs.src1_v){
        return false;
    }
    if(lhs.src2_v != rhs.src2_v){
        return false;
    }
    if(lhs.src1_type != rhs.src1_type){
        return false;
    }
    if(lhs.src2_type != rhs.src2_type){
        return false;
    }
    if(lhs.src1_sensitivity != rhs.src1_sensitivity){
        return false;
    }
    if(lhs.src2_sensitivity != rhs.src2_sensitivity){
        return false;
    }
    
    return true;
}

