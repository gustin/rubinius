#include "builtin/lookuptable.hpp"
#include "builtin/array.hpp"
#include "builtin/class.hpp"
#include "builtin/fixnum.hpp"
#include "builtin/symbol.hpp"
#include "builtin/tuple.hpp"
#include "objectmemory.hpp"
#include "builtin/string.hpp"

#define LOOKUPTABLE_MAX_DENSITY 0.75
#define LOOKUPTABLE_MIN_DENSITY 0.3

#define key_hash(obj) (((unsigned int)(uintptr_t)obj) >> 2)
#define find_bin(hash, bins) (hash & ((bins) - 1))
#define max_density_p(ents,bins) (ents >= LOOKUPTABLE_MAX_DENSITY * bins)
#define min_density_p(ents,bins) (ents < LOOKUPTABLE_MIN_DENSITY * bins)
#define key_to_sym(key) \
  if(kind_of<String>(key)) { \
    key = ((String*)key)->to_sym(state); \
  } \


namespace rubinius {
  LookupTable* LookupTable::create(STATE, size_t size) {
    LookupTable *tbl;

    tbl = (LookupTable*)state->om->new_object(G(lookuptable),
                                              LookupTable::fields);
    tbl->setup(state, size);

    return tbl;
  }

  void LookupTable::setup(STATE, size_t sz = 0) {
    if(!sz) sz = LOOKUPTABLE_MIN_SIZE;
    SET(this, values, Tuple::create(state, sz));
    SET(this, bins, Object::i2n(sz));
    SET(this, entries, Object::i2n(0));
  }

  /* The LookupTable.allocate primitive. */
  LookupTable* LookupTable::allocate(STATE) {
    return LookupTable::create(state, LOOKUPTABLE_MIN_SIZE);
  }

  LookupTable* LookupTable::dup(STATE) {
    size_t size, i;
    LookupTable *dup;

    size = bins->n2i();
    dup = (LookupTable*)LookupTable::create(state, size);
    state->om->set_class(dup, class_object(state));
    size_t num = entries->n2i();

    Array* entries = all_entries(state);
    for(i = 0; i < num; i++) {
      Tuple* entry = as<Tuple>(entries->get(state, i));
      OBJECT key =   entry->at(0);
      OBJECT value = entry->at(1);
      dup->store(state, key, value);
    }
    return dup;

  }

  OBJECT LookupTable::entry_new(STATE, OBJECT key, OBJECT data) {
    Tuple* tup = Tuple::create(state, 3);
    tup->put(state, 0, key);
    tup->put(state, 1, data);
    tup->put(state, 2, Qnil);
    return tup;
  }

  OBJECT LookupTable::entry_append(STATE, Tuple* top, OBJECT nxt) {
    Tuple* cur = try_as<Tuple>(top->at(2));
    Tuple* last = top;

    while(cur) {
      last = cur;
      cur = try_as<Tuple>(cur->at(2));
    }

    last->put(state, 2, nxt);
    return nxt;
  }

  void LookupTable::redistribute(STATE, size_t size) {
    size_t num = bins->n2i();
    Tuple* new_values = Tuple::create(state, size);

    for(size_t i = 0; i < num; i++) {
      Tuple* entry = (Tuple*)values->at(i);

      while(!entry->nil_p()) {
        Tuple* next = (Tuple*)entry->at(2);
        entry->put(state, 2, Qnil);

        size_t bin = find_bin(key_hash(entry->at(0)), size);
        Tuple* slot = try_as<Tuple>(new_values->at(bin));

        if(!slot) {
          new_values->put(state, bin, entry);
        } else {
          entry_append(state, slot, entry);
        }

        entry = next;
      }
    }

    SET(this, values, new_values);
    bins = Object::i2n(size);
  }

  OBJECT LookupTable::store(STATE, OBJECT key, OBJECT val) {
    unsigned int num_entries, num_bins, bin;
    OBJECT new_ent;
    Tuple* cur;
    Tuple* entry;

    key_to_sym(key);
    num_entries = entries->n2i();
    num_bins = bins->n2i();

    if(max_density_p(num_entries, num_bins)) {
      redistribute(state, num_bins <<= 1);
    }

    bin = find_bin(key_hash(key), num_bins);
    cur = entry = (Tuple*)values->at(bin);

    while(!entry->nil_p()) {
      if(entry->at(0) == key) {
        entry->put(state, 1, val);
        return val;
      }
      cur = entry;
      entry = (Tuple*)entry->at(2);
    }

    new_ent = entry_new(state, key, val);
    if(NIL_P(cur)) {
      values->put(state, bin, new_ent);
    } else {
      cur->put(state, 2, new_ent);
    }

    entries = Object::i2n(num_entries + 1);
    return val;
  }

  Tuple* LookupTable::find_entry(STATE, OBJECT key) {
    unsigned int bin;
    Tuple* entry;

    key_to_sym(key);
    bin = find_bin(key_hash(key), bins->n2i());
    entry = try_as<Tuple>(values->at(bin));

    while(entry) {
      if(entry->at(0) == key) {
        return entry;
      }
      entry = try_as<Tuple>(entry->at(2));
    }
    return NULL;
  }

  OBJECT LookupTable::fetch(STATE, OBJECT key) {
    Tuple* entry = find_entry(state, key);
    if(entry) return entry->at(1);
    return Qnil;
  }

  OBJECT LookupTable::fetch(STATE, OBJECT key, bool* found) {
    Tuple* entry = find_entry(state, key);
    if(entry) {
      *found = true;
      return entry->at(1);
    }

    *found = false;
    return Qnil;
  }

  /* lookuptable_find returns Qundef if there is not entry
   * referenced by 'key' in the LookupTable. This is useful
   * to distinguish x = {} from x = {:a => nil} and is used
   * in cpu.c in e.g. cpu_const_get_in_context.
   */
  OBJECT LookupTable::find(STATE, OBJECT key) {
    Tuple* entry = find_entry(state, key);
    if(entry) {
      return entry->at(1);
    }
    return Qundef;
  }

  OBJECT LookupTable::remove(STATE, OBJECT key) {
    hashval bin;
    OBJECT val;
    Tuple* entry;
    Tuple* lst;
    Tuple* lk;

    key_to_sym(key);

    size_t num_entries = entries->n2i();
    size_t num_bins = bins->n2i();

    if(min_density_p(num_entries, num_bins) && (num_bins >> 1) >= LOOKUPTABLE_MIN_SIZE) {
      redistribute(state, num_bins >>= 1);
    }

    bin = find_bin(key_hash(key), num_bins);
    entry = (Tuple*)values->at(bin);

    lst = (Tuple*)Qnil;

    while(!entry->nil_p()) {
      lk = (Tuple*)entry->at(2);

      if(entry->at(0) == key) {
        val = entry->at(1);
        if(lst->nil_p()) {
          values->put(state, bin, lk);
        } else {
          lst->put(state, 2, lk);
        }
        entries = Object::i2n(entries->n2i() - 1);
        return val;
      }

      lst = entry;
      entry = lk;
    }

    return Qnil;
  }

  OBJECT LookupTable::has_key(STATE, OBJECT key) {
    Tuple* entry = find_entry(state, key);

    if(entry) return Qtrue;
    return Qfalse;
  }

  Array* LookupTable::collect(STATE, LookupTable* tbl, OBJECT (*action)(STATE, Tuple*)) {
    size_t i, j;
    Tuple* values;
    Tuple* entry;

    Array* ary = Array::create(state, tbl->entries->n2i());
    size_t num_bins = tbl->bins->n2i();
    values = tbl->values;

    for(i = j = 0; i < num_bins; i++) {
      entry = try_as<Tuple>(values->at(i));

      while(entry) {
        ary->set(state, j++, action(state, entry));
        entry = try_as<Tuple>(entry->at(2));
      }
    }
    return ary;
  }

  OBJECT LookupTable::get_key(STATE, Tuple* entry) {
    return entry->at(0);
  }

  Array* LookupTable::all_keys(STATE) {
    return collect(state, this, get_key);
  }

  OBJECT LookupTable::get_value(STATE, Tuple* entry) {
    return entry->at(1);
  }

  Array* LookupTable::all_values(STATE) {
    return collect(state, this, get_value);
  }

  OBJECT LookupTable::get_entry(STATE, Tuple* entry) {
    return entry;
  }

  Array* LookupTable::all_entries(STATE) {
    return collect(state, this, get_entry);
  }

}