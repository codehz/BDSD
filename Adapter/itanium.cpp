#include <ItaniumDemangle.h>

#include "include/adapter.h"

#include <sstream>

using namespace adapter;
using namespace llvm;
namespace SRC = llvm::itanium_demangle;

namespace itanium {

template <typename NodeType, typename XType> struct BaseContext {
  std::unique_ptr<NodeType> output{};

  BaseContext(SRC::Node const *node) { process(node); }

  void process(SRC::Node const *node) { node->visit<XType &>((XType &) *this); }

  operator std::unique_ptr<NodeType>() { return std::move(output); }
};

struct NameContext {
  std::unique_ptr<NameNode> name;
  std::vector<std::unique_ptr<NamePiece>> &output() { return name->Pieces.Elements; }
  std::optional<NodeArray<TypeNode>> temp;
  NameContext(SRC::Node const *node) : name(std::make_unique<NameNode>()) { process(node); }

  void process(SRC::Node const *node) { node->visit<NameContext &>(*this); }

  template <typename T> void operator()(T node) {}

  operator std::unique_ptr<NameNode>() {
    if (output().size() >= 2 && output()[0]->GetRaw() == "std") {
      if (output()[1]->GetRaw() == "__cxx11") { output().erase(++output().begin()); }
      auto &key = *output()[1];
      if (output().size() == 2 && key.GetRaw() == "basic_string") {
        auto &temp  = output()[1]->GetTemplate()->Elements;
        auto chtype = temp[0]->ToString();
        auto trtype = temp[1]->ToString();
        auto altype = temp[2]->ToString();
        if (chtype == "char" && trtype == "std::char_traits<char>" && altype == "std::allocator<char>") {
          output()[1] = std::make_unique<SimpleNamePiece>("string");
        } else if (chtype == "wchar" && trtype == "std::char_traits<wchar>" && altype == "std::allocator<wchar>") {
          output()[1] = std::make_unique<SimpleNamePiece>("wstring");
        }
      } else if (key.GetRaw() == "vector" || key.GetRaw() == "initializer_list") {
        auto &temp = key.GetTemplate()->Elements;
        if (temp.size() != 2) goto end;
        auto &base  = temp[0];
        auto &alloc = temp[1];
        if (auto st = dynamic_cast<SimpleType const *>(alloc.get())) {
          auto &sp = st->Name->Pieces.Elements;
          if (sp[0]->GetRaw() == "std" && sp[1]->GetRaw() == "allocator") {
            if (sp[1]->GetTemplate()->Elements[0]->ToString() == base->ToString()) {
              temp.resize(1); //
            }
          }
        }
      } else if (key.GetRaw() == "unique_ptr") {
        auto &temp  = key.GetTemplate()->Elements;
        auto &base  = temp[0];
        auto &alloc = temp[1];
        if (auto st = dynamic_cast<SimpleType const *>(alloc.get())) {
          auto &sp = st->Name->Pieces.Elements;
          if (sp[0]->GetRaw() == "std" && sp[1]->GetRaw() == "default_delete") {
            if (sp[1]->GetTemplate()->Elements[0]->ToString() == base->ToString()) {
              temp.resize(1); //
            }
          }
        }
      }
    }
  end:
    return std::move(name);
  }
};

struct RootContext : BaseContext<RootNode, RootContext> {
  using BaseContext::BaseContext;
  template <typename T> void operator()(T node) {
#ifndef NDEBUG
    node->dump();
#endif
    output = std::make_unique<adapter::SkippedRoot>();
  }
};

struct TypeContext : BaseContext<TypeNode, TypeContext> {
  using BaseContext::BaseContext;
  template <typename T> void operator()(T node) { output = std::make_unique<SimpleType>(NameContext{node}); }
};

NodeArray<TypeNode> CollectFunctionParameter(SRC::NodeArray const &Params) {
  NodeArray<TypeNode> ret;
  for (auto param : Params) {
    if (auto exp = dynamic_cast<SRC::ParameterPackExpansion const *>(param)) {
      exp->match([&](auto inner) {
        if (auto pack = dynamic_cast<SRC::ParameterPack const *>(inner)) {
          pack->match([&](auto data) {
            for (auto sub : data) { ret.Elements.emplace_back(TypeContext{sub}); }
          });
        } else
          ret.Elements.emplace_back(TypeContext{inner});
      });
    } else
      ret.Elements.emplace_back(TypeContext{param});
  }
  return ret;
}

template <> void RootContext::operator()(SRC::AbiTagAttr const *abitag) { process(abitag->Base); }
template <> void RootContext::operator()(SRC::CtorVtableSpecialName const *ctorvtbl) {
  output = std::make_unique<adapter::SkippedRoot>();
}
template <> void RootContext::operator()(SRC::LocalName const *local) {
  local->match([&](SRC::Node *root, SRC::Node *name) {
    output = std::make_unique<adapter::LocalNameNode>(RootContext{root}, TypeContext{name}, nullptr);
  });
}
template <> void RootContext::operator()(SRC::NameWithTemplateArgs const *name) {
  output = std::make_unique<adapter::VariableRootNode>(NameContext{name}, nullptr);
}
template <> void RootContext::operator()(SRC::NameType const *name) {
  output = std::make_unique<adapter::VariableRootNode>(NameContext{name}, nullptr);
}
template <> void RootContext::operator()(SRC::NestedName const *nested) {
  output = std::make_unique<adapter::VariableRootNode>(NameContext{nested}, nullptr);
}
template <> void RootContext::operator()(SRC::StdQualifiedName const *stdqual) {
  output = std::make_unique<adapter::VariableRootNode>(NameContext{stdqual}, nullptr);
}
template <> void RootContext::operator()(SRC::SpecialName const *special) {
  special->match([&](llvm::itanium_demangle::SpecialNameType type, auto, llvm::itanium_demangle::Node const *Child) {
    switch (type) {
    case llvm::itanium_demangle::SpecialNameType::virtual_table:
      output = std::make_unique<adapter::SpecialNameNode>(adapter::SpecialNameKind::vtable, TypeContext{Child});
      break;
    case llvm::itanium_demangle::SpecialNameType::type_info:
      output = std::make_unique<adapter::SpecialNameNode>(adapter::SpecialNameKind::type_info, TypeContext{Child});
      break;
    case llvm::itanium_demangle::SpecialNameType::type_info_name:
      output = std::make_unique<adapter::SpecialNameNode>(adapter::SpecialNameKind::type_info_name, TypeContext{Child});
      break;
    default: output = std::make_unique<adapter::SkippedRoot>();
    }
  });
}
template <> void RootContext::operator()(SRC::FunctionEncoding const *func) {
  func->match([this](
                  const SRC::Node *Ret, const SRC::Node *Name, SRC::NodeArray const &Params, const SRC::Node *Attrs,
                  SRC::Qualifiers CVQuals, SRC::FunctionRefQual RefQual) {
    std::unique_ptr<TypeNode> retnode;
    if (Ret) retnode = TypeContext{Ret};
    output = std::make_unique<FunctionRootNode>(
        NameContext{Name}, std::make_unique<FunctionType>(
                               CollectFunctionParameter(Params), std::move(retnode), (Enum::Qualifier) CVQuals,
                               (FunctionReferenceKind) RefQual));
  });
}

template <> void NameContext::operator()(SRC::CtorDtorName const *name) {
  name->match([this](const SRC::Node *Basename, bool IsDtor, int Variant) {
    output().emplace_back(std::make_unique<SimpleNamePiece>(IsDtor ? "$destructor" : "$constructor", std::move(temp)));
  });
}
template <> void NameContext::operator()(SRC::NameType const *name) {
  name->match([this](StringView v) {
    output().emplace_back(std::make_unique<SimpleNamePiece>(v.toString(), std::move(temp)));
    temp.reset();
  });
}
template <> void NameContext::operator()(SRC::TemplateArgs const *args) {
  args->match([this](SRC::NodeArray Params) {
    NodeArray<TypeNode> ret;
    for (auto param : Params) {
      if (auto pack = dynamic_cast<SRC::TemplateArgumentPack const *>(param)) {
        pack->match([&, this](auto arr) {
          for (auto subparam : arr) { ret.Elements.emplace_back(TypeContext{subparam}); }
        });
      } else
        ret.Elements.emplace_back(TypeContext{param});
    }
    output().back()->GetTemplate() = std::move(ret);
  });
}
template <> void NameContext::operator()(SRC::NameWithTemplateArgs const *name) {
  name->match([this](SRC::Node const *Name, SRC::Node const *TemplateArgs) {
    process(Name);
    process(TemplateArgs);
  });
}
template <> void NameContext::operator()(SRC::QualifiedName const *nested) {
  nested->match([this](auto q, auto n) {
    process(q);
    process(n);
  });
}
template <> void NameContext::operator()(SRC::NestedName const *nested) {
  nested->match([this](auto q, auto n) {
    process(q);
    process(n);
  });
}
template <> void NameContext::operator()(SRC::AbiTagAttr const *abitag) { process(abitag->Base); }
template <> void NameContext::operator()(SRC::UnnamedTypeName const *ref) {}
template <> void NameContext::operator()(SRC::ConversionOperatorType const *cast) {
  output().emplace_back(std::make_unique<SimpleNamePiece>("$cast"));
  NodeArray<TypeNode> ret;
  ret.Elements.emplace_back(std::make_unique<SkippedType>()); // TODO
  output().back()->GetTemplate() = std::move(ret);
}
template <> void NameContext::operator()(SRC::StdQualifiedName const *stdq) {
  output().emplace_back(std::make_unique<SimpleNamePiece>("std"));
  process(stdq->Child);
}
template <> void NameContext::operator()(SRC::ExpandedSpecialSubstitution const *ess) {
  ess->match([&](SRC::SpecialSubKind ssk) {
    using SSK = SRC::SpecialSubKind;
    output().emplace_back(std::make_unique<SimpleNamePiece>("std"));
    switch (ssk) {
    case SSK::allocator: output().emplace_back(std::make_unique<SimpleNamePiece>("allocator")); break;
    case SSK::basic_string: output().emplace_back(std::make_unique<SimpleNamePiece>("basic_string")); break;
    case SSK::string: output().emplace_back(std::make_unique<SimpleNamePiece>("string")); break;
    case SSK::istream: output().emplace_back(std::make_unique<SimpleNamePiece>("istream")); break;
    case SSK::ostream: output().emplace_back(std::make_unique<SimpleNamePiece>("ostream")); break;
    case SSK::iostream: output().emplace_back(std::make_unique<SimpleNamePiece>("iostream")); break;
    default: break;
    }
  });
}
template <> void NameContext::operator()(SRC::SpecialSubstitution const *ss) {
  using SSK = SRC::SpecialSubKind;
  output().emplace_back(std::make_unique<SimpleNamePiece>("std"));
  switch (ss->SSK) {
  case SSK::allocator: output().emplace_back(std::make_unique<SimpleNamePiece>("allocator")); break;
  case SSK::basic_string: output().emplace_back(std::make_unique<SimpleNamePiece>("basic_string")); break;
  case SSK::string: output().emplace_back(std::make_unique<SimpleNamePiece>("string")); break;
  case SSK::istream: output().emplace_back(std::make_unique<SimpleNamePiece>("istream")); break;
  case SSK::ostream: output().emplace_back(std::make_unique<SimpleNamePiece>("ostream")); break;
  case SSK::iostream: output().emplace_back(std::make_unique<SimpleNamePiece>("iostream")); break;
  default: break;
  }
}

template <> void TypeContext::operator()(SRC::LocalName const *local) {
  output = std::make_unique<LocalNameTypeNode>((std::unique_ptr<LocalNameNode> &&) RootContext{local});
}
template <> void TypeContext::operator()(SRC::FunctionType const *func) {
  func->match([this](
                  const SRC::Node *Ret, SRC::NodeArray Params, SRC::Qualifiers CVQuals, SRC::FunctionRefQual RefQual,
                  const SRC::Node *ExceptionSpec) {
    std::unique_ptr<TypeNode> retnode;
    if (Ret) retnode = TypeContext{Ret};
    output = std::make_unique<FunctionType>(
        CollectFunctionParameter(Params), std::move(retnode), (Enum::Qualifier) CVQuals,
        (FunctionReferenceKind) RefQual);
  });
}
template <> void TypeContext::operator()(SRC::IntegerLiteral const *ptr) { output = std::make_unique<SkippedType>(); }
template <> void TypeContext::operator()(SRC::EnumLiteral const *ptr) { output = std::make_unique<SkippedType>(); }
template <> void TypeContext::operator()(SRC::BoolExpr const *ptr) { output = std::make_unique<SkippedType>(); }
template <> void TypeContext::operator()(SRC::PrefixExpr const *ptr) { output = std::make_unique<SkippedType>(); }
template <> void TypeContext::operator()(SRC::ArrayType const *ptr) { output = std::make_unique<SkippedType>(); }
template <> void TypeContext::operator()(SRC::ClosureTypeName const *ptr) { output = std::make_unique<SkippedType>(); }
template <> void TypeContext::operator()(SRC::PointerToMemberType const *ptr) {
  output = std::make_unique<SkippedType>();
}
template <> void TypeContext::operator()(SRC::PointerType const *ptr) {
  ptr->match([this](auto inner) { output = std::make_unique<PointerType>(TypeContext{inner}, PointerKind::Pointer); });
}
template <> void TypeContext::operator()(SRC::BinaryExpr const *ref) { output = std::make_unique<SkippedType>(); }
template <> void TypeContext::operator()(SRC::EnclosingExpr const *ref) { output = std::make_unique<SkippedType>(); }
template <> void TypeContext::operator()(SRC::ParameterPack const *ref) { output = std::make_unique<SkippedType>(); }
template <> void TypeContext::operator()(SRC::ParameterPackExpansion const *ref) {
  output = std::make_unique<SkippedType>();
}
template <> void TypeContext::operator()(SRC::ReferenceType const *ref) {
  ref->match([this](auto inner, SRC::ReferenceKind rk) {
    output = std::make_unique<PointerType>(
        TypeContext{inner}, rk == SRC::ReferenceKind::RValue ? PointerKind::RValueReference : PointerKind::Reference);
  });
}
template <> void TypeContext::operator()(SRC::QualType const *qual) {
  qual->match([this](const SRC::Node *Child, SRC::Qualifiers Quals) {
    output = std::make_unique<QualType>(TypeContext{Child}, (Enum::Qualifier) Quals);
  });
}

} // namespace itanium

template <> std::unique_ptr<RootNode> adapter::Adapt(SRC::Node const &node) { return itanium::RootContext{&node}; }