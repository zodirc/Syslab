#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace syslab {

/**
 * @brief Single-owner pointer.
 * 单一所有者指针。
 *
 * The destructor deletes the object. Move transfers ownership. Copy is
 * rejected. reset deletes the previous object before taking the new one.
 * 析构时删除对象。移动转移所有权。禁止拷贝。reset 先删除原对象，再接管新对象。
 */
template <typename T>
class UniquePtr {
 public:
  UniquePtr() = default;
  /**
   * @brief Takes ownership of a pointer allocated with new.
   * 接管一个用 new 分配的指针。
   * @param pointer Object pointer, or null.
   * 对象指针，或空。
   */
  explicit UniquePtr(T* pointer) : pointer_(pointer) {}
  ~UniquePtr() { delete pointer_; }
  UniquePtr(const UniquePtr&) = delete;
  UniquePtr& operator=(const UniquePtr&) = delete;
  UniquePtr(UniquePtr&& other) noexcept : pointer_(other.pointer_) { other.pointer_ = nullptr; }
  UniquePtr& operator=(UniquePtr&& other) noexcept {
    if (this != &other) {
      reset(other.release());
    }
    return *this;
  }

  /**
   * @brief Releases ownership without deleting.
   * 放弃所有权，不删除。
   * @return Previous pointer.
   * 原先的指针。
   */
  T* release() {
    T* pointer = pointer_;
    pointer_ = nullptr;
    return pointer;
  }

  /**
   * @brief Deletes the current object and optionally takes a new one.
   * 删除当前对象，并可选地接管一个新对象。
   * @param pointer New object, or null.
   * 新对象，或空。
   */
  void reset(T* pointer = nullptr) {
    delete pointer_;
    pointer_ = pointer;
  }

  /**
   * @brief Returns the owned pointer.
   * 返回拥有的指针。
   * @return Object pointer, or null.
   * 对象指针，或空。
   */
  T* get() const { return pointer_; }
  T& operator*() const { return *pointer_; }
  T* operator->() const { return pointer_; }
  explicit operator bool() const { return pointer_ != nullptr; }

 private:
  T* pointer_ = nullptr;
};

template <typename T>
class WeakPtr;

/**
 * @brief Shared ownership with an atomic control block.
 * 带原子控制块的共享所有权。
 *
 * The last strong reference deletes the object. The control block remains
 * until the last weak reference is gone, so a weak pointer can observe that
 * the object has expired. Copying a SharedPtr increments the strong count
 * with acquire-release. The destructor's last decrement synchronizes with
 * the deleter. This is a teaching pointer, not std::shared_ptr.
 * 最后一个强引用删除对象。控制块保留到最后一个弱引用消失，因此弱指针可以观察到对象已经过期。拷贝 SharedPtr 用 acquire-release 增加强计数。析构函数的最后一次递减与删除函数同步。这是教学指针，不是 std::shared_ptr。
 */
template <typename T>
class SharedPtr {
 public:
  SharedPtr() = default;

  /**
   * @brief Takes ownership of an object allocated with new.
   * 接管一个用 new 分配的对象。
   * @param pointer Object pointer, or null.
   * 对象指针，或空。
   * @throws std::bad_alloc when the control block cannot be allocated.
   * 无法分配控制块时为 std::bad_alloc。
   */
  explicit SharedPtr(T* pointer) {
    if (pointer == nullptr) {
      return;
    }
    try {
      control_ = new Control(pointer);
    } catch (...) {
      delete pointer;
      throw;
    }
  }

  SharedPtr(const SharedPtr& other) : control_(other.control_) { retain(); }
  SharedPtr& operator=(const SharedPtr& other) {
    if (this != &other) {
      SharedPtr copy(other);
      swap(copy);
    }
    return *this;
  }
  SharedPtr(SharedPtr&& other) noexcept : control_(other.control_) { other.control_ = nullptr; }
  SharedPtr& operator=(SharedPtr&& other) noexcept {
    if (this != &other) {
      reset();
      control_ = other.control_;
      other.control_ = nullptr;
    }
    return *this;
  }
  ~SharedPtr() { reset(); }

  /**
   * @brief Drops this strong reference.
   * 丢弃这个强引用。
   */
  void reset() {
    if (control_ != nullptr) {
      release();
      control_ = nullptr;
    }
  }

  /**
   * @brief Returns the managed pointer.
   * 返回被管理的指针。
   * @return Object pointer, or null when empty.
   * 对象指针。为空时是空指针。
   */
  T* get() const { return control_ == nullptr ? nullptr : control_->pointer; }
  T& operator*() const { return *get(); }
  T* operator->() const { return get(); }
  explicit operator bool() const { return get() != nullptr; }

  /**
   * @brief Returns the strong reference count.
   * 返回强引用计数。
   * @return Strong count, or zero when empty.
   * 强计数。为空时是零。
   */
  std::int64_t use_count() const {
    return control_ == nullptr ? 0 : control_->strong.load(std::memory_order_acquire);
  }

  /**
   * @brief Exchanges two pointers.
   * 交换两个指针。
   * @param other Other pointer.
   * 另一个指针。
   */
  void swap(SharedPtr& other) noexcept { std::swap(control_, other.control_); }

 private:
  friend class WeakPtr<T>;
  struct Control {
    explicit Control(T* object) : pointer(object) {}
    std::atomic<std::int64_t> strong{1};
    std::atomic<std::int64_t> weak{1};
    T* pointer = nullptr;
  };

  explicit SharedPtr(Control* control) : control_(control) {}

  void retain() {
    if (control_ != nullptr) {
      control_->strong.fetch_add(1, std::memory_order_acq_rel);
    }
  }

  void release() {
    if (control_->strong.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      delete control_->pointer;
      control_->pointer = nullptr;
      if (control_->weak.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete control_;
      }
    }
  }

  Control* control_ = nullptr;
};

/**
 * @brief Non-owning reference to a SharedPtr control block.
 * 不拥有 SharedPtr 控制块的引用。
 *
 * lock attempts to create a strong reference. It returns an empty SharedPtr
 * after the object has been destroyed. Destroying the last weak reference
 * deletes the control block.
 * lock 尝试建立强引用。对象已销毁后它返回空的 SharedPtr。销毁最后一个弱引用会删除控制块。
 */
template <typename T>
class WeakPtr {
 public:
  WeakPtr() = default;
  /**
   * @brief Observes a shared pointer.
   * 观察一个共享指针。
   * @param shared Shared pointer. An empty pointer produces an empty weak pointer.
   * 共享指针。空指针会产生空的弱指针。
   */
  explicit WeakPtr(const SharedPtr<T>& shared) : control_(shared.control_) { retain(); }
  WeakPtr(const WeakPtr& other) : control_(other.control_) { retain(); }
  WeakPtr& operator=(const WeakPtr& other) {
    if (this != &other) {
      WeakPtr copy(other);
      std::swap(control_, copy.control_);
    }
    return *this;
  }
  WeakPtr(WeakPtr&& other) noexcept : control_(other.control_) { other.control_ = nullptr; }
  WeakPtr& operator=(WeakPtr&& other) noexcept {
    if (this != &other) {
      reset();
      control_ = other.control_;
      other.control_ = nullptr;
    }
    return *this;
  }
  ~WeakPtr() { reset(); }

  /**
   * @brief Drops this weak reference.
   * 丢弃这个弱引用。
   */
  void reset() {
    if (control_ != nullptr) {
      if (control_->weak.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete control_;
      }
      control_ = nullptr;
    }
  }

  /**
   * @brief Creates a strong pointer if the object is still alive.
   * 对象仍存活时创建一个强指针。
   * @return Shared pointer, or empty when the object has expired.
   * 共享指针。对象已过期时为空。
   */
  SharedPtr<T> lock() const {
    if (control_ == nullptr) {
      return SharedPtr<T>();
    }
    std::int64_t strong = control_->strong.load(std::memory_order_acquire);
    while (strong != 0) {
      if (control_->strong.compare_exchange_weak(strong, strong + 1, std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
        return SharedPtr<T>(control_);
      }
    }
    return SharedPtr<T>();
  }

  /**
   * @brief Reports whether the managed object has expired.
   * 报告被管理的对象是否已过期。
   * @return True when lock would return empty.
   * lock 会返回空时为真。
   */
  bool expired() const {
    return control_ == nullptr || control_->strong.load(std::memory_order_acquire) == 0;
  }

 private:
  void retain() {
    if (control_ != nullptr) {
      control_->weak.fetch_add(1, std::memory_order_acq_rel);
    }
  }

  typename SharedPtr<T>::Control* control_ = nullptr;
};

/**
 * @brief Allocates an object and a shared pointer that owns it.
 * 分配一个对象以及拥有它的共享指针。
 * @tparam T Object type.
 * 对象类型。
 * @tparam Arguments Constructor argument types.
 * 构造函数参数类型。
 * @param arguments Constructor arguments.
 * 构造函数参数。
 * @return Shared pointer.
 * 共享指针。
 */
template <typename T, typename... Arguments>
SharedPtr<T> make_shared(Arguments&&... arguments) {
  return SharedPtr<T>(new T(std::forward<Arguments>(arguments)...));
}

}  // namespace syslab
