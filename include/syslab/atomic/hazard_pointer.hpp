#pragma once

#include <atomic>
#include <cstddef>
#include <utility>
#include <vector>

namespace syslab {

/**
 * @brief Hazard-pointer domain for a bounded number of threads.
 * 线程数有上限的危险指针域。
 *
 * A guard publishes the pointer it is about to use. retire defers deletion
 * until a scan observes that no slot still holds that pointer. The domain
 * supports 64 registered threads and 2 slots each. Exceeding the thread limit
 * throws. Deleters run on the retiring thread. This is a teaching reclamation
 * scheme, not a general memory-management runtime.
 * 守卫公布它即将使用的指针。retire 把删除推迟到扫描发现没有任何槽仍持有该指针。域支持 64 个已注册线程，每线程 2 个槽。超过线程上限会抛出异常。删除函数在退役线程上运行。这是教学回收方案，不是通用内存管理运行时。
 */
class HazardDomain {
 public:
  static constexpr int kMaxThreads = 64;
  static constexpr int kSlots = 2;

  class Guard;

  /**
   * @brief Thread record used by guards. Callers outside the domain should not
   * 守卫使用的线程记录。域外调用者不应该查看它。它是公开的，以便实现文件能够命名该类型。
   * inspect it. It is public so the implementation file can name the type.
   * 查看它。它是公开的，以便实现文件能够命名该类型。
   */
  struct Record {
    std::atomic<void*> slots[kSlots] {};
    std::atomic<bool> active{false};
    std::vector<std::pair<void*, void (*)(void*)>> retired;
  };

  /**
   * @brief Registers the caller if needed and returns a guard for its slots.
   * 必要时注册调用者，并返回其槽位的守卫。
   * @return Guard that clears slots on destruction.
   * 析构时清除槽位的守卫。
   * @throws Error ResourceLimit when more than 64 threads register.
   * 注册线程超过 64 个时抛出 Error ResourceLimit。
   */
  Guard acquire();

  class Guard {
   public:
    Guard() = default;
    ~Guard();
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
    Guard(Guard&& other) noexcept;
    Guard& operator=(Guard&& other) noexcept;

    /**
     * @brief Publishes a hazard for the loaded pointer and returns it.
     * 为已加载的指针公布危险标记并返回它。
     *
     * The load/store loop repeats until the source still contains the pointer
     * that was published. That closes the window in which the pointer is
     * reclaimed before the hazard becomes visible.
     * 加载和存储的循环会重复，直到源里仍然是已公布的那个指针。这关掉了危险标记可见之前指针就被回收的窗口。
     *
     * @tparam T Pointed-to type.
     * 所指类型。
     * @param slot Slot index, 0 or 1.
     * 槽下标，0 或 1。
     * @param source Atomic pointer being traversed.
     * 正在遍历的原子指针。
     * @return The protected pointer, which may be null.
     * 受保护的指针，可以为空。
     */
    template <typename T>
    T* protect(int slot, std::atomic<T*>& source) {
      T* pointer = nullptr;
      do {
        pointer = source.load(std::memory_order_seq_cst);
        slot_at(slot).store(pointer, std::memory_order_seq_cst);
      } while (source.load(std::memory_order_seq_cst) != pointer);
      return pointer;
    }

    /**
     * @brief Clears one hazard slot.
     * 清除一个危险槽。
     * @param slot Slot index, 0 or 1.
     * 槽下标，0 或 1。
     */
    void clear(int slot);

    /**
     * @brief Defers deletion of an object until no hazard references it.
     * 把对象的删除推迟到没有任何危险指针引用它。
     * @param pointer Object to delete. Null is ignored.
     * 要删除的对象。空指针被忽略。
     * @param deleter Function that deletes the object.
     * 删除该对象的函数。
     */
    void retire(void* pointer, void (*deleter)(void*));

   private:
    friend class HazardDomain;
    Guard(HazardDomain* domain, Record* record) : domain_(domain), record_(record) {}
    std::atomic<void*>& slot_at(int slot);
    HazardDomain* domain_ = nullptr;
    Record* record_ = nullptr;
  };

 private:
  friend class Guard;
  Record* records();
  Record* register_thread();
};

}  // namespace syslab
