#include "vm.hpp"
#include "builtin/list.hpp"
#include "builtin/fixnum.hpp"
#include "objectmemory.hpp"

#include <cxxtest/TestSuite.h>

using namespace rubinius;

class TestList : public CxxTest::TestSuite {
  public:

  VM* state;
  Object* one;
  Object* two;

  void setUp() {
    state = new VM(1024);
    one = Fixnum::from(1);
    two = Fixnum::from(2);
  }

  void tearDown() {
    delete state;
  }

  void test_classes() {
    TS_ASSERT(G(list)->kind_of_p(state, G(klass)));
    TS_ASSERT(G(list_node)->kind_of_p(state, G(klass)));
  }

  void test_create() {
    List* lst = List::create(state);
    TS_ASSERT(lst->kind_of_p(state, G(list)));
  }

  void test_append() {
    List* lst = List::create(state);
    TS_ASSERT_EQUALS(lst->size(), (size_t)0);
    lst->append(state, one);
    TS_ASSERT_EQUALS(lst->locate(state, 0), one);
    TS_ASSERT_EQUALS(lst->size(), (size_t)1);

    lst->append(state, one);
    TS_ASSERT_EQUALS(lst->locate(state, 1), one);
    TS_ASSERT_EQUALS(lst->size(), (size_t)2);
    
    lst->append(state, two);
    TS_ASSERT_EQUALS(lst->locate(state, 2), two);
    TS_ASSERT_EQUALS(lst->size(), (size_t)3);
  }

  void test_shift() {
    List* lst = List::create(state);
    lst->append(state, one);
    lst->append(state, two);

    TS_ASSERT_EQUALS(lst->shift(state), one);
    TS_ASSERT_EQUALS(lst->size(), (size_t)1);
    
    TS_ASSERT_EQUALS(lst->shift(state), two);
    TS_ASSERT_EQUALS(lst->size(), (size_t)0);
  }

  void test_remove() {
    List* lst = List::create(state);
    lst->append(state, one);
    lst->append(state, two);
    lst->append(state, one);
    lst->append(state, two);

    TS_ASSERT_EQUALS(lst->remove(state, one), (size_t)2);
    TS_ASSERT_EQUALS(lst->size(), (size_t)2);
    
    TS_ASSERT_EQUALS(lst->remove(state, two), (size_t)2);
    TS_ASSERT_EQUALS(lst->size(), (size_t)0);
  }

};
