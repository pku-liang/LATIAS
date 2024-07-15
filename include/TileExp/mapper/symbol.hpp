#pragma once 

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <set>
#include <functional>
#include <iostream>

namespace Symbol
{

    typedef size_t num_t;
    
    // describe the tiling variables
    struct Entry {
        std::string name_;
        num_t value_;
        int idx_;
        std::set<num_t> candidates_;
        bool fixed_ = false; // leaf node = true
    };

    std::ostream& operator<< (std::ostream& o, const Entry&);  //TBD

    
// store variable
    class  SymbolTable {
        std::unordered_map<std::string, int> name2idx_;  
        std::unordered_map<int, Entry> idx2values_;
        bool failed_ = false; 
        // bool fail_check(const std::vector<Constraint>& constraints_);

    public:
        int idx = 0;
        const Entry& lookup(const std::string& key) const {return idx2values_.at(name2idx_.at(key));}
        const Entry& lookup(int key) const {return idx2values_.at(key);}
        int count(int key) const {return idx2values_.count(key);}
        bool is_terminated() const {
            for(auto& kv: idx2values_) 
                if(!kv.second.fixed_) return false;
            return true;
        }
        int get_num_variables() const {return -idx;}
        int count_unfixed() const {int ret = idx2values_.size(); for (auto& kv: idx2values_) ret -= kv.second.fixed_; return ret;}
        // int get_next_var() const; // TBD
        // void show_brief(std::ostream& o) const; // TBD
        
        
        Entry& operator[](int key) {return idx2values_[key];}

        // insert var to table, and return -idx for denote this is a var and its idx
        int insert(const std::string name = "") {
            std::string name_ = name;
            if (name_ == "?" || name_ == "X"){
                for (int i = 0; name2idx_.count(name_); i++){
                    name_ = name + std::to_string(i);
                }
            }
            if (name2idx_.count(name_) == 0) {
                idx--;
                name2idx_[name_] = idx;
                idx2values_[idx] = {name_, 0, idx, {}, false};
            }
            // return name2idx_.at(name_); // Ray
            return -name2idx_.at(name_);
        }

        // void init(const std::vector<Constraint>& constraints_);
        // void fix_and_update(int index, num_t value, const std::vector<Constraint>& constraints_);
    };

    extern SymbolTable global_symbol_table_;

    
} // namespace Symbol
