// Copyright Benoit Blanchon 2014-2016
// MIT License
//
// Arduino JSON library
// https://github.com/bblanchon/ArduinoJson
// If you like this project, please add a star!

#pragma once

#include "Internals/JsonBufferAllocated.hpp"
#include "Internals/JsonPrintable.hpp"
#include "Internals/List.hpp"
#include "Internals/ReferenceType.hpp"
#include "JsonPair.hpp"
#include "String.hpp"
#include "TypeTraits/EnableIf.hpp"
#include "TypeTraits/IsFloatingPoint.hpp"
#include "TypeTraits/IsReference.hpp"
#include "TypeTraits/IsSame.hpp"

// Returns the size (in bytes) of an object with n elements.
// Can be very handy to determine the size of a StaticJsonBuffer.
#define JSON_OBJECT_SIZE(NUMBER_OF_ELEMENTS) \
  (sizeof(JsonObject) + (NUMBER_OF_ELEMENTS) * sizeof(JsonObject::node_type))

namespace ArduinoJson {

// Forward declarations
class JsonArray;
class JsonBuffer;

// A dictionary of JsonVariant indexed by string (char*)
//
// The constructor is private, instances must be created via
// JsonBuffer::createObject() or JsonBuffer::parseObject().
// A JsonObject can be serialized to a JSON string via JsonObject::printTo().
// It can also be deserialized from a JSON string via JsonBuffer::parseObject().
class JsonObject : public Internals::JsonPrintable<JsonObject>,
                   public Internals::ReferenceType,
                   public Internals::List<JsonPair>,
                   public Internals::JsonBufferAllocated {
 public:
  // A meta-function that returns true if type T can be used in
  // JsonObject::set()
  template <typename T>
  struct CanSet {
    static const bool value = JsonVariant::IsConstructibleFrom<T>::value ||
                              TypeTraits::IsSame<T, String&>::value ||
                              TypeTraits::IsSame<T, const String&>::value;
  };

  // Create an empty JsonArray attached to the specified JsonBuffer.
  // You should not use this constructor directly.
  // Instead, use JsonBuffer::createObject() or JsonBuffer.parseObject().
  explicit JsonObject(JsonBuffer* buffer) : Internals::List<JsonPair>(buffer) {}

  // Gets or sets the value associated with the specified key.
  JsonObjectSubscript<const char*> operator[](const char* key);
  JsonObjectSubscript<const String&> operator[](const String& key);

  // Gets the value associated with the specified key.
  template <typename TString>
  JsonVariant operator[](const TString& key) const {
    return get(key);
  }

  // Sets the specified key with the specified value.
  // bool set(TKey key, bool value);
  // bool set(TKey key, char value);
  // bool set(TKey key, long value);
  // bool set(TKey key, int value);
  // bool set(TKey key, short value);
  // bool set(TKey key, float value);
  // bool set(TKey key, double value);
  // bool set(TKey key, const char* value);
  // bool set(TKey key, RawJson value);
  template <typename TValue, typename TString>
  bool set(const TString& key, TValue value,
           typename TypeTraits::EnableIf<
               CanSet<TValue>::value &&
               !TypeTraits::IsReference<TValue>::value>::type* = 0) {
    return setNodeAt<TValue>(key, value);
  }
  // bool set(Key, String&);
  // bool set(Key, JsonArray&);
  // bool set(Key, JsonObject&);
  // bool set(Key, JsonVariant&);
  template <typename TValue, typename TString>
  bool set(const TString& key, const TValue& value,
           typename TypeTraits::EnableIf<CanSet<TValue&>::value>::type* = 0) {
    return setNodeAt<TValue&>(key, const_cast<TValue&>(value));
  }
  // bool set(Key, float value, uint8_t decimals);
  // bool set(Key, double value, uint8_t decimals);
  template <typename TValue, typename TString>
  bool set(const TString& key, TValue value, uint8_t decimals,
           typename TypeTraits::EnableIf<
               TypeTraits::IsFloatingPoint<TValue>::value>::type* = 0) {
    return setNodeAt<const JsonVariant&>(key, JsonVariant(value, decimals));
  }

  // Gets the value associated with the specified key.
  template <typename TString>
  JsonVariant get(const TString& key) const {
    node_type* node = getNodeAt(key);
    return node ? node->content.value : JsonVariant();
  }

  // Gets the value associated with the specified key.
  template <typename TValue, typename TString>
  typename Internals::JsonVariantAs<TValue>::type get(
      const TString& key) const {
    node_type* node = getNodeAt(key);
    return node ? node->content.value.as<TValue>()
                : JsonVariant::defaultValue<TValue>();
  }

  // Checks the type of the value associated with the specified key.
  template <typename TValue, typename TString>
  bool is(const TString& key) const {
    node_type* node = getNodeAt(key);
    return node ? node->content.value.is<TValue>() : false;
  }

  // Creates and adds a JsonArray.
  // This is a shortcut for JsonBuffer::createArray() and JsonObject::add().
  template <typename TString>
  JsonArray& createNestedArray(const TString& key);

  // Creates and adds a JsonObject.
  // This is a shortcut for JsonBuffer::createObject() and JsonObject::add().
  template <typename TString>
  JsonObject& createNestedObject(const TString& key);

  // Tells weither the specified key is present and associated with a value.
  template <typename TString>
  bool containsKey(const TString& key) const {
    return getNodeAt(key) != NULL;
  }

  // Removes the specified key and the associated value.
  template <typename TString>
  void remove(const TString& key) {
    removeNode(getNodeAt(key));
  }

  // Returns a reference an invalid JsonObject.
  // This object is meant to replace a NULL pointer.
  // This is used when memory allocation or JSON parsing fail.
  static JsonObject& invalid() {
    static JsonObject instance(NULL);
    return instance;
  }

 private:
  // Returns the list node that matches the specified key.
  template <typename TString>
  node_type* getNodeAt(const TString& key) const {
    for (node_type* node = _firstNode; node; node = node->next) {
      if (JsonObjectKey::equals(node->content.key, key)) return node;
    }
    return NULL;
  }

  template <typename TValue, typename TString>
  bool setNodeAt(const TString& key, TValue value) {
    node_type* node = getNodeAt(key);
    if (!node) {
      node = addNewNode();
      if (!node || !setNodeKey(node, key)) return false;
    }
    return setNodeValue<TValue>(node, value);
  }

  template <typename TString>
  typename TypeTraits::EnableIf<!JsonObjectKey::ShouldDuplicate<TString>::value,
                                bool>::type
  setNodeKey(node_type* node, const TString& key) {
    node->content.key = key;
    return true;
  }

  template <typename TString>
  typename TypeTraits::EnableIf<JsonObjectKey::ShouldDuplicate<TString>::value,
                                bool>::type
  setNodeKey(node_type* node, const TString& key) {
    node->content.key = _buffer->strdup(key);
    if (node->content.key == NULL) return false;
    return true;
  }

  template <typename TValue>
  bool setNodeValue(node_type* node, TValue value) {
    node->content.value = value;
    return true;
  }
};
}
