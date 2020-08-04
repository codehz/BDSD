#include "MicrosoftDemangleNodes.h"

#include "include/adapter.h"

#include <sstream>

using namespace adapter;
using namespace llvm;
namespace SRC = llvm::ms_demangle;

namespace msvc {

struct LocalNamePiece : NamePiece {
  std::unique_ptr<RootNode> LocalRoot;

  LocalNamePiece(std::unique_ptr<RootNode> LocalRoot) : LocalRoot(std::move(LocalRoot)) {}

  std::string &GetRaw() override {
    static std::string raw;
    raw.clear();
    return raw;
  }
  std::optional<NodeArray<TypeNode>> &GetTemplate() override { throw; }
  void Print(std::ostream &os) const override { throw; }
};

#define dispatch(T) else if (auto sp = dynamic_cast<T const *>(node)) visit(sp)

template <typename Output> struct BaseContext {
  std::unique_ptr<Output> output{};
  operator std::unique_ptr<Output>() { return std::move(output); }
};

struct TypeContext : BaseContext<TypeNode> {
  template <typename N> void visit(N const *node) { output = std::make_unique<SkippedType>(); }
  void fixQualifier(SRC::TypeNode const *node);
  TypeContext(SRC::FunctionSignatureNode const *node);
  TypeContext(SRC::TypeNode const *node);
};

struct NameContext : BaseContext<NameNode> {
  template <typename N> void visit(N const *node) {}
  NameContext(SRC::QualifiedNameNode const *node);
};

struct NamePieceContext : BaseContext<NamePiece> {
  template <typename N> void visit(N const *node) {}
  NamePieceContext(SRC::IdentifierNode const *node);
};

struct RootContext : BaseContext<RootNode> {
  template <typename N> void visit(N const *node) { output = std::make_unique<SkippedRoot>(); }
  RootContext(SRC::SymbolNode const *node);
};

template <> void NamePieceContext::visit(SRC::LocalNamedIdentifierNode const *node) {
  output = std::make_unique<LocalNamePiece>(RootContext{(SRC::SymbolNode const *) node->Scope});
}
template <> void NamePieceContext::visit(SRC::NamedIdentifierNode const *node) {
  std::optional<NodeArray<TypeNode>> temp = std::nullopt;
  if (auto params = node->TemplateParams) {
    temp = NodeArray<TypeNode>{};
    for (auto param : *params) temp->append(TypeContext{(SRC::TypeNode const *) param});
  }
  output = std::make_unique<SimpleNamePiece>(node->Name.toString(), std::move(temp));
}
template <> void NamePieceContext::visit(SRC::StructorIdentifierNode const *node) {
  output = std::make_unique<SimpleNamePiece>(node->IsDestructor ? "$destructor" : "$constructor");
}
template <> void NamePieceContext::visit(SRC::IntrinsicFunctionIdentifierNode const *node) {
  char const *str = "$SKIP_NAME";
  switch (node->Operator) {
  case SRC::IntrinsicFunctionKind::New: str = "operator new"; break;
  case SRC::IntrinsicFunctionKind::Delete: str = "operator delete"; break;
  case SRC::IntrinsicFunctionKind::Assign: str = "operator="; break;
  case SRC::IntrinsicFunctionKind::RightShift: str = "operator>>"; break;
  case SRC::IntrinsicFunctionKind::LeftShift: str = "operator<<"; break;
  case SRC::IntrinsicFunctionKind::LogicalNot: str = "operator!"; break;
  case SRC::IntrinsicFunctionKind::Equals: str = "operator=="; break;
  case SRC::IntrinsicFunctionKind::NotEquals: str = "operator!="; break;
  case SRC::IntrinsicFunctionKind::ArraySubscript: str = "operator[]"; break;
  case SRC::IntrinsicFunctionKind::Pointer: str = "operator->"; break;
  case SRC::IntrinsicFunctionKind::Increment: str = "operator++"; break;
  case SRC::IntrinsicFunctionKind::Decrement: str = "operator--"; break;
  case SRC::IntrinsicFunctionKind::Minus: str = "operator-"; break;
  case SRC::IntrinsicFunctionKind::Plus: str = "operator+"; break;
  case SRC::IntrinsicFunctionKind::Dereference: str = "operator*"; break;
  case SRC::IntrinsicFunctionKind::BitwiseAnd: str = "operator&"; break;
  case SRC::IntrinsicFunctionKind::MemberPointer: str = "operator->*"; break;
  case SRC::IntrinsicFunctionKind::Divide: str = "operator/"; break;
  case SRC::IntrinsicFunctionKind::Modulus: str = "operator%"; break;
  case SRC::IntrinsicFunctionKind::LessThan: str = "operator<"; break;
  case SRC::IntrinsicFunctionKind::LessThanEqual: str = "operator<="; break;
  case SRC::IntrinsicFunctionKind::GreaterThan: str = "operator>"; break;
  case SRC::IntrinsicFunctionKind::GreaterThanEqual: str = "operator>="; break;
  case SRC::IntrinsicFunctionKind::Comma: str = "operator,"; break;
  case SRC::IntrinsicFunctionKind::Parens: str = "operator()"; break;
  case SRC::IntrinsicFunctionKind::BitwiseNot: str = "operator~"; break;
  case SRC::IntrinsicFunctionKind::BitwiseXor: str = "operator^"; break;
  case SRC::IntrinsicFunctionKind::BitwiseOr: str = "operator|"; break;
  case SRC::IntrinsicFunctionKind::LogicalAnd: str = "operator&&"; break;
  case SRC::IntrinsicFunctionKind::LogicalOr: str = "operator||"; break;
  case SRC::IntrinsicFunctionKind::TimesEqual: str = "operator*="; break;
  case SRC::IntrinsicFunctionKind::PlusEqual: str = "operator+="; break;
  case SRC::IntrinsicFunctionKind::MinusEqual: str = "operator-="; break;
  case SRC::IntrinsicFunctionKind::DivEqual: str = "operator/="; break;
  case SRC::IntrinsicFunctionKind::ModEqual: str = "operator%="; break;
  case SRC::IntrinsicFunctionKind::RshEqual: str = "operator>>="; break;
  case SRC::IntrinsicFunctionKind::LshEqual: str = "operator<<="; break;
  case SRC::IntrinsicFunctionKind::BitwiseAndEqual: str = "operator&="; break;
  case SRC::IntrinsicFunctionKind::BitwiseOrEqual: str = "operator|="; break;
  case SRC::IntrinsicFunctionKind::BitwiseXorEqual: str = "operator^="; break;
  case SRC::IntrinsicFunctionKind::VbaseDtor: str = "`vbase dtor'"; break;
  case SRC::IntrinsicFunctionKind::VecDelDtor: str = "`vector deleting dtor'"; break;
  case SRC::IntrinsicFunctionKind::DefaultCtorClosure: str = "`default ctor closure'"; break;
  case SRC::IntrinsicFunctionKind::ScalarDelDtor: str = "`scalar deleting dtor'"; break;
  case SRC::IntrinsicFunctionKind::VecCtorIter: str = "`vector ctor iterator'"; break;
  case SRC::IntrinsicFunctionKind::VecDtorIter: str = "`vector dtor iterator'"; break;
  case SRC::IntrinsicFunctionKind::VecVbaseCtorIter: str = "`vector vbase ctor iterator'"; break;
  case SRC::IntrinsicFunctionKind::VdispMap: str = "`virtual displacement map'"; break;
  case SRC::IntrinsicFunctionKind::EHVecCtorIter: str = "`eh vector ctor iterator'"; break;
  case SRC::IntrinsicFunctionKind::EHVecDtorIter: str = "`eh vector dtor iterator'"; break;
  case SRC::IntrinsicFunctionKind::EHVecVbaseCtorIter: str = "`eh vector vbase ctor iterator'"; break;
  case SRC::IntrinsicFunctionKind::CopyCtorClosure: str = "`copy ctor closure'"; break;
  case SRC::IntrinsicFunctionKind::LocalVftableCtorClosure: str = "`local vftable ctor closure'"; break;
  case SRC::IntrinsicFunctionKind::ArrayNew: str = "operator new[]"; break;
  case SRC::IntrinsicFunctionKind::ArrayDelete: str = "operator delete[]"; break;
  case SRC::IntrinsicFunctionKind::ManVectorCtorIter: str = "`managed vector ctor iterator'"; break;
  case SRC::IntrinsicFunctionKind::ManVectorDtorIter: str = "`managed vector dtor iterator'"; break;
  case SRC::IntrinsicFunctionKind::EHVectorCopyCtorIter: str = "`EH vector copy ctor iterator'"; break;
  case SRC::IntrinsicFunctionKind::EHVectorVbaseCopyCtorIter: str = "`EH vector vbase copy ctor iterator'"; break;
  case SRC::IntrinsicFunctionKind::VectorCopyCtorIter: str = "`vector copy ctor iterator'"; break;
  case SRC::IntrinsicFunctionKind::VectorVbaseCopyCtorIter: str = "`vector vbase copy constructor iterator'"; break;
  case SRC::IntrinsicFunctionKind::ManVectorVbaseCopyCtorIter:
    str = "`managed vector vbase copy constructor iterator'";
    break;
  case SRC::IntrinsicFunctionKind::CoAwait: str = "operator co_await"; break;
  case SRC::IntrinsicFunctionKind::Spaceship: str = "operator<=>"; break;
  default: break;
  }
  output = std::make_unique<SimpleNamePiece>(str);
}

template <> void NameContext::visit(SRC::QualifiedNameNode const *node) {
  output     = std::make_unique<NameNode>();
  auto &list = output->Pieces.Elements;

  for (auto it : *node->Components) list.emplace_back(NamePieceContext{(SRC::IdentifierNode const *) it});
#pragma region optimize
  if (list.size() >= 2 && list[0]->GetRaw() == "std") {
    auto &key = *list[1];
    if (key.GetRaw() == "basic_string") {
      auto &temp  = key.GetTemplate()->Elements;
      auto chtype = temp[0]->ToString();
      auto trtype = temp[1]->ToString();
      auto altype = temp[2]->ToString();
      if (chtype == "char" && trtype == "std::char_traits<char>" && altype == "std::allocator<char>") {
        list[1] = std::make_unique<SimpleNamePiece>("string");
      } else if (chtype == "wchar" && trtype == "std::char_traits<wchar>" && altype == "std::allocator<wchar>") {
        list[1] = std::make_unique<SimpleNamePiece>("wstring");
      }
    } else if (key.GetRaw() == "vector" || key.GetRaw() == "initializer_list") {
      auto &temp = key.GetTemplate()->Elements;
      if (temp.size() != 2) return;
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
#pragma endregion std type
}

void TypeContext::fixQualifier(SRC::TypeNode const *node) {
  if (node->Quals & (SRC::Q_Const | SRC::Q_Volatile | SRC::Q_Restrict)) {
    Enum::Qualifier qual{};
    if (node->Quals & SRC::Q_Const) qual |= Enum::Const;
    if (node->Quals & SRC::Q_Volatile) qual |= Enum::Volatile;
    if (node->Quals & SRC::Q_Const) qual |= Enum::Const;
    output = std::make_unique<QualType>(std::move(output), qual);
  }
}

template <> void TypeContext::visit(SRC::FunctionSignatureNode const *node) {
  NodeArray<TypeNode> params;
  if (node->Params) {
    for (auto param : *node->Params) params.append(TypeContext{(SRC::TypeNode const *) param});
  }
  std::unique_ptr<TypeNode> ret;
  if (node->ReturnType) ret = TypeContext(node->ReturnType);
  Enum::Qualifier qual{};
  if (node->Quals & SRC::Q_Const) qual |= Enum::Const;
  if (node->Quals & SRC::Q_Volatile) qual |= Enum::Volatile;
  if (node->Quals & SRC::Q_Const) qual |= Enum::Const;
  output = std::make_unique<FunctionType>(
      std::move(params), std::move(ret), qual, (FunctionReferenceKind) node->RefQualifier);
}
template <> void TypeContext::visit(SRC::PrimitiveTypeNode const *node) {
  char const *str = "unknown";
  switch (node->PrimKind) {
  case SRC::PrimitiveKind::Void: str = "void"; break;
  case SRC::PrimitiveKind::Bool: str = "bool"; break;
  case SRC::PrimitiveKind::Char: str = "char"; break;
  case SRC::PrimitiveKind::Schar: str = "signed char"; break;
  case SRC::PrimitiveKind::Uchar: str = "unsigned char"; break;
  case SRC::PrimitiveKind::Char8: str = "char8"; break;
  case SRC::PrimitiveKind::Char16: str = "char16"; break;
  case SRC::PrimitiveKind::Char32: str = "char32"; break;
  case SRC::PrimitiveKind::Short: str = "short"; break;
  case SRC::PrimitiveKind::Ushort: str = "unsigned short"; break;
  case SRC::PrimitiveKind::Int: str = "int"; break;
  case SRC::PrimitiveKind::Uint: str = "unsigned"; break;
  case SRC::PrimitiveKind::Long: str = "long"; break;
  case SRC::PrimitiveKind::Ulong: str = "unsigned long"; break;
  case SRC::PrimitiveKind::Int64: str = "int64"; break;
  case SRC::PrimitiveKind::Uint64: str = "uint64"; break;
  case SRC::PrimitiveKind::Wchar: str = "wchar"; break;
  case SRC::PrimitiveKind::Float: str = "float"; break;
  case SRC::PrimitiveKind::Double: str = "double"; break;
  case SRC::PrimitiveKind::Ldouble: str = "double double"; break;
  case SRC::PrimitiveKind::Nullptr: str = "nullptr"; break;
  default: break;
  }
  auto name = std::make_unique<NameNode>();
  name->Pieces.Elements.emplace_back(std::make_unique<SimpleNamePiece>(str));
  output = std::make_unique<SimpleType>(std::move(name));
}
template <> void TypeContext::visit(SRC::TagTypeNode const *node) {
  output = std::make_unique<SimpleType>(NameContext{node->QualifiedName});
  fixQualifier(node);
}

template <> void TypeContext::visit(SRC::PointerTypeNode const *node) {
  output = std::make_unique<PointerType>(TypeContext{node->Pointee}, (PointerKind)((int) node->Affinity - 1));
  fixQualifier(node);
}

template <> void RootContext::visit(SRC::FunctionSymbolNode const *node) {
  output = std::make_unique<FunctionRootNode>(NameContext{node->Name}, TypeContext{node->Signature});
}

template <> void RootContext::visit(SRC::VariableSymbolNode const *node) {
  auto temp  = std::make_unique<VariableRootNode>(NameContext{node->Name}, TypeContext{node->Type});
  auto &list = temp->Name->Pieces.Elements;
  if (auto local = dynamic_cast<LocalNamePiece *>(list[0].get())) {
    auto xroot = std::move(local->LocalRoot);
    list.erase(list.begin());
    output = std::make_unique<LocalNameNode>(
        std::move(xroot), std::make_unique<SimpleType>(std::move(temp->Name)), std::move(temp->Type));
  } else {
    output = std::move(temp);
  }
}

template <> void RootContext::visit(SRC::SpecialTableSymbolNode const *node) {
  if (node->IntrinsicKind == SRC::SpecialIntrinsicKind::Vftable) {
    auto name = NameContext{node->Name};
    name.output->Pieces.Elements.pop_back();
    output = std::make_unique<SpecialNameNode>(SpecialNameKind::vtable, std::make_unique<SimpleType>(std::move(name)));
  } else if (node->IntrinsicKind == SRC::SpecialIntrinsicKind::RttiCompleteObjLocator) {
    auto name = NameContext{node->Name};
    name.output->Pieces.Elements.pop_back();
    output = std::make_unique<SpecialNameNode>(
        SpecialNameKind::complete_object_locator, std::make_unique<SimpleType>(std::move(name)));
  } else
    output = std::make_unique<SkippedRoot>();
}

NameContext::NameContext(SRC::QualifiedNameNode const *node) { visit(node); }
RootContext::RootContext(SRC::SymbolNode const *node) {
  if (0) {}
  dispatch(SRC::FunctionSymbolNode);
  dispatch(SRC::VariableSymbolNode);
  dispatch(SRC::SpecialTableSymbolNode);
  else output = std::make_unique<SkippedRoot>();
}
TypeContext::TypeContext(SRC::FunctionSignatureNode const *node) { visit(node); }
TypeContext::TypeContext(SRC::TypeNode const *node) {
  if (0) {}
  dispatch(SRC::PrimitiveTypeNode);
  dispatch(SRC::PointerTypeNode);
  dispatch(SRC::TagTypeNode);
  dispatch(SRC::FunctionSignatureNode);
  else output = std::make_unique<SkippedType>();
}

NamePieceContext::NamePieceContext(SRC::IdentifierNode const *node) {
  if (0) {}
  dispatch(SRC::LocalNamedIdentifierNode);
  dispatch(SRC::NamedIdentifierNode);
  dispatch(SRC::StructorIdentifierNode);
  dispatch(SRC::IntrinsicFunctionIdentifierNode);
  else output = std::make_unique<SimpleNamePiece>("$SKIP_NAME");
}

} // namespace msvc

template <> std::unique_ptr<RootNode> adapter::Adapt(SRC::SymbolNode const &node) { return msvc::RootContext{&node}; }