#include "parser/ast.h"

#include <boost/variant.hpp>

#include "dag/node.h"
#include "parser/build_context.h"

namespace ccs { namespace ast {

struct AstVisitor : public boost::static_visitor<void> {
  BuildContext &buildContext;
  BuildContext &baseContext;

  AstVisitor(BuildContext &buildContext, BuildContext &baseContext) :
    buildContext(buildContext), baseContext(baseContext) {}

  void operator()(Import &import) const {
    // TODO import.ast.addTo(buildContext, baseContext);
  }

  void operator()(PropDef &propDef) const {
    buildContext.addProperty(propDef);
  }

  void operator()(Constraint &constraint) const {
    buildContext.node().addConstraint(constraint.key_);
  }

  void operator()(Nested &nested) const {
    nested.addTo(buildContext, baseContext);
  }
};

void Nested::addTo(BuildContext &buildContext, BuildContext &baseContext) {
  BuildContext *bc = &buildContext;
  if (selector_)
    bc = selector_->traverse(buildContext, baseContext);
  for (auto it = rules_.begin(); it != rules_.end(); ++it)
    boost::apply_visitor(AstVisitor(*bc, baseContext), *it);
}


struct Wrap : public SelectorLeaf {
  std::vector<SelectorBranch *> branches;
  SelectorLeaf *right;

  Wrap(SelectorBranch *branch, SelectorLeaf *leaf) : right(leaf)
    { branches.push_back(branch); }

  virtual SelectorLeaf *descendant(SelectorLeaf *right)
    { return push(SelectorBranch::descendant(this->right), right); }
  virtual SelectorLeaf *conjunction(SelectorLeaf *right)
    { return push(SelectorBranch::conjunction(this->right), right); }
  virtual SelectorLeaf *disjunction(SelectorLeaf *right)
    { return push(SelectorBranch::disjunction(this->right), right); }

  SelectorLeaf *push(SelectorBranch *newBranch, SelectorLeaf *newRight) {
    branches.push_back(newBranch);
    right = newRight;
    return this;
  }

  virtual Node &traverse(BuildContext &context) {
    BuildContext *tmp = &context;
    for (auto it = branches.begin(); it != branches.end(); ++it)
      tmp = (*it)->traverse(*tmp, context);
    return tmp->traverse(right);
  }
};

struct Step : public SelectorLeaf {
  Key key_;

  Step(const Key &key) : key_(key) {}

  virtual SelectorLeaf *descendant(SelectorLeaf *right)
    { return new Wrap(SelectorBranch::descendant(this), right); }
  virtual SelectorLeaf *conjunction(SelectorLeaf *right)
    { return new Wrap(SelectorBranch::conjunction(this), right); }
  virtual SelectorLeaf *disjunction(SelectorLeaf *right)
    { return new Wrap(SelectorBranch::disjunction(this), right); }

  virtual Node &traverse(BuildContext &context)
    { return context.node().addChild(key_); }
};

SelectorLeaf *SelectorLeaf::step(const Key &key) {
  return new Step(key);
}

class BranchImpl : public SelectorBranch {
  typedef std::function<BuildContext *(BuildContext &,
      BuildContext &)> Traverse;
  Traverse traverse_;

public:
  BranchImpl(Traverse traverse)
  : traverse_(traverse) {}

  virtual BuildContext *traverse(BuildContext &context,
      BuildContext &baseContext) {
    return traverse_(context, baseContext);
  }
};

SelectorBranch *SelectorBranch::descendant(SelectorLeaf *first) {
  return new BranchImpl([first](BuildContext &context,
      BuildContext &baseContext) {
    return context.descendant(context.traverse(first));
  });
}

SelectorBranch *SelectorBranch::conjunction(SelectorLeaf *first) {
  return new BranchImpl([first](BuildContext &context,
      BuildContext &baseContext) {
    return context.conjunction(context.traverse(first), baseContext);
  });
}

SelectorBranch *SelectorBranch::disjunction(SelectorLeaf *first) {
  return new BranchImpl([first](BuildContext &context,
      BuildContext &baseContext) {
    return context.disjunction(context.traverse(first), baseContext);
  });
}

}}
