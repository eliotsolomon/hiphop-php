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

#include <runtime/base/shared/thread_shared_variant.h>
#include <runtime/ext/ext_variable.h>
#include <runtime/ext/ext_apc.h>
#include <runtime/base/shared/shared_map.h>
#include <runtime/base/runtime_option.h>

using namespace std;

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

ThreadSharedVariant::ThreadSharedVariant(CVarRef source, bool serialized,
                                         bool inner /* = false */,
                                         bool unserializeObj /* = false */)
  : m_count (1), m_shouldCache(false), m_flags(0){
  ASSERT(!serialized || source.isString());

  m_type = source.getType();

  switch (m_type) {
  case KindOfBoolean:
    {
      m_data.num = source.toBoolean();
      break;
    }
  case KindOfByte:
  case KindOfInt16:
  case KindOfInt32:
  case KindOfInt64:
    {
      m_type = KindOfInt64;
      m_data.num = source.toInt64();
      break;
    }
  case KindOfDouble:
    {
      m_data.dbl = source.toDouble();
      break;
    }
  case KindOfStaticString:
  case KindOfString:
    {
      String s = source.toString();
      if (serialized) {
        m_type = KindOfObject;
        // It is priming, and there might not be the right class definitions
        // for unserialization.
        s = apc_reserialize(s);
      }
      m_data.str = s->copy(true);
      break;
    }
  case KindOfArray:
    {
      ArrayData *arr = source.getArrayData();

      if (!inner) {
        // only need to call hasInternalReference() on the toplevel array
        PointerSet seen;
        if (arr->hasInternalReference(seen)) {
          setSerializedArray();
          m_shouldCache = true;
          String s = apc_serialize(source);
          m_data.str = new StringData(s.data(), s.size(), CopyString);
          break;
        }
      }

      size_t size = arr->size();
      if (arr->isVectorData()) {
        setIsVector();
        m_data.vec = new VectorData(size);
        uint i = 0;
        for (ArrayIter it(arr); !it.end(); it.next(), i++) {
          ThreadSharedVariant* val = Create(it.secondRef(), false, true,
                                            unserializeObj);
          if (val->m_shouldCache) m_shouldCache = true;
          m_data.vec->vals[i] = val;
        }
      } else if (RuntimeOption::ApcUseGnuMap) {
        // Configured to use old Gnu Map
        m_data.gnuMap = new MapData(size);
        uint i = 0;
        for (ArrayIter it(arr); !it.end(); it.next(), i++) {
          ThreadSharedVariant* key = Create(it.first(), false, true,
                                            unserializeObj);
          ThreadSharedVariant* val = Create(it.secondRef(), false, true,
                                            unserializeObj);
          if (val->m_shouldCache) m_shouldCache = true;
          m_data.gnuMap->set(i, key, val);
        }
      } else {
        m_data.map = new ImmutableMap(size);
        for (ArrayIter it(arr); !it.end(); it.next()) {
          ThreadSharedVariant* key = Create(it.first(), false, true,
                                            unserializeObj);
          ThreadSharedVariant* val = Create(it.secondRef(), false, true,
                                            unserializeObj);
          if (val->m_shouldCache) m_shouldCache = true;
          m_data.map->add(key, val);
        }
      }
      break;
    }
  case KindOfNull:
    {
      m_data.num = 0;
      break;
    }
  default:
    {
      ASSERT(source.isObject());
      m_shouldCache = true;
      if (unserializeObj) {
        // This assumes hasInternalReference(seen, true) is false
        ImmutableObj* obj = new ImmutableObj(source.getObjectData());
        m_data.obj = obj;
        setIsObj();
      } else {
        String s = apc_serialize(source);
        m_data.str = new StringData(s.data(), s.size(), CopyString);
      }
      break;
    }
  }
}

Variant ThreadSharedVariant::toLocal() {
  switch (m_type) {
  case KindOfBoolean:
    {
      return (bool)m_data.num;
    }
  case KindOfInt64:
    {
      return m_data.num;
    }
  case KindOfDouble:
    {
      return m_data.dbl;
    }
  case KindOfStaticString:
    {
      return m_data.str;
    }
  case KindOfString:
    {
      return NEW(StringData)(this);
    }
  case KindOfArray:
    {
      if (getSerializedArray()) {
        return apc_unserialize(String(m_data.str->data(), m_data.str->size(),
                                      AttachLiteral));
      }
      return NEW(SharedMap)(this);
    }
  case KindOfNull:
    {
      return null_variant;
    }
  default:
    {
      ASSERT(m_type == KindOfObject);
      if (getIsObj()) {
        return m_data.obj->getObject();
      }
      return apc_unserialize(String(m_data.str->data(), m_data.str->size(),
                                    AttachLiteral));
    }
  }
}

void ThreadSharedVariant::dump(std::string &out) {
  out += "ref(";
  out += boost::lexical_cast<string>(m_count);
  out += ") ";
  switch (m_type) {
  case KindOfBoolean:
    out += "boolean: ";
    out += m_data.num ? "true" : "false";
    break;
  case KindOfInt64:
    out += "int: ";
    out += boost::lexical_cast<string>(m_data.num);
    break;
  case KindOfDouble:
    out += "double: ";
    out += boost::lexical_cast<string>(m_data.dbl);
    break;
  case KindOfStaticString:
  case KindOfString:
    out += "string(";
    out += boost::lexical_cast<string>(stringLength());
    out += "): ";
    out += stringData();
    break;
  case KindOfArray:
    if (getSerializedArray()) {
      out += "array: ";
      out += m_data.str->data();
    } else {
      SharedMap(this).dump(out);
    }
    break;
  case KindOfNull:
    out += "null";
    break;
  default:
    out += "object: ";
    out += m_data.str->data();
    break;
  }
  out += "\n";
}

ThreadSharedVariant::~ThreadSharedVariant() {
  switch (m_type) {
  case KindOfObject:
    if (getIsObj()) {
      delete m_data.obj;
      break;
    }
    // otherwise fall through
  case KindOfString:
    m_data.str->destruct();
    break;
  case KindOfArray:
    {
      if (getSerializedArray()) {
        m_data.str->destruct();
        break;
      }

      if (getIsVector()) {
        delete m_data.vec;
      } else if (RuntimeOption::ApcUseGnuMap) {
        delete m_data.gnuMap;
      } else {
        delete m_data.map;
      }
    }
    break;
  default:
    break;
  }
}

///////////////////////////////////////////////////////////////////////////////

const char *ThreadSharedVariant::stringData() const {
  ASSERT(is(KindOfString) || is(KindOfStaticString));
  return m_data.str->data();
}

size_t ThreadSharedVariant::stringLength() const {
  ASSERT(is(KindOfString) || is(KindOfStaticString));
  return m_data.str->size();
}

size_t ThreadSharedVariant::arrSize() const {
  ASSERT(is(KindOfArray));
  if (getIsVector()) return m_data.vec->size;
  if (RuntimeOption::ApcUseGnuMap) return m_data.gnuMap->size;
  return m_data.map->size();
}

int ThreadSharedVariant::getIndex(CVarRef key) {
  ASSERT(is(KindOfArray));
  switch (key.getType()) {
  case KindOfByte:
  case KindOfInt16:
  case KindOfInt32:
  case KindOfInt64: {
    int64 num = key.getNumData();
    if (getIsVector()) {
      if (num < 0 || (size_t) num >= m_data.vec->size) return -1;
      return num;
    }
    if (RuntimeOption::ApcUseGnuMap) {
      Int64ToIntMap::const_iterator it = m_data.gnuMap->intMap->find(num);
      if (it == m_data.gnuMap->intMap->end()) return -1;
      return it->second;
    }
    return m_data.map->indexOf(num);
  }
  case KindOfStaticString:
  case KindOfString: {
    if (getIsVector()) return -1;
    StringData *sd = key.getStringData();
    if (RuntimeOption::ApcUseGnuMap) {
      StringDataToIntMap::const_iterator it = m_data.gnuMap->strMap->find(sd);
      if (it == m_data.gnuMap->strMap->end()) return -1;
      return it->second;
    }
    return m_data.map->indexOf(sd);
  }
  default:
    // No other types are legitimate keys
    break;
  }
  return -1;
}

int ThreadSharedVariant::getIndex(CStrRef key) {
  ASSERT(is(KindOfArray));
  if (getIsVector()) return -1;
  StringData *sd = key.get();
  if (RuntimeOption::ApcUseGnuMap) {
    StringDataToIntMap::const_iterator it = m_data.gnuMap->strMap->find(sd);
    if (it == m_data.gnuMap->strMap->end()) return -1;
    return it->second;
  }
  return m_data.map->indexOf(sd);
}

int ThreadSharedVariant::getIndex(litstr key) {
  ASSERT(is(KindOfArray));
  if (getIsVector()) return -1;
  StringData sd(key);
  if (RuntimeOption::ApcUseGnuMap) {
    StringDataToIntMap::const_iterator it = m_data.gnuMap->strMap->find(&sd);
    if (it == m_data.gnuMap->strMap->end()) return -1;
    return it->second;
  }
  return m_data.map->indexOf(&sd);
}

int ThreadSharedVariant::getIndex(int64 key) {
  ASSERT(is(KindOfArray));
  if (getIsVector()) {
    if (key < 0 || (size_t) key >= m_data.vec->size) return -1;
    return key;
  }
  if (RuntimeOption::ApcUseGnuMap) {
    Int64ToIntMap::const_iterator it = m_data.gnuMap->intMap->find(key);
    if (it == m_data.gnuMap->intMap->end()) return -1;
    return it->second;
  }
  return m_data.map->indexOf(key);
}

Variant ThreadSharedVariant::getKey(ssize_t pos) const {
  ASSERT(is(KindOfArray));
  if (getIsVector()) {
    ASSERT(pos < (ssize_t) m_data.vec->size);
    return pos;
  }
  if (RuntimeOption::ApcUseGnuMap) {
    ASSERT(pos < (ssize_t) m_data.gnuMap->size);
    return m_data.gnuMap->keys[pos]->toLocal();
  }
  return m_data.map->getKeyIndex(pos)->toLocal();
}

SharedVariant* ThreadSharedVariant::getValue(ssize_t pos) const {
  ASSERT(is(KindOfArray));
  if (getIsVector()) {
    ASSERT(pos < (ssize_t) m_data.vec->size);
    return m_data.vec->vals[pos];
  }
  if (RuntimeOption::ApcUseGnuMap) {
    ASSERT(pos < (ssize_t) m_data.gnuMap->size);
    return m_data.gnuMap->vals[pos];
  }
  return m_data.map->getValIndex(pos);
}

void ThreadSharedVariant::loadElems(ArrayData *&elems,
                                    const SharedMap &sharedMap,
                                    bool keepRef /* = false */) {
  ASSERT(is(KindOfArray));
  uint count = arrSize();
  ArrayInit ai(count, getIsVector(), keepRef);
  for (uint i = 0; i < count; i++) {
    if (getIsVector()) {
      ai.add((int64)i, sharedMap.getValueRef(i), true);
    } else {
      if (RuntimeOption::ApcUseGnuMap) {
        ai.add(m_data.gnuMap->keys[i]->toLocal(), sharedMap.getValueRef(i),
               true);
      } else {
        ai.add(m_data.map->getKeyIndex(i)->toLocal(), sharedMap.getValueRef(i),
               true);
      }
    }
  }
  elems = ai.create();
  if (elems->isStatic()) elems = elems->copy();
}

ThreadSharedVariant *ThreadSharedVariant::Create
(CVarRef source, bool serialized, bool inner /* = false */,
 bool unserializeObj /* = false*/) {
  SharedVariant *wrapped = source.getSharedVariant();
  if (wrapped && !unserializeObj) {
    wrapped->incRef();
    // static cast should be enough
    return (ThreadSharedVariant *)wrapped;
  }
  return new ThreadSharedVariant(source, serialized, inner, unserializeObj);
}

SharedVariant* ThreadSharedVariant::convertObj(CVarRef var) {
  if (!var.is(KindOfObject) || getObjAttempted()) {
    return NULL;
  }
  setObjAttempted();
  PointerSet seen;
  ObjectData *obj = var.getObjectData();
  CArrRef arr = obj->o_toArray();
  if (arr->hasInternalReference(seen, true)) {
    return NULL;
  }
  ThreadSharedVariant *tmp = new ThreadSharedVariant(var, false, true, true);
  tmp->setObjAttempted();
  return tmp;
}

int32 ThreadSharedVariant::getSpaceUsage() {
  int32 size = sizeof(ThreadSharedVariant);
  if (!IS_REFCOUNTED_TYPE(m_type)) return size;
  switch (m_type) {
  case KindOfObject:
    if (getIsObj()) {
      return size + m_data.obj->getSpaceUsage();
    }
    // fall through
  case KindOfString:
    size += sizeof(StringData) + m_data.str->size();
    break;
  default:
    ASSERT(is(KindOfArray));
    if (getSerializedArray()) {
      size += sizeof(StringData) + m_data.str->size();
    } else if (getIsVector()) {
      size += sizeof(VectorData) +
              sizeof(ThreadSharedVariant*) * m_data.vec->size;
      for (size_t i = 0; i < m_data.vec->size; i++) {
        size += m_data.vec->vals[i]->getSpaceUsage();
      }
    } else if (RuntimeOption::ApcUseGnuMap) {
      // Not accurate
      size += sizeof(MapData);
    } else {
      ImmutableMap *map = m_data.map;
      size += map->getStructSize();
      for (int i = 0; i < map->size(); i++) {
        size += map->getKeyIndex(i)->getSpaceUsage();
        size += map->getValIndex(i)->getSpaceUsage();
      }
    }
    break;
  }
  return size;
}


void ThreadSharedVariant::getStats(SharedVariantStats *stats) {
  stats->initStats();
  stats->variantCount = 1;
  switch (m_type) {
  case KindOfNull:
  case KindOfBoolean:
  case KindOfInt64:
  case KindOfDouble:
  case KindOfStaticString:
    stats->dataSize = sizeof(m_data.dbl);
    stats->dataTotalSize = sizeof(ThreadSharedVariant);
    break;
  case KindOfObject:
    if (getIsObj()) {
      SharedVariantStats childStats;
      m_data.obj->getSizeStats(&childStats);
      stats->addChildStats(&childStats);
      break;
    }
    // fall through
  case KindOfString:
    stats->dataSize = m_data.str->size();
    stats->dataTotalSize = sizeof(ThreadSharedVariant) + sizeof(StringData) +
                           stats->dataSize;
    break;
  default:
    ASSERT(is(KindOfArray));
    if (getSerializedArray()) {
      stats->dataSize = m_data.str->size();
      stats->dataTotalSize = sizeof(ThreadSharedVariant) + sizeof(StringData) +
                             stats->dataSize;
      break;
    }
    if (getIsVector()) {
      stats->dataTotalSize = sizeof(ThreadSharedVariant) + sizeof(VectorData);
      stats->dataTotalSize += sizeof(ThreadSharedVariant*) * m_data.vec->size;
      for (size_t i = 0; i < m_data.vec->size; i++) {
        ThreadSharedVariant *v = m_data.vec->vals[i];
        SharedVariantStats childStats;
        v->getStats(&childStats);
        stats->addChildStats(&childStats);
      }
    } else if (RuntimeOption::ApcUseGnuMap) {
      // There is no way to calculate this accurately, and this should be only
      // used if ImmutableMap is seriously broken, when profiling is less
      // important. Just not do basic thing here:
      stats->dataTotalSize = sizeof(MapData);
    } else {
      ImmutableMap *map = m_data.map;
      stats->dataTotalSize = sizeof(ThreadSharedVariant) + map->getStructSize();
      for (int i = 0; i < map->size(); i++) {
        SharedVariantStats childStats;
        map->getKeyIndex(i)->getStats(&childStats);
        stats->addChildStats(&childStats);
        map->getValIndex(i)->getStats(&childStats);
        stats->addChildStats(&childStats);
      }
    }
    break;
  }
}

///////////////////////////////////////////////////////////////////////////////
}
