#pragma once

namespace wnet {

class noncopyable {
  protected:
    noncopyable() {}
    ~noncopyable() {}

    noncopyable( const noncopyable& ) = delete;
    const noncopyable& operator=( const noncopyable& ) = delete;
};

}