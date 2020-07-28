#include <trajopt_utils/macros.h>
TRAJOPT_IGNORE_WARNINGS_PUSH
#include <gtest/gtest.h>
#include <iostream>

#include <ifopt/problem.h>
#include <ifopt/ipopt_solver.h>
#include <ifopt/variable_set.h>
#include <ifopt/constraint_set.h>
#include <ifopt/cost_term.h>

TRAJOPT_IGNORE_WARNINGS_POP

const bool DEBUG = false;

namespace ifopt
{
using Eigen::Vector2d;

class ExVariables : public VariableSet
{
public:
  // Every variable set has a name, here "var_set1". this allows the constraints
  // and costs to define values and Jacobians specifically w.r.t this variable set.
  ExVariables() : ExVariables("var_set1"){};
  ExVariables(const std::string& name) : VariableSet(8, name)
  {
    // the initial values where the NLP starts iterating from
    values_ = VectorXd::Ones(8)*5; //start all at 5
    bounds_.at(0) = Bounds(0.0, 10.0);
    bounds_.at(1) = Bounds(0.0, 10.0);
    bounds_.at(2) = Bounds(0.0, 10.0);
    bounds_.at(3) = Bounds(0.0, 10.0);
    bounds_.at(4) = Bounds(0.0, 10.0);
    bounds_.at(5) = Bounds(0.0, 10.0);
    bounds_.at(6) = Bounds(0.0, 10.0);
  }

  // Here is where you can transform the Eigen::Vector into whatever
  // internal representation of your variables you have (here two doubles, but
  // can also be complex classes such as splines, etc..
  void SetVariables(const VectorXd& x) override
  {
    values_ = x;
  };

  // Here is the reverse transformation from the internal representation to
  // to the Eigen::Vector
  VectorXd GetValues() const override { return values_ };

  // Each variable has an upper and lower bound set here
  VecBound GetBounds() const override
  {
    return bounds_;
  }

private:
  VecBound bounds_;
  Eigen::VectorXd values_;
};

class ExConstraint : public ConstraintSet
{
public:
  ExConstraint() : ExConstraint("constraint1") {}

  // This constraint set just contains 1 constraint, however generally
  // each set can contain multiple related constraints.
  ExConstraint(const std::string& name) : ConstraintSet(1, name) {}

  // The constraint value minus the constant value "1", moved to bounds.
  VectorXd GetValues() const override
  {
    VectorXd g(static_cast<size_t>(GetRows()));
    Vector2d x = GetVariables()->GetComponent("var_set1")->GetValues();
    g(0) = x(0) + x(1) + x(2) + x(3);
    return g;
  };

  // The only constraint in this set is an equality constraint to 1.
  // Constant values should always be put into GetBounds(), not GetValues().
  // For inequality constraints (<,>), use Bounds(x, inf) or Bounds(-inf, x).
  VecBound GetBounds() const override
  {
    VecBound b(static_cast<size_t>(GetRows()));
    b.at(0) = Bounds(1.0, 1.0);
    return b;
  }

  // This function provides the first derivative of the constraints.
  // In case this is too difficult to write, you can also tell the solvers to
  // approximate the derivatives by finite differences and not overwrite this
  // function, e.g. in ipopt.cc::use_jacobian_approximation_ = true
  void FillJacobianBlock(std::string var_set, Jacobian& jac_block) const override
  {
    // must fill only that submatrix of the overall Jacobian that relates
    // to this constraint and "var_set1". even if more constraints or variables
    // classes are added, this submatrix will always start at row 0 and column 0,
    // thereby being independent from the overall problem.
    if (var_set == "var_set1")
    {
      Vector2d x = GetVariables()->GetComponent("var_set1")->GetValues();

      jac_block.coeffRef(0, 0) = 2.0 * x(0);  // derivative of first constraint w.r.t x0
      jac_block.coeffRef(0, 1) = 1.0;         // derivative of first constraint w.r.t x1
    }
  }
};

class ExCost : public CostTerm
{
public:
  ExCost() : ExCost("cost_term1") {}
  ExCost(const std::string& name) : CostTerm(name) {}

  double GetCost() const override
  {
    Vector2d x = GetVariables()->GetComponent("var_set1")->GetValues();
    return -std::pow(x(1) - 2, 2);
  };

  void FillJacobianBlock(std::string var_set, Jacobian& jac) const override
  {
    if (var_set == "var_set1")
    {
      Vector2d x = GetVariables()->GetComponent("var_set1")->GetValues();

      jac.coeffRef(0, 0) = 0.0;                  // derivative of cost w.r.t x0
      jac.coeffRef(0, 1) = -2.0 * (x(1) - 2.0);  // derivative of cost w.r.t x1
    }
  }
};

}  // namespace ifopt

class BasicOptimization : public testing::TestWithParam<const char*>
{
public:
  // 1) Create the problem
  ifopt::Problem nlp_;

  void SetUp() override
  {
    if (DEBUG)
      console_bridge::setLogLevel(console_bridge::LogLevel::CONSOLE_BRIDGE_LOG_DEBUG);
    else
      console_bridge::setLogLevel(console_bridge::LogLevel::CONSOLE_BRIDGE_LOG_NONE);

    nlp_.AddVariableSet(std::make_shared<ifopt::ExVariables>());
    nlp_.AddConstraintSet(std::make_shared<ifopt::ExConstraint>());
    nlp_.AddCostSet(std::make_shared<ifopt::ExCost>());

    if (DEBUG)
    {
      nlp_.PrintCurrent();
      std::cout << "Jacobian: \n" << nlp_.GetJacobianOfConstraints() << std::endl;
    }
  }
};

/**
 * @brief Allows for any solver to run tests on the above class. The solver has to have a .Solve(ifopt::Problem&)
 * function
 * @param solver Any solver with a .Solve(ifopt::Problem&) function
 * @param nlp_opt An ifopt defined problem (like the one above)
 */
template <class T>
inline void runTests(T solver, ifopt::Problem nlp_opt)
{
  // Solve and test nlp
  solver.Solve(nlp_opt);
  Eigen::VectorXd x = nlp_opt.GetOptVariables()->GetValues();
  EXPECT_NEAR(x(0), 1.0, 1e-5);
  EXPECT_NEAR(x(1), 0.0, 1e-5);

  if (DEBUG)
  {
    std::cout << std::endl;
    std::cout << "x(0): " << x(0) << std::endl;
    std::cout << "x(1): " << x(1) << std::endl;
    std::cout << std::endl;
    nlp_opt.PrintCurrent();
  }
}
