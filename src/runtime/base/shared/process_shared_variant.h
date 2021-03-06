/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010 Facebook, Inc. (http://www.facebook.com)          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#ifndef __HPHP_PROCESS_SHARED_VARIANT_H__
#define __HPHP_PROCESS_SHARED_VARIANT_H__

#include <runtime/base/types.h>
#include <runtime/base/complex_types.h>
#include <util/shared_memory_allocator.h>
#include <runtime/base/memory/unsafe_pointer.h>
#include <runtime/base/shared/shared_variant.h>

#if (defined(__APPLE__) || defined(__APPLE_CC__)) && (defined(__BIG_ENDIAN__) || defined(__LITTLE_ENDIAN__))
# if defined(__LITTLE_ENDIAN__)
#  undef WORDS_BIGENDIAN
# else
#  if defined(__BIG_ENDIAN__)
#   define WORDS_BIGENDIAN
#  endif
# endif
#endif

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

typedef boost::interprocess::interprocess_mutex ProcessSharedVariantLock;

/**
 * We could directly use boost::interprocess::offset_ptr<SharedVariant> as
 * SharedVariantOffPtr, but we ran into a compilation error with boost-1.41+.
 * This one level of indirection helped.
 */
class SharedVariantOffPtr {
public:
  SharedVariantOffPtr(SharedVariant *sv) : p(sv) {}
  boost::interprocess::offset_ptr<SharedVariant> p;
};
class SharedVariantOffPtrLess {
public:
  bool operator()(const SharedVariantOffPtr &x,
                  const SharedVariantOffPtr &y) const {
    if (x.p && y.p) {
      return *x.p < *y.p;
    }
    return x.p < y.p;
  }
};

typedef SharedMemoryMapWithComp
<SharedVariantOffPtr, int, SharedVariantOffPtrLess>
ProcessSharedVariantToIntMap;

class ProcessSharedVariantMapData {
public:
  ProcessSharedVariantToIntMap* map;
  SharedMemoryVector<SharedVariant*>* keys;
  SharedMemoryVector<SharedVariant*>* vals;
};

class ProcessSharedVariant : public SharedVariant {
 public:
  ProcessSharedVariant(CVarRef source, ProcessSharedVariantLock* lock);
  ProcessSharedVariant(SharedMemoryString& source);
  virtual ~ProcessSharedVariant();

  bool is(DataType d) const { return m_type == d; }
  virtual DataType getType() const { return (DataType)m_type;}
  virtual CVarRef asCVarRef() const {
    // Must be non-refcounted types
    ASSERT(m_shouldCache == false);
    ASSERT(m_flags == 0);
    ASSERT(!IS_REFCOUNTED_TYPE(m_tv.m_type));
    return tvAsCVarRef(&m_tv);
  }

  virtual void incRef();
  virtual void decRef();

  virtual Variant toLocal();
  virtual bool operator<(const SharedVariant& svother) const;

  virtual const char* stringData() const {
    ASSERT(is(KindOfString));
    return getString()->c_str();
  }
  virtual size_t stringLength() const {
    ASSERT(is(KindOfString));
    return getString()->size();
  }

  virtual size_t arrSize() const {
    return map().size();
  }

  virtual int getIndex(CVarRef key);
  virtual int getIndex(CStrRef key) {
    return getIndex(Variant(key));
  }
  virtual int getIndex(litstr key) {
    return getIndex(Variant(key));
  }
  virtual int getIndex(int64 key) {
    return getIndex(Variant(key));
  }

  virtual void loadElems(ArrayData *&elems, const SharedMap &sharedMap,
                         bool keepRef = false);

  virtual Variant getKey(ssize_t pos) const;
  virtual SharedVariant* getValue(ssize_t pos) const;

  SharedMemoryString* getString() const {
    return getPtr(m_data.str);
  }

  // implementing LeakDetectable
  virtual void dump(std::string &out);

  virtual bool shouldCache() const { return m_shouldCache; }

 protected:

  /* See comments in thread_shared_variant.h */
#ifdef WORDS_BIGENDIAN
 #define SharedVarData \
  union {\
    int64 num;\
    double dbl;\
    SharedMemoryString* str;\
    ProcessSharedVariantMapData* map;\
  } m_data;\
  int m_count;\
  bool m_shouldCache;\
  uint8 m_flags;\
  uint16 m_type

#else
 #define SharedVarData \
  union {\
    int64 num;\
    double dbl;\
    SharedMemoryString* str;\
    ProcessSharedVariantMapData* map;\
  } m_data;\
  int m_count;\
  uint16 m_type;\
  bool m_shouldCache;\
  uint8 m_flags

#endif

  struct SharedVar {
    SharedVarData;
  };

  union {
    struct {
      SharedVarData;
    };
    TypedValue m_tv;
  };
#undef SharedVarData

  ProcessSharedVariantLock* m_lock;

  static void compileTimeAssertions() {
    CT_ASSERT(offsetof(SharedVar, m_data) == offsetof(TypedValue, m_data));
    CT_ASSERT(offsetof(SharedVar, m_count) == offsetof(TypedValue, _count));
    CT_ASSERT(offsetof(SharedVar, m_type) == offsetof(TypedValue, m_type));
  }

  bool getSerializedArray() const { return (bool)(m_flags & SerializedArray);}
  void setSerializedArray() { m_flags |= SerializedArray;}
  void clearSerializedArray() { m_flags &= ~SerializedArray;}

  template<class T>
  T getPtr(T p) const {
    return (T)((size_t)p+(size_t)this);
  }
  template<class T>
  T putPtr(T p) const {
    return (T)((size_t)p-(size_t)this);
  }

  virtual SharedVariant* getKeySV(ssize_t pos) const;

  ProcessSharedVariantMapData* getMapData() const {
    return getPtr(m_data.map);
  }
  ProcessSharedVariantLock* getLock() const {
    return getPtr(m_lock);
  }

  const ProcessSharedVariantToIntMap& map() const {
    return *getPtr(getMapData()->map);
  }

  const SharedMemoryVector<SharedVariant*>& keys() const {
    ASSERT(is(KindOfArray));
    return *getPtr(getMapData()->keys);
  }
  const SharedMemoryVector<SharedVariant*>& vals() const {
    ASSERT(is(KindOfArray));
    return *getPtr(getMapData()->vals);
  }
  ProcessSharedVariantToIntMap::const_iterator lookup(CVarRef key);
};


///////////////////////////////////////////////////////////////////////////////
}

#endif /* __HPHP_PROCESS_SHARED_VARIANT_H__ */
