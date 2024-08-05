/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef TVM_META_SCHEDULE_SCHEDULE_RULE_H_
#define TVM_META_SCHEDULE_SCHEDULE_RULE_H_

#include <tvm/ir/expr.h>
#include <tvm/node/reflection.h>
#include <tvm/runtime/container/array.h>
#include <tvm/runtime/container/map.h>
#include <tvm/runtime/container/optional.h>
#include <tvm/runtime/container/string.h>
#include <tvm/runtime/object.h>
#include <tvm/runtime/packed_func.h>
#include <tvm/tir/schedule/schedule.h>

namespace tvm {
namespace meta_schedule {

class TuneContext;
class ScheduleRule;

/*! \brief Rules to modify a block in a schedule. */
class ScheduleRuleNode : public runtime::Object {
 public:
  /*! \brief Virtual destructor. */
  virtual ~ScheduleRuleNode() = default;

  void VisitAttrs(tvm::AttrVisitor* v) {}

  /*!
   * \brief Initialize the design space generator with tuning context.
   * \param context The tuning context for initialization.
   * \note This method is supposed to be called only once before every other method.
   */
  virtual void InitializeWithTuneContext(const TuneContext& context) = 0;

  /*!
   * \brief Apply a schedule rule to the specific block in the given schedule.
   * \param sch The schedule to be modified.
   * \param block The specific block to apply the schedule rule.
   * \return The list of schedules generated by applying the schedule rule.
   */
  virtual runtime::Array<tir::Schedule> Apply(const tir::Schedule& sch,
                                              const tir::BlockRV& block) = 0;

  /*!
   * \brief Deep clone the schedule rule.
   * \return The cloned schedule rule.
   */
  virtual ScheduleRule Clone() const = 0;

  static constexpr const char* _type_key = "meta_schedule.ScheduleRule";
  TVM_DECLARE_BASE_OBJECT_INFO(ScheduleRuleNode, Object);
};

/*!
 * \brief Managed reference to ScheduleRuleNode
 * \sa ScheduleRuleNode
 */
class ScheduleRule : public runtime::ObjectRef {
 public:
  /*!
   * \brief The function type of `InitializeWithTuneContext` method.
   * \param context The tuning context for initialization.
   */
  using FInitializeWithTuneContext = runtime::TypedPackedFunc<void(const TuneContext&)>;
  /*!
   * \brief The function type of `Apply` method.
   * \param sch The schedule to be modified.
   * \param block The specific block to apply the schedule rule.
   * \return The list of schedules generated by applying the schedule rule.
   */
  using FApply =
      runtime::TypedPackedFunc<Array<tir::Schedule>(const tir::Schedule&, const tir::BlockRV&)>;
  /*!
   * \brief Get the schedule rule as string with name.
   * \return The string of the schedule rule.
   */
  using FAsString = runtime::TypedPackedFunc<String()>;
  /*!
   * \brief The function type of `Clone` method.
   * \return The cloned schedule rule.
   */
  using FClone = runtime::TypedPackedFunc<ScheduleRule()>;
  /*!
   * \brief Create a rule that applies customized rules registered using block attribute
   * `schedule_rule`. The rule will be dispatched according to target keys.
   * \return The created schedule rule.
   */
  TVM_DLL static ScheduleRule ApplyCustomRule();
  /*! \brief Check if the rule is `ApplyCustomRule` */
  TVM_DLL static bool IsApplyCustomRule(const ScheduleRule& rule);
  /*!
   * \brief Create an auto-inline rule that inlines spatial blocks if it satisfies some conditions
   * \param into_producer If allows to inline a block into its producer
   * \param into_consumer If allows to inline a block into its consumer
   * \param inline_const_tensor Always inline constant tensors
   * \param disallow_if_then_else Always disallow if-then-else-like constructs
   * \param require_ordered Always require the read-to-write mapping to be ordered
   * \param require_injective Always require the read-to-write mapping to be injective
   * \param disallow_op The operators that are disallowed in auto inline
   * \return The schedule rule created
   */
  TVM_DLL static ScheduleRule AutoInline(bool into_producer,          //
                                         bool into_consumer,          //
                                         bool inline_const_tensor,    //
                                         bool disallow_if_then_else,  //
                                         bool require_injective,      //
                                         bool require_ordered,        //
                                         Optional<Array<String>> disallow_op);

  /*!
   * \brief Inline blocks that produce a constant scalar. Such blocks get in the way of
   * ReverseComputeInline during AutoInline, since they are also counted as a producer block
   * unless they are inlined first. So it is recommended to run InlineConstantScalars before
   * AutoInline.
   * \return The schedule rule created
   */
  TVM_DLL static ScheduleRule InlineConstantScalars();

  /*!
   * \brief Create a mega rule: multi-level tiling with data reuse
   * \param structure The tiling structure. Recommended:
   * - 'SSRSRS' on CPU
   * - 'SSSRRSRS' on GPU
   * \param tile_binds For each level of tiles, which thread axis it is bound to. Recommended:
   * - NullOpt on CPU
   * - [blockIdx.x, vthread.x, threadIdx.x] on GPU
   * \param max_innermost_factor The maximum size of the innermost factor. NullOpt means no limit
   * \param vector_load_lens The length of vector lane in vectorized cooperative fetching.
   * NullOpt means disable vectorization
   * \param reuse_read Data reuse configuration for reading. NullOpt means no reuse.
   * \param reuse_write Data reuse configuration for writing. NullOpt means no reuse.
   * \param filter_fn A function that can be passed to overwrite the default condition for applying
   * MultiLevelTiling to a block. Its signature must be (Schedule, BlockRV) -> bool.
   * This is useful if there is a need to apply MultiLevelTiling to an operation / block which is
   * ignored  by default. This function should return True for a block that should be tiled.
   * \return The schedule rule created
   */
  TVM_DLL static ScheduleRule MultiLevelTiling(String structure,                             //
                                               Optional<Array<String>> tile_binds,           //
                                               Optional<Integer> max_innermost_factor,       //
                                               Optional<Array<Integer>> vector_load_lens,    //
                                               Optional<Map<String, ObjectRef>> reuse_read,  //
                                               Optional<Map<String, ObjectRef>> reuse_write,
                                               Optional<runtime::PackedFunc> filter_fn = NullOpt);

  /*!
   * \brief Extension of MultiLevelTiling for auto-tensorization with a single intrinsic.
   * \param intrin_name The name of a tensor intrinsic, must be registered via
   * TensorIntrin.register(...) beforehand
   * \param structure The tiling structure. Recommended:
   * - 'SSRSRS' on CPU
   * - 'SSSRRSRS' on GPU
   * \param tile_binds For each level of tiles, which thread axis it is bound to. Recommended:
   * - NullOpt on CPU
   * - [blockIdx.x, vthread.x, threadIdx.x] on GPU
   * \param max_innermost_factor The maximum size of the innermost factor. NullOpt means no limit
   * \param vector_load_lens The length of vector lane in vectorized cooperative fetching.
   * NullOpt means disable vectorization
   * \param reuse_read Data reuse configuration for reading. NullOpt means no reuse.
   * \param reuse_write Data reuse configuration for writing. NullOpt means no reuse.
   * \return The schedule rule created
   */
  TVM_DLL static ScheduleRule MultiLevelTilingWithIntrin(
      String intrin_name, String structure, Optional<Array<String>> tile_binds,
      Optional<Integer> max_innermost_factor, Optional<Array<Integer>> vector_load_lens,
      Optional<Map<String, ObjectRef>> reuse_read, Optional<Map<String, ObjectRef>> reuse_write);

  /*!
   * \brief Extension of MultiLevelTiling for auto-tensorization with multiple groups of candidate
   * tensor core intrinsics
   * \param intrin_groups A list of groups of tensor core intrinsics. The map should contains key
   * "init", "load_a", "load_b", "compute", "store", which represent the tensor intrin for
   * initialization, loading operand A, loading operand B, tensor core computation, storing the
   * result. The value of the map should be names of tensor intrinsics, must be registered via
   * TensorIntrin.register(...) beforehand
   * \param structure The tiling structure. Recommended:
   * - 'SSSRRSRS' on GPU
   * \param tile_binds For each level of tiles, which thread axis it is bound to. Recommended:
   * - [blockIdx.y, blockIdx.x, threadIdx.y] on GPU
   * \param max_innermost_factor The maximum size of the innermost factor. NullOpt means no limit
   * \param vector_load_lens The length of vector lane in vectorized cooperative fetching.
   * NullOpt means disable vectorization
   * \param reuse_read Data reuse configuration for reading. NullOpt means no reuse.
   * \param reuse_write Data reuse configuration for writing. NullOpt means no reuse.
   * \param use_software_pipeline Whether use the software pipeline.
   * \return The schedule rule created
   */
  TVM_DLL static ScheduleRule MultiLevelTilingTensorCore(
      Array<Map<String, String>> intrin_groups, String structure,
      Optional<Array<String>> tile_binds, Optional<Integer> max_innermost_factor,
      Optional<Array<Integer>> vector_load_lens, Optional<Map<String, ObjectRef>> reuse_read,
      Optional<Map<String, ObjectRef>> reuse_write, bool use_software_pipeline);

  /*!
   * \brief Extension of MultiLevelTiling for backends with wide vectors.
   * The loop over the innermost spatial axis of the output buffer is always vectorized with the
   * maximum vector length.
   * \param structure The tiling structure. 'SSRSRS' is recommended.
   * \param vector_length_in_bits The length of a vector register in bits.
   * \param max_innermost_factor The maximum size of the innermost factor. NullOpt means no limit
   * \param reuse_read Data reuse configuration for reading. NullOpt means no reuse.
   * \param reuse_write Data reuse configuration for writing. NullOpt means no reuse.
   * \return The schedule rule created
   */
  TVM_DLL static ScheduleRule MultiLevelTilingWideVector(
      String structure, Integer vector_length_in_bits, Optional<Integer> max_innermost_factor,
      Optional<Map<String, ObjectRef>> reuse_read, Optional<Map<String, ObjectRef>> reuse_write);

  /*!
   * \brief Create a rule: add-rfactor to some blocks if needed
   * \param max_jobs_per_core The maximum number of jobs to be launched per CPU core. It sets the
   * uplimit of CPU parallelism, i.e. `num_cores * max_jobs_per_core`. Use -1 to disable
   * parallelism.
   * \param max_innermost_factor The maximum size of the innermost factor. NullOpt means no limit
   * \return The schedule rule created
   */
  TVM_DLL static ScheduleRule AddRFactor(int max_jobs_per_core,  //
                                         Optional<Integer> max_innermost_factor);
  /*!
   * \brief Create a schedule rule which applies cross-thread reduction to some reduction blocks
   * correspondingly when needed
   * \param thread_extents Candidates of thread axis extent (values are required to be positive).
   * \return The schedule rule created
   */
  TVM_DLL static ScheduleRule CrossThreadReduction(Array<runtime::Int> thread_extents);
  /*!
   * \brief A rule that randomly select a compute-at location for a free block
   * \return The schedule rule created
   */
  TVM_DLL static ScheduleRule RandomComputeLocation();
  /*!
   * \brief Mark parallelize, vectorize and unroll to the root block. The mark will be applied to
   * each block in a follow-up post processor
   * \param max_jobs_per_core The maximum number of jobs to be launched per CPU core. It sets the
   * upper limit of CPU parallelism, i.e. `num_cores * max_jobs_per_core`. Use -1 to disable
   * parallelism.
   * \param max_vectorize_extent The maximum extent to be vectorized.
   * It sets the upper limit of the hardware target vectorization. Use -1 to disable vectorization.
   * \param unroll_max_steps The options of the maximum number of unroll steps to be done.
   * Use an empty array to disable unroll.
   * \param unroll_explicit Whether to explicitly unroll the loop, or just add an "unroll" pragma.
   * \return The schedule rule created
   */
  TVM_DLL static ScheduleRule ParallelizeVectorizeUnroll(int max_jobs_per_core,                 //
                                                         int max_vectorize_extent,              //
                                                         Array<runtime::Int> unroll_max_steps,  //
                                                         bool unroll_explicit);
  /*!
   * \brief Auto bind loops around the block to BlockIdx and ThreadIdx
   * \param max_threadblocks The maximum number of threadblock on GPU
   * \param thread_extents Candidates of thread axis extent.
   * \param max_threads_per_block The maximum number of threads per block, if it is known
   * when this schedule rule is created.
   * \return The schedule rule created
   */
  TVM_DLL static ScheduleRule AutoBind(int max_threadblocks, Array<Integer> thread_extents,
                                       int max_threads_per_block = -1);
  /*!
   * \brief Create a schedule rule with customized methods on the python-side.
   * \param f_initialize_with_tune_context The packed function of `InitializeWithTuneContext`.
   * \param f_apply The packed function of `Apply`.
   * \param f_clone The packed function of `Clone`.
   * \param f_as_string The packed function of `AsString`.
   * \return The schedule rule created.
   */
  TVM_DLL static ScheduleRule PyScheduleRule(
      FInitializeWithTuneContext f_initialize_with_tune_context,  //
      FApply f_apply,                                             //
      FClone f_clone,                                             //
      FAsString f_as_string);

  /*! \brief Create default schedule rules for LLVM */
  TVM_DLL static Array<ScheduleRule, void> DefaultLLVM();
  /*! \brief Create default schedule rules for x86 (AVX512 and VNNI) */
  TVM_DLL static Array<ScheduleRule, void> DefaultX86(const String& type);
  /*! \brief Create default schedule rules for CUDA */
  TVM_DLL static Array<ScheduleRule, void> DefaultCUDA();
  /*! \brief Create default postprocessors for CUDA with TensorCore */
  TVM_DLL static Array<ScheduleRule, void> DefaultCUDATensorCore();
  /*! \brief Create default schedule rules for Hexagon */
  TVM_DLL static Array<ScheduleRule, void> DefaultHexagon();
  /*! \brief Create default schedule rules for Micro */
  TVM_DLL static Array<ScheduleRule, void> DefaultMicro();
  /*! \brief Create default schedule rules for ARM CPU (NEON and DOTPROD) */
  TVM_DLL static Array<ScheduleRule, void> DefaultARM(const String& type);

  TVM_DEFINE_MUTABLE_OBJECT_REF_METHODS(ScheduleRule, ObjectRef, ScheduleRuleNode);
};

/*! \brief The schedule rule with customized methods on the python-side. */
class PyScheduleRuleNode : public ScheduleRuleNode {
 public:
  using FInitializeWithTuneContext = ScheduleRule::FInitializeWithTuneContext;
  using FApply = ScheduleRule::FApply;
  using FClone = ScheduleRule::FClone;
  using FAsString = ScheduleRule::FAsString;

  /*! \brief The packed function to the `InitializeWithTuneContext` function. */
  FInitializeWithTuneContext f_initialize_with_tune_context;
  /*! \brief The packed function to the `Apply` function. */
  FApply f_apply;
  /*! \brief The packed function to the `AsString` function. */
  FAsString f_as_string;
  /*! \brief The packed function to the `Clone` function. */
  FClone f_clone;

  void VisitAttrs(tvm::AttrVisitor* v) {
    // `f_initialize_with_tune_context` is not visited
    // `f_apply` is not visited
    // `f_as_string` is not visited
    // `f_clone` is not visited
  }

  void InitializeWithTuneContext(const TuneContext& context) final;
  Array<tir::Schedule> Apply(const tir::Schedule& sch, const tir::BlockRV& block) final;
  ScheduleRule Clone() const final;

  static constexpr const char* _type_key = "meta_schedule.PyScheduleRule";
  TVM_DECLARE_FINAL_OBJECT_INFO(PyScheduleRuleNode, ScheduleRuleNode);
};

}  // namespace meta_schedule
}  // namespace tvm

#endif  // TVM_META_SCHEDULE_SCHEDULE_RULE_H_
