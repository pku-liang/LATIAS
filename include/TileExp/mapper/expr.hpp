#pragma once 

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <set>
#include <functional>
#include <iostream>

namespace TileExp {
    const int ERROR_OUT=1;
    const int NORMAL_OUT=2;

    typedef size_t num_t;

    struct Constraint;

    struct Entry {
        std::string name_;
        num_t value_;
        int idx_;
        std::set<num_t> candidates_;
        bool fixed_ = false; 
    };

    std::ostream& operator<< (std::ostream& o, const Entry&);  //TBD

    class  SymbolTable {
        std::unordered_map<std::string, int> name2idx_;  
        std::unordered_map<int, Entry> idx2values_;
        bool failed_ = false; 
        bool fail_check(const std::vector<Constraint>& constraints_);

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
        int get_next_var() const;
        void show_brief(std::ostream& o) const;
        
        
        Entry& operator[](int key) {return idx2values_[key];}
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
            return name2idx_.at(name_);
        }

        void init(const std::vector<Constraint>& constraints_);
        void fix_and_update(int index, num_t value, const std::vector<Constraint>& constraints_);
    };

    struct Constraint {
        enum {
            LOOPCOUNT,
            MEM, 
            SPATIAL
        }type_; 
        std::shared_ptr<Expr> expr;
        std::string msg;
        std::string short_msg = "";
    };
}
