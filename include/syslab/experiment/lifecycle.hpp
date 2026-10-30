#pragma once

#include "syslab/core/json.hpp"
#include "syslab/experiment/model.hpp"

#include <string>
#include <vector>

namespace syslab {

/**
 * @brief Records legal lifecycle transitions for one invocation.
 * 记录一次调用中合法的生命周期迁移。
 *
 * Each transition stores the previous state, the new state, a reason, and a
 * UTC timestamp. Terminal states reject further transitions.
 * 每次迁移保存前一状态、新状态、原因和 UTC 时间。终止状态拒绝后续迁移。
 */
class Lifecycle {
 public:
  /**
   * @brief Starts an invocation in the Registered state.
   * 在 Registered 状态开始一次调用。
   * @param experiment_id Experiment identifier copied into the history.
   * 写入历史的实验标识。
   */
  explicit Lifecycle(std::string experiment_id);

  /**
   * @brief Moves to the next state when the edge is legal.
   * 边合法时迁移到下一状态。
   * @param next Destination state.
   * 目标状态。
   * @param reason Why the transition happened.
   * 发生迁移的原因。
   * @throws Error State when the edge is not in the lifecycle graph.
   * 该边不在生命周期图中时抛出 Error State。
   */
  void transition(ExperimentState next, std::string reason);

  /**
   * @brief Returns the current state.
   * 返回当前状态。
   * @return Current lifecycle state.
   * 当前生命周期状态。
   */
  ExperimentState state() const noexcept { return state_; }

  /**
   * @brief Returns the experiment identifier.
   * 返回实验标识。
   * @return Identifier supplied at construction.
   * 构造时提供的标识。
   */
  const std::string& experiment_id() const noexcept { return experiment_id_; }

  /**
   * @brief Serializes the transition log.
   * 把迁移日志序列化为 JSON。
   * @return JSON array of transition objects.
   * 迁移对象组成的 JSON 数组。
   */
  Json history() const;

 private:
  std::string experiment_id_;
  ExperimentState state_ = ExperimentState::Registered;
  struct Event {
    ExperimentState from;
    ExperimentState to;
    std::string reason;
    std::string at;
  };
  std::vector<Event> events_;
};

/**
 * @brief Reports whether a state has no legal outgoing edge.
 * 报告一个状态是否没有合法出边。
 * @param state Lifecycle state.
 * 生命周期状态。
 * @return True for Passed, Failed, Unsupported, and Interrupted.
 * 对 Passed、Failed、Unsupported 和 Interrupted 为真。
 */
bool is_terminal(ExperimentState state) noexcept;

}  // namespace syslab
