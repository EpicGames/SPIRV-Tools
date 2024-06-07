// Copyright (c) 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SOURCE_OPT_ADV_INTERFACE_VAR_SROA_H_
#define SOURCE_OPT_ADV_INTERFACE_VAR_SROA_H_

#include <unordered_set>

#include "source/opt/pass.h"

namespace spvtools {
namespace opt {

// See optimizer.hpp for documentation.
//
// Note that the there is another existing pass
// InterfaceVariableScalarReplacement, which doesn't handle tricky instruction
// chains and interface variables which are arrays of scalars. The plan is to
// replace that pass with this one.
class AdvancedInterfaceVariableScalarReplacement : public Pass {
 public:
  AdvancedInterfaceVariableScalarReplacement(bool process_matrices)
      : process_matrices_(process_matrices) {}

  const char* name() const override {
    return "adv-interface-variable-scalar-replacement";
  }
  Status Process() override;

  IRContext::Analysis GetPreservedAnalyses() override {
    return IRContext::kAnalysisDecorations | IRContext::kAnalysisDefUse |
           IRContext::kAnalysisConstants | IRContext::kAnalysisTypes;
  }

 private:
  // A struct describing a single interface variable.
  struct InterfaceVar {
    // The corresponding OpVariable.
    Instruction* def;
    // The corresponding OpType*.
    Instruction* type;
    // If |extra_array_length| is not 0, it means that this interface variable
    // has a Patch decoration. This will add extra-arrayness to the replacing
    // scalar variables.
    uint32_t extra_array_length;

    InterfaceVar(Instruction* def, Instruction* type,
                 uint32_t extra_array_length)
      : def(def), type(type), extra_array_length(extra_array_length) {
      assert(def);
      assert(type);
    }
  };

  // A struct containing components of a composite interface variable. If the
  // composite consists of multiple or recursive components, |scalar_var| is
  // nullptr and |children| keeps the nested components. If it has a single
  // component, |children| is empty and |scalar_var| is the component. Note that
  // each element of |children| has the Replacement struct as its type that can
  // recursively keep the components.
  struct Replacement {
    explicit Replacement(uint32_t type_id)
        : scalar_var(nullptr), type_id(type_id) {}

    bool HasChildren() const { return !children.empty(); }

    std::vector<Replacement>& GetChildren() {
      return children;
    }

    const std::vector<Replacement>& GetChildren() const {
      return children;
    }

    Replacement& AppendChild(uint32_t child_type_id) {
      assert(!scalar_var && "Can add children only for non-scalars.");
      return children.emplace_back(child_type_id);
    }

    Instruction* GetScalarVariable() const { return scalar_var; }

    void SetSingleScalarVariable(Instruction* var) { scalar_var = var; }

    uint32_t GetTypeId() const { return type_id; }

   private:
    std::vector<Replacement> children;
    Instruction* scalar_var;
    uint32_t type_id;
  };

  // Collects all interface variables used by the |entry_point|.
  std::vector<Instruction*> CollectInterfaceVariables(Instruction& entry_point);

  // Returns whether |var| has the extra arrayness for the entry point
  // |entry_point| or not.
  bool HasExtraArrayness(Instruction& entry_point, Instruction* var);

  // Finds a Location BuiltIn decoration of |var| and returns it via
  // |location|. Returns true whether the location exists or not.
  bool GetVariableLocation(Instruction* var, uint32_t* location);

  // Finds a Component BuiltIn decoration of |var| and returns it via
  // |component|. Returns true whether the component exists or not.
  bool GetVariableComponent(Instruction* var, uint32_t* component);

  // Returns the type of |var| as an instruction.
  Instruction* GetTypeOfVariable(Instruction* var);

  // Replaces an interface variable |var| with scalars and returns whether it
  // succeeds or not. |location| is the value of Location Decoration for |var|.
  // |all_scalar_vars| will be appended with the replacement scalar vars for
  // |var|.
  bool ReplaceInterfaceVariable(InterfaceVar var, uint32_t location,
                                std::vector<Instruction*>* all_scalar_vars);

  // Creates scalar variables to replace an interface variable |var|.
  // |scalar_vars| will be filled as a list of all replacement scalar variables.
  // As |Replacement| represents a tree, shaped as the original interface
  // variable, this list will contain every leaf from that tree, stored in the
  // depth-first order.
  Replacement CreateReplacementVariables(
      InterfaceVar var, std::vector<Instruction*>* scalar_vars);

  // Recursively adds Location and Component decorations to variables in
  // |vars| with |location| and |optional_component|. Increases |location| by
  // one after it actually adds Location and Component decorations for a
  // variable.
  void AddLocationAndComponentDecorations(const Replacement& vars,
                                          uint32_t* location,
                                          uint32_t* optional_component);

  // Clones an annotation instruction |annotation_inst| and sets the target
  // operand of the new annotation instruction as |var_id|.
  void CloneAnnotationForVariable(Instruction* annotation_inst,
                                  uint32_t var_id);

  // Replaces all the interface variables, which will be replaced, in the
  // operands of the entry point |entry_point| with a set of variables from the
  // |scalar_vars|.
  bool ReplaceInEntryPoint(Instruction* entry_point,
                           const std::unordered_set<uint32_t>& inteface_vars,
                           const std::vector<Instruction*>& scalar_vars);

  // Replaces the load instruction |load| of the original interface variable or
  // its part with a load from each replacement scalar variable from
  // |replacement| followed by a composite construction. If target load is only
  // transitively dependent on the replaced interface var, then the
  // corresponding access chain |optional_access_chain| will be passed.
  bool ReplaceLoad(Instruction* load, const Replacement& replacement,
                    Instruction* optional_access_chain,
                    uint32_t extra_array_length);

  // Replaces the store instruction |store| of the original interface variable
  // or its part with a series of composite extracts and stores using the
  // replacement scalar variables from |replacement|. If target load is only
  // transitively dependent on the replaced interface var, then the
  // corresponding access chain |optional_access_chain| will be passed.
  bool ReplaceStore(Instruction* store, const Replacement& replacement,
                    Instruction* optional_access_chain,
                    uint32_t extra_array_length);

  // Looks up the replacement node according to the indices from the access
  // chain |access_chain|, using the passed |root| as a base. If any index in
  // the chain is non-constant or ouf-of-bound, return nullptr. If
  // |extra_array_length| is not zero, the first index in the chain is skipped,
  // as it is the one used for extra arrayness.
  const Replacement* LookupReplacement(Instruction* access_chain,
                                       const Replacement* root,
                                       uint32_t extra_array_length);

  // Creates a variable with type |type_id| and storage class |storage_class|.
  // Debug info for the newly created variable is copied from the source
  // |debug_info_source|.
  std::unique_ptr<Instruction> CreateVariable(
      uint32_t type_id, spv::StorageClass storage_class,
      const Instruction* debug_info_source, uint32_t extra_array_length);

  // Generate OpName instructions for the variable |destination_id|, based on
  // the name of source variable |source_id| and a list of indices |indices| to
  // make a suffix.
  void GenerateNames(uint32_t source_id, uint32_t destination_id,
                     const std::vector<uint32_t>& indices,
                     std::vector<std::unique_ptr<Instruction>>* names_to_add);

  // Returns the pointee type of the type of variable |var|.
  uint32_t GetPointeeTypeIdOfVar(Instruction* var);

  // Returns the result id of OpTypeArray instrunction whose Element Type
  // operand is |elem_type_id| and Length operand is |array_length|.
  uint32_t GetArrayType(uint32_t elem_type_id, uint32_t array_length);

  // Kills all OpDecorate instructions for Location and Component of the
  // variable whose id is |var_id|.
  void KillLocationAndComponentDecorations(uint32_t var_id);

  // If |var| has the extra arrayness for an entry point, reports an error and
  // returns true. Otherwise, returns false.
  bool ReportErrorIfHasExtraArraynessForOtherEntry(Instruction* var);

  // If |var| does not have the extra arrayness for an entry point, reports an
  // error and returns true. Otherwise, returns false.
  bool ReportErrorIfHasNoExtraArraynessForOtherEntry(Instruction* var);

  // If |var| has the extra arrayness for an entry point but it does not have
  // one for another entry point, reports an error and returns false. Otherwise,
  // returns true.
  bool CheckExtraArraynessConflictBetweenEntries(InterfaceVar var);

  // Conducts the scalar replacement for the interface variables used by the
  // |entry_point|.
  Pass::Status ProcessEntryPoint(Instruction& entry_point);

  // A set of interface variables with the extra arrayness for any of the entry
  // points.
  std::unordered_set<Instruction*> vars_with_extra_arrayness;

  // A set of interface variables without the extra arrayness for any of the
  // entry points.
  std::unordered_set<Instruction*> vars_without_extra_arrayness;

  // Whether we need to replace matrix interface variables with scalars or not.
  bool process_matrices_;
};

}  // namespace opt
}  // namespace spvtools

#endif  // SOURCE_OPT_ADV_INTERFACE_VAR_SROA_H_
