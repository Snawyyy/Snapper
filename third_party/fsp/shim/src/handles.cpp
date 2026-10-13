// Memory handles (PF_Handle): a pointer to a pointer to the block, so a
// plugin can read the block with *handle as After Effects allows.

#include <cassert>
#include <memory>
#include <vector>

#include "fsp_shim/host_api.h"

namespace {

// The handle points at `data`, the first member, so a handle converts
// back to its block. The handle owns the block until disposed.
struct HandleBlock {
  void* data = nullptr;
  std::vector<unsigned char> bytes;
};

HandleBlock* BlockOf(PF_Handle handle) {
  assert(handle != nullptr);
  return reinterpret_cast<HandleBlock*>(handle);
}

// Keeps data pointing at the bytes (never null, even when empty).
void Resize(HandleBlock* block, A_u_long size) {
  assert(block != nullptr);
  block->bytes.resize(static_cast<std::size_t>(size) + 1, 0);
  block->data = block->bytes.data();
}

}  // namespace

PF_Handle fsp_shim_new_handle(A_u_long size) {
  auto block = std::make_unique<HandleBlock>();
  Resize(block.get(), size);
  return &block.release()->data;
}

void* fsp_shim_lock_handle(PF_Handle handle) {
  const bool is_null = handle == nullptr;
  return is_null ? nullptr : *handle;
}

void fsp_shim_unlock_handle(PF_Handle handle) { (void)handle; }

void fsp_shim_dispose_handle(PF_Handle handle) {
  const bool is_null = handle == nullptr;
  if (is_null) {
    return;
  }
  const std::unique_ptr<HandleBlock> block(BlockOf(handle));
}

A_u_long fsp_shim_handle_size(PF_Handle handle) {
  const bool is_null = handle == nullptr;
  if (is_null) {
    return 0;
  }
  return static_cast<A_u_long>(BlockOf(handle)->bytes.size() - 1);
}

PF_Err fsp_shim_resize_handle(A_u_long size, PF_Handle* handle) {
  const bool is_bad = handle == nullptr || *handle == nullptr;
  if (is_bad) {
    return fsp_shim_fail("resize of a null handle");
  }
  Resize(BlockOf(*handle), size);
  return PF_Err_NONE;
}
