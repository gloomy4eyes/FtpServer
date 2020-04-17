#pragma once
#include <memory>

namespace swf {
class FileBlob {
  // std::vector<char> _data;
  std::unique_ptr<char> _dataPtr;
  size_t _size;

public:
  FileBlob(char* data, size_t size) : _dataPtr(data), _size(size) {}
 ~FileBlob() {}

  char* data() const { return _dataPtr.get(); }
  size_t size() const { return _size; }

  FileBlob(const FileBlob& other) = delete;

  FileBlob(FileBlob&& other) noexcept
    : _dataPtr(std::move(other._dataPtr)),
      _size(other._size) {}

  FileBlob& operator=(const FileBlob& other) = delete;

  FileBlob& operator=(FileBlob&& other) noexcept {
    if (this == &other)
      return *this;
    _dataPtr = std::move(other._dataPtr);
    _size = other._size;
    return *this;
  }

  void setData(const char* data, size_t size) {
    if (data != nullptr && size > 0) {
      _dataPtr.reset(new char[size]());
      memcpy(_dataPtr.get(), data, size);
      _size = size;
    }
  }
};
}
