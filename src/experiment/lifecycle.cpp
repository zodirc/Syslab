#include "syslab/experiment/lifecycle.hpp"

#include "syslab/core/error.hpp"
#include "syslab/core/time.hpp"

namespace syslab {
namespace {

bool edge_allowed(ExperimentState from, ExperimentState to) {
  if (is_terminal(from)) {
    return false;
  }
  switch (from) {
    case ExperimentState::Registered:
      return to == ExperimentState::Validating;
    case ExperimentState::Validating:
      return to == ExperimentState::EnvironmentCheck || to == ExperimentState::Failed;
    case ExperimentState::EnvironmentCheck:
      return to == ExperimentState::Building || to == ExperimentState::Unsupported ||
             to == ExperimentState::Failed;
    case ExperimentState::Building:
      return to == ExperimentState::Built || to == ExperimentState::Failed ||
             to == ExperimentState::Interrupted;
    case ExperimentState::Built:
      return to == ExperimentState::Running || to == ExperimentState::Failed ||
             to == ExperimentState::Interrupted;
    case ExperimentState::Running:
      return to == ExperimentState::Observing || to == ExperimentState::Failed ||
             to == ExperimentState::Interrupted;
    case ExperimentState::Observing:
      return to == ExperimentState::Verifying || to == ExperimentState::Failed ||
             to == ExperimentState::Interrupted;
    case ExperimentState::Verifying:
      return to == ExperimentState::Passed || to == ExperimentState::Failed ||
             to == ExperimentState::Unsupported || to == ExperimentState::Interrupted;
    case ExperimentState::Passed:
    case ExperimentState::Failed:
    case ExperimentState::Unsupported:
    case ExperimentState::Interrupted:
      return false;
  }
  return false;
}

}  // namespace

bool is_terminal(ExperimentState state) noexcept {
  return state == ExperimentState::Passed || state == ExperimentState::Failed ||
         state == ExperimentState::Unsupported || state == ExperimentState::Interrupted;
}

Lifecycle::Lifecycle(std::string experiment_id) : experiment_id_(std::move(experiment_id)) {
  Event event;
  event.from = ExperimentState::Registered;
  event.to = ExperimentState::Registered;
  event.reason = "manifest loaded";
  event.at = utc_timestamp();
  events_.push_back(std::move(event));
}

void Lifecycle::transition(ExperimentState next, std::string reason) {
  if (!edge_allowed(state_, next)) {
    throw Error(Error::Code::State, std::string("illegal transition ") + to_string(state_) + " -> " +
                                         to_string(next));
  }
  if (reason.empty()) {
    throw Error(Error::Code::InvalidArgument, "transition reason is required");
  }
  Event event;
  event.from = state_;
  event.to = next;
  event.reason = std::move(reason);
  event.at = utc_timestamp();
  state_ = next;
  events_.push_back(std::move(event));
}

Json Lifecycle::history() const {
  Json array = Json::array();
  for (const Event& event : events_) {
    Json item = Json::object();
    item.set("experiment_id", Json::string(experiment_id_));
    item.set("from", Json::string(to_string(event.from)));
    item.set("to", Json::string(to_string(event.to)));
    item.set("reason", Json::string(event.reason));
    item.set("at", Json::string(event.at));
    array.push(std::move(item));
  }
  return array;
}

}  // namespace syslab
