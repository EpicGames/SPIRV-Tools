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

#include <iostream>

#include "test/opt/pass_fixture.h"
#include "test/opt/pass_utils.h"

namespace spvtools {
namespace opt {
namespace {

using AdvancedInterfaceVariableScalarReplacementTest = PassTest<::testing::Test>;

TEST_F(AdvancedInterfaceVariableScalarReplacementTest,
       ReplaceInterfaceVarsWithScalars) {
  const std::string spirv = R"(
               OpCapability Shader
               OpCapability Tessellation
               OpMemoryModel Logical GLSL450
               OpEntryPoint TessellationControl %func "shader" %x %y %z %w %u %v %gl_InvocationID

; CHECK:     OpName [[x:%\w+]] "x"
; CHECK-NOT: OpName {{%\w+}} "x"
; CHECK:     OpName [[y:%\w+]] "y"
; CHECK-NOT: OpName {{%\w+}} "y"
; CHECK:     OpName [[k:%\w+]] "k"
; CHECK-NOT: OpName {{%\w+}} "k"
; CHECK:     OpName [[s:%\w+]] "s"
; CHECK-NOT: OpName {{%\w+}} "s"
; CHECK:     OpName [[q:%\w+]] "q"
; CHECK-NOT: OpName {{%\w+}} "q"
; CHECK:     OpName [[gl_InvocationID:%\w+]] "gl_InvocationID"
; CHECK-NOT: OpName {{%\w+}} "gl_InvocationID"
; CHECK:     OpName [[z0:%\w+]] "z[0]"
; CHECK:     OpName [[z1:%\w+]] "z[1]"
; CHECK:     OpName [[w0:%\w+]] "w[0]"
; CHECK:     OpName [[w1:%\w+]] "w[1]"
; CHECK:     OpName [[u0:%\w+]] "u[0]"
; CHECK:     OpName [[u1:%\w+]] "u[1]"
; CHECK:     OpName [[v0:%\w+]] "v[0][0]"
; CHECK:     OpName [[v1:%\w+]] "v[0][1]"
; CHECK:     OpName [[v2:%\w+]] "v[1][0]"
; CHECK:     OpName [[v3:%\w+]] "v[1][1]"
; CHECK:     OpName [[v4:%\w+]] "v[2][0]"
; CHECK:     OpName [[v5:%\w+]] "v[2][1]"
               OpName %x "x"
               OpName %y "y"
               OpName %z "z"
               OpName %w "w"
               OpName %u "u"
               OpName %v "v"
               OpName %k "k"
               OpName %s "s"
               OpName %q "q"
               OpName %gl_InvocationID "gl_InvocationID"

; CHECK-DAG: OpDecorate [[x]] Location 2
; CHECK-DAG: OpDecorate [[y]] Location 0
; CHECK-DAG: OpDecorate [[gl_InvocationID]] BuiltIn InvocationId
; CHECK-DAG: OpDecorate [[z0]] Location 0
; CHECK-DAG: OpDecorate [[z0]] Component 0
; CHECK-DAG: OpDecorate [[z1]] Location 1
; CHECK-DAG: OpDecorate [[z1]] Component 0
; CHECK-DAG: OpDecorate [[z0]] Patch
; CHECK-DAG: OpDecorate [[z1]] Patch
; CHECK-DAG: OpDecorate [[w0]] Location 2
; CHECK-DAG: OpDecorate [[w0]] Component 0
; CHECK-DAG: OpDecorate [[w1]] Location 3
; CHECK-DAG: OpDecorate [[w1]] Component 0
; CHECK-DAG: OpDecorate [[w0]] Patch
; CHECK-DAG: OpDecorate [[w1]] Patch
; CHECK-DAG: OpDecorate [[u0]] Location 3
; CHECK-DAG: OpDecorate [[u0]] Component 2
; CHECK-DAG: OpDecorate [[u1]] Location 4
; CHECK-DAG: OpDecorate [[u1]] Component 2
; CHECK-DAG: OpDecorate [[v0]] Location 3
; CHECK-DAG: OpDecorate [[v0]] Component 3
; CHECK-DAG: OpDecorate [[v1]] Location 4
; CHECK-DAG: OpDecorate [[v1]] Component 3
; CHECK-DAG: OpDecorate [[v2]] Location 5
; CHECK-DAG: OpDecorate [[v2]] Component 3
; CHECK-DAG: OpDecorate [[v3]] Location 6
; CHECK-DAG: OpDecorate [[v3]] Component 3
; CHECK-DAG: OpDecorate [[v4]] Location 7
; CHECK-DAG: OpDecorate [[v4]] Component 3
; CHECK-DAG: OpDecorate [[v5]] Location 8
; CHECK-DAG: OpDecorate [[v5]] Component 3
               OpDecorate %z Patch
               OpDecorate %w Patch
               OpDecorate %z Location 0
               OpDecorate %x Location 2
               OpDecorate %v Location 3
               OpDecorate %v Component 3
               OpDecorate %y Location 0
               OpDecorate %w Location 2
               OpDecorate %u Location 3
               OpDecorate %u Component 2
               OpDecorate %gl_InvocationID BuiltIn InvocationId

        %int = OpTypeInt 32 1
       %uint = OpTypeInt 32 0
     %uint_1 = OpConstant %uint 1
     %uint_2 = OpConstant %uint 2
     %uint_3 = OpConstant %uint 3
     %uint_4 = OpConstant %uint 4
%_arr_uint_uint_2 = OpTypeArray %uint %uint_2
%_ptr_Output__arr_uint_uint_2 = OpTypePointer Output %_arr_uint_uint_2
%_ptr_Input__arr_uint_uint_2 = OpTypePointer Input %_arr_uint_uint_2
%_ptr_Input_int = OpTypePointer Input %int
%_ptr_Input_uint = OpTypePointer Input %uint
%_ptr_Output_uint = OpTypePointer Output %uint
%_arr_arr_uint_uint_2_3 = OpTypeArray %_arr_uint_uint_2 %uint_3
%_ptr_Input__arr_arr_uint_uint_2_3 = OpTypePointer Input %_arr_arr_uint_uint_2_3
%_arr_arr_arr_uint_uint_2_3_4 = OpTypeArray %_arr_arr_uint_uint_2_3 %uint_4
%_ptr_Output__arr_arr_arr_uint_uint_2_3_4 = OpTypePointer Output %_arr_arr_arr_uint_uint_2_3_4
%_ptr_Output__arr_arr_uint_uint_2_3 = OpTypePointer Output %_arr_arr_uint_uint_2_3
%_ptr_Function__arr__arr__arr_uint_uint_2_uint_3_uint_4 = OpTypePointer Function %_arr_arr_arr_uint_uint_2_3_4
%_ptr_Function_uint = OpTypePointer Function %uint
%_ptr_Function__arr_uint_uint_2 = OpTypePointer Function %_arr_uint_uint_2

          %gl_InvocationID = OpVariable %_ptr_Input_int Input
          %z = OpVariable %_ptr_Output__arr_uint_uint_2 Output
          %x = OpVariable %_ptr_Output__arr_uint_uint_2 Output
          %y = OpVariable %_ptr_Input__arr_uint_uint_2 Input
          %w = OpVariable %_ptr_Input__arr_uint_uint_2 Input
          %u = OpVariable %_ptr_Input__arr_arr_uint_uint_2_3 Input
          %v = OpVariable %_ptr_Output__arr_arr_arr_uint_uint_2_3_4 Output

; CHECK-DAG:  [[x]] = OpVariable %_ptr_Output__arr_uint_uint_2 Output
; CHECK-DAG:  [[y]] = OpVariable %_ptr_Input__arr_uint_uint_2 Input
; CHECK-DAG:  [[gl_InvocationID]] = OpVariable %_ptr_Input_int Input
; CHECK-DAG: [[z0]] = OpVariable %_ptr_Output_uint Output
; CHECK-DAG: [[z1]] = OpVariable %_ptr_Output_uint Output
; CHECK-DAG: [[w0]] = OpVariable %_ptr_Input_uint Input
; CHECK-DAG: [[w1]] = OpVariable %_ptr_Input_uint Input
; CHECK-DAG: [[u0]] = OpVariable %_ptr_Input__arr_uint_uint_3 Input
; CHECK-DAG: [[u1]] = OpVariable %_ptr_Input__arr_uint_uint_3 Input
; CHECK-DAG: [[v0]] = OpVariable %_ptr_Output__arr_uint_uint_4 Output
; CHECK-DAG: [[v1]] = OpVariable %_ptr_Output__arr_uint_uint_4 Output
; CHECK-DAG: [[v2]] = OpVariable %_ptr_Output__arr_uint_uint_4 Output
; CHECK-DAG: [[v3]] = OpVariable %_ptr_Output__arr_uint_uint_4 Output
; CHECK-DAG: [[v4]] = OpVariable %_ptr_Output__arr_uint_uint_4 Output
; CHECK-DAG: [[v5]] = OpVariable %_ptr_Output__arr_uint_uint_4 Output

     %void   = OpTypeVoid
     %void_f = OpTypeFunction %void
     %func   = OpFunction %void None %void_f
     %label  = OpLabel

     %k      = OpVariable %_ptr_Function__arr__arr__arr_uint_uint_2_uint_3_uint_4 Function
     %s      = OpVariable %_ptr_Function_uint Function
     %q      = OpVariable %_ptr_Function__arr_uint_uint_2 Function
; CHECK-DAG:  [[k]] = OpVariable %_ptr_Function__arr__arr__arr_uint_uint_2_uint_3_uint_4 Function
; CHECK-DAG:  [[s]] = OpVariable %_ptr_Function_uint Function
; CHECK-DAG:  [[q]] = OpVariable %_ptr_Function__arr_uint_uint_2 Function

; CHECK: [[w0_value:%\w+]] = OpLoad %uint [[w0]]
; CHECK: [[w1_value:%\w+]] = OpLoad %uint [[w1]]
; CHECK:  [[w_value:%\w+]] = OpCompositeConstruct %_arr_uint_uint_2 [[w0_value]] [[w1_value]]
; CHECK:       [[w0:%\w+]] = OpCompositeExtract %uint [[w_value]] 0
; CHECK:                     OpStore [[z0]] [[w0]]
; CHECK:       [[w1:%\w+]] = OpCompositeExtract %uint [[w_value]] 1
; CHECK:                     OpStore [[z1]] [[w1]]
    %w_value = OpLoad %_arr_uint_uint_2 %w
               OpStore %z %w_value

; CHECK:    [[u00_ptr:%\w+]] = OpAccessChain %_ptr_Input_uint [[u0]] %uint_0
; CHECK:        [[u00:%\w+]] = OpLoad %uint [[u00_ptr]]
; CHECK:    [[u10_ptr:%\w+]] = OpAccessChain %_ptr_Input_uint [[u1]] %uint_0
; CHECK:        [[u10:%\w+]] = OpLoad %uint [[u10_ptr]]
; CHECK-DAG: [[u0_val:%\w+]] = OpCompositeConstruct %_arr_uint_uint_2 [[u00]] [[u10]]
; CHECK:    [[u01_ptr:%\w+]] = OpAccessChain %_ptr_Input_uint [[u0]] %uint_1
; CHECK:        [[u01:%\w+]] = OpLoad %uint [[u01_ptr]]
; CHECK:    [[u11_ptr:%\w+]] = OpAccessChain %_ptr_Input_uint [[u1]] %uint_1
; CHECK:        [[u11:%\w+]] = OpLoad %uint [[u11_ptr]]
; CHECK-DAG: [[u1_val:%\w+]] = OpCompositeConstruct %_arr_uint_uint_2 [[u01]] [[u11]]
; CHECK:    [[u02_ptr:%\w+]] = OpAccessChain %_ptr_Input_uint [[u0]] %uint_2
; CHECK:        [[u02:%\w+]] = OpLoad %uint [[u02_ptr]]
; CHECK:    [[u12_ptr:%\w+]] = OpAccessChain %_ptr_Input_uint [[u1]] %uint_2
; CHECK:        [[u12:%\w+]] = OpLoad %uint [[u12_ptr]]
; CHECK-DAG: [[u2_val:%\w+]] = OpCompositeConstruct %_arr_uint_uint_2 [[u02]] [[u12]]

; CHECK: [[u_val:%\w+]] = OpCompositeConstruct %_arr__arr_uint_uint_2_uint_3 [[u0_val]] [[u1_val]] [[u2_val]]

; CHECK: [[val:%\w+]] = OpCompositeExtract %uint [[u_val]] 0 0
; CHECK: [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v0]] %uint_1
; CHECK:                OpStore [[ptr]] [[val]]
; CHECK: [[val:%\w+]] = OpCompositeExtract %uint [[u_val]] 0 1
; CHECK: [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v1]] %uint_1
; CHECK:                OpStore [[ptr]] [[val]]
; CHECK: [[val:%\w+]] = OpCompositeExtract %uint [[u_val]] 1 0
; CHECK: [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v2]] %uint_1
; CHECK:                OpStore [[ptr]] [[val]]
; CHECK: [[val:%\w+]] = OpCompositeExtract %uint [[u_val]] 1 1
; CHECK: [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v3]] %uint_1
; CHECK:                OpStore [[ptr]] [[val]]
; CHECK: [[val:%\w+]] = OpCompositeExtract %uint [[u_val]] 2 0
; CHECK: [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v4]] %uint_1
; CHECK:                OpStore [[ptr]] [[val]]
; CHECK: [[val:%\w+]] = OpCompositeExtract %uint [[u_val]] 2 1
; CHECK: [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v5]] %uint_1
; CHECK:                OpStore [[ptr]] [[val]]
     %v_ptr  = OpAccessChain %_ptr_Output__arr_arr_uint_uint_2_3 %v %uint_1
     %u_val  = OpLoad %_arr_arr_uint_uint_2_3 %u
               OpStore %v_ptr %u_val

; CHECK: [[k_val:%\w+]] = OpLoad %_arr__arr__arr_uint_uint_2_uint_3_uint_4 %k
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 0 0 0
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v0]] %uint_0
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 1 0 0
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v0]] %uint_1
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 2 0 0
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v0]] %uint_2
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 3 0 0
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v0]] %uint_3
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 0 0 1
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v1]] %uint_0
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 1 0 1
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v1]] %uint_1
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 2 0 1
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v1]] %uint_2
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 3 0 1
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v1]] %uint_3
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 0 1 0
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v2]] %uint_0
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 1 1 0
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v2]] %uint_1
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 2 1 0
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v2]] %uint_2
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 3 1 0
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v2]] %uint_3
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 0 1 1
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v3]] %uint_0
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 1 1 1
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v3]] %uint_1
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 2 1 1
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v3]] %uint_2
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 3 1 1
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v3]] %uint_3
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 0 2 0
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v4]] %uint_0
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 1 2 0
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v4]] %uint_1
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 2 2 0
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v4]] %uint_2
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 3 2 0
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v4]] %uint_3
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 0 2 1
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v5]] %uint_0
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 1 2 1
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v5]] %uint_1
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 2 2 1
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v5]] %uint_2
; CHECK                   OpStore [[ptr]] [[val]]
; CHECK:   [[val:%\w+]] = OpCompositeExtract %uint [[k_val]] 3 2 1
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v5]] %uint_3
; CHECK                   OpStore [[ptr]] [[val]]
     %k_val  = OpLoad %_arr_arr_arr_uint_uint_2_3_4 %k
               OpStore %v %k_val

; CHECK: [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v5]] %uint_3
; CHECK: [[val:%\w+]] = OpLoad %uint [[ptr]]
; CHECK:                OpStore %s [[val]]
     %v213_ptr = OpAccessChain %_ptr_Output_uint %v %uint_3 %uint_2 %uint_1
     %v213_val = OpLoad %uint %v213_ptr
                 OpStore %s %v213_val

; CHECK: [[v320_ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v4]] %uint_3
; CHECK: [[v320_val:%\w+]] = OpLoad %uint [[v320_ptr]]
; CHECK: [[v321_ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v5]] %uint_3
; CHECK: [[v321_val:%\w+]] = OpLoad %uint [[v321_ptr]]
; CHECK:  [[v32_val:%\w+]] = OpCompositeConstruct %_arr_uint_uint_2 [[v320_val]] [[v321_val]]
; CHECK:                     OpStore %q [[v32_val]]
     %v32_ptr  = OpAccessChain %_ptr_Output__arr_uint_uint_2 %v %uint_3 %uint_2
     %v32_val  = OpLoad %_arr_uint_uint_2 %v32_ptr
                 OpStore %q %v32_val

; CHECK:    [[id:%\w+]] = OpLoad %int %gl_InvocationID
; CHECK: [[s_val:%\w+]] = OpLoad %uint %s
; CHECK:   [[ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v5]] [[id]]
; CHECK:                  OpStore [[ptr]] [[s_val]]
     %id       = OpLoad %int %gl_InvocationID
     %s_val    = OpLoad %uint %s
     %vi21_ptr = OpAccessChain %_ptr_Output_uint %v %id %uint_2 %uint_1
                 OpStore %vi21_ptr %s_val

; CHECK:    [[q_val:%\w+]] = OpLoad %_arr_uint_uint_2 %q
; CHECK:       [[q0:%\w+]] = OpCompositeExtract %uint [[q_val]] 0
; CHECK: [[vi20_ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v4]] [[id]]
; CHECK:                     OpStore [[vi20_ptr]] [[q0]]
; CHECK:       [[q1:%\w+]] = OpCompositeExtract %uint [[q_val]] 1
; CHECK: [[vi21_ptr:%\w+]] = OpAccessChain %_ptr_Output_uint [[v5]] [[id]]
; CHECK:                     OpStore [[vi21_ptr]] [[q1]]
     %q_val    = OpLoad %_arr_uint_uint_2 %q
     %vi2_ptr  = OpAccessChain %_ptr_Output__arr_uint_uint_2 %v %id %uint_2
                 OpStore %vi2_ptr %q_val

               OpReturn
               OpFunctionEnd
  )";

  SinglePassRunAndMatch<AdvancedInterfaceVariableScalarReplacement>(spirv, true, true);
}

TEST_F(AdvancedInterfaceVariableScalarReplacementTest,
       CheckPatchDecorationPreservation) {
  // Make sure scalars for the variables with the extra arrayness have the extra
  // arrayness after running the pass while others do not have it.
  // Only "y" does not have the extra arrayness in the following SPIR-V.
  const std::string spirv = R"(
               OpCapability Shader
               OpCapability Tessellation
               OpMemoryModel Logical GLSL450
               OpEntryPoint TessellationEvaluation %func "shader" %x %y %z %w
               OpDecorate %z Patch
               OpDecorate %w Patch
               OpDecorate %z Location 0
               OpDecorate %x Location 2
               OpDecorate %y Location 0
               OpDecorate %w Location 1
               OpName %x "x"
               OpName %y "y"
               OpName %z "z"
               OpName %w "w"

  ; CHECK:     OpName [[y:%\w+]] "y"
  ; CHECK-NOT: OpName {{%\w+}} "y"
  ; CHECK-DAG: OpName [[z0:%\w+]] "z[0]"
  ; CHECK-DAG: OpName [[z1:%\w+]] "z[1]"
  ; CHECK-DAG: OpName [[w0:%\w+]] "w[0]"
  ; CHECK-DAG: OpName [[w1:%\w+]] "w[1]"
  ; CHECK-DAG: OpName [[x0:%\w+]] "x[0]"
  ; CHECK-DAG: OpName [[x1:%\w+]] "x[1]"

       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%_arr_uint_uint_2 = OpTypeArray %uint %uint_2
%_ptr_Output__arr_uint_uint_2 = OpTypePointer Output %_arr_uint_uint_2
%_ptr_Input__arr_uint_uint_2 = OpTypePointer Input %_arr_uint_uint_2
          %z = OpVariable %_ptr_Output__arr_uint_uint_2 Output
          %x = OpVariable %_ptr_Output__arr_uint_uint_2 Output
          %y = OpVariable %_ptr_Input__arr_uint_uint_2 Input
          %w = OpVariable %_ptr_Input__arr_uint_uint_2 Input

  ; CHECK-DAG: [[y]] = OpVariable %_ptr_Input__arr_uint_uint_2 Input
  ; CHECK-DAG: [[z0]] = OpVariable %_ptr_Output_uint Output
  ; CHECK-DAG: [[z1]] = OpVariable %_ptr_Output_uint Output
  ; CHECK-DAG: [[w0]] = OpVariable %_ptr_Input_uint Input
  ; CHECK-DAG: [[w1]] = OpVariable %_ptr_Input_uint Input
  ; CHECK-DAG: [[x0]] = OpVariable %_ptr_Output_uint Output
  ; CHECK-DAG: [[x1]] = OpVariable %_ptr_Output_uint Output

     %void   = OpTypeVoid
     %void_f = OpTypeFunction %void
     %func   = OpFunction %void None %void_f
     %label  = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  SinglePassRunAndMatch<AdvancedInterfaceVariableScalarReplacement>(spirv, true, true);
}

TEST_F(AdvancedInterfaceVariableScalarReplacementTest,
       CheckEntryPointInterfaceOperands) {
  const std::string spirv = R"(
               OpCapability Shader
               OpCapability Tessellation
               OpMemoryModel Logical GLSL450
               OpEntryPoint TessellationEvaluation %tess "tess" %x %y
               OpEntryPoint Vertex %vert "vert" %w
               OpDecorate %z Location 0
               OpDecorate %x Location 2
               OpDecorate %y Location 0
               OpDecorate %w Location 1
               OpName %x "x"
               OpName %y "y"
               OpName %z "z"
               OpName %w "w"

  ; CHECK:     OpName [[y:%\w+]] "y"
  ; CHECK-DAG: OpName [[z:%\w+]] "z"
  ; CHECK-NOT: OpName {{%\w+}} "z"
  ; CHECK-NOT: OpName {{%\w+}} "y"
  ; CHECK-DAG: OpName [[x0:%\w+]] "x[0]"
  ; CHECK-DAG: OpName [[x1:%\w+]] "x[1]"
  ; CHECK-DAG: OpName [[w0:%\w+]] "w[0]"
  ; CHECK-DAG: OpName [[w1:%\w+]] "w[1]"

       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%_arr_uint_uint_2 = OpTypeArray %uint %uint_2
%_ptr_Output__arr_uint_uint_2 = OpTypePointer Output %_arr_uint_uint_2
%_ptr_Input__arr_uint_uint_2 = OpTypePointer Input %_arr_uint_uint_2
          %z = OpVariable %_ptr_Output__arr_uint_uint_2 Output
          %x = OpVariable %_ptr_Output__arr_uint_uint_2 Output
          %y = OpVariable %_ptr_Input__arr_uint_uint_2 Input
          %w = OpVariable %_ptr_Input__arr_uint_uint_2 Input

  ; CHECK-DAG: [[y]] = OpVariable %_ptr_Input__arr_uint_uint_2 Input
  ; CHECK-DAG: [[z]] = OpVariable %_ptr_Output__arr_uint_uint_2 Output
  ; CHECK-DAG: [[w0]] = OpVariable %_ptr_Input_uint Input
  ; CHECK-DAG: [[w1]] = OpVariable %_ptr_Input_uint Input
  ; CHECK-DAG: [[x0]] = OpVariable %_ptr_Output_uint Output
  ; CHECK-DAG: [[x1]] = OpVariable %_ptr_Output_uint Output

     %void   = OpTypeVoid
     %void_f = OpTypeFunction %void
     %tess   = OpFunction %void None %void_f
     %bb0    = OpLabel
               OpReturn
               OpFunctionEnd
     %vert   = OpFunction %void None %void_f
     %bb1    = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  SinglePassRunAndMatch<AdvancedInterfaceVariableScalarReplacement>(spirv, true, true);
}

class InterfaceVarSROAErrorTest : public PassTest<::testing::Test> {
 public:
  InterfaceVarSROAErrorTest()
      : consumer_([this](spv_message_level_t level, const char*,
                         const spv_position_t& position, const char* message) {
          if (!error_message_.empty()) error_message_ += "\n";
          switch (level) {
            case SPV_MSG_FATAL:
            case SPV_MSG_INTERNAL_ERROR:
            case SPV_MSG_ERROR:
              error_message_ += "ERROR";
              break;
            case SPV_MSG_WARNING:
              error_message_ += "WARNING";
              break;
            case SPV_MSG_INFO:
              error_message_ += "INFO";
              break;
            case SPV_MSG_DEBUG:
              error_message_ += "DEBUG";
              break;
          }
          error_message_ +=
              ": " + std::to_string(position.index) + ": " + message;
        }) {}

  Pass::Status RunPass(const std::string& text) {
    std::unique_ptr<IRContext> context_ =
        spvtools::BuildModule(SPV_ENV_UNIVERSAL_1_2, consumer_, text);
    if (!context_.get()) return Pass::Status::Failure;

    PassManager manager;
    manager.SetMessageConsumer(consumer_);
    manager.AddPass<AdvancedInterfaceVariableScalarReplacement>(true);

    return manager.Run(context_.get());
  }

  std::string GetErrorMessage() const { return error_message_; }

  void TearDown() override { error_message_.clear(); }

 private:
  spvtools::MessageConsumer consumer_;
  std::string error_message_;
};

TEST_F(InterfaceVarSROAErrorTest, CheckConflictOfExtraArraynessBetweenEntries) {
  const std::string spirv = R"(
               OpCapability Shader
               OpCapability Tessellation
               OpMemoryModel Logical GLSL450
               OpEntryPoint TessellationControl %tess "tess" %x %y %z
               OpEntryPoint Vertex %vert "vert" %z %w
               OpDecorate %z Location 0
               OpDecorate %x Location 2
               OpDecorate %y Location 0
               OpDecorate %w Location 1
               OpName %x "x"
               OpName %y "y"
               OpName %z "z"
               OpName %w "w"
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%_arr_uint_uint_2 = OpTypeArray %uint %uint_2
%_ptr_Output__arr_uint_uint_2 = OpTypePointer Output %_arr_uint_uint_2
%_ptr_Input__arr_uint_uint_2 = OpTypePointer Input %_arr_uint_uint_2
          %z = OpVariable %_ptr_Output__arr_uint_uint_2 Output
          %x = OpVariable %_ptr_Output__arr_uint_uint_2 Output
          %y = OpVariable %_ptr_Input__arr_uint_uint_2 Input
          %w = OpVariable %_ptr_Input__arr_uint_uint_2 Input
     %void   = OpTypeVoid
     %void_f = OpTypeFunction %void
     %tess   = OpFunction %void None %void_f
     %bb0    = OpLabel
               OpReturn
               OpFunctionEnd
     %vert   = OpFunction %void None %void_f
     %bb1    = OpLabel
               OpReturn
               OpFunctionEnd
  )";

  EXPECT_EQ(RunPass(spirv), Pass::Status::Failure);
  const char expected_error[] =
      "ERROR: 0: A variable is arrayed for an entry point but it is not "
      "arrayed for another entry point\n"
      "  %z = OpVariable %_ptr_Output__arr_uint_uint_2 Output";
  EXPECT_STREQ(GetErrorMessage().c_str(), expected_error);
}

}  // namespace
}  // namespace opt
}  // namespace spvtools
