#ifndef CCS_SEARCH_STATE_H_
#define CCS_SEARCH_STATE_H_

#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <set>

#include "ccs/domain.h"
#include "dag/key.h"
#include "dag/specificity.h"

namespace ccs {

class AndTally;
class CcsProperty;
class Node;
class TallyState;

class SearchState {
  // we need to be sure to retain a reference to the root of the dag. the
  // simplest way is to just make everything in 'nodes' shared_ptrs, but that
  // seems awfully heavy-handed. instead we'll just retain a direct reference
  // to the root in the root search state. the parent links are shared, so
  // this is sufficient.
  std::shared_ptr<const Node> root;
  std::shared_ptr<const SearchState> parent;
  std::map<Specificity, std::set<const Node *>> nodes;
  // the TallyStates here should rightly be unique_ptrs, but gcc 4.5 can't
  // support that in a map. bummer.
  std::map<const AndTally *, const TallyState *> tallyMap;
  CcsLogger &log;
  Key key;
  bool logAccesses;
  bool constraintsChanged;

  SearchState(const std::shared_ptr<const SearchState> &parent, const Key &key);

public:
  SearchState(std::shared_ptr<const Node> &root,
      CcsLogger &log, bool logAccesses);
  SearchState(const SearchState &) = delete;
  SearchState &operator=(const SearchState &) = delete;
  ~SearchState();

  static std::shared_ptr<SearchState> newChild(
      const std::shared_ptr<const SearchState> &parent, const Key &key);

  bool extendWith(const SearchState &priorState);

  const CcsProperty *findProperty(const std::string &propertyName) const;

  void add(Specificity spec, const Node *node)
    { nodes[spec].insert(node); }

  void constrain(const Key &constraints)
    { constraintsChanged |= key.addAll(constraints); }

  template <typename T>
  const CcsProperty *findProperty(const std::string &propertyName,
      T defaultVal) {
    const CcsProperty *prop = doSearch(propertyName, true);
    if (!prop) prop = doSearch(propertyName, false);
    if (logAccesses) {
      std::ostringstream msg;
      if (prop) {
        msg << "Found property: " << propertyName << " = "
          << prop->strValue() << "\n";
      } else {
        msg << "Property not found: " << propertyName << ". Default = "
          << defaultVal << "\n";
      }
      msg << "    in context: [" << *this << "]";
      log.info(msg.str());
    }
    return prop;
  }

  const TallyState *getTallyState(const AndTally *tally) const;
  void setTallyState(const AndTally *tally, const TallyState *state);

private:
  const CcsProperty *doSearch(const std::string &propertyName,
      bool override) const;

  friend std::ostream &operator<<(std::ostream &, const SearchState &);
};

}

#endif /* CCS_SEARCH_STATE_H_ */
