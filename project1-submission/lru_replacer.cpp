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
