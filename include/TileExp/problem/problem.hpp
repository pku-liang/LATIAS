#pragma once 

#include "mapping/parser.hpp"

#include "./TileExp/common.hpp"


namespace problem {

namespace TileExp {

    typedef unsigned TensorIndex;

    class Workloads;

    class Workload: public problem::Workload {
        std::vector<std::string> ins_; // Input tensor
        std::string out_;
        std::string name_;  
        Workloads& workloads_;
        bool binding_applied = false;
    public: 
        Workload(Workloads& workloads): workloads_(workloads){}
        inline void set_name(const std::string & name){name_ = name;}
        void set_io(const std::vector<std::string>& ins, const std::vector<std::string>& outs);
        inline const std::vector<std::string>& get_ins() const { return ins_; }
        inline const std::string & get_out() const {return out_;}
        inline const std::string & get_name() const {return name_;} 
        inline const problem::Workload::FactorizedBounds& get_factorized_bounds() {return factorized_bounds_;}
        void Print(std::ostream& o = std::cout);
        friend class Workloads;
        // void set_common_shape();
    };

    class Workloads{
        std::unordered_map<std::string, std::shared_ptr<problem::TileExp::Workload> > workloads_;
        std::vector<std::string> ins_;
        std::vector<std::string> outs_;
        config::CompoundConfigNode coeffs_;

        problem::Workload::FactorizedBounds factorized_bounds_; // loop factorized
        problem::Workload::FlattenedBounds flattened_bounds_; // loop flatten
        problem::Workload::Coefficients coefficients_; // other problem input param
        problem::Workload::Densities densities_; // do not use
        problem::Shape common_shape_;

        mutable problem::Workload workload_;
        mutable bool workload_constructed_ = false;

    public:
        Workloads() {
            common_shape_.UsesFlattening = false; coefficients_[-1] = 1; 
            common_shape_.DefaultCoefficients[-1] = 1;}
        bool add_workload(const std::string& name, std::shared_ptr<problem::TileExp::Workload>& workload);
        std::shared_ptr<problem::TileExp::Workload> get_workload(const std::string & op_name) const {
            TILEEXP_ASSERT(workloads_.count(op_name), op_name << " Not FOUND");
            return workloads_.at(op_name);
        }
        void set_io(const std::vector<std::string>& ins, const std::vector<std::string>& outs);
        void set_coeffs(const config::CompoundConfigNode& coeffs);
        void set_factorized_bound(const std::string& dim, int bound);
        void set_dims(const std::vector<std::string>& dims);

        void Print();
        
        const std::vector<std::string>& get_ins() const {return ins_;}
        const std::vector<std::string>& get_outs() const {return outs_;}
        const problem::Workload& get_workload() const;
        const problem::Shape& get_shape() const {return common_shape_;}
        const problem::Workload::FactorizedBounds get_factorized_bounds() const {return factorized_bounds_;}
        // const problem::Shape& get_common_shape() { return common_shape_; }

        friend class Workload;
    };

    void ParseWorkloads(config::CompoundConfigNode config, Workloads& workloads_); // analyze prob.yaml

} // namespace TileExp 

} // namespace problem