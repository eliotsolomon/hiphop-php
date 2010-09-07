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
// @generated by HipHop Compiler

#ifndef __GENERATED_cls_SplObjectStorage_h__
#define __GENERATED_cls_SplObjectStorage_h__

#include <cls/Iterator.h>
#include <cls/Countable.h>

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

/* SRC: classes/splobjectstorage.php line 12 */
class c_SplObjectStorage : public ExtObjectData {
  public:

  // Properties
  Variant m_storage;
  int64 m_index;

  // Class Map
  BEGIN_CLASS_MAP(SplObjectStorage)
    PARENT_CLASS(Countable)
    PARENT_CLASS(Iterator)
    PARENT_CLASS(Traversable)
  END_CLASS_MAP(SplObjectStorage)
  DECLARE_CLASS_COMMON(SplObjectStorage, SplObjectStorage)
  DECLARE_INVOKE_EX(SplObjectStorage, SplObjectStorage, ObjectData)

  // DECLARE_STATIC_PROP_OPS
  public:
  static Variant os_getInit(CStrRef s);
  #define OMIT_JUMP_TABLE_CLASS_STATIC_GET_SplObjectStorage 1
  #define OMIT_JUMP_TABLE_CLASS_STATIC_LVAL_SplObjectStorage 1
  #define OMIT_JUMP_TABLE_CLASS_CONSTANT_SplObjectStorage 1

  // DECLARE_INSTANCE_PROP_OPS
  public:
  virtual void o_getArray(Array &props) const;
  virtual void o_setArray(CArrRef props);
  virtual Variant *o_realProp(CStrRef s, int flags,
                              CStrRef context = null_string) const;
  Variant *o_realPropPrivate(CStrRef s, int flags) const;
  virtual Variant o_get(CStrRef s, bool error = true,
                        CStrRef context = null_string);
  Variant o_getPrivate(CStrRef s, bool error = true);
  virtual Variant o_set(CStrRef s, CVarRef v, bool forInit = false,
                        CStrRef context = null_string);
  Variant o_setPrivate(CStrRef s, CVarRef v, bool forInit);
  virtual Variant &o_lval(CStrRef s, CStrRef context = null_string);
  Variant &o_lvalPrivate(CStrRef s);

  // DECLARE_INSTANCE_PUBLIC_PROP_OPS
  public:
  #define OMIT_JUMP_TABLE_CLASS_realProp_PUBLIC_SplObjectStorage 1
  #define OMIT_JUMP_TABLE_CLASS_get_PUBLIC_SplObjectStorage 1
  #define OMIT_JUMP_TABLE_CLASS_set_PUBLIC_SplObjectStorage 1
  #define OMIT_JUMP_TABLE_CLASS_lval_PUBLIC_SplObjectStorage 1

  // DECLARE_COMMON_INVOKE
  #define OMIT_JUMP_TABLE_CLASS_STATIC_INVOKE_SplObjectStorage 1
  virtual Variant o_invoke(MethodIndex methodIndex, const char *s, CArrRef ps,
                           int64 h, bool f = true);
  virtual Variant o_invoke_few_args(MethodIndex methodIndex, const char *s,
                                    int64 h, int count,
                                    INVOKE_FEW_ARGS_DECL_ARGS);

  public:
  DECLARE_INVOKES_FROM_EVAL
  void init();
  public: void t_rewind();
  public: bool t_valid();
  public: int64 t_key();
  public: Variant t_current();
  public: void t_next();
  public: int t_count();
  public: bool t_contains(CVarRef v_obj);
  public: void t_attach(CVarRef v_obj);
  public: void t_detach(CVarRef v_obj);
};
extern struct ObjectStaticCallbacks cw_SplObjectStorage;

///////////////////////////////////////////////////////////////////////////////
}

#endif // __GENERATED_cls_SplObjectStorage_h__