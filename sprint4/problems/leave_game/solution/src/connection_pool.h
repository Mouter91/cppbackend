#pragma once

#include <pqxx/pqxx>
#include <memory>
#include <vector>
#include <stack>
#include <mutex>
#include <condition_variable>

class ConnectionPool {
  using PoolType = ConnectionPool;
  using ConnectionPtr = std::shared_ptr<pqxx::connection>;

 public:
  class ConnectionWrapper {
   public:
    ConnectionWrapper(ConnectionPtr&& conn, PoolType& pool, size_t idx) noexcept
        : conn_(std::move(conn)), pool_(&pool), idx_(idx) {
    }

    ConnectionWrapper(const ConnectionWrapper&) = delete;
    ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;

    ConnectionWrapper(ConnectionWrapper&&) = default;
    ConnectionWrapper& operator=(ConnectionWrapper&&) = default;

    pqxx::connection& operator*() const noexcept {
      return *conn_;
    }
    pqxx::connection* operator->() const noexcept {
      return conn_.get();
    }

    ~ConnectionWrapper() {
      if (conn_) {
        pool_->ReturnConnection(std::move(conn_), idx_);
      }
    }

   private:
    ConnectionPtr conn_;
    PoolType* pool_;
    size_t idx_;
  };

  template <typename ConnectionFactory>
  explicit ConnectionPool(size_t capacity, ConnectionFactory&& connection_factory) {
    pool_.reserve(capacity);
    for (size_t i = 0; i < capacity; ++i) {
      pool_.emplace_back(connection_factory());
      free_indices_.push(i);
    }
  }

  ConnectionWrapper GetConnection() {
    std::unique_lock lock{mutex_};
    cond_var_.wait(lock, [this] { return !free_indices_.empty(); });
    size_t idx = free_indices_.top();
    free_indices_.pop();
    return {std::move(pool_[idx]), *this, idx};
  }

 private:
  void ReturnConnection(ConnectionPtr&& conn, size_t idx) {
    std::lock_guard lock{mutex_};
    pool_[idx] = std::move(conn);
    free_indices_.push(idx);
    cond_var_.notify_one();
  }

  std::mutex mutex_;
  std::condition_variable cond_var_;
  std::vector<ConnectionPtr> pool_;
  std::stack<size_t> free_indices_;
};
