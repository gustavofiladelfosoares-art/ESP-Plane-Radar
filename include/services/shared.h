#pragma once

namespace services {

/**
 * One lock for everything the network task hands to the UI task (aircraft,
 * weather, route, map). Hold it only to copy data in or out.
 */
void sharedInit();

class SharedLock {
 public:
  SharedLock();
  ~SharedLock();
  SharedLock(const SharedLock&) = delete;
  SharedLock& operator=(const SharedLock&) = delete;
};

}  // namespace services
