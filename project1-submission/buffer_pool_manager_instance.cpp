//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager_instance.cpp
//
// Identification: src/buffer/buffer_pool_manager.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/buffer_pool_manager_instance.h"

#include "common/macros.h"

namespace bustub {

BufferPoolManagerInstance::BufferPoolManagerInstance(size_t pool_size, DiskManager *disk_manager,
                                                     LogManager *log_manager)
    : BufferPoolManagerInstance(pool_size, 1, 0, disk_manager, log_manager) {}

BufferPoolManagerInstance::BufferPoolManagerInstance(size_t pool_size, uint32_t num_instances, uint32_t instance_index,
                                                     DiskManager *disk_manager, LogManager *log_manager)
    : pool_size_(pool_size),
      num_instances_(num_instances),
      instance_index_(instance_index),
      next_page_id_(instance_index),
      disk_manager_(disk_manager),
      log_manager_(log_manager) {
  BUSTUB_ASSERT(num_instances > 0, "If BPI is not part of a pool, then the pool size should just be 1");
  BUSTUB_ASSERT(
      instance_index < num_instances,
      "BPI index cannot be greater than the number of BPIs in the pool. In non-parallel case, index should just be 1.");
  // We allocate a consecutive memory space for the buffer pool.
  pages_ = new Page[pool_size_];
  replacer_ = new LRUReplacer(pool_size);

  // Initially, every page is in the free list.
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<int>(i));
  }
}

BufferPoolManagerInstance::~BufferPoolManagerInstance() {
  delete[] pages_;
  delete replacer_;
}

bool BufferPoolManagerInstance::FlushPgImp(page_id_t page_id) { 
  // Make sure you call DiskManager::WritePage!

  std::lock_guard<std::mutex> guard(latch_);

  // 1 判断该页是否在 BFM 中
  if (page_table_.find(page_id) == page_table_.end())
  {
    return false;
  }

  frame_id_t frame_id_flush = page_table_[page_id];
  Page* page_flush = &pages_[frame_id_flush];

  /*
  // 2 判断该页是否为脏页
  if (page_flush->IsDirty())
  {
    /// 2.1 判断该页是否 Unpin
    if (page_flush->pin_count_ == 0)
    {
      disk_manager_->WritePage(page_id, page_flush->GetData());
      page_flush->is_dirty_ = false;
      return true;
    }
  }
  */
  disk_manager_->WritePage(page_id, page_flush->GetData());
  page_flush->is_dirty_ = false;
  return true;
}

void BufferPoolManagerInstance::FlushAllPgsImp() {
  // You can do it!
  std::lock_guard<std::mutex> guard(latch_);

  std::cout << "FlushALLPgs 中的页码为：";
  for(frame_id_t i = 0; i < static_cast<frame_id_t>(pool_size_);i++)
  {
    if (pages_[i].IsDirty())
    {
      if (pages_[i].pin_count_ == 0)
      {
        disk_manager_->WritePage(pages_[i].page_id_, pages_[i].GetData());
        pages_[i].is_dirty_ = false;
        std::cout <<pages_[i].page_id_ << " ";
      }
    }
  }
  std::cout << std::endl;
}

Page *BufferPoolManagerInstance::NewPgImp(page_id_t *page_id) {
  // 0.   Make sure you call AllocatePage!
  // 1.   If all the pages in the buffer pool are pinned, return nullptr.
  // 2.   Pick a victim page P from either the free list or the replacer. Always pick from the free list first.
  // 3.   Update P's metadata, zero out memory and add P to the page table.
  // 4.   Set the page ID output parameter. Return a pointer to P.
  
  // 0
  // AllocatePage 作用： Allocate a page on disk
  // 意思就是在磁盘上开辟一个新的空间

  std::lock_guard<std::mutex> guard(latch_);

  // 1 如果所有缓冲池的 page 都被 pin，则返回空指针
  bool all_pinned = true; // 定义 all_pinned 变量，表示缓冲池的页面是否都被 pin
  for (frame_id_t i = 0; i < static_cast<frame_id_t>(pool_size_); i++) // 遍历缓冲池的所有 pages_
  {
    if (pages_[i].pin_count_ == 0) // 如果有 page 没有被 pin
    {
      all_pinned = false;  // 则 all_pinned 为 false
      break;
    }
  }
  if (all_pinned == true)
  {
    return nullptr; // 否则，不能添加新 page，返回空指针
  }

  // 2 选择一个新 page，优先从 free_list_ 中选择，如果 free_list_ 为空，则在 LRU_replacer 中选择
  frame_id_t new_frame_id;
  
  if (free_list_.empty() == false) // 
  {
    new_frame_id = free_list_.back();
    free_list_.pop_back();
  }
  else 
  {
    bool has_victim = replacer_->Victim(& new_frame_id);
    if (has_victim == false)
    {
      return nullptr;
    }
  }
  // 2. 处理旧 page
  Page* old_page = &pages_[new_frame_id];
  if (old_page->IsDirty()) // 如果旧 page 是脏的，则需要写回磁盘（在磁盘中更新数据）
  {
    disk_manager_->WritePage(old_page->GetPageId(), old_page->GetData()); // 写回磁盘
    old_page->is_dirty_ = false;
  }
  page_table_.erase(old_page->GetPageId()); // 然后把旧 page 删除；

  // 3 通过 frame_id 寻找到物理页的物理地址，将新的 pageId 和内容写入内存
  // victim_page: 受害者物理页，从内存中获取

  /// 删除旧页的元数据
  page_id_t new_page_id = AllocatePage(); // 在磁盘中分配一个新的空间用来存放该页
  Page *victim_page = &pages_[new_frame_id]; // 将该页的对象指针指向索引 new_frame_id 的页对象（不会调用构造函数）
  victim_page->ResetMemory(); // 清空该页的数据为0

  // 设置新页的元数据
  victim_page->page_id_ = new_page_id; // 设置该页的 page_id 号
  victim_page->pin_count_ = 1; // pin 自增
  victim_page->is_dirty_ = false; // 初始化该页为非脏页

  // 加入 hash 表 ，pin 这一页，并设置指针地址
  page_table_[new_page_id] = new_frame_id; // 加入 page_table 这一 Hash 表中

  replacer_->Pin(new_frame_id); // 从 Replacer 中删除该页

  *page_id = new_page_id; // 并输出该页的 page_id
  return victim_page; // 返回该页的对象指针
}

Page *BufferPoolManagerInstance::FetchPgImp(page_id_t page_id) {

  // 1.     Search the page table for the requested page (P).
  // 1.1    If P exists, pin it and return it immediately.
  // 1.2    If P does not exist, find a replacement page (R) from either the free list or the replacer.
  //        Note that pages are always found from the free list first.
  // 2.     If R is dirty, write it back to the disk.
  // 3.     Delete R from the page table and insert P.
  // 4.     Update P's metadata, read in the page content from disk, and then return a pointer to P.
  
  std::lock_guard<std::mutex> guard(latch_);

  Page* page_find;

  // 1 先在 page table 中搜索该页
  auto pos = page_table_.find(page_id);
  /// 1.1 在 page table 中搜索到了
  if (pos != page_table_.end())
  {
    page_find = &pages_[pos->second];
    page_find -> pin_count_++;
    replacer_->Pin(pos->second); // 从 Replacer 中 Pin 掉！！
    page_find -> is_dirty_ = true; // 初始化该页为脏页，表示从内存中抓取该页，即该页变脏
    return page_find;
  } 
  /// 1.2 如果在 page_table 没有搜索到，就有三种情况
  //// 1.2.1 如果 BFM 中的所有页都被 pin 了，则返回 NULL
  bool all_pinned = true;
  for (frame_id_t i = 0; i < static_cast<frame_id_t>(pool_size_); i++)
  {
    if (pages_[i].pin_count_ == 0)
    {
      all_pinned = false;
      break;
    }
  }
  if (all_pinned == true)
  {
    return nullptr;
  }
  //// 1.2.2 如果 free list 有值，
  //// 则说明还有未被 BFM 用的 page，在 free list 弹出一个空闲的 frame_id
  frame_id_t frame_id_find;
  if (!free_list_.empty())
  {
    frame_id_find = free_list_.back();
    free_list_.pop_back();
  }
  //// 1.2.3 如果 free list 为空，
  //// 则说明需要在 LRU_replacer 中找出受害者的 frame_id，调用 Victim 函数
  else 
  {
    bool has_victim = replacer_->Victim(& frame_id_find);
    if (has_victim == false) // 如果没有受害者，即无法淘汰任何 page
    {
      return nullptr; // 则返回空
    }
  }
  // 2 判断要替换的页是不是脏页
  Page* old_page = &pages_[frame_id_find];
  if (old_page -> IsDirty())
  {
    disk_manager_->WritePage(old_page->GetPageId(), old_page->GetData());
    old_page->is_dirty_ = false;
  }
  /// 3.1 删除 frame_id_find 对应的 page_table_ 的一个映射
  page_table_.erase(old_page->GetPageId());

  /// 3.2 加入 hash 表，并在 Replacer 中 Pin
  page_find = &pages_[frame_id_find]; // page_find 对象指针指向 BFM 中 frame_id_find 索引的页空间
  page_table_[page_id] = frame_id_find; // 加入 page_table_ 的哈希表
  replacer_->Pin(frame_id_find); // 在 LRU 中固定该页

  // 4 更新页 page_id 的元数据
  disk_manager_->ReadPage(page_id, page_find->GetData());
  page_find -> pin_count_ = 1; // pin 自增
  page_find -> is_dirty_ = false; // 初始化该页为脏页
  page_find -> page_id_ = page_id;

  return page_find;
}

bool BufferPoolManagerInstance::DeletePgImp(page_id_t page_id) {
  // 0.   Make sure you call DeallocatePage!
  // 1.   Search the page table for the requested page (P).
  // 1.   If P does not exist, return true.
  // 2.   If P exists, but has a non-zero pin-count, return false. Someone is using the page.
  // 3.   Otherwise, P can be deleted. Remove P from the page table, reset its metadata and return it to the free list.
  
  std::lock_guard<std::mutex> guard(latch_);
  // 1. 如果 page_id 在 BFM 中不存在，则返回 true
  if (page_table_.find(page_id) == page_table_.end())
  {
    return true;
  }
  // 2 如果 page_id 在 BFM 中存在
  frame_id_t frame_id_delete = page_table_[page_id]; // 找到要删除页的 frame_id
  Page* page_delete = &pages_[frame_id_delete]; // 定义一个 Page 指针指向要删除的 Page 对象
  /// 2.1 如果其 pin_count 不等于 0，则返回 false，因为该页有线程/进程在使用
  if (page_delete->pin_count_ != 0)
  {
    return false;
  }
  /// 2.2 数据更新
  page_table_.erase(page_id); // 在 hash 表中删除
  free_list_.push_back(frame_id_delete); // 向 free_list_中添加要删除页的 frame_id

  /// 初始化 pages_ 数组那一页的所有元数据
  page_delete->pin_count_ = 0; 
  page_delete->is_dirty_ = false;
  page_delete->ResetMemory();

  replacer_->Pin(frame_id_delete); // 在 replacer 中删除 frame_id 对应的内存空间

  DeallocatePage(page_id); // 删除该页的内存空间

  return true;
}

bool BufferPoolManagerInstance::UnpinPgImp(page_id_t page_id, bool is_dirty) { 
  // 1. 首先，对该页的 pin_count 自减，赋予是否是脏页的属性；
  /// 1.1 如果该页的 pin_count > 0, 则不能删除该页，保留，返回 false;
  /// 1.2 如果该页的 pin_count = 0, 则需要将该页添加到 LRU 中，返回 true;

  std::lock_guard<std::mutex> guard(latch_);

  // 0 如果 BFM 中没有这个页，则返回 false
  if (page_table_.find(page_id) == page_table_.end())
  {
    return false;
  }
  
  // 1 获取当前页的 frame_id 
  frame_id_t frame_id = page_table_[page_id];
  Page* unpin_page = &pages_[frame_id];

  unpin_page -> is_dirty_ = is_dirty; // 设置该页的 dirty 属性

  if (unpin_page -> pin_count_ <= 0) // 如果该页已经 Unpin 了，则返回 false
  {
    return false;
  }

  unpin_page -> pin_count_--; // 否则 pin_count --

  if (unpin_page -> pin_count_ > 0) // 如果该页的 pin_count 依然大于 0, 直接返回 true，表示还有线程在使用
  {
    return true;
  }
  replacer_->Unpin(frame_id); // 否则，即 pin_count = 0，将该页添加在 replacer 中待删除，返回 true
  return true;
 }

page_id_t BufferPoolManagerInstance::AllocatePage() {
  const page_id_t next_page_id = next_page_id_;
  next_page_id_ += num_instances_;
  ValidatePageId(next_page_id);
  return next_page_id;
}

void BufferPoolManagerInstance::ValidatePageId(const page_id_t page_id) const {
  assert(page_id % num_instances_ == instance_index_);  // allocated pages mod back to this BPI
}

}  // namespace bustub
