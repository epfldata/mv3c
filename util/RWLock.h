#pragma once
#ifndef __COMMON_RW_LOCK_H__
#define __COMMON_RW_LOCK_H__

#include <cassert>
#include "SpinLock.h"

class RWLock {
	enum LockType : size_t{ NO_LOCK, READ_LOCK, WRITE_LOCK };
public:
	RWLock() : lock_type_(0), reader_count_(0) {}

	void AcquireReadLock() {
		while (1) {
			while (lock_type_ == WRITE_LOCK);
			spinlock_.lock();
			if (lock_type_ == WRITE_LOCK) {
				spinlock_.unlock();
			}
			else {
				if (lock_type_ == NO_LOCK) {
					lock_type_ = READ_LOCK;
					++reader_count_;
				}
				else {
					assert(lock_type_ == READ_LOCK);
					++reader_count_;
				}
				spinlock_.unlock();
				return;
			}
		}
	}

	bool TryReadLock() {
		bool rt = false;
		spinlock_.lock();
		if (lock_type_ == NO_LOCK) {
			lock_type_ = READ_LOCK;
			++reader_count_;
			rt = true;
		}
		else if (lock_type_ == READ_LOCK) {
			++reader_count_;
			rt = true;
		}
		else {
			rt = false;
		}
		spinlock_.unlock();
		return rt;
	}

	void AcquireWriteLock() {
		while (1) {
			while (lock_type_ != NO_LOCK);
			spinlock_.lock();
			if (lock_type_ != NO_LOCK) {
				spinlock_.unlock();
			}
			else {
				assert(lock_type_ == NO_LOCK);
				lock_type_ = WRITE_LOCK;
				spinlock_.unlock();
				return;
			}
		}
	}

	bool TryWriteLock() {
		bool rt = false;
		spinlock_.lock();
		if (lock_type_ == NO_LOCK) {
			lock_type_ = WRITE_LOCK;
			rt = true;
		}
		else {
			rt = false;
		}
		spinlock_.unlock();
		return rt;
	}

	void ReleaseReadLock() {
		spinlock_.lock();
		--reader_count_;
		if (reader_count_ == 0) {
			lock_type_ = NO_LOCK;
		}
		spinlock_.unlock();
	}

	void ReleaseWriteLock() {
		spinlock_.lock();
		lock_type_ = NO_LOCK;
		spinlock_.unlock();
	}

	bool ExistsWriteLock() const {
		return (lock_type_ == WRITE_LOCK);
	}

private:
	SpinLock spinlock_;
        volatile size_t lock_type_;
	volatile size_t reader_count_;
	
};

#endif
