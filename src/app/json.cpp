#include "app/json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace hl {

namespace {
const Json kNull;

struct Parser {
  const std::string& s;
  size_t i = 0;
  bool ok = true;

  void ws() {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
  }
  bool lit(const char* t) {
    size_t n = std::char_traits<char>::length(t);
    if (s.compare(i, n, t) == 0) {
      i += n;
      return true;
    }
    return false;
  }
  std::string str() {
    std::string out;
    ++i;  // opening quote
    while (i < s.size() && s[i] != '"') {
      char c = s[i++];
      if (c == '\\' && i < s.size()) {
        char e = s[i++];
        switch (e) {
          case 'n': out += '\n'; break;
          case 't': out += '\t'; break;
          case 'r': out += '\r'; break;
          case 'b': out += '\b'; break;
          case 'f': out += '\f'; break;
          case 'u': {
            unsigned cp = static_cast<unsigned>(std::strtoul(s.substr(i, 4).c_str(), nullptr, 16));
            i += 4;
            if (cp < 0x80) out += static_cast<char>(cp);
            else if (cp < 0x800) {
              out += static_cast<char>(0xC0 | (cp >> 6));
              out += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
              out += static_cast<char>(0xE0 | (cp >> 12));
              out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
              out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            break;
          }
          default: out += e;
        }
      } else
        out += c;
    }
    if (i >= s.size()) ok = false;
    ++i;
    return out;
  }
  Json value(int depth) {
    if (depth > 200) {
      ok = false;
      return {};
    }
    ws();
    if (i >= s.size()) {
      ok = false;
      return {};
    }
    char c = s[i];
    if (c == '{') {
      ++i;
      Json::Object o;
      ws();
      if (i < s.size() && s[i] == '}') {
        ++i;
        return Json(o);
      }
      while (ok) {
        ws();
        if (i >= s.size() || s[i] != '"') {
          ok = false;
          break;
        }
        std::string k = str();
        ws();
        if (i >= s.size() || s[i] != ':') {
          ok = false;
          break;
        }
        ++i;
        o[k] = value(depth + 1);
        ws();
        if (i < s.size() && s[i] == ',') {
          ++i;
          continue;
        }
        if (i < s.size() && s[i] == '}') {
          ++i;
          break;
        }
        ok = false;
      }
      return Json(o);
    }
    if (c == '[') {
      ++i;
      Json::Array a;
      ws();
      if (i < s.size() && s[i] == ']') {
        ++i;
        return Json(a);
      }
      while (ok) {
        a.push_back(value(depth + 1));
        ws();
        if (i < s.size() && s[i] == ',') {
          ++i;
          continue;
        }
        if (i < s.size() && s[i] == ']') {
          ++i;
          break;
        }
        ok = false;
      }
      return Json(a);
    }
    if (c == '"') return Json(str());
    if (lit("true")) return Json(true);
    if (lit("false")) return Json(false);
    if (lit("null")) return Json();
    char* end = nullptr;
    double d = std::strtod(s.c_str() + i, &end);
    if (end == s.c_str() + i) {
      ok = false;
      return {};
    }
    i = static_cast<size_t>(end - s.c_str());
    return Json(d);
  }
};

void escape(const std::string& in, std::string& out) {
  out += '"';
  for (unsigned char c : in) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", c);
          out += buf;
        } else
          out += static_cast<char>(c);
    }
  }
  out += '"';
}
}  // namespace

Json Json::parse(const std::string& text) {
  Parser p{text};
  Json v = p.value(0);
  return p.ok ? v : Json();
}

const Json::Array& Json::arr() const {
  static const Array empty;
  return a_ ? *a_ : empty;
}
Json::Array& Json::arr() {
  if (type_ != Type::Array) {
    type_ = Type::Array;
    a_ = std::make_shared<Array>();
  } else if (a_.use_count() > 1)
    a_ = std::make_shared<Array>(*a_);  // copy-on-write keeps copies independent
  return *a_;
}
const Json::Object& Json::obj() const {
  static const Object empty;
  return o_ ? *o_ : empty;
}
Json::Object& Json::obj() {
  if (type_ != Type::Object) {
    type_ = Type::Object;
    o_ = std::make_shared<Object>();
  } else if (o_.use_count() > 1)
    o_ = std::make_shared<Object>(*o_);  // copy-on-write keeps copies independent
  return *o_;
}

const Json& Json::operator[](const std::string& key) const {
  if (type_ != Type::Object) return kNull;
  auto it = o_->find(key);
  return it == o_->end() ? kNull : it->second;
}
Json& Json::operator[](const std::string& key) { return obj()[key]; }
void Json::push(Json v) { arr().push_back(std::move(v)); }
size_t Json::size() const { return type_ == Type::Array ? a_->size() : type_ == Type::Object ? o_->size() : 0; }

void Json::dumpTo(std::string& out) const {
  switch (type_) {
    case Type::Null: out += "null"; break;
    case Type::Bool: out += b_ ? "true" : "false"; break;
    case Type::Number: {
      if (std::isfinite(n_) && n_ == std::floor(n_) && std::fabs(n_) < 1e15) out += std::to_string(static_cast<long long>(n_));
      else if (std::isfinite(n_)) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.6g", n_);
        out += buf;
      } else
        out += "0";
      break;
    }
    case Type::String: escape(s_, out); break;
    case Type::Array: {
      out += '[';
      bool first = true;
      for (auto& v : *a_) {
        if (!first) out += ',';
        first = false;
        v.dumpTo(out);
      }
      out += ']';
      break;
    }
    case Type::Object: {
      out += '{';
      bool first = true;
      for (auto& [k, v] : *o_) {
        if (!first) out += ',';
        first = false;
        escape(k, out);
        out += ':';
        v.dumpTo(out);
      }
      out += '}';
      break;
    }
  }
}

std::string Json::dump() const {
  std::string out;
  dumpTo(out);
  return out;
}

}  // namespace hl
