#include "printing.hpp"


void print_assign(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security){
    assignment_instruction n_instr = boost::get<assignment_instruction>(instr.instr);
    if(print_security){
        if(instr.sensitivity_level != 1){
            std::cout << "PUB ";
        }
        else{
            std::cout << "SEC ";
        }
    }
    std::cout << virtual_variables.at(n_instr.dest.index_to_variable).ssa_name << "(" << std::to_string(virtual_variables.at(n_instr.dest.index_to_variable).sensitivity) << ")" << " = ";
    if(n_instr.src.op_type == DATA_TYPE::NUM){
        std::cout << std::to_string(n_instr.src.op_data) << "(0)" << std::endl;
    }
    else{
        if((n_instr.src.op_type == DATA_TYPE::INPUT_ARR) || (n_instr.src.op_type == DATA_TYPE::INPUT_ARR_ADDR) || (n_instr.src.op_type == DATA_TYPE::LOCAL_ARR) || (n_instr.src.op_type == DATA_TYPE::LOCAL_ARR_ADDR) || (n_instr.src.op_type == DATA_TYPE::VIRTUAL_ARR) || (n_instr.src.op_type == DATA_TYPE::VIRTUAL_ARR_ADDR)){
            if((n_instr.src.op_type == DATA_TYPE::INPUT_ARR_ADDR) || (n_instr.src.op_type == DATA_TYPE::LOCAL_ARR_ADDR) || (n_instr.src.op_type == DATA_TYPE::VIRTUAL_ARR_ADDR)){
                std::cout << "&";
            }
            std::cout << virtual_variables.at(n_instr.src.op_data).ssa_name;
            array_information a_info = boost::get<array_information>(virtual_variables.at(n_instr.src.op_data).data_information);
            for(uint32_t i = 0; i < a_info.dimension; ++i){
                if(a_info.depth_per_dimension.at(i).op_type == DATA_TYPE::NUM){
                    std::cout << "[" << std::to_string(a_info.depth_per_dimension.at(i).op_data) << "]";
                }
                else{
                    std::cout << "[" << virtual_variables.at(a_info.depth_per_dimension.at(i).op_data).ssa_name << "]";
                }
                
            }
            std::cout << "(" << std::to_string(virtual_variables.at(n_instr.src.op_data).sensitivity)  << ")" << std::endl;
        }
        else if((n_instr.src.op_type == DATA_TYPE::INPUT_ARR_PTR) || (n_instr.src.op_type == DATA_TYPE::LOCAL_ARR_PTR) || (n_instr.src.op_type == DATA_TYPE::VIRTUAL_ARR_PTR)){
            array_information a_info = boost::get<array_information>(virtual_variables.at(n_instr.src.op_data).data_information);
            for(uint32_t i = 0; i < a_info.dimension_without_size ; ++i){
                std::cout << "*";
            }
            std::cout << virtual_variables.at(n_instr.src.op_data).ssa_name;
            
            for(uint32_t i = a_info.dimension_without_size; i < a_info.dimension; ++i){
                if(a_info.depth_per_dimension.at(i).op_type == DATA_TYPE::NUM){
                    std::cout << "[" << std::to_string(a_info.depth_per_dimension.at(i).op_data) << "]";
                }
                else{
                    std::cout << "[" << virtual_variables.at(a_info.depth_per_dimension.at(i).op_data).ssa_name << "]";
                }
                
            }
            std::cout << "(" << std::to_string(virtual_variables.at(n_instr.src.op_data).sensitivity)  << ")" << std::endl;
        }
        else{
            std::cout << virtual_variables.at(n_instr.src.op_data).ssa_name << "(" << std::to_string(virtual_variables.at(n_instr.src.op_data).sensitivity)  << ")";
        }
        std::cout << std::endl;
    }
}

void print_nop_expr(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security){
    normal_instruction n_instr = boost::get<normal_instruction>(instr.instr);
    if(print_security){
        if(instr.sensitivity_level != 1){
            std::cout << "PUB ";
        }
        else{
            std::cout << "SEC ";
        }
    }
    std::cout << virtual_variables.at(n_instr.dest.index_to_variable).ssa_name << "(" << std::to_string(virtual_variables.at(n_instr.dest.index_to_variable).sensitivity)<< ")" << " = " << virtual_variables.at(n_instr.src1.op_data).ssa_name << "(" << std::to_string(virtual_variables.at(n_instr.src1.op_data).sensitivity ) << ")" << std::endl;
}

void print_negate_expr(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security){
    normal_instruction n_instr = boost::get<normal_instruction>(instr.instr);
    if(print_security){
        if(instr.sensitivity_level != 1){
            std::cout << "PUB ";
        }
        else{
            std::cout << "SEC ";
        }
    }
    std::cout << virtual_variables.at(n_instr.dest.index_to_variable).ssa_name << "(" << std::to_string(virtual_variables.at(n_instr.dest.index_to_variable).sensitivity)<< ")" << " = -" << virtual_variables.at(n_instr.src1.op_data).ssa_name << "(" << std::to_string(virtual_variables.at(n_instr.src1.op_data).sensitivity ) << ")" << std::endl;
}

void print_label_expr(ssa& instr){
    label_instruction l_instr = boost::get<label_instruction>(instr.instr);
    std::cout << "." << l_instr.label_name << std::endl;
}

void print_normal(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security){
    normal_instruction n_instr = boost::get<normal_instruction>(instr.instr);
    if(print_security){
        if(instr.sensitivity_level != 1){
            std::cout << "PUB ";
        }
        else{
            std::cout << "SEC ";
        }
    }
    std::cout << virtual_variables.at(n_instr.dest.index_to_variable).ssa_name << "(" << std::to_string(virtual_variables.at(n_instr.dest.index_to_variable).sensitivity) << ")" << " = " << " ";
    if(n_instr.src1.op_type == DATA_TYPE::NUM){
        std::cout << std::to_string(n_instr.src1.op_data) << " ";
    }
    else{
        if((n_instr.src1.op_type == DATA_TYPE::INPUT_ARR) || (n_instr.src1.op_type == DATA_TYPE::INPUT_ARR_ADDR) || (n_instr.src1.op_type == DATA_TYPE::LOCAL_ARR) || (n_instr.src1.op_type == DATA_TYPE::LOCAL_ARR_ADDR) || (n_instr.src1.op_type == DATA_TYPE::VIRTUAL_ARR) || (n_instr.src1.op_type == DATA_TYPE::VIRTUAL_ARR_ADDR)){
            if((n_instr.src1.op_type == DATA_TYPE::INPUT_ARR_ADDR) || (n_instr.src1.op_type == DATA_TYPE::LOCAL_ARR_ADDR) || (n_instr.src1.op_type == DATA_TYPE::VIRTUAL_ARR_ADDR)){
                std::cout << "&";
            }
            std::cout << virtual_variables.at(n_instr.src1.op_data).ssa_name;
            array_information a_info = boost::get<array_information>(virtual_variables.at(n_instr.src1.op_data).data_information);
            for(uint32_t i = 0; i < a_info.dimension; ++i){
                if(a_info.depth_per_dimension.at(i).op_type == DATA_TYPE::NUM){
                    std::cout << "[" << std::to_string(a_info.depth_per_dimension.at(i).op_data) << "]";
                }
                else{
                    std::cout << "[" << virtual_variables.at(a_info.depth_per_dimension.at(i).op_data).ssa_name << "]";
                }
                
            }
            std::cout << "(" << std::to_string(virtual_variables.at(n_instr.src1.op_data).sensitivity)  << ")" << std::endl;
        }
        else if((n_instr.src1.op_type == DATA_TYPE::INPUT_ARR_PTR) || (n_instr.src1.op_type == DATA_TYPE::LOCAL_ARR_PTR) || (n_instr.src1.op_type == DATA_TYPE::VIRTUAL_ARR_PTR)){
            array_information a_info = boost::get<array_information>(virtual_variables.at(n_instr.src1.op_data).data_information);
            for(uint32_t i = 0; i < a_info.dimension_without_size ; ++i){
                std::cout << "*";
            }
            std::cout << virtual_variables.at(n_instr.src1.op_data).ssa_name;
            
            for(uint32_t i = a_info.dimension_without_size; i < a_info.dimension; ++i){
                if(a_info.depth_per_dimension.at(i).op_type == DATA_TYPE::NUM){
                    std::cout << "[" << std::to_string(a_info.depth_per_dimension.at(i).op_data) << "]";
                }
                else{
                    std::cout << "[" << virtual_variables.at(a_info.depth_per_dimension.at(i).op_data).ssa_name << "]";
                }
                
            }
            std::cout << "(" << std::to_string(virtual_variables.at(n_instr.src1.op_data).sensitivity)  << ")" << std::endl;
        }
        else{
            std::cout << virtual_variables.at(n_instr.src1.op_data).ssa_name << "(" << std::to_string(virtual_variables.at(n_instr.src1.op_data).sensitivity)  << ")";
        }
        std::cout << " ";
    }
    switch(n_instr.operation){
        case(Operation::ADD): std::cout << "+ "; break;
        case(Operation::SUB): std::cout << "- "; break;
        case(Operation::MUL): std::cout << "* "; break;
        case(Operation::XOR): std::cout << "^ "; break;
        case(Operation::INVERT): std::cout << "~ "; break;
        case(Operation::DIV): std::cout << "/ "; break;
        case(Operation::LOGICAL_AND): std::cout << "& "; break;
        case(Operation::LOGICAL_OR): std::cout << "| "; break;
        case(Operation::LSL): std::cout << "<< "; break;
        case(Operation::LSR): std::cout << ">> "; break; 
        default: throw std::runtime_error("in n_instr: unkown operation");

    }

    if(n_instr.src2.op_type == DATA_TYPE::NUM){
        std::cout << std::to_string(n_instr.src2.op_data) << std::endl;
    }
    else if(n_instr.src2.op_type == DATA_TYPE::NONE){
        std::cout << std::endl;
    }
    else{
        if((n_instr.src2.op_type == DATA_TYPE::INPUT_ARR) || (n_instr.src2.op_type == DATA_TYPE::INPUT_ARR_ADDR) || (n_instr.src2.op_type == DATA_TYPE::LOCAL_ARR) || (n_instr.src2.op_type == DATA_TYPE::LOCAL_ARR_ADDR) || (n_instr.src2.op_type == DATA_TYPE::VIRTUAL_ARR) || (n_instr.src2.op_type == DATA_TYPE::VIRTUAL_ARR_ADDR)){
            if((n_instr.src2.op_type == DATA_TYPE::INPUT_ARR_ADDR) || (n_instr.src2.op_type == DATA_TYPE::LOCAL_ARR_ADDR) || (n_instr.src2.op_type == DATA_TYPE::VIRTUAL_ARR_ADDR)){
                std::cout << "&";
            }
            std::cout << virtual_variables.at(n_instr.src2.op_data).ssa_name;
            array_information a_info = boost::get<array_information>(virtual_variables.at(n_instr.src2.op_data).data_information);
            for(uint32_t i = 0; i < a_info.dimension; ++i){
                if(a_info.depth_per_dimension.at(i).op_type == DATA_TYPE::NUM){
                    std::cout << "[" << std::to_string(a_info.depth_per_dimension.at(i).op_data) << "]";
                }
                else{
                    std::cout << "[" << virtual_variables.at(a_info.depth_per_dimension.at(i).op_data).ssa_name << "]";
                }
                
            }
            std::cout << "(" << std::to_string(virtual_variables.at(n_instr.src2.op_data).sensitivity)  << ")" << std::endl;
        }
        else if((n_instr.src2.op_type == DATA_TYPE::INPUT_ARR_PTR) || (n_instr.src2.op_type == DATA_TYPE::LOCAL_ARR_PTR) || (n_instr.src2.op_type == DATA_TYPE::VIRTUAL_ARR_PTR)){
            array_information a_info = boost::get<array_information>(virtual_variables.at(n_instr.src2.op_data).data_information);
            for(uint32_t i = 0; i < a_info.dimension_without_size ; ++i){
                std::cout << "*";
            }
            std::cout << virtual_variables.at(n_instr.src2.op_data).ssa_name;
            
            for(uint32_t i = a_info.dimension_without_size; i < a_info.dimension; ++i){
                if(a_info.depth_per_dimension.at(i).op_type == DATA_TYPE::NUM){
                    std::cout << "[" << std::to_string(a_info.depth_per_dimension.at(i).op_data) << "]";
                }
                else{
                    std::cout << "[" << virtual_variables.at(a_info.depth_per_dimension.at(i).op_data).ssa_name << "]";
                }
                
            }
            std::cout << "(" << std::to_string(virtual_variables.at(n_instr.src2.op_data).sensitivity)  << ")" << std::endl;
        }
        else{
            std::cout << virtual_variables.at(n_instr.src2.op_data).ssa_name << "(" << std::to_string(virtual_variables.at(n_instr.src2.op_data).sensitivity)  << ")";
        }
        std::cout << " ";
        std::cout << std::endl;
    }
    
}
void print_condition(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security){
    if(print_security){
        if(instr.sensitivity_level != 1){
            std::cout << "PUB ";
        }
        else{
            std::cout << "SEC ";
        }
    }
    condition_instruction c_instr = boost::get<condition_instruction>(instr.instr);

    std::cout << "if ("; 
    if(c_instr.op1.op_type == DATA_TYPE::NUM){
        std::cout << std::to_string(c_instr.op1.op_data) <<"(0) ";
    }
    else{
        std::cout << virtual_variables.at(c_instr.op1.op_data).ssa_name << "(" << std::to_string(virtual_variables.at(c_instr.op1.op_data).sensitivity) << ")" << " ";
    }

    switch(c_instr.op){
        case(Operation::LESS): std::cout << "< "; break;
        case(Operation::GREATER): std::cout << "> "; break;
        case(Operation::LESS_EQUAL): std::cout << "<= "; break;
        case(Operation::GREATER_EQUAL): std::cout << ">= "; break;
        case(Operation::EQUAL): std::cout << "== "; break;
        case(Operation::NOT_EQUAL): std::cout << "!= "; break;
        default: throw std::runtime_error("in c_instr: unkown operation");
    }

    if(c_instr.op2.op_type == DATA_TYPE::NUM){
        std::cout << std::to_string(c_instr.op2.op_data) << "(0)" << ") "<< std::endl;
    }
    else{
        std::cout << virtual_variables.at(c_instr.op2.op_data).ssa_name << "(" << std::to_string(virtual_variables.at(c_instr.op2.op_data).sensitivity) << ")" << ")";
    }
    std::cout << " goto bb" << std::to_string(c_instr.true_dest) << " else goto bb" << std::to_string(c_instr.false_dest) << std::endl;

}
void print_branch(ssa& instr){
    branch_instruction b_instr = boost::get<branch_instruction>(instr.instr);
    std::cout << "PUB ";
    std::cout << "JMP " << std::to_string(b_instr.dest_bb) << std::endl;
}
void print_func_return(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security){
    if(print_security){
        if(instr.sensitivity_level != 1){
            std::cout << "PUB ";
        }
        else{
            std::cout << "SEC ";
        }
    }
    function_call_with_return_instruction f_instr = boost::get<function_call_with_return_instruction>(instr.instr);
    std::cout << virtual_variables.at(f_instr.dest.index_to_variable).ssa_name << " = " << f_instr.func_name << "(";
    for(const auto& param: f_instr.parameters){
        if(param.op.op_type == DATA_TYPE::NUM){
            std::cout << std::to_string(param.op.op_data) << " ";
        }
        else{
            std::cout << virtual_variables.at(param.op.op_data).ssa_name << " ";
        }
    }
    std::cout << ")" << std::endl;
}
void print_func_no_return(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security){
    function_call_without_return_instruction f_instr = boost::get<function_call_without_return_instruction>(instr.instr);
    if(print_security){
        if(instr.sensitivity_level != 1){
            std::cout << "PUB ";
        }
        else{
            std::cout << "SEC ";
        }
    }
    std::cout << f_instr.func_name << "(";
    for(const auto& param: f_instr.parameters){
        if(param.op.op_type == DATA_TYPE::NUM){
            std::cout << std::to_string(param.op.op_data) << " ";
        }
        else{
            std::cout << virtual_variables.at(param.op.op_data).ssa_name << " ";
        }
    }
    std::cout << ")" << std::endl;
}

void print_deref(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security){
    dereference_instruction a_instr = boost::get<dereference_instruction>(instr.instr);
    if(print_security){
        if(instr.sensitivity_level != 1){
            std::cout << "PUB ";
        }
        else{
            std::cout << "SEC ";
        }
    }

    std::cout << virtual_variables.at(a_instr.dest.index_to_variable).ssa_name << "(" << std::to_string(virtual_variables.at(a_instr.dest.index_to_variable).sensitivity) << ") = ";
    if((virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::INPUT_ARR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::INPUT_ARR_ADDR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::LOCAL_ARR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::LOCAL_ARR_ADDR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::VIRTUAL_ARR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::VIRTUAL_ARR_ADDR)){
        if((virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::INPUT_ARR_ADDR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::LOCAL_ARR_ADDR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::VIRTUAL_ARR_ADDR)){
            std::cout << "&";
        }
        std::cout << virtual_variables.at(a_instr.src.index_to_variable).ssa_name;
        array_information a_info = boost::get<array_information>(virtual_variables.at(a_instr.src.index_to_variable).data_information);
        for(uint32_t i = 0; i < a_info.dimension; ++i){
            if(a_info.depth_per_dimension.at(i).op_type == DATA_TYPE::NUM){
                std::cout << "[" << std::to_string(a_info.depth_per_dimension.at(i).op_data) << "]";
            }
            else{
                std::cout << "[" << virtual_variables.at(a_info.depth_per_dimension.at(i).op_data).ssa_name << "]";
            }
            
        }
        std::cout << "(" << std::to_string(virtual_variables.at(a_instr.src.index_to_variable).sensitivity)  << ")" << std::endl;
    }
    else if((virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::INPUT_ARR_PTR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::LOCAL_ARR_PTR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::VIRTUAL_ARR_PTR)){
        array_information a_info = boost::get<array_information>(virtual_variables.at(a_instr.src.index_to_variable).data_information);
        for(uint32_t i = 0; i < a_info.dimension_without_size ; ++i){
            std::cout << "*";
        }
        std::cout << virtual_variables.at(a_instr.src.index_to_variable).ssa_name;
        
        for(uint32_t i = a_info.dimension_without_size; i < a_info.dimension; ++i){
            if(a_info.depth_per_dimension.at(i).op_type == DATA_TYPE::NUM){
                std::cout << "[" << std::to_string(a_info.depth_per_dimension.at(i).op_data) << "]";
            }
            else{
                std::cout << "[" << virtual_variables.at(a_info.depth_per_dimension.at(i).op_data).ssa_name << "]";
            }
            
        }
        std::cout << "(" << std::to_string(virtual_variables.at(a_instr.src.index_to_variable).sensitivity)  << ")" << std::endl;
    }
    else{
        std::cout << virtual_variables.at(a_instr.src.index_to_variable).ssa_name << "(" << std::to_string(virtual_variables.at(a_instr.src.index_to_variable).sensitivity)  << ")";
    }
    std::cout << std::endl;
}

void print_mem_load(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security){
    mem_load_instruction m_instr = boost::get<mem_load_instruction>(instr.instr);
    if(print_security){
        if(instr.sensitivity_level != 1){
            std::cout << "PUB ";
        }
        else{
            std::cout << "SEC ";
        }
    }
    std::cout << virtual_variables.at(m_instr.dest.index_to_variable).ssa_name << "(" << std::to_string(virtual_variables.at(m_instr.dest.index_to_variable).sensitivity) << ")" << "= MEM[" << virtual_variables.at(m_instr.base_address.index_to_variable).ssa_name;
    std::cout << "(" << virtual_variables.at(m_instr.base_address.index_to_variable).sensitivity << ")" << " + " << std::to_string(m_instr.offset.op_data) << std::endl;
}

void print_mem_store(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security){
    mem_store_instruction m_instr = boost::get<mem_store_instruction>(instr.instr);
    if(print_security){
        if(instr.sensitivity_level != 1){
            std::cout << "PUB ";
        }
        else{
            std::cout << "SEC ";
        }
    }
    std::cout << "*" << virtual_variables.at(m_instr.dest.index_to_variable).ssa_name << "(" << std::to_string(virtual_variables.at(m_instr.dest.index_to_variable).sensitivity) << ")" << " = ";
    if(m_instr.value.op_type == DATA_TYPE::NUM){
        std::cout << std::to_string(m_instr.value.op_data) << "(0)" << std::endl;
    } 
    else{
        //TODO: ignore arrays and pointers for now
        std::cout <<  virtual_variables.at(m_instr.value.op_data).ssa_name << "(" << virtual_variables.at(m_instr.value.op_data).sensitivity << ")" << std::endl;
    }
}

void print_return(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security){
    return_instruction r_instr = boost::get<return_instruction>(instr.instr);
    if(print_security){
        if(instr.sensitivity_level != 1){
            std::cout << "PUB ";
        }
        else{
            std::cout << "SEC ";
        }
    }
    std::cout << "return " ;
    if(r_instr.return_value.op_type != DATA_TYPE::NONE){
        if(r_instr.return_value.op_type == DATA_TYPE::NUM){
            std::cout << std::to_string(r_instr.return_value.op_data) << "(0)" << std::endl;
        } 
        else{
            std::cout <<  virtual_variables.at(r_instr.return_value.op_data).ssa_name << "(" << virtual_variables.at(r_instr.return_value.op_data).sensitivity << ")" << std::endl;
        }
    }

}

void print_ref(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security){
    reference_instruction a_instr = boost::get<reference_instruction>(instr.instr);
    if(print_security){
        if(instr.sensitivity_level != 1){
            std::cout << "PUB ";
        }
        else{
            std::cout << "SEC ";
        }
    }
    std::cout << virtual_variables.at(a_instr.dest.index_to_variable).ssa_name << "(" << std::to_string(virtual_variables.at(a_instr.dest.index_to_variable).sensitivity) << ")" << "= ";
    if((virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::INPUT_ARR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::INPUT_ARR_ADDR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::LOCAL_ARR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::LOCAL_ARR_ADDR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::VIRTUAL_ARR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::VIRTUAL_ARR_ADDR)){
        if((virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::INPUT_ARR_ADDR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::LOCAL_ARR_ADDR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::VIRTUAL_ARR_ADDR)){
            std::cout << "&";
        }
        std::cout << virtual_variables.at(a_instr.src.index_to_variable).ssa_name;
        array_information a_info = boost::get<array_information>(virtual_variables.at(a_instr.src.index_to_variable).data_information);
        for(uint32_t i = 0; i < a_info.dimension; ++i){
            if(a_info.depth_per_dimension.at(i).op_type == DATA_TYPE::NUM){
                std::cout << "[" << std::to_string(a_info.depth_per_dimension.at(i).op_data) << "]";
            }
            else{
                std::cout << "[" << virtual_variables.at(a_info.depth_per_dimension.at(i).op_data).ssa_name << "]";
            }
            
        }
        std::cout << "(" << std::to_string(virtual_variables.at(a_instr.src.index_to_variable).sensitivity)  << ")" << std::endl;
    }
    else if((virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::INPUT_ARR_PTR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::LOCAL_ARR_PTR) || (virtual_variables.at(a_instr.src.index_to_variable).data_type == DATA_TYPE::VIRTUAL_ARR_PTR)){
        array_information a_info = boost::get<array_information>(virtual_variables.at(a_instr.src.index_to_variable).data_information);
        for(uint32_t i = 0; i < a_info.dimension_without_size ; ++i){
            std::cout << "*";
        }
        std::cout << virtual_variables.at(a_instr.src.index_to_variable).ssa_name;
        
        for(uint32_t i = a_info.dimension_without_size; i < a_info.dimension; ++i){
            if(a_info.depth_per_dimension.at(i).op_type == DATA_TYPE::NUM){
                std::cout << "[" << std::to_string(a_info.depth_per_dimension.at(i).op_data) << "]";
            }
            else{
                std::cout << "[" << virtual_variables.at(a_info.depth_per_dimension.at(i).op_data).ssa_name << "]";
            }
            
        }
        std::cout << "(" << std::to_string(virtual_variables.at(a_instr.src.index_to_variable).sensitivity)  << ")" << std::endl;
    }
    else{
        std::cout << virtual_variables.at(a_instr.src.index_to_variable).ssa_name << "(" << std::to_string(virtual_variables.at(a_instr.src.index_to_variable).sensitivity)  << ")";
    }
    std::cout << std::endl;
}

void print_phi(ssa& instr, virtual_variables_vec& virtual_variables, bool& print_security){
    phi_instruction a_instr = boost::get<phi_instruction>(instr.instr);
    if(print_security){
        if(instr.sensitivity_level != 1){
            std::cout << "PUB ";
        }
        else{
            std::cout << "SEC ";
        }
    }
    std::cout << virtual_variables.at(a_instr.dest.index_to_variable).ssa_name << "(" << std::to_string(virtual_variables.at(a_instr.dest.index_to_variable).sensitivity) << ")" << "= PHI(";
    for(const auto& tup_src: a_instr.srcs){
        uint32_t src_index = std::get<0>(tup_src);
        if((virtual_variables.at(src_index).data_type == DATA_TYPE::INPUT_ARR) || (virtual_variables.at(src_index).data_type == DATA_TYPE::INPUT_ARR_ADDR) || (virtual_variables.at(src_index).data_type == DATA_TYPE::LOCAL_ARR) || (virtual_variables.at(src_index).data_type == DATA_TYPE::LOCAL_ARR_ADDR) || (virtual_variables.at(src_index).data_type == DATA_TYPE::VIRTUAL_ARR) || (virtual_variables.at(src_index).data_type == DATA_TYPE::VIRTUAL_ARR_ADDR)){
            if((virtual_variables.at(src_index).data_type == DATA_TYPE::INPUT_ARR_ADDR) || (virtual_variables.at(src_index).data_type == DATA_TYPE::LOCAL_ARR_ADDR) || (virtual_variables.at(src_index).data_type == DATA_TYPE::VIRTUAL_ARR_ADDR)){
                std::cout << "&";
            }
            std::cout << virtual_variables.at(src_index).ssa_name;
            array_information a_info = boost::get<array_information>(virtual_variables.at(src_index).data_information);
            for(uint32_t i = 0; i < a_info.dimension; ++i){
                if(a_info.depth_per_dimension.at(i).op_type == DATA_TYPE::NUM){
                    std::cout << "[" << std::to_string(a_info.depth_per_dimension.at(i).op_data) << "]";
                }
                else{
                    std::cout << "[" << virtual_variables.at(a_info.depth_per_dimension.at(i).op_data).ssa_name << "]";
                }
                
            }
            std::cout << "(" << std::to_string(virtual_variables.at(src_index).sensitivity)  << ")" << std::endl;
        }
        else if((virtual_variables.at(src_index).data_type == DATA_TYPE::INPUT_ARR_PTR) || (virtual_variables.at(src_index).data_type == DATA_TYPE::LOCAL_ARR_PTR) || (virtual_variables.at(src_index).data_type == DATA_TYPE::VIRTUAL_ARR_PTR)){
            array_information a_info = boost::get<array_information>(virtual_variables.at(src_index).data_information);
            for(uint32_t i = 0; i < a_info.dimension_without_size ; ++i){
                std::cout << "*";
            }
            std::cout << virtual_variables.at(src_index).ssa_name;
            
            for(uint32_t i = a_info.dimension_without_size; i < a_info.dimension; ++i){
                if(a_info.depth_per_dimension.at(i).op_type == DATA_TYPE::NUM){
                    std::cout << "[" << std::to_string(a_info.depth_per_dimension.at(i).op_data) << "]";
                }
                else{
                    std::cout << "[" << virtual_variables.at(a_info.depth_per_dimension.at(i).op_data).ssa_name << "]";
                }
                
            }
            std::cout << "(" << std::to_string(virtual_variables.at(src_index).sensitivity)  << ")" << std::endl;
        }
        else{
            std::cout << virtual_variables.at(src_index).ssa_name << "(" << std::to_string(virtual_variables.at(src_index).sensitivity)  << ")";
        }
    }
    std::cout << std::endl;
}

void print_func(function& func, virtual_variables_vec& virtual_variables, bool& print_security){
    for(basic_block& bb: func.bb){
        std::cout << "BB " << bb.bb_number << std::endl;
        for(ssa& instr: bb.ssa_instruction){
            switch(instr.instr_type){
                case(INSTRUCTION_TYPE::INSTR_BRANCH): print_branch(instr); break;
                case(INSTRUCTION_TYPE::INSTR_NORMAL): print_normal(instr, virtual_variables, print_security);break;
                case(INSTRUCTION_TYPE::INSTR_REF):  print_ref(instr, virtual_variables, print_security);break;
                case(INSTRUCTION_TYPE::INSTR_DEREF): print_deref(instr, virtual_variables, print_security); break;
                case(INSTRUCTION_TYPE::INSTR_FUNC_RETURN): print_func_return(instr, virtual_variables, print_security);break;
                case(INSTRUCTION_TYPE::INSTR_FUNC_NO_RETURN): print_func_no_return(instr, virtual_variables, print_security);break;
                case(INSTRUCTION_TYPE::INSTR_CONDITON): print_condition(instr, virtual_variables, print_security);break;
                case(INSTRUCTION_TYPE::INSTR_SPECIAL): throw std::runtime_error("there is no special instruction"); break;
                case(INSTRUCTION_TYPE::INSTR_ASSIGN): print_assign(instr, virtual_variables, print_security); break;
                case(INSTRUCTION_TYPE::INSTR_PHI): print_phi(instr, virtual_variables, print_security); break;
                case(INSTRUCTION_TYPE::INSTR_LOAD): print_mem_load(instr, virtual_variables, print_security); break;
                case(INSTRUCTION_TYPE::INSTR_STORE): print_mem_store(instr, virtual_variables, print_security); break;
                case(INSTRUCTION_TYPE::INSTR_RETURN): print_return(instr, virtual_variables, print_security); break;
                case(INSTRUCTION_TYPE::INSTR_DEFAULT): std::cout << "default instruction " << std::endl; break;
                case(INSTRUCTION_TYPE::INSTR_NEGATE): print_negate_expr(instr, virtual_variables, print_security); break;
                case(INSTRUCTION_TYPE::INSTR_LABEL): print_label_expr(instr); break;
                case(INSTRUCTION_TYPE::INSTR_INITIALIZE_LOCAL_ARR): break;//print_initialize_local_arr_expr(instr, virtual_variables); break;
                default: throw std::runtime_error("unkown instruction type");
            }
        }
        std::cout << std::endl;
    }

}


void print_bb(basic_block& bb, virtual_variables_vec& virtual_variables, bool& print_security){

        std::cout << "BB " << bb.bb_number << std::endl;
        for(ssa& instr: bb.ssa_instruction){
            switch(instr.instr_type){
                case(INSTRUCTION_TYPE::INSTR_BRANCH): print_branch(instr); break;
                case(INSTRUCTION_TYPE::INSTR_NORMAL): print_normal(instr, virtual_variables, print_security);break;
                case(INSTRUCTION_TYPE::INSTR_REF):  print_ref(instr, virtual_variables, print_security);break;
                case(INSTRUCTION_TYPE::INSTR_DEREF): print_deref(instr, virtual_variables, print_security); break;
                case(INSTRUCTION_TYPE::INSTR_FUNC_RETURN): print_func_return(instr, virtual_variables, print_security);break;
                case(INSTRUCTION_TYPE::INSTR_FUNC_NO_RETURN): print_func_no_return(instr, virtual_variables, print_security);break;
                case(INSTRUCTION_TYPE::INSTR_CONDITON): print_condition(instr, virtual_variables, print_security);break;
                case(INSTRUCTION_TYPE::INSTR_SPECIAL): throw std::runtime_error("there is no special instruction"); break;
                case(INSTRUCTION_TYPE::INSTR_ASSIGN): print_assign(instr, virtual_variables, print_security); break;
                case(INSTRUCTION_TYPE::INSTR_PHI): print_phi(instr, virtual_variables, print_security); break;
                case(INSTRUCTION_TYPE::INSTR_LOAD): print_mem_load(instr, virtual_variables, print_security); break;
                case(INSTRUCTION_TYPE::INSTR_RETURN): print_return(instr, virtual_variables, print_security); break;
                case(INSTRUCTION_TYPE::INSTR_DEFAULT): std::cout << "default instruction " << std::endl; break;
                case(INSTRUCTION_TYPE::INSTR_NEGATE): print_negate_expr(instr, virtual_variables, print_security); break;
                case(INSTRUCTION_TYPE::INSTR_LABEL): print_label_expr(instr); break;
                case(INSTRUCTION_TYPE::INSTR_INITIALIZE_LOCAL_ARR): break;//print_initialize_local_arr_expr(instr, virtual_variables); break;
                default: throw std::runtime_error("unkown instruction type");
            }
        }
        std::cout << std::endl;

}