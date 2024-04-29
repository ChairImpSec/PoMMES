#include "check_functions.hpp"

bool is_comment(std::string& line){
    std::string token = line.substr(0, line.find(" "));
    if(token == ";;")
    {
        return true;
    }
    return false;
}

bool is_number(const std::string token){
    boost::regex expression("^[0-9]*[.]?[0-9]+$");
    if(boost::regex_match(token, expression)){
        return true;
    }
    return false;
}

bool is_array(const std::string token){
    if(token.find("[") != std::string::npos){
        return true;
    }
    return false;
}


bool is_variable(const std::string token){
    if(token.rfind("*", 0) == 0){
        return false;
    }
    if(token.find("[") != std::string::npos){
        return false;
    }
    if(token.find("&") != std::string::npos){
        return false;
    }
    return true;
}

/**
 * @brief all virtual variables have a '_' at the start of the identifier, if this is the case for token the variable is virtual
 * 
 * @param token 
 * @return true 
 * @return false 
 */
bool is_virtual_variable(const std::string token){
    if(token.rfind("_", 0) == 0){
        return true;
    }
    return false;
}


bool is_end_of_function(const std::string& line){
    if(line.find("}") != std::string::npos)
    {
        return true;
    }
    return false;
}

bool is_start_of_bb(const std::string& line){
    if(line.find(":") != std::string::npos)
    {
        return true;
    }
    return false;
}

bool is_end_of_bb(const std::string& line){
    if(line.size() == 0)
    {
        return true;
    }
    return false;
}

bool is_empty_or_blank(const std::string &s) {
    return s.find_first_not_of(" \t") == std::string::npos;
}

bool is_phi_ssa(std::vector<std::string> tokens){
    

    if((std::find(tokens.begin(), tokens.end(), "gimple_phi") != tokens.end()) && (tokens.at(0) == "#"))
    {
        return true;
    }
    else{
        return false;
    }
}
