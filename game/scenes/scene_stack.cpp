#include "scenes/scene_stack.h"

namespace dc::scenes {

void SceneStack::push(std::unique_ptr<Scene> scene) {
    pending_.push_back({Operation::Push, std::move(scene)});
}

void SceneStack::pop() {
    pending_.push_back({Operation::Pop, nullptr});
}

void SceneStack::replace(std::unique_ptr<Scene> scene) {
    pending_.push_back({Operation::Replace, std::move(scene)});
}

void SceneStack::applyPending(SceneContext& context) {
    for (Pending& pending : pending_) {
        if ((pending.operation == Operation::Pop || pending.operation == Operation::Replace) && !stack_.empty()) {
            stack_.back()->onExit(context);
            stack_.pop_back();
        }
        if (pending.operation == Operation::Push || pending.operation == Operation::Replace) {
            stack_.push_back(std::move(pending.scene));
            stack_.back()->onEnter(context);
        }
    }
    pending_.clear();
}

Scene* SceneStack::top() const noexcept {
    return stack_.empty() ? nullptr : stack_.back().get();
}

}  // namespace dc::scenes
