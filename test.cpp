#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <regex>
// std::vector<std::string> parseName2Vec(std::string name) {
//   std::vector<std::string> res;
//   auto posStart = name.find("[");
//   auto posEnd = name.find("]");
//   auto posDot = name.find(",");
//   // if (posStart != 0) {
//   while (posDot != std::string::npos) {
//     posStart += 1;
//     std::string part = name.substr(posStart, posDot - posStart);
//     if (!part.empty()) {
//         res.push_back(part);
//     }
//     posStart = posDot;
//     posDot = name.find(',', posStart);
//   }
//   if (posStart==std::string::npos && res.empty()){
//     res.push_back(name);
//   }
//   else{
//     std::string part = name.substr(posStart, posEnd - posStart);
//     if (!part.empty()) {
//         res.push_back(part);
//     }
//   }
//   return res;
// }

std::string trim(const std::string& str) {
    
    size_t first_s = str.find_first_not_of(' ');
    size_t last_s = str.find_last_not_of(' ');

    auto str_tmp = str.substr(first_s, last_s - first_s + 1);

    size_t first_l = str_tmp.find_first_not_of('[');
    size_t last_r = str_tmp.find_last_not_of(']');

    // size_t start = std::max(first_l, first_s);
    // size_t end = std::min(last_r, last_s);

    return str_tmp.substr(first_l, last_r - first_l + 1);
}

std::vector<std::string> parseName2Vec(const std::string& name) {
    std::vector<std::string> names;
    size_t start = 0;
    size_t end = name.find(',');

    while (end != std::string::npos) {
        std::string part = trim(name.substr(start, end - start));
        if (!part.empty()) {
            names.push_back(part);
        }
        start = end + 1;
        end = name.find(',', start);
    }

    // 添加最后一个部分
    std::string part = trim(name.substr(start));
    if (!part.empty()) {
        names.push_back(part);
    }

    return names;
}

std::unordered_map<std::string, std::pair<int, int> > ParseFactors(
    const std::string& buffer) {
    std::unordered_map<std::string, std::pair<int, int> > loop_bounds;
    std::regex re("([A-Za-z]+)[[:space:]]*[=]*[[:space:]]*([0-9A-Za-z_?]+)(,([0-9]+))?", std::regex::extended);
    std::smatch sm;
    std::string str = std::string(buffer);
    str = str.substr(0, str.find("#")); // remove comments

    while (std::regex_search(str, sm, re)) // each time load one []=[]
    {
        std::string dimension_name = sm[1];

        int end;
        char* ptr = nullptr;
        end = std::strtol(sm[2].str().c_str(), &ptr, 10);

        int residual_end = end;
        if (sm[4] != "")
        {
            residual_end = std::stoi(sm[4]);
        }

        loop_bounds[dimension_name] = {end, residual_end};

        str = sm.suffix().str();
    }

    return loop_bounds;
}

int main(){
    std::string name = "name";
    std::string name2 = "[name1, name2, name3]";

    std::vector<std::string> res = parseName2Vec(name);

    for (auto s : res){
        std::cout << s << std::endl;
    }

    std::vector<std::string> res2 = parseName2Vec(name2);

    for (auto s : res2){
        std::cout << s << std::endl;
    }

    std::string factors = "A=4,1, B=3, C=4";
    auto factor_p = ParseFactors(factors);
    for(auto& tmp : factor_p){
        std::cout << "Dim" << tmp.first << ":" << tmp.second.first << "end" << tmp.second.second << std::endl;
    }
}
