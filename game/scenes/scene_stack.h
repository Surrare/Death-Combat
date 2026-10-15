#pragma once

#include "scenes/scene.h"

#include <memory>
#include <vector>

namespace dc::scenes {

// Стек сцен. push / pop / replace ставятся в очередь и применяются между кадрами,
// никогда посреди update (docs/ARCHITECTURE.md §10.1). Пауза — сцена-оверлей поверх матча.
class SceneStack {
public:
    void push(std::unique_ptr<Scene> scene);
    void pop();
    void replace(std::unique_ptr<Scene> scene);

    void applyPending(SceneContext& context);  // в начале кадра

    [[nodiscard]] Scene* top() const noexcept;  // не владеет; nullptr, если стек пуст
    [[nodiscard]] bool empty() const noexcept { return stack_.empty(); }

private:
    enum class Operation { Push, Pop, Replace };
    struct Pending {
        Operation operation;
        std::unique_ptr<Scene> scene;
    };

    std::vector<std::unique_ptr<Scene>> stack_;
    std::vector<Pending> pending_;
};

}  // namespace dc::scenes
