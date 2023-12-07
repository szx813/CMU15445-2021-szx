//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// parallel_buffer_pool_manager.cpp
//
// Identification: src/buffer/buffer_pool_manager.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/parallel_buffer_pool_manager.h"

namespace bustub {

ParallelBufferPoolManager::ParallelBufferPoolManager(size_t num_instances, size_t pool_size, DiskManager *disk_manager,
                                                     LogManager *log_manager)
    : num_instances_(num_instances), pool_size_(pool_size), disk_manager_(disk_manager), next_instance_(0) {
  // Allocate and create individual BufferPoolManagerInstances
  for (size_t i = 0; i < num_instances; i++) {
    BufferPoolManager *tmp = new BufferPoolManagerInstance(pool_size, num_instances, i, disk_manager, log_manager);
    instances_.push_back(tmp);
  }
}

// Update constructor to destruct all BufferPoolManagerInstances and deallocate any associated memory
ParallelBufferPoolManager::~ParallelBufferPoolManager() {
  for (size_t i = 0; i < num_instances_; i++) {
    delete (instances_[i]);
  }
};

size_t ParallelBufferPoolManager::GetPoolSize() {
  // Get size of all BufferPoolManagerInstances
  return num_instances_ * pool_size_;
}

BufferPoolManager *ParallelBufferPoolManager::GetBufferPoolManager(page_id_t page_id) {
  // Get BufferPoolManager responsible for handling given page id. You can use this method in your other methods.
  return instances_[page_id % num_instances_];
}

Page *ParallelBufferPoolManager::FetchPgImp(page_id_t page_id) {
  // Fetch page for page_id from responsible BufferPoolManagerInstance
  BufferPoolManager *instances_fetch = GetBufferPoolManager(page_id);
  Page *pages_fetch = instances_fetch->FetchPage(page_id);
  return pages_fetch;
}

bool ParallelBufferPoolManager::UnpinPgImp(page_id_t page_id, bool is_dirty) {
  // Unpin page_id from responsible BufferPoolManagerInstance
  BufferPoolManager *instances_unpin = GetBufferPoolManager(page_id);
  return instances_unpin->UnpinPage(page_id, is_dirty);
}

bool ParallelBufferPoolManager::FlushPgImp(page_id_t page_id) {
  // Flush page_id from responsible BufferPoolManagerInstance
  BufferPoolManager *instances_flush = GetBufferPoolManager(page_id);
  return instances_flush->FlushPage(page_id);
}

Page *ParallelBufferPoolManager::NewPgImp(page_id_t *page_id) {
  // create new page. We will request page allocation in a round robin manner from the underlying
  // BufferPoolManagerInstances
  // 1.   From a starting index of the BPMIs, call NewPageImpl until either 1) success and return 2) looped around to
  // starting index and return nullptr
  // 2.   Bump the starting index (mod number of instances) to start search at a different BPMI each time this function
  // is called
  Page *ret;
  size_t idx;
  for (size_t i = 0; i < num_instances_; i++) {
    idx = (next_instance_ + i) % num_instances_;
    ret = instances_[idx]->NewPage(page_id);
    if (ret != nullptr) {
      break;
    }
  }
  // 这里有个小争议，BPI的next_instance_究竟是在 idx 的基础上 + 1，还是在原有的基础上 + 1（二者都能通过测试）
  next_instance_ = (idx + 1) % num_instances_;
  // next_instance_ = (next_instance_ + 1) % num_instances_;
  return ret;
}

bool ParallelBufferPoolManager::DeletePgImp(page_id_t page_id) {
  // Delete page_id from responsible BufferPoolManagerInstance
  BufferPoolManager *instances_delete = GetBufferPoolManager(page_id);
  return instances_delete->DeletePage(page_id);
}

void ParallelBufferPoolManager::FlushAllPgsImp() {
  // flush all pages from all BufferPoolManagerInstances
  for (size_t i = 0; i < num_instances_; i++) {
    instances_[i]->FlushAllPages();
  }
}
}  // namespace bustub
