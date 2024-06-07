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

#include "source/opt/adv_interface_var_sroa.h"

#include <optional>

#include "source/opt/decoration_manager.h"
#include "source/opt/def_use_manager.h"
#include "source/opt/log.h"
#include "source/opt/type_manager.h"

namespace spvtools {
namespace opt {
namespace {
constexpr uint32_t kOpDecorateDecorationInOperandIndex = 1;
constexpr uint32_t kOpDecorateLiteralInOperandIndex = 2;
constexpr uint32_t kOpEntryPointInOperandInterface = 3;
constexpr uint32_t kOpVariableStorageClassInOperandIndex = 0;
constexpr uint32_t kOpTypeArrayElemTypeInOperandIndex = 0;
constexpr uint32_t kOpTypeArrayLengthInOperandIndex = 1;
constexpr uint32_t kOpTypeMatrixColCountInOperandIndex = 1;
constexpr uint32_t kOpTypeMatrixColTypeInOperandIndex = 0;
constexpr uint32_t kOpTypePtrTypeInOperandIndex = 1;
constexpr uint32_t kOpConstantValueInOperandIndex = 0;

// Get the length of the OpTypeArray |array_type|.
uint32_t GetArrayLength(analysis::DefUseManager* def_use_mgr,
                        Instruction* array_type) {
  assert(array_type->opcode() == spv::Op::OpTypeArray);
  uint32_t const_int_id =
      array_type->GetSingleWordInOperand(kOpTypeArrayLengthInOperandIndex);
  Instruction* array_length_inst = def_use_mgr->GetDef(const_int_id);
  assert(array_length_inst->opcode() == spv::Op::OpConstant);
  return array_length_inst->GetSingleWordInOperand(
      kOpConstantValueInOperandIndex);
}

// Get the element type instruction of the OpTypeArray |array_type|.
Instruction* GetArrayElementType(analysis::DefUseManager* def_use_mgr,
                                 Instruction* array_type) {
  assert(array_type->opcode() == spv::Op::OpTypeArray);
  uint32_t elem_type_id =
      array_type->GetSingleWordInOperand(kOpTypeArrayElemTypeInOperandIndex);
  return def_use_mgr->GetDef(elem_type_id);
}

// Get the column type instruction of the OpTypeMatrix |matrix_type|.
Instruction* GetMatrixColumnType(analysis::DefUseManager* def_use_mgr,
                                 Instruction* matrix_type) {
  assert(matrix_type->opcode() == spv::Op::OpTypeMatrix);
  uint32_t column_type_id =
      matrix_type->GetSingleWordInOperand(kOpTypeMatrixColTypeInOperandIndex);
  return def_use_mgr->GetDef(column_type_id);
}

// Returns the storage class of the instruction |var|.
spv::StorageClass GetStorageClass(Instruction* var) {
  return static_cast<spv::StorageClass>(
      var->GetSingleWordInOperand(kOpVariableStorageClassInOperandIndex));
}

// Creates an OpDecorate instruction whose Target is |var_id| and Decoration is
// |decoration|. Adds |literal| as an extra operand of the instruction.
void CreateDecoration(analysis::DecorationManager* decoration_mgr,
                      uint32_t var_id, spv::Decoration decoration,
                      uint32_t literal) {
  std::vector<Operand> operands({
      {SPV_OPERAND_TYPE_ID, {var_id}},
      {SPV_OPERAND_TYPE_DECORATION, {static_cast<uint32_t>(decoration)}},
      {SPV_OPERAND_TYPE_LITERAL_INTEGER, {literal}},
  });
  decoration_mgr->AddDecoration(spv::Op::OpDecorate, std::move(operands));
}

std::unique_ptr<Instruction> CreateAccessChain(IRContext* context, uint32_t id,
                                               Instruction* base_var,
                                               uint32_t type_id,
                                               Operand index) {
  assert(context);
  assert(base_var);

  auto storage_class = GetStorageClass(base_var);
  uint32_t ptr_type_id =
      context->get_type_mgr()->FindPointerToType(type_id, storage_class);

  std::unique_ptr<Instruction> access_chain(
      new Instruction(context, spv::Op::OpAccessChain, ptr_type_id, id,
                      {{SPV_OPERAND_TYPE_ID, {base_var->result_id()}}, index}));

  return access_chain;
}

// Creates an OpCompositeExtract instruction to extract the part with Result
// type |type_id| from the Composite that is |input_id| and Indexes are
// |indices|. If optional extra array index |extra_array_index| is passed, it is
// injected as a very first index.
std::unique_ptr<Instruction> CreateCompositeExtract(
    IRContext* context, uint32_t id, uint32_t type_id, uint32_t input_id,
    const std::vector<uint32_t>& indices, uint32_t* extra_array_index) {
  assert(context);
  assert(!indices.empty());
  std::unique_ptr<Instruction> extract(
      new Instruction(context, spv::Op::OpCompositeExtract, type_id, id,
                      {{SPV_OPERAND_TYPE_ID, {input_id}}}));
  if (extra_array_index) {
    extract->AddOperand(
        {SPV_OPERAND_TYPE_LITERAL_INTEGER, {*extra_array_index}});
  }
  for (uint32_t i : indices) {
    extract->AddOperand({SPV_OPERAND_TYPE_LITERAL_INTEGER, {i}});
  }
  return extract;
}

// Creates an OpStore instruction to store value |what_id| to pointer
// |where_id|, while copying the memory attributes from another instruction
// |original_store|.
std::unique_ptr<Instruction> CreateStore(IRContext* context, uint32_t where_id,
                                         uint32_t what_id,
                                         Instruction* original_store) {
  assert(context);
  assert(original_store);

  std::unique_ptr<Instruction> store(new Instruction(
      context, spv::Op::OpStore, 0, 0,
      {{SPV_OPERAND_TYPE_ID, {where_id}}, {SPV_OPERAND_TYPE_ID, {what_id}}}));

  // Copy memory access attributes which start at index 2. Index 0 is the
  // pointer and index 1 is the data.
  for (uint32_t i = 2; i < original_store->NumInOperands(); ++i) {
    store->AddOperand(original_store->GetInOperand(i));
  }

  return store;
}

// Creates an OpLoad instruction with id |load_id| to load value of type
// |type_id| from |ptr_id|, while copying the memory attributes from another
// instruction |original_load|.
std::unique_ptr<Instruction> CreateLoad(IRContext* context, uint32_t type_id,
                                        uint32_t ptr_id, uint32_t load_id,
                                        Instruction* original_load) {
  assert(context);
  assert(original_load);
  std::unique_ptr<Instruction> load(
      new Instruction(context, spv::Op::OpLoad, type_id, load_id,
                      {{SPV_OPERAND_TYPE_ID, {ptr_id}}}));
  // Copy memory access attributes which start at index 1. Index 0 is
  // the pointer to load.
  for (uint32_t i = 1; i < original_load->NumInOperands(); ++i) {
    load->AddOperand(original_load->GetInOperand(i));
  }

  return load;
}

}  // namespace

Pass::Status AdvancedInterfaceVariableScalarReplacement::Process() {
  Pass::Status status = Status::SuccessWithoutChange;
  for (Instruction& entry_point : get_module()->entry_points()) {
    status = CombineStatus(status, ProcessEntryPoint(entry_point));
  }
  return status;
}

Pass::Status AdvancedInterfaceVariableScalarReplacement::ProcessEntryPoint(
    Instruction& entry_point) {
  std::vector<Instruction*> interface_vars =
      CollectInterfaceVariables(entry_point);
  Pass::Status status = Status::SuccessWithoutChange;
  std::unordered_set<uint32_t> replaced_interface_vars;
  std::vector<Instruction*> scalar_vars;

  for (Instruction* var : interface_vars) {
    uint32_t location;
    if (!GetVariableLocation(var, &location)) continue;

    Instruction* var_type = GetTypeOfVariable(var);
    uint32_t extra_array_length = 0;
    if (HasExtraArrayness(entry_point, var)) {
      extra_array_length =
          GetArrayLength(context()->get_def_use_mgr(), var_type);
      var_type = GetArrayElementType(context()->get_def_use_mgr(), var_type);
      vars_with_extra_arrayness.insert(var);
    } else {
      vars_without_extra_arrayness.insert(var);
    }

    InterfaceVar interface_var(var, var_type, extra_array_length);

    if (!CheckExtraArraynessConflictBetweenEntries(interface_var)) {
      return Pass::Status::Failure;
    }

    spv::Op opcode = var_type->opcode();
    bool should_process = false;
    should_process |= opcode == spv::Op::OpTypeArray;
    should_process |= process_matrices_ && opcode == spv::Op::OpTypeMatrix;
    if (!should_process) {
      continue;
    }

    replaced_interface_vars.insert(var->result_id());
    if (!ReplaceInterfaceVariable(interface_var, location, &scalar_vars)) {
      return Pass::Status::Failure;
    }

    status = Pass::Status::SuccessWithChange;
  }

  ReplaceInEntryPoint(&entry_point, replaced_interface_vars, scalar_vars);

  return status;
}

bool AdvancedInterfaceVariableScalarReplacement::ReplaceInterfaceVariable(
    InterfaceVar var, uint32_t location,
    std::vector<Instruction*>* all_scalar_vars) {
  assert(all_scalar_vars);

  std::vector<Instruction*> scalar_vars;
  Replacement replacement = CreateReplacementVariables(var, &scalar_vars);
  assert(!scalar_vars.empty());

  for (auto* scalar_var : scalar_vars) {
    all_scalar_vars->push_back(scalar_var);
  }

  uint32_t component = 0;
  bool has_component_decoration = GetVariableComponent(var.def, &component);
  AddLocationAndComponentDecorations(
      replacement, &location, has_component_decoration ? &component : nullptr);
  KillLocationAndComponentDecorations(var.def->result_id());

  std::vector<Instruction*> decoration_work_list;
  std::vector<Instruction*> access_chain_work_list;
  struct LoadStore {
    // Original interface variable touching instruction.
    Instruction* to_be_replaced;
    // Node representing the replacement for the part of interface variable,
    // the instruction targets.
    const Replacement* target;
    // This is set only if instruction uses the extra arrayed scalar var.
    Instruction* optional_access_chain = nullptr;
  };
  std::vector<LoadStore> load_work_list;
  std::vector<LoadStore> store_work_list;

  // Finds out all the interface variable usages to populate the work lists.
  bool failed = !get_def_use_mgr()->WhileEachUser(
      var.def, [this, &replacement, &decoration_work_list, &load_work_list,
                &store_work_list, &access_chain_work_list](Instruction* user) {
        if (user->IsDecoration()) {
          decoration_work_list.push_back(user);
          return true;
        }

        switch (user->opcode()) {
          case spv::Op::OpEntryPoint:
            // Nothing to do here, it is handled later in |ProcessEntryPoint|.
            return true;
          case spv::Op::OpName:
            decoration_work_list.push_back(user);
            return true;
          case spv::Op::OpLoad:
            load_work_list.push_back({user, &replacement});
            return true;
          case spv::Op::OpStore:
            store_work_list.push_back({user, &replacement});
            return true;
          case spv::Op::OpAccessChain:
          case spv::Op::OpInBoundsAccessChain:
            access_chain_work_list.push_back(user);
            return true;
          default:
            context()->EmitErrorMessage(
                "Variable cannot be replaced: unexpected instruction", user);
            return false;
        }
      });

  if (failed) {
    // Error has been reported already.
    return false;
  }

  std::vector<Instruction*> dead;

  for (Instruction* decoration : decoration_work_list) {
    spv::Op opcode = decoration->opcode();
    // Name decorations are already created for each replacement scalar
    // variable.
    if (opcode != spv::Op::OpName) {
      for (const auto* scalar_var : scalar_vars) {
        CloneAnnotationForVariable(decoration, scalar_var->result_id());
      }
    }

    // Decorations will be killed together with the variable instruction,
    // there is no need to add anything to |dead|.
  }

  // Access chains are processed as a stack, as there might exist chains of
  // access chains, which must be eventually fully replaced with loads/stores.
  // Hence, processing of one access chain, might add more work to this stack.
  // IMPORTANT: Access chains are processed _before_ the loads/stores as this
  // processing can create more work for the loads/stores one.
  while (!access_chain_work_list.empty()) {
    Instruction* access_chain = access_chain_work_list.back();
    access_chain_work_list.pop_back();

    assert(access_chain->opcode() == spv::Op::OpAccessChain ||
           access_chain->opcode() == spv::Op::OpInBoundsAccessChain);
    assert(access_chain->NumInOperands() > 1 &&
           "OpAccessChain does not have Indexes operand");

    // We are going to replace the access chain with either direct usage of the
    // replacement scalar variable, or a set of composite loads/stores.

    const Replacement* target =
        LookupReplacement(access_chain, &replacement, var.extra_array_length);
    if (!target) {
      // Error has been already logged by |LookupReplacement|.
      return false;
    }

    if (!target->HasChildren() && var.extra_array_length == 0) {
      // Replace with a direct use of the scalar variable.
      auto scalar = target->GetScalarVariable();
      assert(scalar);
      context()->ReplaceAllUsesWith(access_chain->result_id(),
                                    scalar->result_id());
    } else {
      // The current access chain's target is a composite, meaning that there
      // are other instructions using the pointer. We need to convert those to
      // use the replacement scalar variables.
      failed = !get_def_use_mgr()->WhileEachUser(
          access_chain, [this, target, &access_chain_work_list, &load_work_list,
                         &store_work_list, access_chain](Instruction* user) {
            switch (user->opcode()) {
              case spv::Op::OpLoad:
                load_work_list.push_back({user, target, access_chain});
                return true;
              case spv::Op::OpStore:
                store_work_list.push_back({user, target, access_chain});
                return true;
              case spv::Op::OpAccessChain:
              case spv::Op::OpInBoundsAccessChain:
                access_chain_work_list.push_back(user);
                return true;
              default:
                context()->EmitErrorMessage(
                    "Variable cannot be replaced: unexpected instruction",
                    user);
                return false;
            }
          });

      if (failed) {
        return false;
      }
    }

    dead.push_back(access_chain);
  }

  for (auto [load, target_replacement, opt_access_chain] : load_work_list) {
    if (!ReplaceLoad(load, *target_replacement, opt_access_chain,
                     var.extra_array_length)) {
      return false;
    }
    dead.push_back(load);
  }

  for (auto [store, target_replacement, opt_access_chain] : store_work_list) {
    if (!ReplaceStore(store, *target_replacement, opt_access_chain,
                      var.extra_array_length)) {
      return false;
    }
    dead.push_back(store);
  }

  dead.push_back(var.def);

  while (!dead.empty()) {
    Instruction* to_kill = dead.back();
    dead.pop_back();
    context()->KillInst(to_kill);
  }

  return true;
}

bool AdvancedInterfaceVariableScalarReplacement::ReplaceInEntryPoint(
    Instruction* entry_point,
    const std::unordered_set<uint32_t>& interface_vars,
    const std::vector<Instruction*>& scalar_vars) {
  Instruction::OperandList new_operands;

  if (scalar_vars.empty()) {
    return true;
  }

  // Copy all operands except all interface variables, which will be replaced.
  bool found = false;
  for (uint32_t i = 0; i < entry_point->NumOperands(); ++i) {
    Operand& op = entry_point->GetOperand(i);
    if (op.type == SPV_OPERAND_TYPE_ID &&
        interface_vars.find(op.words[0]) != interface_vars.end()) {
      found = true;
    } else {
      new_operands.emplace_back(std::move(op));
    }
  }

  if (!found) {
    context()->EmitErrorMessage(
        "Interface variables are not operands of the entry point", entry_point);
    return false;
  }

  // Add all the new replacement variables.
  for (auto scalar : scalar_vars) {
    new_operands.push_back({SPV_OPERAND_TYPE_ID, {scalar->result_id()}});
  }

  entry_point->ReplaceOperands(new_operands);
  context()->UpdateDefUse(entry_point);

  return true;
}

bool AdvancedInterfaceVariableScalarReplacement::ReplaceLoad(
    Instruction* load, const Replacement& replacement,
    Instruction* optional_access_chain, uint32_t extra_array_length) {
  assert(load && load->opcode() == spv::Op::OpLoad);

  const auto insert_before =
      [this, load](Instruction* where,
                   std::unique_ptr<Instruction> what) -> Instruction* {
    auto inst = where->InsertBefore(std::move(what));
    inst->UpdateDebugInfoFrom(load);
    get_def_use_mgr()->AnalyzeInstDefUse(inst);
    return inst;
  };

  std::vector<Instruction*> pending_instructions;
  // We do a post-order traversal of the tree of composite replacements to emit
  // properly nested loads and composite constructions to match the original
  // interface variable shape.
  std::vector<std::pair<const Replacement*, bool>> todo;

  uint32_t num_passes = 1;
  // If we have an optional access chain, we need to load a single element of
  // the extra array. Otherwise, we load it fully.
  if (!optional_access_chain && extra_array_length != 0) {
    num_passes = extra_array_length;
  }
  for (uint32_t pass = 0; pass < num_passes; ++pass) {
    std::optional<Operand> extra_array_index;
    if (extra_array_length != 0) {
      if (optional_access_chain) {
        extra_array_index = optional_access_chain->GetInOperand(1);
      } else {
        uint32_t index_id = context()->get_constant_mgr()->GetUIntConstId(pass);
        extra_array_index = {SPV_OPERAND_TYPE_ID, {index_id}};
      }
    }
    todo.push_back({&replacement, false});

    while (!todo.empty()) {
      const auto [node, inserted] = todo.back();
      assert(node);

      if (inserted) {
        todo.pop_back();

        if (node->HasChildren()) {
          // Construct the composite component from already loaded scalars.
          uint32_t composite_id = TakeNextId();
          if (composite_id == 0) {
            return false;
          }
          std::unique_ptr<Instruction> construct(
              new Instruction(context(), spv::Op::OpCompositeConstruct,
                              node->GetTypeId(), composite_id, {}));

          // As we are doing a post-order traversal, out children instructions
          // should already be laid out and ready to be used as our operands.
          const auto& children = node->GetChildren();
          size_t num_children_left = children.size();
          assert(pending_instructions.size() >= num_children_left &&
                 "Post-order traversal is broken");
          size_t i = pending_instructions.size() - num_children_left;
          while (num_children_left > 0) {
            construct->AddOperand(
                {SPV_OPERAND_TYPE_ID, {pending_instructions[i]->result_id()}});

            ++i;
            --num_children_left;
          }
          for (size_t i = 0; i < children.size(); ++i) {
            pending_instructions.pop_back();
          }

          auto inst = insert_before(load, std::move(construct));
          pending_instructions.push_back(inst);
        } else {
          auto scalar = node->GetScalarVariable();
          assert(scalar);

          Instruction* ptr = scalar;

          if (extra_array_index.has_value()) {
            // Indirection access chain to get a pointer to the extra array
            // element.

            uint32_t indirection_id = TakeNextId();
            if (indirection_id == 0) {
              return false;
            }

            std::unique_ptr<Instruction> access_chain =
                CreateAccessChain(context(), indirection_id, ptr,
                                  node->GetTypeId(), extra_array_index.value());
            ptr = insert_before(load, std::move(access_chain));
          }

          uint32_t subload_id = TakeNextId();
          if (subload_id == 0) {
            return false;
          }

          std::unique_ptr<Instruction> subload = CreateLoad(
              context(), node->GetTypeId(), ptr->result_id(), subload_id, load);

          auto inst = insert_before(load, std::move(subload));
          pending_instructions.push_back(inst);
        }
      } else {
        todo.back().second = true;

        const auto& children = node->GetChildren();
        for (const auto& child :
             make_range(children.rbegin(), children.rend())) {
          todo.push_back({&child, false});
        }
      }
    }
  }
  assert(pending_instructions.size() == num_passes);
  if (num_passes > 1) {
    uint32_t extra_array_type_id =
        GetArrayType(replacement.GetTypeId(), extra_array_length);

    // Construct the composite component from already loaded scalars.
    uint32_t extra_array_id = TakeNextId();
    if (extra_array_id == 0) {
      return false;
    }
    std::unique_ptr<Instruction> extra_construct(
        new Instruction(context(), spv::Op::OpCompositeConstruct,
                        extra_array_type_id, extra_array_id, {}));
    for (auto& pending : pending_instructions) {
      Operand op(SPV_OPERAND_TYPE_ID, {pending->result_id()});
      extra_construct->AddOperand(std::move(op));
    }
    auto inst = insert_before(load, std::move(extra_construct));
    pending_instructions.push_back(inst);
  }

  context()->ReplaceAllUsesWith(load->result_id(),
                                pending_instructions.back()->result_id());
  return true;
}

bool AdvancedInterfaceVariableScalarReplacement::ReplaceStore(
    Instruction* store, const Replacement& replacement,
    Instruction* optional_access_chain, uint32_t extra_array_length) {
  assert(store && store->opcode() == spv::Op::OpStore);

  uint32_t input_id = store->GetSingleWordInOperand(1);

  // This is a managed stack of indices, which will contain a chain of indices
  // coming to the currently processed node.
  std::vector<uint32_t> indices_chain;

  struct Entry {
    // Currently processed node.
    const Replacement* node;
    // Local index of the node inside of the parent.
    uint32_t index;
    // Current node depth in the nodes tree.
    size_t depth;
  };
  std::vector<Entry> todo;
  todo.push_back({&replacement, 0, 0});
  size_t current_depth = 0;

  // We do an in-order traversal of the tree of composite replacements to emit
  // proper stores with composite extracts to get the data we need, considering
  // the original interface variable shape.
  while (!todo.empty()) {
    const auto entry = todo.back();
    const auto node = entry.node;
    const auto index = entry.index;
    const auto depth = entry.depth;
    todo.pop_back();

    assert(node);

    while (current_depth > depth) {
      indices_chain.pop_back();
      --current_depth;
    }
    current_depth = depth;
    if (node != &replacement) {
      indices_chain.push_back(index);
    }

    if (node->HasChildren()) {
      const auto& children = node->GetChildren();
      uint32_t child_index = uint32_t(children.size());
      while (child_index > 0) {
        --child_index;
        todo.push_back(
            {&children[child_index], child_index, current_depth + 1});
      }
    } else {
      const auto insert_before =
          [this, store](Instruction* where,
                        std::unique_ptr<Instruction> what) -> Instruction* {
        auto inst = where->InsertBefore(std::move(what));
        inst->UpdateDebugInfoFrom(store);
        get_def_use_mgr()->AnalyzeInstDefUse(inst);
        return inst;
      };

      const auto store_to_scalar = [this, store, node, &indices_chain,
                                    &insert_before](
                                       uint32_t value_to_store_id,
                                       uint32_t* extra_array_index_for_extract,
                                       uint32_t* extra_array_index_id) -> bool {
        // This one is empty if replacement root is already a scalar,
        // e.g. ivar[1][2] = scalar;
        // hence we do not need the compositeextract.
        if (!indices_chain.empty()) {
          uint32_t extract_id = TakeNextId();
          if (extract_id == 0) {
            return false;
          }

          // Composite extract the nested scalar value.
          std::unique_ptr<Instruction> extract = CreateCompositeExtract(
              context(), extract_id, node->GetTypeId(), value_to_store_id,
              indices_chain, extra_array_index_for_extract);

          insert_before(store, std::move(extract));

          // To be used by the OpStore below.
          value_to_store_id = extract_id;
        }

        auto scalar = node->GetScalarVariable();
        assert(scalar);

        Instruction* ptr = scalar;
        // Indirection access chain to get a pointer to the extra array
        // element.
        if (extra_array_index_id) {
          uint32_t indirection_id = TakeNextId();
          if (indirection_id == 0) {
            return false;
          }

          std::unique_ptr<Instruction> access_chain = CreateAccessChain(
              context(), indirection_id, ptr, node->GetTypeId(),
              {SPV_OPERAND_TYPE_ID, {*extra_array_index_id}});

          ptr = insert_before(store, std::move(access_chain));
        }

        // Store the value to the corresponding variable.
        std::unique_ptr<Instruction> store_to_scalar =
            CreateStore(context(), ptr->result_id(), value_to_store_id, store);

        insert_before(store, std::move(store_to_scalar));

        return true;
      };

      bool ok = true;
      if (extra_array_length == 0) {
        ok = store_to_scalar(input_id, nullptr, nullptr);
      } else if (optional_access_chain) {
        uint32_t indirect_index =
            optional_access_chain->GetSingleWordInOperand(1);
        ok = store_to_scalar(input_id, nullptr, &indirect_index);
      } else {
        for (uint32_t i = 0; i < extra_array_length; ++i) {
          uint32_t extra_array_index_id =
              context()->get_constant_mgr()->GetUIntConstId(i);

          ok &= store_to_scalar(input_id, &i, &extra_array_index_id);
        }
      }

      if (!ok) {
        return false;
      }

      // It might be empty if current node is both scalar and a root.
      if (!indices_chain.empty()) {
        indices_chain.pop_back();
      }
    }
  }

  return true;
}

const AdvancedInterfaceVariableScalarReplacement::Replacement*
AdvancedInterfaceVariableScalarReplacement::LookupReplacement(
    Instruction* access_chain, const Replacement* root,
    uint32_t extra_array_length) {
  assert(access_chain);

  analysis::ConstantManager* const_mgr = context()->get_constant_mgr();

  // In case of extra arrayness, the first index always targets that extra
  // array, hence we skip it when looking-up the rest.
  uint32_t start_index = extra_array_length == 0 ? 1 : 2;

  // Finds the target replacement, which might be a scalar or nested
  // composite.
  for (uint32_t i = start_index; i < access_chain->NumInOperands(); ++i) {
    uint32_t index_id = access_chain->GetSingleWordInOperand(i);

    const analysis::Constant* index_constant =
        const_mgr->FindDeclaredConstant(index_id);
    if (!index_constant) {
      context()->EmitErrorMessage(
          "Variable cannot be replaced: index is not constant", access_chain);
      return nullptr;
    }

    assert(root->HasChildren());
    const auto& children = root->GetChildren();

    // OpAccessChain treats indices as signed.
    int64_t index_value = index_constant->GetSignExtendedValue();
    if (index_value < 0 ||
        index_value >= static_cast<int64_t>(children.size())) {
      // Out of bounds access, this is illegal IR.
      // Notice that OpAccessChain indexing is 0-based, so we should also
      // reject index == size-of-array.
      context()->EmitErrorMessage("Variable cannot be replaced: invalid index",
                                  access_chain);
      return nullptr;
    }

    root = &children[index_value];
  }
  return root;
}

AdvancedInterfaceVariableScalarReplacement::Replacement
AdvancedInterfaceVariableScalarReplacement::CreateReplacementVariables(
    InterfaceVar var, std::vector<Instruction*>* scalar_vars) {
  assert(scalar_vars);

  auto* def_use_mgr = get_def_use_mgr();
  auto storage_class = GetStorageClass(var.def);

  // Composite replacement tree we are building here.
  Replacement root(var.type->result_id());
  // Names for newly added scalars.
  std::vector<std::unique_ptr<Instruction>> names_to_add;

  // A managed stack of indices, which will contain a chain of indices coming to
  // the currently processed replacement node.
  std::vector<uint32_t> indices_chain;

  struct Entry {
    // Currently processed node.
    Replacement* node;
    // Type of the interface variable part, which this node is about.
    Instruction* var_type;
    // Local index of the node inside of the parent.
    uint32_t index;
    // Current node depth in the nodes tree.
    size_t depth;
  };
  std::vector<Entry> todo;
  todo.push_back({&root, var.type, 0, 0});
  size_t current_depth = 0;

  while (!todo.empty()) {
    const auto [node, type, index, depth] = todo.back();
    todo.pop_back();

    assert(node);

    while (current_depth > depth) {
      indices_chain.pop_back();
      --current_depth;
    }
    current_depth = depth;
    if (node != &root) {
      indices_chain.push_back(index);
    }

    spv::Op opcode = type->opcode();
    if (opcode == spv::Op::OpTypeArray || opcode == spv::Op::OpTypeMatrix) {
      // Handle array and matrix case.

      uint32_t length = 0;
      Instruction* child_type{};

      switch (type->opcode()) {
        case spv::Op::OpTypeArray:
          length = GetArrayLength(def_use_mgr, type);
          child_type = GetArrayElementType(def_use_mgr, type);
          break;
        case spv::Op::OpTypeMatrix:
          length =
              type->GetSingleWordInOperand(kOpTypeMatrixColCountInOperandIndex);
          child_type = GetMatrixColumnType(def_use_mgr, type);
          break;
        default:
          assert(false && "Unexpected type.");
          break;
      }
      assert(child_type);
      uint32_t child_type_id = child_type->result_id();

      for (uint32_t i = 0; i < length; ++i) {
        node->AppendChild(child_type_id);
      }

      auto& children = node->GetChildren();
      while (length > 0) {
        --length;
        todo.push_back(
            {&children[length], child_type, length, current_depth + 1});
      }
    } else {
      // Handle scalar or vector case.

      std::unique_ptr<Instruction> variable = CreateVariable(
          type->result_id(), storage_class, var.def, var.extra_array_length);

      node->SetSingleScalarVariable(variable.get());
      scalar_vars->push_back(variable.get());

      uint32_t var_id = variable->result_id();
      context()->AddGlobalValue(std::move(variable));
      GenerateNames(var.def->result_id(), var_id, indices_chain, &names_to_add);

      indices_chain.pop_back();
    }
  }

  // We shouldn't add the new names when we are iterating over name ranges
  // above. We can add all the new names now.
  for (auto& new_name : names_to_add) {
    context()->AddDebug2Inst(std::move(new_name));
  }

  return root;
}

std::unique_ptr<Instruction> AdvancedInterfaceVariableScalarReplacement::CreateVariable(
    uint32_t type_id, spv::StorageClass storage_class,
    const Instruction* debug_info_source, uint32_t extra_array_length) {
  assert(debug_info_source);

  if (extra_array_length != 0) {
    type_id = GetArrayType(type_id, extra_array_length);
  }

  uint32_t ptr_type_id =
      context()->get_type_mgr()->FindPointerToType(type_id, storage_class);

  uint32_t id = TakeNextId();
  if (id == 0) {
    return {};
  }

  std::unique_ptr<Instruction> variable(
      new Instruction(context(), spv::Op::OpVariable, ptr_type_id, id,
                      {{SPV_OPERAND_TYPE_STORAGE_CLASS,
                        {static_cast<uint32_t>(storage_class)}}}));
  variable->UpdateDebugInfoFrom(debug_info_source);

  return variable;
}

void AdvancedInterfaceVariableScalarReplacement::GenerateNames(
    uint32_t source_id, uint32_t destination_id,
    const std::vector<uint32_t>& indices,
    std::vector<std::unique_ptr<Instruction>>* names_to_add) {
  assert(names_to_add);
  auto* def_use_mgr = get_def_use_mgr();
  for (auto [_, name_inst] : context()->GetNames(source_id)) {
    std::string name_str = utils::MakeString(name_inst->GetOperand(1).words);
    for (uint32_t i : indices) {
      name_str += "[" + utils::ToString(i) + "]";
    }

    std::unique_ptr<Instruction> new_name(new Instruction(
        context(), spv::Op::OpName, 0, 0,
        {{SPV_OPERAND_TYPE_ID, {destination_id}},
         {SPV_OPERAND_TYPE_LITERAL_STRING, utils::MakeVector(name_str)}}));
    def_use_mgr->AnalyzeInstDefUse(new_name.get());
    names_to_add->push_back(std::move(new_name));
  }
}

bool AdvancedInterfaceVariableScalarReplacement::
    CheckExtraArraynessConflictBetweenEntries(InterfaceVar var) {
  if (var.extra_array_length != 0) {
    return !ReportErrorIfHasNoExtraArraynessForOtherEntry(var.def);
  }
  return !ReportErrorIfHasExtraArraynessForOtherEntry(var.def);
}

bool AdvancedInterfaceVariableScalarReplacement::GetVariableLocation(
    Instruction* var, uint32_t* location) {
  return !context()->get_decoration_mgr()->WhileEachDecoration(
      var->result_id(), uint32_t(spv::Decoration::Location),
      [location](const Instruction& inst) {
        *location =
            inst.GetSingleWordInOperand(kOpDecorateLiteralInOperandIndex);
        return false;
      });
}

bool AdvancedInterfaceVariableScalarReplacement::GetVariableComponent(
    Instruction* var, uint32_t* component) {
  return !context()->get_decoration_mgr()->WhileEachDecoration(
      var->result_id(), uint32_t(spv::Decoration::Component),
      [component](const Instruction& inst) {
        *component =
            inst.GetSingleWordInOperand(kOpDecorateLiteralInOperandIndex);
        return false;
      });
}

std::vector<Instruction*>
AdvancedInterfaceVariableScalarReplacement::CollectInterfaceVariables(
    Instruction& entry_point) {
  std::vector<Instruction*> interface_vars;
  for (uint32_t i = kOpEntryPointInOperandInterface;
       i < entry_point.NumInOperands(); ++i) {
    Instruction* interface_var = context()->get_def_use_mgr()->GetDef(
        entry_point.GetSingleWordInOperand(i));
    assert(interface_var->opcode() == spv::Op::OpVariable);

    spv::StorageClass storage_class = GetStorageClass(interface_var);
    if (storage_class != spv::StorageClass::Input &&
        storage_class != spv::StorageClass::Output) {
      continue;
    }

    interface_vars.push_back(interface_var);
  }
  return interface_vars;
}

void AdvancedInterfaceVariableScalarReplacement::KillLocationAndComponentDecorations(
    uint32_t var_id) {
  context()->get_decoration_mgr()->RemoveDecorationsFrom(
      var_id, [](const Instruction& inst) {
        auto decoration = spv::Decoration(
            inst.GetSingleWordInOperand(kOpDecorateDecorationInOperandIndex));
        return decoration == spv::Decoration::Location ||
               decoration == spv::Decoration::Component;
      });
}

void AdvancedInterfaceVariableScalarReplacement::
    AddLocationAndComponentDecorations(const Replacement& vars,
                                       uint32_t* location,
                                       uint32_t* optional_component) {
  if (!vars.HasChildren()) {
    uint32_t var_id = vars.GetScalarVariable()->result_id();
    CreateDecoration(context()->get_decoration_mgr(), var_id,
                     spv::Decoration::Location, *location);
    if (optional_component) {
      CreateDecoration(context()->get_decoration_mgr(), var_id,
                       spv::Decoration::Component, *optional_component);
    }
    ++(*location);
    return;
  }
  for (const auto& var : vars.GetChildren()) {
    AddLocationAndComponentDecorations(var, location, optional_component);
  }
}

void AdvancedInterfaceVariableScalarReplacement::CloneAnnotationForVariable(
    Instruction* annotation_inst, uint32_t var_id) {
  assert(annotation_inst->opcode() == spv::Op::OpDecorate ||
         annotation_inst->opcode() == spv::Op::OpDecorateId ||
         annotation_inst->opcode() == spv::Op::OpDecorateString);
  std::unique_ptr<Instruction> new_inst(annotation_inst->Clone(context()));
  new_inst->SetInOperand(0, {var_id});
  context()->AddAnnotationInst(std::move(new_inst));
}

bool AdvancedInterfaceVariableScalarReplacement::HasExtraArrayness(
    Instruction& entry_point, Instruction* var) {
  auto execution_model =
      static_cast<spv::ExecutionModel>(entry_point.GetSingleWordInOperand(0));
  if (execution_model != spv::ExecutionModel::TessellationEvaluation &&
      execution_model != spv::ExecutionModel::TessellationControl) {
    return false;
  }
  if (!context()->get_decoration_mgr()->HasDecoration(
          var->result_id(), uint32_t(spv::Decoration::Patch))) {
    if (execution_model == spv::ExecutionModel::TessellationControl)
      return true;
    return GetStorageClass(var) != spv::StorageClass::Output;
  }
  return false;
}

uint32_t AdvancedInterfaceVariableScalarReplacement::GetPointeeTypeIdOfVar(
    Instruction* var) {
  assert(var->opcode() == spv::Op::OpVariable);

  uint32_t ptr_type_id = var->type_id();
  analysis::DefUseManager* def_use_mgr = context()->get_def_use_mgr();
  Instruction* ptr_type_inst = def_use_mgr->GetDef(ptr_type_id);

  assert(ptr_type_inst->opcode() == spv::Op::OpTypePointer &&
         "Variable must have a pointer type.");
  return ptr_type_inst->GetSingleWordInOperand(kOpTypePtrTypeInOperandIndex);
}

uint32_t AdvancedInterfaceVariableScalarReplacement::GetArrayType(
    uint32_t elem_type_id, uint32_t array_length) {
  analysis::Type* elem_type = context()->get_type_mgr()->GetType(elem_type_id);
  uint32_t array_length_id =
      context()->get_constant_mgr()->GetUIntConstId(array_length);
  analysis::Array array_type(
      elem_type,
      analysis::Array::LengthInfo{array_length_id, {0, array_length}});
  return context()->get_type_mgr()->GetTypeInstruction(&array_type);
}

Instruction* AdvancedInterfaceVariableScalarReplacement::GetTypeOfVariable(
    Instruction* var) {
  assert(var->opcode() == spv::Op::OpVariable);
  uint32_t pointee_type_id = GetPointeeTypeIdOfVar(var);
  return context()->get_def_use_mgr()->GetDef(pointee_type_id);
}

bool AdvancedInterfaceVariableScalarReplacement::
    ReportErrorIfHasExtraArraynessForOtherEntry(Instruction* var) {
  if (vars_with_extra_arrayness.find(var) == vars_with_extra_arrayness.end())
    return false;

  std::string message(
      "A variable is arrayed for an entry point but it is not "
      "arrayed for another entry point");
  message +=
      "\n  " + var->PrettyPrint(SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES);
  context()->consumer()(SPV_MSG_ERROR, "", {0, 0, 0}, message.c_str());
  return true;
}

bool AdvancedInterfaceVariableScalarReplacement::
    ReportErrorIfHasNoExtraArraynessForOtherEntry(Instruction* var) {
  if (vars_without_extra_arrayness.find(var) ==
      vars_without_extra_arrayness.end())
    return false;

  std::string message(
      "A variable is not arrayed for an entry point but it is "
      "arrayed for another entry point");
  message +=
      "\n  " + var->PrettyPrint(SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES);
  context()->consumer()(SPV_MSG_ERROR, "", {0, 0, 0}, message.c_str());
  return true;
}

}  // namespace opt
}  // namespace spvtools
