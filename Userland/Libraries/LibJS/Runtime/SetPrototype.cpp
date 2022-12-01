/*
 * Copyright (c) 2021-2022, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/HashTable.h>
#include <AK/TypeCasts.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/FunctionObject.h>
#include <LibJS/Runtime/IteratorOperations.h>
#include <LibJS/Runtime/SetIterator.h>
#include <LibJS/Runtime/SetPrototype.h>

namespace JS {

SetPrototype::SetPrototype(Realm& realm)
    : PrototypeObject(*realm.intrinsics().object_prototype())
{
}

void SetPrototype::initialize(Realm& realm)
{
    auto& vm = this->vm();
    Object::initialize(realm);
    u8 attr = Attribute::Writable | Attribute::Configurable;

    define_native_function(realm, vm.names.add, add, 1, attr);
    define_native_function(realm, vm.names.clear, clear, 0, attr);
    define_native_function(realm, vm.names.delete_, delete_, 1, attr);
    define_native_function(realm, vm.names.entries, entries, 0, attr);
    define_native_function(realm, vm.names.forEach, for_each, 1, attr);
    define_native_function(realm, vm.names.has, has, 1, attr);
    define_native_function(realm, vm.names.values, values, 0, attr);
    define_native_accessor(realm, vm.names.size, size_getter, {}, Attribute::Configurable);

    define_direct_property(vm.names.keys, get_without_side_effects(vm.names.values), attr);

    // 24.2.3.11 Set.prototype [ @@iterator ] ( ), https://tc39.es/ecma262/#sec-set.prototype-@@iterator
    define_direct_property(*vm.well_known_symbol_iterator(), get_without_side_effects(vm.names.values), attr);

    // 24.2.3.12 Set.prototype [ @@toStringTag ], https://tc39.es/ecma262/#sec-set.prototype-@@tostringtag
    define_direct_property(*vm.well_known_symbol_to_string_tag(), js_string(vm, vm.names.Set.as_string()), Attribute::Configurable);
}

// 24.2.3.1 Set.prototype.add ( value ), https://tc39.es/ecma262/#sec-set.prototype.add
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::add)
{
    auto* set = TRY(typed_this_object(vm));
    auto value = vm.argument(0);
    if (value.is_negative_zero())
        value = Value(0);
    set->set_add(value);
    return set;
}

// 24.2.3.2 Set.prototype.clear ( ), https://tc39.es/ecma262/#sec-set.prototype.clear
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::clear)
{
    auto* set = TRY(typed_this_object(vm));
    set->set_clear();
    return js_undefined();
}

// 24.2.3.4 Set.prototype.delete ( value ), https://tc39.es/ecma262/#sec-set.prototype.delete
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::delete_)
{
    auto* set = TRY(typed_this_object(vm));
    return Value(set->set_remove(vm.argument(0)));
}

// 24.2.3.5 Set.prototype.entries ( ), https://tc39.es/ecma262/#sec-set.prototype.entries
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::entries)
{
    auto& realm = *vm.current_realm();

    auto* set = TRY(typed_this_object(vm));

    return SetIterator::create(realm, *set, Object::PropertyKind::KeyAndValue);
}

// 24.2.3.6 Set.prototype.forEach ( callbackfn [ , thisArg ] ), https://tc39.es/ecma262/#sec-set.prototype.foreach
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::for_each)
{
    auto* set = TRY(typed_this_object(vm));
    if (!vm.argument(0).is_function())
        return vm.throw_completion<TypeError>(ErrorType::NotAFunction, vm.argument(0).to_string_without_side_effects());
    auto this_value = vm.this_value();
    for (auto& entry : *set)
        TRY(call(vm, vm.argument(0).as_function(), vm.argument(1), entry.key, entry.key, this_value));
    return js_undefined();
}

// 24.2.3.7 Set.prototype.has ( value ), https://tc39.es/ecma262/#sec-set.prototype.has
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::has)
{
    auto* set = TRY(typed_this_object(vm));
    return Value(set->set_has(vm.argument(0)));
}

// 24.2.3.10 Set.prototype.values ( ), https://tc39.es/ecma262/#sec-set.prototype.values
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::values)
{
    auto& realm = *vm.current_realm();

    auto* set = TRY(typed_this_object(vm));

    return SetIterator::create(realm, *set, Object::PropertyKind::Value);
}

// 24.2.3.9 get Set.prototype.size, https://tc39.es/ecma262/#sec-get-set.prototype.size
JS_DEFINE_NATIVE_FUNCTION(SetPrototype::size_getter)
{
    auto* set = TRY(typed_this_object(vm));
    return Value(set->set_size());
}

// 8 Set Records, https://tc39.es/proposal-set-methods/#sec-set-records
struct SetRecord {
    NonnullGCPtr<Object> set;          // [[Set]]
    double size { 0 };                 // [[Size]
    NonnullGCPtr<FunctionObject> has;  // [[Has]]
    NonnullGCPtr<FunctionObject> keys; // [[Keys]]
};

// 9 GetSetRecord ( obj ), https://tc39.es/proposal-set-methods/#sec-getsetrecord
static ThrowCompletionOr<SetRecord> get_set_record(VM& vm, Value value)
{
    // 1. If obj is not an Object, throw a TypeError exception.
    if (!value.is_object())
        return vm.throw_completion<TypeError>(ErrorType::NotAnObject, value.to_string_without_side_effects());
    auto const& object = value.as_object();

    // 2. Let rawSize be ? Get(obj, "size").
    auto raw_size = TRY(object.get(vm.names.size));

    // 3. Let numSize be ? ToNumber(rawSize).
    auto number_size = TRY(raw_size.to_number(vm));

    // 4. NOTE: If rawSize is undefined, then numSize will be NaN.
    // 5. If numSize is NaN, throw a TypeError exception.
    if (number_size.is_nan())
        return vm.throw_completion<TypeError>(ErrorType::IntlNumberIsNaN, "size"sv);

    // 6. Let intSize be ! ToIntegerOrInfinity(numSize).
    auto integer_size = MUST(number_size.to_integer_or_infinity(vm));

    // 7. Let has be ? Get(obj, "has").
    auto has = TRY(object.get(vm.names.has));

    // 8. If IsCallable(has) is false, throw a TypeError exception.
    if (!has.is_function())
        return vm.throw_completion<TypeError>(ErrorType::NotAFunction, has.to_string_without_side_effects());

    // 9. Let keys be ? Get(obj, "keys").
    auto keys = TRY(object.get(vm.names.keys));

    // 10. If IsCallable(keys) is false, throw a TypeError exception.
    if (!keys.is_function())
        return vm.throw_completion<TypeError>(ErrorType::NotAFunction, keys.to_string_without_side_effects());

    // 11. Return a new Set Record { [[Set]]: obj, [[Size]]: intSize, [[Has]]: has, [[Keys]]: keys }.
    return SetRecord { .set = object, .size = integer_size, .has = has.as_function(), .keys = keys.as_function() };
}

// 10 GetKeysIterator ( setRec ), https://tc39.es/proposal-set-methods/#sec-getkeysiterator
static ThrowCompletionOr<Iterator> get_keys_iterator(VM& vm, SetRecord const& set_record)
{
    // 1. Let keysIter be ? Call(setRec.[[Keys]], setRec.[[Set]]).
    auto keys_iterator = TRY(call(vm, *set_record.keys, set_record.set));

    // 2. If keysIter is not an Object, throw a TypeError exception.
    if (!keys_iterator.is_object())
        return vm.throw_completion<TypeError>(ErrorType::NotAnObject, keys_iterator.to_string_without_side_effects());

    // 3. Let nextMethod be ? Get(keysIter, "next").
    auto next_method = TRY(keys_iterator.as_object().get(vm.names.next));

    // 4. If IsCallable(nextMethod) is false, throw a TypeError exception.
    if (!next_method.is_function())
        return vm.throw_completion<TypeError>(ErrorType::NotAFunction, next_method.to_string_without_side_effects());

    // 5. Return a new Iterator Record { [[Iterator]]: keysIter, [[NextMethod]]: nextMethod, [[Done]]: false }.
    return Iterator { .iterator = &keys_iterator.as_object(), .next_method = next_method, .done = false };
}

}
