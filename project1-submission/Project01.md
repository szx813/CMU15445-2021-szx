# 关于环境搭建

需要：

* WSL Linux 子系统
* VScode 及相关插件

参考教程：<[CMU 15445 vscode/clion clang12 cmake环境配置 - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/592802373)>

了解 GTSET 常用断言 <[GTest常用断言 - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/176975141)>

# Project 01

## TASK 1 LRU（Least Recently Used）替换策略

### 题目描述

> 该组件负责跟踪缓冲池中页面的使用情况。您将在`src/include/buffer/lru_replacer.h`中实现一个名为 `LRUReplacer`的新子类，并在`src/buffer/lru_replacer.cpp`中实现相应的实现文件。

> LRUReplacer 扩展了抽象的 Replacer 类 (`src/include/buffer/replacer.h`)，该类包含函数规范。

> LRUReplacer 的最大页数与缓冲池的大小相同，因为它包含 BufferPoolManager 中所有帧的占位符。不过，在任何给定时刻，并非所有帧都被视为在 LRUReplacer 中。LRUReplacer 初始化时没有帧。然后，只有Unpin过的帧才会被视为在 LRUReplacer 中。

### 思路

​	LRU 的较优数据结构实现是通过**双向链表 + hash表**实现的，因为 hash 映射使得其查找的时间复杂度为 O(1)，而 List 使得其插入，删除的时间复杂度为 O(1)，这样可以大大降低程序运行时间，提高效率！

![在这里插入图片描述](https://img-blog.csdnimg.cn/20200724194750177.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3ljX2N5MTk5OQ==,size_16,color_FFFFFF,t_70)

* `Victim(frame_id_t*)` ：删除与 Replacer 追踪的所有其他元素相比最近访问次数最少的 frame_id，并将其存储到输出参数中，并返回 true；如果 Replacer 为空，则返回 false。
  * LRU Replacer 里 list 存储的是 frame_id，并不是页面本身的内容；
  * Victim 的输入参数是一个 frame_id_t 类型的指针，系统每执行一次 Victim，函数都需要将访问次数最少的，也就是在 List 尾部的 frame_id_t 删除并记录为在 `*frame_id_t` 当中；
* `Pin(frame_id_t)` ：page 被固定到 BufferPoolManager 中的一个 frame 后，应调用此方法。它将从 LRUReplacer 中移除包含被固定页面的 frame。
  * 也就是说，被 pin 的 frame，即有进程或线程访问该页时，若该页的 frame 在 LRU_Replacer 当中，则需要从 LRU_Replacer 中移出，即该页不能被 LRU 所淘汰。
* `Unpin(frame_id_t)` ：当页面的 pin_count 变为 0 时，应调用此方法。此方法应将包含未置顶页面的帧添加到 LRUReplacer 中。
  * 和 Pin 函数相反，如果有进程或线程没有访问该页时，则该页的 frame 就需要被 Unpin，被 Unpin 的 frame 需要添加进 LRU_Replacer 当中；
* Size() ：该方法返回当前 LRUReplacer 中的 frame 数。
* 注意每个函数前加锁！

### 程序设计

* `lru_replacer.h` 的私有成员：

```cpp
#pragma once

#include <cstddef>
#include <list>
#include <mutex>  // NOLINT
#include <vector>
#include <unordered_map>

#include "buffer/replacer.h"
#include "common/config.h"

namespace bustub {

/**
 * LRUReplacer implements the Least Recently Used replacement policy.
 */
class LRUReplacer : public Replacer {
 public:
  /**
   * Create a new LRUReplacer.
   * @param num_pages the maximum number of pages the LRUReplacer will be required to store
   */
  explicit LRUReplacer(size_t num_pages);

  /**
   * Destroys the LRUReplacer.
   */
  ~LRUReplacer() override;

  auto Victim(frame_id_t *frame_id) -> bool override;

  void Pin(frame_id_t frame_id) override;

  void Unpin(frame_id_t frame_id) override;

  auto Size() -> size_t override;

 private:
  // TODO(student): implement me!
  size_t capacity; // lru_replace 的容量
  size_t size; // lru_replace 的当前大小
  std::list<frame_id_t> lists; // 存储 frame 的双向链表
  std::unordered_map<frame_id_t, int> maps; // 哈希映射
  std::mutex mtx;
};
}  // namespace bustub
```

* `lru_replacer.cpp` 函数实现：

```cpp
//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_replacer.cpp
//
// Identification: src/buffer/lru_replacer.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_replacer.h"

namespace bustub {

LRUReplacer::LRUReplacer(size_t num_pages) {
  // 初始化成员
  capacity = num_pages;
  maps.clear();
  lists.clear();
  size = lists.size();
}

LRUReplacer::~LRUReplacer() = default;

bool LRUReplacer::Victim(frame_id_t *frame_id) {
  std::lock_guard<std::mutex> guard(mtx);
  if (lists.empty()) {
    return false;
  }
  frame_id_t last_frame = lists.back();
  maps.erase(last_frame);
  lists.pop_back();
  *frame_id = last_frame;
  return true;
}

void LRUReplacer::Pin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> guard(mtx);
  if (maps.count(frame_id) == 0) {
    return;
  }
  lists.remove(frame_id);
  maps.erase(frame_id);
}

void LRUReplacer::Unpin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> guard(mtx);
  if (maps.count(frame_id) != 0) {
    return;
  }
  if (lists.size() == capacity) {
    frame_id_t last_frame = lists.back();
    maps.erase(last_frame);
    lists.pop_back();
  }
  lists.push_front(frame_id);
  maps[frame_id] = 1;
}

size_t LRUReplacer::Size() {
  std::lock_guard<std::mutex> guard(mtx);
  return size = lists.size();
}

}  // namespace bustub


```

### 测试

![image-20231206165444091](C:\Users\11246\AppData\Roaming\Typora\typora-user-images\image-20231206165444091.png)

## TASK 2 缓冲池管理器

### 题目描述

> ​	接下来，您需要在系统中实现缓冲池管理器（BufferPoolManagerInstance）。BufferPoolManagerInstance 负责从 DiskManager 中获取数据库页面并将其存储在内存中。当 BufferPoolManagerInstance 收到明确指示或需要删除页面为新页面腾出空间时，它还可以将脏页面写入磁盘。
>
> ​	为确保您的实现能与系统的其他部分正确配合，我们将为您提供一些已填好的函数。您也不需要实现实际读取和写入磁盘数据的代码（在我们的实现中称为 DiskManager）。我们将为您提供该功能。
>
> ​	系统中的所有内存页面都由 Page 对象表示。BufferPoolManagerInstance 不需要了解这些页面的内容。但作为系统开发人员，您必须明白，Page 对象只是缓冲池中内存的容器，因此并不特定于某个页面。也就是说，每个页面对象都包含一个内存块，DiskManager 将把它用作从磁盘读取物理页面内容的复制位置。
>
> ​	BufferPoolManagerInstance 将重复使用相同的页面对象来存储数据，因为它在磁盘上来回移动。这意味着在系统的整个生命周期中，同一个页面对象可能包含不同的物理页面。页面对象的标识符（page_id）可追踪其包含的物理页面；如果页面对象不包含物理页面，则其 page_id 必须设置为 INVALID_PAGE_ID。
>
> ​	每个页面对象还维护一个计数器，用于记录已 "钉住 "该页面的线程数。BufferPoolManagerInstance 不允许释放被钉住的页面。每个页面对象也会记录它是否变脏。你的任务就是记录页面在解除固定前是否被修改过。BufferPoolManagerInstance 必须将脏页面的内容写回磁盘，然后才能重新使用该对象。
>
> ​	您的 BufferPoolManagerInstance 实现将使用您在本作业前几步中创建的 LRUReplacer 类。它将使用 LRUReplacer 来跟踪页面对象被访问的时间，以便在必须释放帧以腾出空间从磁盘复制新的物理页面时，决定驱逐哪个页面对象。
>
> ​	您需要在源文件 `(src/buffer/buffer_pool_manager_instance.cpp)` 中实现头文件`(src/include/buffer/buffer_pool_manager_instance.h)` 中定义的以下函数：
>
> - `FetchPgImp(page_id)`
> - `UnpinPgImp(page_id, is_dirty)`
> - `FlushPgImp(page_id)`
> - `NewPgImp(page_id)`
> - `DeletePgImp(page_id)`
> - `FlushAllPagesImpl()`
>
> ​	对于 `FetchPgImp`，如果空闲列表中没有可用页面，且所有其他页面当前都被钉住，则应返回 NULL。FlushPgImp 则会刷新页面，而不管其是否处于固定状态。
>
> ​	对于 `UnpinPgImp`，`is_dirty` 参数用于跟踪页面在被固定时是否被修改。
>
> ​	有关如何实现这些函数的详细信息，请参阅函数文档。请勿使用非模板版本，我们需要这些版本来对您的代码进行分级。
>
> ​	注意：在 LRUReplacer 和 BufferPoolManagerInstance 的上下文中，Pin 和 Unpin 具有相反的含义。在 LRUReplacer 的上下文中，钉住一个页面意味着我们不应该驱逐该页面，因为它正在使用中。这意味着我们应该将其从 LRUReplacer 中移除。另一方面，在缓冲池管理器实例（BufferPoolManagerInstance）中钉住一个页面意味着我们要使用该页面，而且不应该将其从缓冲池中移除。

### 思路

* 理清 `.h` 头文件的内容和之间的关系：

  * 在 `buffer_pool_manager_instance.h` 中，buffer_pool_manager_instance 公有继承了抽象类 buffer_pool_manager：

    * `buffer_pool_manager_instance.h` 是 `buffer_pool_manager.h` 的具体实现，即后者是前者的一个**抽象类**（抽象类的成员函数有纯虚函数，因此抽象类不能被实例化，即不能创建对象）；
    * `buffer_pool_manager.h` 内要实现的 6 个成员函数均为纯虚函数，并且在 `buffer_pool_manager_instances.h` 中被覆盖，因此，buffer_pool_manager_instance 类是可以实例化的；

  * 在 `buffer_pool_manager_instance.h` 中找出其类中的一些重要的对象成员：

    * `const size_t pool_size_` 表示 BPM 的大小；

    * `Page *pages_` 指向 BPM 中存放的 Page 的对象指针，每一个元素都表示一个 Page 类；

    * `DiskManager *disk_manager_ __attribute__((__unused__))` 一个指向磁盘管理器的对象指针；

    * `std::unordered_map<page_id_t, frame_id_t> page_table_ ` 一个在 BPM 中 page 的 page_id 和缓冲池的 frame_id 的映射关系；

    * `Replacer *replacer_` 一个 lru 对象指针，用于后续利用 lru 策略删减不需要的 page。

    * `std::list<frame_id_t> free_list_` 表示还没有被分配过的 frame_id；

    * `std::mutex latch_` 锁存器对象，用来线程保护；
    * `const uint32_t num_instances_ = 1` 表示在并行 BPM 中有多少 BPM 实例，在该 TASK 中默认为 1，即只实例一个 BPM 类；
    * `const uint32_t instance_index_ = 0` 表示在并行 BPM 中当前 BPM 实例的索引号，在该 TASK 中默认为 0；
    * `std::atomic<page_id_t> next_page_id_ = instance_index_` 表示每个 BPI 要存放下一个 page 的 id 号，在该 TASK 中默认每创建一个新 page，则 `next_page_id + 1`；
      * [C++原子变量atomic详解 - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/599202353)

  * 在 `Page.h` 文件中，将 buffer_pool_manager_instance 类声明成了 Page 类的友元类，那么，buffer_pool_manager_instance   类就可访问 Page 类的所有成员（函数）了！

  * Page 类包含了一个 Page 的所有属性，包括一些成员和对应的成员函数：

    * `char data_[PAGE_SIZE]` 表示 Page 的内容；
    * `page_id_t page_id_` 表示 Page 的 ID 号；
    * `int pin_count_` 表示当前 Page 的进程数；
    * `bool is_dirty_` 表示当前 Page 是否是脏页；

* 实现 `NewPgImp()`函数：在 BPM 中新建一个页，返回该页的 page_id （作为指针传参）和 Page 对象指针。

  * 如果 BPM 中的 page 都被 Pin，则表示 BPM 中的所有 page 都在被线程使用，无法新建页！

  * 否则，则可以新建一个 page！需要先确定新 page 对应的 frame_id !
    * 根据题目要求，优先从 free_list_ 中选择 frame_id ，即弹出一个值；
    * 若 free_list_ 为空，则表示 BPM 中的所有 frame_id 都已经被分配，则需要调用 lru 中的 `Victim` 函数确定要删除 page 对应的 frame_id；

  * 接着，就要从 BPM 中删除掉旧页的相关元数据：
    * 如果该页是脏的，则需要写回磁盘；
    * 然后，清空数据（reset），从 page_table_中删除 hash 映射关系；

  * 最后，设置为新 page 分配新的磁盘空间，更新元数据，返回新的页对象指针：
    * 分配新空间，即返回一个新的 page_id；
    * 创建一个 Page 对象指针指向该页的内存空间；
    * 设置它的元数据（`page_id`，`pin_count_`，`is_dirty`）；
    * 加入 page_tabe_ 的 hash 映射；
    * 从 replacer 中 Pin 掉该页（因为该页正在有线程访问，不能出现在 replacer 中待删除！）

* 实现 `FetchPgImp()` 函数：给定 page_id，从 BPM 中抓取该页，并返回该页的对象指针。

  * 首先，需要判断该页是否在缓冲池（内存）中，即在 page_table_ 表中搜索 page_id：

  * 如果在内存中搜索到，则返回该页的对象指针！
    * `pin_count++` ；
    * **从 replacer 中 Pin 掉该页；**
    * **变脏，因为这是从内存中搜索到的！**

  * 如果没有，则需要讨论以下三种情况：
    * BPM 中的所有 page 都被 pin 了，因此不能抓取其他页到内存，即返回 nullptr！
    * 否则，优先从 free_list_ 中选择 frame_id ，即弹出一个值；
    * 若 free_list_ 为空，则表示 BPM 中的所有 frame_id 都已经被分配，则需要调用 lru 中的 `Victim` 函数确定要删除 page 对应的 frame_id；
      * 根据 lru 淘汰的旧页需要判断是否是脏页（写入磁盘），删除原来的 hash 映射；
      * 然后把要抓取 page 的 page_id 和得到的 frame_id 加入 hash 映射；
      * 更新要抓取 page 的元数据（`disk_mananger_ -> ReadPage()`，`pin_count`，`is_dirty = false`， `page_id`）
        * **这里的 `is_dirty` 是 `false` ，因为这是从磁盘中抓取新 page 到内存的，默认是非脏页！**
      * 从 replacer 中 Pin 掉该页（原因同上！）

* 实现 `UnpinPgImp()` 函数：对 page_id 对应的页的线程减一，赋予是否 dirty 的属性，并执行相关操作，返回 bool 值。

  * 首先，判断内存中是否存在该页，不存在返回 false！

  * 然后，判断该页的 `pin_count` 是否 <= 0，若是，则该页已经 Unpin 或者不正确了，则返回 false！

  * 否则，`pin_count--`，然后判断以下条件
    * 如果此时 `pin_count` 依旧大于 0，则直接返回 true！表示还有线程在使用
    * 否则，即 `pin_count = 0` 表示已经没有线程在使用了，即将该页加入 replacer 中待删除，返回 true！

* 实现 `DeletePgImp(page_id_t page_id)` 函数： 删除内存中 page_id 对应的 page 对象，返回 bool 值。

  * 判断内存中是否存在该页，不存在，表示该页不在内存中，则返回 true！

  * 否则，需要判断该页的 `pin_count` 是否等于 0：
    * != 0，返回 false，该页有线程/进程使用；
    * = 0，执行以下操作：
      * 删除 hash 映射，并把它放回 free_list_ 中；
      * 初始化元数据；
      * 从 replacer 中 Pin 掉该页（原因同上）；
      * 删除内存空间 `DeallocatePage(page_id)`，返回 true！

* 实现 `FlushPgImp(page_id_t page_id)` 函数：将 page_id 对应 page 写入磁盘，返回 bool 值。

  * 判断内存中是否存在该页

  * 判断是否为脏页

  * 判断是否 Unpin
    * 写入磁盘，并初始化 dirty 属性为 false！

* 实现 `FlushAllPgsImp()` 函数：将内存中的所有 page 写入磁盘，返回 void。
  * `for` 遍历 缓冲池（内存）中的所有页，然后同 `FlushPgImp(page_id_t page_id)` 判断，写入磁盘即可！

### 测试

#### Sample Test 过程分析

![微信图片_20231206114625](C:\Users\11246\Desktop\微信图片_20231206114625.jpg)

#### Gradescope 测试案例

![image-20231206214619698](C:\Users\11246\AppData\Roaming\Typora\typora-user-images\image-20231206214619698.png)

![image-20231206214629296](C:\Users\11246\AppData\Roaming\Typora\typora-user-images\image-20231206214629296.png)

## TASK 3 并行缓冲池优化

### 题目描述

> ​	您可能在前面的任务中注意到，单个缓冲池管理器实例需要使用锁存器才能保证线程安全。当每个线程与缓冲池交互时，都要争夺一个锁存器，这可能会造成大量争用。一种可能的解决方案是在系统中使用多个缓冲池，每个缓冲池都有自己的锁存器。
>
> ​	`ParallelBufferPoolManager` 是一个拥有多个 `BufferPoolManagerInstance` 的类。对于每个操作，`ParallelBufferPoolManager` 都会选择一个 `BufferPoolManagerInstance` 并委托给该实例。
>
> ​	我们使用给定的页面 id 来决定使用哪个特定的 `BufferPoolManagerInstance`。如果我们有多个 `BufferPoolManagerInstances`，那么我们需要某种方法将给定的页面 id 映射到 `[0, num_instances]` 范围内的某个数字。在本项目中，我们将使用 modulo 运算符，`page_id mod num_instances` 将把给定的 `page_id` 映射到正确的范围。
>
> ​	当 `ParallelBufferPoolManager` 首次实例化时，它的起始索引应为 0。每次创建新页面时，将从起始索引开始尝试每个 `BufferPoolManagerInstance`，直到有一个成功为止。然后将起始索引增加一个。
>
> ​	确保在创建单个 `BufferPoolManagerInstances` 时，使用包含 `uint32_t num_instances` 和 `uint32_t instance_index` 的构造函数，以便正确创建页面 id。
>
> ​	您需要在源文件` (src/buffer/parallel_buffer_pool_manager.cpp) `中实现头文件 `(src/include/buffer/parallel_buffer_pool_manager.h) `中定义的以下函数：
>
> - `ParallelBufferPoolManager(num_instances, pool_size, disk_manager, log_manager)`
> - `~ParallelBufferPoolManager()`
> - `GetPoolSize()`
> - `GetBufferPoolManager(page_id)`
> - `FetchPgImp(page_id)`
> - `UnpinPgImp(page_id, is_dirty)`
> - `FlushPgImp(page_id)`
> - `NewPgImp(page_id)`
> - `DeletePgImp(page_id)`
> - `FlushAllPagesImpl()`

### 思路

* 确定 `ParallelBufferPoolManager` 类的成员变量；

  * 在 `parallel_buffer_pool_mananger.h` 的构造函数中可以得出已知的成员变量有：

    ```cpp
      size_t num_instances_; // 缓冲池个数
      size_t pool_size_; // 每个缓冲池的容量
      DiskManager* disk_manager_; // 磁盘管理器
      std::mutex latch_; // 线程锁
    ```

  * 其次，根据题目要求，并行 BPM 对于每个操作，都会选择一个 `BufferPoolManagerInstance` 并委托给该实例，并且是根据 `page_id`，来确定使用哪个 BPI 的，涉及到查询操作，因此可以采用 `std::vector` 存放每个 `BufferPoolManager*`，并在构造函数中，给每个 `BufferPoolManager*` 指向一个 `BufferPoolManagerInstance` 实例的内存空间进行初始化操作！

    ```cpp
      std::vector<BufferPoolManager *> instances_; // BPM* vector 数组
    
      // 初始化该数组
      for (size_t i = 0; i < num_instances; i++)
      {
        BufferPoolManager *tmp = new BufferPoolManagerInstance(pool_size, num_instances, i, disk_manager, log_manager);
        instances_.push_back(tmp);
      }
    ```

  * 当我们创建新 page 时，往往需要确定该 page 需要存放在哪个 BPI，因此需要额外定义一个成员变量，来确定下一个新 page 存放的 BPI 编号：

    ```cpp
    size_t next_instance_; // 下一个新页需要放的 BPI 的编号
    ```

* 一些细节：

  * 给定 page_id 可以根据 `page_id % num_instances_` 来确定当前的 BPI 编号；

  * 对于 `NewPgImp (page_id_t *page_id)` 函数的实现，要注意从 next_instance_ 开始遍历 BPI 编号，如果创建 page 成功，则跳出循环，否则执行遍历完成所有BPI，返回nullptr。

    ```cpp
    Page *ParallelBufferPoolManager::NewPgImp(page_id_t *page_id) {
      // create new page. We will request page allocation in a round robin manner from the underlying
      // BufferPoolManagerInstances
      // 1.   From a starting index of the BPMIs, call NewPageImpl until either 1) success and return 2) looped around to
      // starting index and return nullptr
      // 2.   Bump the starting index (mod number of instances) to start search at a different BPMI each time this function
      // is called
      Page* ret;
      size_t idx;
      for (size_t i = 0; i < num_instances_; i++)
      {
        idx = (next_instance_ + i) % num_instances_;
        ret = instances_[idx]->NewPage(page_id);
        if (ret != nullptr)
        {
          break;
        }
      }
      next_instance_ = (idx + 1) % num_instances_;
      // next_instance_ = (next_instance_ + 1) % num_instances_; 这里有个小争议，BPI的next_instance_究竟是在 idx 的基础上 + 1，还是在原有的基础上 + 1（二者都能通过测试）
      return ret;
    }
    ```

### GradeScope 测试

![image-20231206214421685](C:\Users\11246\AppData\Roaming\Typora\typora-user-images\image-20231206214421685.png)

![image-20231206214438033](C:\Users\11246\AppData\Roaming\Typora\typora-user-images\image-20231206214438033.png)