#pragma once

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace adapter {

enum struct RootKind : int {
  Unknown     = 0,
  Variable    = 1,
  Function    = 2,
  SpecialName = 3,
  LocalName   = 4,
};

namespace Enum {
enum Qualifier { None = 0, Const = 1, Volatile = 2, Restrict = 4 };

inline Qualifier operator|(Qualifier lhs, Qualifier rhs) { return (Qualifier)(((int) lhs) | ((int) rhs)); }
inline Qualifier &operator|=(Qualifier &lhs, Qualifier rhs) {
  lhs = lhs | rhs;
  return lhs;
}

inline std::ostream &operator<<(std::ostream &os, Qualifier q) {
  if (q & Const) os << "const ";
  if (q & Volatile) os << "volatile ";
  if (q & Restrict) os << "restrict ";
  return os;
}

} // namespace Enum

enum class PointerKind { Pointer, Reference, RValueReference };

inline std::ostream &operator<<(std::ostream &os, PointerKind k) {
  switch (k) {
  case adapter::PointerKind::Pointer: os << "* "; break;
  case adapter::PointerKind::Reference: os << "& "; break;
  case adapter::PointerKind::RValueReference: os << "&& "; break;
  default: break;
  }
  return os;
}

enum class FunctionReferenceKind { None, Reference, RValueReference };

inline std::ostream &operator<<(std::ostream &os, FunctionReferenceKind k) {
  switch (k) {
  case adapter::FunctionReferenceKind::None: break;
  case adapter::FunctionReferenceKind::Reference: os << "& "; break;
  case adapter::FunctionReferenceKind::RValueReference: os << "&& "; break;
  default: break;
  }
  return os;
}

struct Node {
  virtual ~Node() {}
  virtual void Print(std::ostream &os) const = 0;

  friend std::ostream &operator<<(std::ostream &os, const Node &node) {
    node.Print(os);
    return os;
  }

  std::string ToString() {
    std::ostringstream oss;
    oss << *this;
    return oss.str();
  }
};

struct RootNode : Node {
  RootKind Kind;
  RootNode(RootKind Kind) : Kind(Kind) {}
};
struct TypeNode : Node {};

template <typename T> struct NodeArray {
  std::vector<std::unique_ptr<T>> Elements;

  void Print(std::ostream &os, std::string_view del = ", ") const {
    bool first = true;
    for (auto &element : Elements) {
      if (first)
        first = false;
      else
        os << del;
      os << *element;
    }
  }

  void append(std::unique_ptr<T> &&r) { Elements.emplace_back(std::move(r)); }
};

struct SkippedRoot : RootNode {
  SkippedRoot() : RootNode(RootKind::Unknown) {}
  virtual void Print(std::ostream &os) const override { os << "$SKIP_ROOT"; }
};
struct SkippedType : TypeNode {
  virtual void Print(std::ostream &os) const override { os << "$SKIP_TYPE"; }
};

struct NamePiece : Node {
  virtual std::string &GetRaw()                             = 0;
  virtual std::optional<NodeArray<TypeNode>> &GetTemplate() = 0;
};

struct SimpleNamePiece : NamePiece {
  std::string Raw;
  std::optional<NodeArray<TypeNode>> TemplateParameters;

  SimpleNamePiece(std::string Raw, std::optional<NodeArray<TypeNode>> TemplateParameters = std::nullopt)
      : Raw(std::move(Raw)), TemplateParameters(std::move(TemplateParameters)) {}

  std::string &GetRaw() override { return Raw; }
  std::optional<NodeArray<TypeNode>> &GetTemplate() override { return TemplateParameters; }

  virtual void Print(std::ostream &os) const override {
    os << Raw;
    if (TemplateParameters) {
      os << "<";
      TemplateParameters->Print(os);
      os << ">";
    }
  }
};

struct NameNode : Node {
  NodeArray<NamePiece> Pieces;

  virtual void Print(std::ostream &os) const override { Pieces.Print(os, "::"); }
};

struct SpecialType : TypeNode {
  std::string Name;

  SpecialType(std::string Name) : Name(std::move(Name)) {}

  virtual void Print(std::ostream &os) const override { os << "$$" << Name; }
};

struct SimpleType : TypeNode {
  std::unique_ptr<NameNode> Name;

  SimpleType(std::unique_ptr<NameNode> Name) : Name(std::move(Name)) {}

  virtual void Print(std::ostream &os) const override { os << *Name; }
};

struct QualType : TypeNode {
  std::unique_ptr<TypeNode> Child;
  Enum::Qualifier Qualifier;

  QualType(std::unique_ptr<TypeNode> Child, Enum::Qualifier Qualifier)
      : Child(std::move(Child)), Qualifier(Qualifier) {}

  virtual void Print(std::ostream &os) const override { os << Qualifier << *Child; }
};

struct PointerType : TypeNode {
  std::unique_ptr<TypeNode> Child;
  PointerKind Type;

  PointerType(std::unique_ptr<TypeNode> Child, PointerKind Type) : Child(std::move(Child)), Type(Type) {}

  virtual void Print(std::ostream &os) const override { os << Type << *Child; }
};

struct FunctionType : TypeNode {
  NodeArray<TypeNode> Params;
  std::unique_ptr<TypeNode> ReturnType;
  Enum::Qualifier Qualifier;
  FunctionReferenceKind FuncReference;

  FunctionType(
      NodeArray<TypeNode> Params, std::unique_ptr<TypeNode> ReturnType, Enum::Qualifier Qualifier,
      FunctionReferenceKind FuncReference = FunctionReferenceKind::None)
      : Params(std::move(Params)), ReturnType(std::move(ReturnType)), Qualifier(Qualifier),
        FuncReference(FuncReference) {}

  virtual void Print(std::ostream &os) const override {
    os << "(";
    Params.Print(os);
    os << ") ";
    os << Qualifier << FuncReference;
    os << "-> ";
    if (ReturnType)
      os << *ReturnType;
    else
      os << "unknown";
  }
};

struct FunctionRootNode : RootNode {
  std::unique_ptr<NameNode> Name;
  std::unique_ptr<TypeNode> Signature;

  FunctionRootNode(std::unique_ptr<NameNode> Name, std::unique_ptr<TypeNode> Signature)
      : RootNode(RootKind::Function), Name(std::move(Name)), Signature(std::move(Signature)) {}

  virtual void Print(std::ostream &os) const override { os << *Name << *Signature; }
};

struct VariableRootNode : RootNode {
  std::unique_ptr<NameNode> Name;
  std::unique_ptr<TypeNode> Type;

  VariableRootNode(std::unique_ptr<NameNode> Name, std::unique_ptr<TypeNode> Type)
      : RootNode(RootKind::Variable), Name(std::move(Name)), Type(std::move(Type)) {}

  virtual void Print(std::ostream &os) const override {
    os << *Name << " -> ";
    if (Type)
      os << *Type;
    else
      os << "$unknown";
  }
};

enum struct SpecialNameKind { vtable, type_info, type_info_name, complete_object_locator };

struct SpecialNameNode : RootNode {
  SpecialNameKind Kind;
  std::unique_ptr<TypeNode> Type;

  SpecialNameNode(SpecialNameKind Kind, std::unique_ptr<TypeNode> Type)
      : RootNode(RootKind::SpecialName), Kind(Kind), Type(std::move(Type)) {}

  virtual void Print(std::ostream &os) const override {
    if (Kind == SpecialNameKind::vtable) {
      os << *Type << "::$vtable";
    } else if (Kind == SpecialNameKind::complete_object_locator) {
      os << *Type << "::$complete_object_locator";
    } else {
      os << *Type << " <- ";
      switch (Kind) {
      case SpecialNameKind::type_info: os << "$type_info"; break;
      case SpecialNameKind::type_info_name: os << "$type_info_name"; break;
      default: os << "$unknow_special"; break;
      }
    }
  }
};

struct LocalNameNode : RootNode {
  std::unique_ptr<RootNode> Root;
  std::unique_ptr<TypeNode> Name;
  std::unique_ptr<TypeNode> Type;

  LocalNameNode(std::unique_ptr<RootNode> Root, std::unique_ptr<TypeNode> Name, std::unique_ptr<TypeNode> Type)
      : RootNode(RootKind::LocalName), Root(std::move(Root)), Name(std::move(Name)), Type(std::move(Type)) {}

  virtual void Print(std::ostream &os) const override {
    os << *Root << " | " << *Name << " -> ";
    if (Type)
      os << *Type;
    else
      os << "$unknown";
  }
};

struct LocalNameTypeNode : TypeNode {
  std::unique_ptr<LocalNameNode> LocalName;

  LocalNameTypeNode(std::unique_ptr<LocalNameNode> LocalName) : LocalName(std::move(LocalName)) {}

  virtual void Print(std::ostream &os) const override { os << *LocalName; }
};

template <typename T> std::unique_ptr<RootNode> Adapt(T const &node) { return nullptr; }

} // namespace adapter

namespace llvm {
namespace itanium_demangle {
class Node;
}
namespace ms_demangle {
struct SymbolNode;
}
} // namespace llvm

template <> std::unique_ptr<adapter::RootNode> adapter::Adapt<>(llvm::itanium_demangle::Node const &node);
template <> std::unique_ptr<adapter::RootNode> adapter::Adapt<>(llvm::ms_demangle::SymbolNode const &node);