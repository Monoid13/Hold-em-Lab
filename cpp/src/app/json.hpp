#pragma once
// Minimal JSON value, parser and serializer for saving settings, stats and history.

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace hl {

class Json {
 public:
  enum class Type { Null, Bool, Number, String, Array, Object };
  using Array = std::vector<Json>;
  using Object = std::map<std::string, Json>;

  Json() = default;
  Json(std::nullptr_t) {}
  Json(bool b) : type_(Type::Bool), b_(b) {}
  Json(int n) : type_(Type::Number), n_(n) {}
  Json(long n) : type_(Type::Number), n_(static_cast<double>(n)) {}
  Json(double n) : type_(Type::Number), n_(n) {}
  Json(const char* s) : type_(Type::String), s_(s) {}
  Json(std::string s) : type_(Type::String), s_(std::move(s)) {}
  Json(Array a) : type_(Type::Array), a_(std::make_shared<Array>(std::move(a))) {}
  Json(Object o) : type_(Type::Object), o_(std::make_shared<Object>(std::move(o))) {}

  static Json array() { return Json(Array{}); }
  static Json object() { return Json(Object{}); }
  static Json parse(const std::string& text);  // returns Null on error

  Type type() const { return type_; }
  bool isNull() const { return type_ == Type::Null; }
  bool isObject() const { return type_ == Type::Object; }
  bool isArray() const { return type_ == Type::Array; }

  bool asBool(bool def = false) const { return type_ == Type::Bool ? b_ : def; }
  double asNum(double def = 0) const { return type_ == Type::Number ? n_ : def; }
  int asInt(int def = 0) const { return type_ == Type::Number ? static_cast<int>(n_) : def; }
  std::string asStr(const std::string& def = "") const { return type_ == Type::String ? s_ : def; }

  const Array& arr() const;
  Array& arr();
  const Object& obj() const;
  Object& obj();

  const Json& operator[](const std::string& key) const;  // Null if missing
  Json& operator[](const std::string& key);              // creates (turns Null into Object)
  void push(Json v);
  size_t size() const;

  std::string dump() const;

 private:
  Type type_ = Type::Null;
  bool b_ = false;
  double n_ = 0;
  std::string s_;
  std::shared_ptr<Array> a_;
  std::shared_ptr<Object> o_;
  void dumpTo(std::string& out) const;
};

}  // namespace hl
