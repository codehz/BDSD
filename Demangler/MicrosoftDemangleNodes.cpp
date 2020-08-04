//===- MicrosoftDemangle.cpp ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines a demangler for MSVC-style mangled symbols.
//
//===----------------------------------------------------------------------===//

#include "include/MicrosoftDemangleNodes.h"
#include "include/Demangle.h"
#include "include/DemangleConfig.h"
#include "include/StringView.h"
#include "include/Utility.h"
#include <cctype>
#include <string>
#include <sstream>

using namespace llvm;
using namespace ms_demangle;

#define OUTPUT_ENUM_CLASS_VALUE(Enum, Value, Desc)                                                                     \
  case Enum::Value: OS << Desc; break;

// Writes a space if the last token does not end with a punctuation.
static void outputSpaceIfNecessary(OutputStream &OS) {
  if (OS.empty()) return;

  char C = OS.back();
  if (std::isalnum(C) || C == '>') OS << " ";
}

static void outputSingleQualifier(OutputStream &OS, Qualifiers Q) {
  switch (Q) {
  case Q_Const: OS << "const"; break;
  case Q_Volatile: OS << "volatile"; break;
  case Q_Restrict: OS << "__restrict"; break;
  default: break;
  }
}

static bool outputQualifierIfPresent(OutputStream &OS, Qualifiers Q, Qualifiers Mask, bool NeedSpace) {
  if (!(Q & Mask)) return NeedSpace;

  if (NeedSpace) OS << " ";

  outputSingleQualifier(OS, Mask);
  return true;
}

static void outputQualifiers(OutputStream &OS, Qualifiers Q, bool SpaceBefore, bool SpaceAfter) {
  if (Q == Q_None) return;

  size_t Pos1 = OS.getCurrentPosition();
  SpaceBefore = outputQualifierIfPresent(OS, Q, Q_Const, SpaceBefore);
  SpaceBefore = outputQualifierIfPresent(OS, Q, Q_Volatile, SpaceBefore);
  SpaceBefore = outputQualifierIfPresent(OS, Q, Q_Restrict, SpaceBefore);
  size_t Pos2 = OS.getCurrentPosition();
  if (SpaceAfter && Pos2 > Pos1) OS << " ";
}

static void outputCallingConvention(OutputStream &OS, CallingConv CC) {
  outputSpaceIfNecessary(OS);

  switch (CC) {
  case CallingConv::Cdecl: OS << "__cdecl"; break;
  case CallingConv::Fastcall: OS << "__fastcall"; break;
  case CallingConv::Pascal: OS << "__pascal"; break;
  case CallingConv::Regcall: OS << "__regcall"; break;
  case CallingConv::Stdcall: OS << "__stdcall"; break;
  case CallingConv::Thiscall: OS << "__thiscall"; break;
  case CallingConv::Eabi: OS << "__eabi"; break;
  case CallingConv::Vectorcall: OS << "__vectorcall"; break;
  case CallingConv::Clrcall: OS << "__clrcall"; break;
  default: break;
  }
}

std::string Node::toString(OutputFlags Flags) const {
  OutputStream OS;
  initializeOutputStream(nullptr, nullptr, OS, 1024);
  this->output(OS, Flags);
  OS << '\0';
  return {OS.getBuffer()};
}

void PrimitiveTypeNode::outputPre(OutputStream &OS, OutputFlags Flags) const {
  switch (PrimKind) {
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Void, "void");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Bool, "bool");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Char, "char");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Schar, "signed char");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Uchar, "unsigned char");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Char8, "char8_t");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Char16, "char16_t");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Char32, "char32_t");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Short, "short");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Ushort, "unsigned short");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Int, "int");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Uint, "unsigned int");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Long, "long");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Ulong, "unsigned long");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Int64, "__int64");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Uint64, "unsigned __int64");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Wchar, "wchar_t");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Float, "float");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Double, "double");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Ldouble, "long double");
    OUTPUT_ENUM_CLASS_VALUE(PrimitiveKind, Nullptr, "std::nullptr_t");
  }
  outputQualifiers(OS, Quals, true, false);
}

void NodeArrayNode::output(OutputStream &OS, OutputFlags Flags) const { output(OS, Flags, ", "); }

void NodeArrayNode::output(OutputStream &OS, OutputFlags Flags, StringView Separator) const {
  if (Count == 0) return;
  if (Nodes[0]) Nodes[0]->output(OS, Flags);
  for (size_t I = 1; I < Count; ++I) {
    OS << Separator;
    Nodes[I]->output(OS, Flags);
  }
}

void EncodedStringLiteralNode::output(OutputStream &OS, OutputFlags Flags) const {
  switch (Char) {
  case CharKind::Wchar: OS << "L\""; break;
  case CharKind::Char: OS << "\""; break;
  case CharKind::Char16: OS << "u\""; break;
  case CharKind::Char32: OS << "U\""; break;
  }
  OS << DecodedString << "\"";
  if (IsTruncated) OS << "...";
}

void IntegerLiteralNode::output(OutputStream &OS, OutputFlags Flags) const {
  if (IsNegative) OS << '-';
  OS << Value;
}

void TemplateParameterReferenceNode::output(OutputStream &OS, OutputFlags Flags) const {
  if (ThunkOffsetCount > 0)
    OS << "{";
  else if (Affinity == PointerAffinity::Pointer)
    OS << "&";

  if (Symbol) {
    Symbol->output(OS, Flags);
    if (ThunkOffsetCount > 0) OS << ", ";
  }

  if (ThunkOffsetCount > 0) OS << ThunkOffsets[0];
  for (int I = 1; I < ThunkOffsetCount; ++I) { OS << ", " << ThunkOffsets[I]; }
  if (ThunkOffsetCount > 0) OS << "}";
}

void IdentifierNode::outputTemplateParameters(OutputStream &OS, OutputFlags Flags) const {
  if (!TemplateParams) return;
  OS << "<";
  TemplateParams->output(OS, Flags);
  OS << ">";
}

void DynamicStructorIdentifierNode::output(OutputStream &OS, OutputFlags Flags) const {
  if (IsDestructor)
    OS << "`dynamic atexit destructor for ";
  else
    OS << "`dynamic initializer for ";

  if (Variable) {
    OS << "`";
    Variable->output(OS, Flags);
    OS << "''";
  } else {
    OS << "'";
    Name->output(OS, Flags);
    OS << "''";
  }
}

void NamedIdentifierNode::output(OutputStream &OS, OutputFlags Flags) const {
  OS << Name;
  outputTemplateParameters(OS, Flags);
}

void IntrinsicFunctionIdentifierNode::output(OutputStream &OS, OutputFlags Flags) const {
  switch (Operator) {
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, New, "operator new");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, Delete, "operator delete");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, Assign, "operator=");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, RightShift, "operator>>");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, LeftShift, "operator<<");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, LogicalNot, "operator!");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, Equals, "operator==");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, NotEquals, "operator!=");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, ArraySubscript, "operator[]");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, Pointer, "operator->");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, Increment, "operator++");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, Decrement, "operator--");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, Minus, "operator-");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, Plus, "operator+");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, Dereference, "operator*");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, BitwiseAnd, "operator&");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, MemberPointer, "operator->*");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, Divide, "operator/");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, Modulus, "operator%");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, LessThan, "operator<");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, LessThanEqual, "operator<=");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, GreaterThan, "operator>");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, GreaterThanEqual, "operator>=");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, Comma, "operator,");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, Parens, "operator()");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, BitwiseNot, "operator~");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, BitwiseXor, "operator^");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, BitwiseOr, "operator|");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, LogicalAnd, "operator&&");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, LogicalOr, "operator||");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, TimesEqual, "operator*=");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, PlusEqual, "operator+=");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, MinusEqual, "operator-=");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, DivEqual, "operator/=");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, ModEqual, "operator%=");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, RshEqual, "operator>>=");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, LshEqual, "operator<<=");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, BitwiseAndEqual, "operator&=");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, BitwiseOrEqual, "operator|=");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, BitwiseXorEqual, "operator^=");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, VbaseDtor, "`vbase dtor'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, VecDelDtor, "`vector deleting dtor'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, DefaultCtorClosure, "`default ctor closure'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, ScalarDelDtor, "`scalar deleting dtor'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, VecCtorIter, "`vector ctor iterator'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, VecDtorIter, "`vector dtor iterator'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, VecVbaseCtorIter, "`vector vbase ctor iterator'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, VdispMap, "`virtual displacement map'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, EHVecCtorIter, "`eh vector ctor iterator'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, EHVecDtorIter, "`eh vector dtor iterator'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, EHVecVbaseCtorIter, "`eh vector vbase ctor iterator'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, CopyCtorClosure, "`copy ctor closure'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, LocalVftableCtorClosure, "`local vftable ctor closure'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, ArrayNew, "operator new[]");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, ArrayDelete, "operator delete[]");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, ManVectorCtorIter, "`managed vector ctor iterator'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, ManVectorDtorIter, "`managed vector dtor iterator'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, EHVectorCopyCtorIter, "`EH vector copy ctor iterator'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, EHVectorVbaseCopyCtorIter, "`EH vector vbase copy ctor iterator'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, VectorCopyCtorIter, "`vector copy ctor iterator'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, VectorVbaseCopyCtorIter, "`vector vbase copy constructor iterator'");
    OUTPUT_ENUM_CLASS_VALUE(
        IntrinsicFunctionKind, ManVectorVbaseCopyCtorIter, "`managed vector vbase copy constructor iterator'");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, CoAwait, "operator co_await");
    OUTPUT_ENUM_CLASS_VALUE(IntrinsicFunctionKind, Spaceship, "operator<=>");
  case IntrinsicFunctionKind::MaxIntrinsic:
  case IntrinsicFunctionKind::None: break;
  }
  outputTemplateParameters(OS, Flags);
}

void LocalStaticGuardIdentifierNode::output(OutputStream &OS, OutputFlags Flags) const {
  if (IsThread)
    OS << "`local static thread guard'";
  else
    OS << "`local static guard'";
  if (ScopeIndex > 0) OS << "{" << ScopeIndex << "}";
}

void ConversionOperatorIdentifierNode::output(OutputStream &OS, OutputFlags Flags) const {
  OS << "operator";
  outputTemplateParameters(OS, Flags);
  OS << " ";
  TargetType->output(OS, Flags);
}

void StructorIdentifierNode::output(OutputStream &OS, OutputFlags Flags) const {
  if (IsDestructor) OS << "~";
  Class->output(OS, Flags);
  outputTemplateParameters(OS, Flags);
}

void LiteralOperatorIdentifierNode::output(OutputStream &OS, OutputFlags Flags) const {
  OS << "operator \"\"" << Name;
  outputTemplateParameters(OS, Flags);
}

void FunctionSignatureNode::outputPre(OutputStream &OS, OutputFlags Flags) const {
  if (!(Flags & OF_NoAccessSpecifier)) {
    if (FunctionClass & FC_Public) OS << "public: ";
    if (FunctionClass & FC_Protected) OS << "protected: ";
    if (FunctionClass & FC_Private) OS << "private: ";
  }

  if (!(Flags & OF_NoMemberType)) {
    if (!(FunctionClass & FC_Global)) {
      if (FunctionClass & FC_Static) OS << "static ";
    }
    if (FunctionClass & FC_Virtual) OS << "virtual ";

    if (FunctionClass & FC_ExternC) OS << "extern \"C\" ";
  }

  if (!(Flags & OF_NoReturnType) && ReturnType) {
    ReturnType->outputPre(OS, Flags);
    OS << " ";
  }

  if (!(Flags & OF_NoCallingConvention)) outputCallingConvention(OS, CallConvention);
}

void FunctionSignatureNode::outputPost(OutputStream &OS, OutputFlags Flags) const {
  if (!(FunctionClass & FC_NoParameterList)) {
    OS << "(";
    if (Params)
      Params->output(OS, Flags);
    else
      OS << "void";

    if (IsVariadic) {
      if (OS.back() != '(') OS << ", ";
      OS << "...";
    }
    OS << ")";
  }

  if (Quals & Q_Const) OS << " const";
  if (Quals & Q_Volatile) OS << " volatile";
  if (Quals & Q_Restrict) OS << " __restrict";
  if (Quals & Q_Unaligned) OS << " __unaligned";

  if (IsNoexcept) OS << " noexcept";

  if (RefQualifier == FunctionRefQualifier::Reference)
    OS << " &";
  else if (RefQualifier == FunctionRefQualifier::RValueReference)
    OS << " &&";

  if (!(Flags & OF_NoReturnType) && ReturnType) ReturnType->outputPost(OS, Flags);
}

void ThunkSignatureNode::outputPre(OutputStream &OS, OutputFlags Flags) const {
  OS << "[thunk]: ";

  FunctionSignatureNode::outputPre(OS, Flags);
}

void ThunkSignatureNode::outputPost(OutputStream &OS, OutputFlags Flags) const {
  if (FunctionClass & FC_StaticThisAdjust) {
    OS << "`adjustor{" << ThisAdjust.StaticOffset << "}'";
  } else if (FunctionClass & FC_VirtualThisAdjust) {
    if (FunctionClass & FC_VirtualThisAdjustEx) {
      OS << "`vtordispex{" << ThisAdjust.VBPtrOffset << ", " << ThisAdjust.VBOffsetOffset << ", "
         << ThisAdjust.VtordispOffset << ", " << ThisAdjust.StaticOffset << "}'";
    } else {
      OS << "`vtordisp{" << ThisAdjust.VtordispOffset << ", " << ThisAdjust.StaticOffset << "}'";
    }
  }

  FunctionSignatureNode::outputPost(OS, Flags);
}

void PointerTypeNode::outputPre(OutputStream &OS, OutputFlags Flags) const {
  if (Pointee->kind() == NodeKind::FunctionSignature) {
    // If this is a pointer to a function, don't output the calling convention.
    // It needs to go inside the parentheses.
    const FunctionSignatureNode *Sig = static_cast<const FunctionSignatureNode *>(Pointee);
    Sig->outputPre(OS, OF_NoCallingConvention);
  } else
    Pointee->outputPre(OS, Flags);

  outputSpaceIfNecessary(OS);

  if (Quals & Q_Unaligned) OS << "__unaligned ";

  if (Pointee->kind() == NodeKind::ArrayType) {
    OS << "(";
  } else if (Pointee->kind() == NodeKind::FunctionSignature) {
    OS << "(";
    const FunctionSignatureNode *Sig = static_cast<const FunctionSignatureNode *>(Pointee);
    outputCallingConvention(OS, Sig->CallConvention);
    OS << " ";
  }

  if (ClassParent) {
    ClassParent->output(OS, Flags);
    OS << "::";
  }

  switch (Affinity) {
  case PointerAffinity::Pointer: OS << "*"; break;
  case PointerAffinity::Reference: OS << "&"; break;
  case PointerAffinity::RValueReference: OS << "&&"; break;
  default: assert(false);
  }
  outputQualifiers(OS, Quals, false, false);
}

void PointerTypeNode::outputPost(OutputStream &OS, OutputFlags Flags) const {
  if (Pointee->kind() == NodeKind::ArrayType || Pointee->kind() == NodeKind::FunctionSignature) OS << ")";

  Pointee->outputPost(OS, Flags);
}

void TagTypeNode::outputPre(OutputStream &OS, OutputFlags Flags) const {
  if (!(Flags & OF_NoTagSpecifier)) {
    switch (Tag) {
      OUTPUT_ENUM_CLASS_VALUE(TagKind, Class, "class");
      OUTPUT_ENUM_CLASS_VALUE(TagKind, Struct, "struct");
      OUTPUT_ENUM_CLASS_VALUE(TagKind, Union, "union");
      OUTPUT_ENUM_CLASS_VALUE(TagKind, Enum, "enum");
    }
    OS << " ";
  }
  QualifiedName->output(OS, Flags);
  outputQualifiers(OS, Quals, true, false);
}

void TagTypeNode::outputPost(OutputStream &OS, OutputFlags Flags) const {}

void ArrayTypeNode::outputPre(OutputStream &OS, OutputFlags Flags) const {
  ElementType->outputPre(OS, Flags);
  outputQualifiers(OS, Quals, true, false);
}

void ArrayTypeNode::outputOneDimension(OutputStream &OS, OutputFlags Flags, Node *N) const {
  assert(N->kind() == NodeKind::IntegerLiteral);
  IntegerLiteralNode *ILN = static_cast<IntegerLiteralNode *>(N);
  if (ILN->Value != 0) ILN->output(OS, Flags);
}

void ArrayTypeNode::outputDimensionsImpl(OutputStream &OS, OutputFlags Flags) const {
  if (Dimensions->Count == 0) return;

  outputOneDimension(OS, Flags, Dimensions->Nodes[0]);
  for (size_t I = 1; I < Dimensions->Count; ++I) {
    OS << "][";
    outputOneDimension(OS, Flags, Dimensions->Nodes[I]);
  }
}

void ArrayTypeNode::outputPost(OutputStream &OS, OutputFlags Flags) const {
  OS << "[";
  outputDimensionsImpl(OS, Flags);
  OS << "]";

  ElementType->outputPost(OS, Flags);
}

void SymbolNode::output(OutputStream &OS, OutputFlags Flags) const { Name->output(OS, Flags); }

void FunctionSymbolNode::output(OutputStream &OS, OutputFlags Flags) const {
  Signature->outputPre(OS, Flags);
  outputSpaceIfNecessary(OS);
  Name->output(OS, Flags);
  Signature->outputPost(OS, Flags);
}

void VariableSymbolNode::output(OutputStream &OS, OutputFlags Flags) const {
  const char *AccessSpec = nullptr;
  bool IsStatic          = true;
  switch (SC) {
  case StorageClass::PrivateStatic: AccessSpec = "private"; break;
  case StorageClass::PublicStatic: AccessSpec = "public"; break;
  case StorageClass::ProtectedStatic: AccessSpec = "protected"; break;
  default: IsStatic = false; break;
  }
  if (!(Flags & OF_NoAccessSpecifier) && AccessSpec) OS << AccessSpec << ": ";
  if (!(Flags & OF_NoMemberType) && IsStatic) OS << "static ";

  if (Type) {
    Type->outputPre(OS, Flags);
    outputSpaceIfNecessary(OS);
  }
  Name->output(OS, Flags);
  if (Type) Type->outputPost(OS, Flags);
}

void CustomTypeNode::outputPre(OutputStream &OS, OutputFlags Flags) const { Identifier->output(OS, Flags); }
void CustomTypeNode::outputPost(OutputStream &OS, OutputFlags Flags) const {}

void QualifiedNameNode::output(OutputStream &OS, OutputFlags Flags) const { Components->output(OS, Flags, "::"); }

void RttiBaseClassDescriptorNode::output(OutputStream &OS, OutputFlags Flags) const {
  OS << "`RTTI Base Class Descriptor at (";
  OS << NVOffset << ", " << VBPtrOffset << ", " << VBTableOffset << ", " << this->Flags;
  OS << ")'";
}

void LocalStaticGuardVariableNode::output(OutputStream &OS, OutputFlags Flags) const { Name->output(OS, Flags); }

void VcallThunkIdentifierNode::output(OutputStream &OS, OutputFlags Flags) const {
  OS << "`vcall'{" << OffsetInVTable << ", {flat}}";
}

void SpecialTableSymbolNode::output(OutputStream &OS, OutputFlags Flags) const {
  outputQualifiers(OS, Quals, false, true);
  Name->output(OS, Flags);
  if (TargetName) {
    OS << "{for `";
    TargetName->output(OS, Flags);
    OS << "'}";
  }
  return;
}

bool TypeNode::operator==(Node const &rhs) const {
  if (!Node::operator==(rhs)) return false;
  auto &r = (TypeNode const &) rhs;
  return Quals == r.Quals;
}

bool PrimitiveTypeNode::operator==(Node const &rhs) const {
  if (!TypeNode::operator==(rhs)) return false;
  auto &r = (PrimitiveTypeNode const &) rhs;
  return PrimKind == r.PrimKind;
}

bool FunctionSignatureNode::operator==(Node const &rhs) const {
  if (!TypeNode::operator==(rhs)) return false;
  auto &r = (FunctionSignatureNode const &) rhs;
  return Affinity == r.Affinity && CallConvention == r.CallConvention && FunctionClass == r.FunctionClass &&
         RefQualifier == r.RefQualifier && ReturnType == r.ReturnType && IsVariadic == r.IsVariadic &&
         Params == r.Params && IsNoexcept == r.IsNoexcept;
}

bool IdentifierNode::operator==(Node const &rhs) const {
  if (!Node::operator==(rhs)) return false;
  auto &r = (IdentifierNode const &) rhs;
  if ((TemplateParams == nullptr) != (r.TemplateParams == nullptr)) return false;
  return TemplateParams && TemplateParams->operator==(*r.TemplateParams);
}

bool VcallThunkIdentifierNode::operator==(Node const &rhs) const {
  if (!IdentifierNode::operator==(rhs)) return false;
  auto &r = (VcallThunkIdentifierNode const &) rhs;
  return OffsetInVTable == r.OffsetInVTable;
}

bool DynamicStructorIdentifierNode::operator==(Node const &rhs) const {
  if (!IdentifierNode::operator==(rhs)) return false;
  auto &r = (DynamicStructorIdentifierNode const &) rhs;
  return Variable == r.Variable && Name == r.Name && IsDestructor == r.IsDestructor;
}

bool NamedIdentifierNode::operator==(Node const &rhs) const {
  if (!IdentifierNode::operator==(rhs)) return false;
  auto &r = (NamedIdentifierNode const &) rhs;
  return Name == r.Name;
}

bool IntrinsicFunctionIdentifierNode::operator==(Node const &rhs) const {
  if (!IdentifierNode::operator==(rhs)) return false;
  auto &r = (IntrinsicFunctionIdentifierNode const &) rhs;
  return Operator == r.Operator;
}

bool LiteralOperatorIdentifierNode::operator==(Node const &rhs) const {
  if (!IdentifierNode::operator==(rhs)) return false;
  auto &r = (LiteralOperatorIdentifierNode const &) rhs;
  return Name == r.Name;
}

bool LocalStaticGuardIdentifierNode::operator==(Node const &rhs) const {
  if (!IdentifierNode::operator==(rhs)) return false;
  auto &r = (LocalStaticGuardIdentifierNode const &) rhs;
  return IsThread == r.IsThread && ScopeIndex == r.ScopeIndex;
}

bool ConversionOperatorIdentifierNode::operator==(Node const &rhs) const {
  if (!IdentifierNode::operator==(rhs)) return false;
  auto &r = (ConversionOperatorIdentifierNode const &) rhs;
  if ((TargetType == nullptr) != (r.TargetType == nullptr)) return false;
  return TargetType && TargetType->operator==(*r.TargetType);
  return true;
}

bool StructorIdentifierNode::operator==(Node const &rhs) const {
  if (!IdentifierNode::operator==(rhs)) return false;
  auto &r = (StructorIdentifierNode const &) rhs;
  if ((Class == nullptr) != (r.Class == nullptr)) return false;
  return Class && Class->operator==(*r.Class) && IsDestructor == r.IsDestructor;
  return IsDestructor == r.IsDestructor;
}

bool ThunkSignatureNode::operator==(Node const &rhs) const {
  if (!FunctionSignatureNode::operator==(rhs)) return false;
  auto &r = (ThunkSignatureNode const &) rhs;
  return ThisAdjust == r.ThisAdjust;
}

bool PointerTypeNode::operator==(Node const &rhs) const {
  if (!TypeNode::operator==(rhs)) return false;
  auto &r = (PointerTypeNode const &) rhs;
  if ((ClassParent == nullptr) != (r.ClassParent == nullptr)) return false;
  if (ClassParent && !ClassParent->operator==(*r.ClassParent)) return false;
  if ((Pointee == nullptr) != (r.Pointee == nullptr)) return false;
  if (Pointee && !Pointee->operator==(*r.Pointee)) return false;
  return true;
}

bool TagTypeNode::operator==(Node const &rhs) const {
  if (!TypeNode::operator==(rhs)) return false;
  auto &r = (TagTypeNode const &) rhs;
  if ((QualifiedName == nullptr) != (r.QualifiedName == nullptr)) return false;
  if (QualifiedName && !QualifiedName->operator==(*r.QualifiedName)) return false;
  return Tag == r.Tag;
}

bool ArrayTypeNode::operator==(Node const &rhs) const {
  if (!TypeNode::operator==(rhs)) return false;
  auto &r = (ArrayTypeNode const &) rhs;
  if ((Dimensions == nullptr) != (r.Dimensions == nullptr)) return false;
  if (Dimensions && !Dimensions->operator==(*r.Dimensions)) return false;
  if ((ElementType == nullptr) != (r.ElementType == nullptr)) return false;
  if (ElementType && !ElementType->operator==(*r.ElementType)) return false;
  return true;
}

bool CustomTypeNode::operator==(Node const &rhs) const {
  if (!TypeNode::operator==(rhs)) return false;
  auto &r = (CustomTypeNode const &) rhs;
  if ((Identifier == nullptr) != (r.Identifier == nullptr)) return false;
  if (Identifier && !Identifier->operator==(*r.Identifier)) return false;
  return true;
}

bool NodeArrayNode::operator==(NodeArrayNode const &rhs) const {
  if (Count != rhs.Count) return false;
  for (size_t i = 0; i < Count; ++i)
    if (!Nodes[i]->operator==(*rhs.Nodes[i])) return false;
  return true;
}

bool QualifiedNameNode::operator==(Node const &rhs) const {
  if (!Node::operator==(rhs)) return false;
  auto &r = (QualifiedNameNode const &) rhs;
  if ((Components == nullptr) != (r.Components == nullptr)) return false;
  if (Components && !Components->operator==(*r.Components)) return false;
  return true;
}

bool TemplateParameterReferenceNode::operator==(Node const &rhs) const {
  if (!Node::operator==(rhs)) return false;
  auto &r = (TemplateParameterReferenceNode const &) rhs;
  if ((Symbol == nullptr) != (r.Symbol == nullptr)) return false;
  if (Symbol && !Symbol->operator==(*r.Symbol)) return false;
  return ThunkOffsets == r.ThunkOffsets && Affinity == r.Affinity && IsMemberPointer == r.IsMemberPointer;
}

bool IntegerLiteralNode::operator==(Node const &rhs) const {
  if (!Node::operator==(rhs)) return false;
  auto &r = (IntegerLiteralNode const &) rhs;
  return Value == r.Value && IsNegative == r.IsNegative;
}

bool RttiBaseClassDescriptorNode::operator==(Node const &rhs) const {
  if (!Node::operator==(rhs)) return false;
  auto &r = (RttiBaseClassDescriptorNode const &) rhs;
  return NVOffset == r.NVOffset && VBPtrOffset == r.VBPtrOffset && VBTableOffset == r.VBTableOffset && Flags == r.Flags;
}

bool SymbolNode::operator==(Node const &rhs) const {
  if (!Node::operator==(rhs)) return false;
  auto &r = (SymbolNode const &) rhs;
  if ((Name == nullptr) != (r.Name == nullptr)) return false;
  if (Name && !Name->operator==(*r.Name)) return false;
  return true;
}

bool SpecialTableSymbolNode::operator==(Node const &rhs) const {
  if (!Node::operator==(rhs)) return false;
  auto &r = (SpecialTableSymbolNode const &) rhs;
  if ((TargetName == nullptr) != (r.TargetName == nullptr)) return false;
  if (TargetName && !TargetName->operator==(*r.TargetName)) return false;
  return Quals == r.Quals;
}

bool LocalStaticGuardVariableNode::operator==(Node const &rhs) const {
  if (!Node::operator==(rhs)) return false;
  auto &r = (LocalStaticGuardVariableNode const &) rhs;
  return IsVisible == r.IsVisible;
}

bool EncodedStringLiteralNode::operator==(Node const &rhs) const {
  if (!Node::operator==(rhs)) return false;
  auto &r = (EncodedStringLiteralNode const &) rhs;
  return DecodedString == r.DecodedString && IsTruncated == r.IsTruncated && Char == r.Char;
}

bool VariableSymbolNode::operator==(Node const &rhs) const {
  if (!Node::operator==(rhs)) return false;
  auto &r = (VariableSymbolNode const &) rhs;
  if ((Type == nullptr) != (r.Type == nullptr)) return false;
  if (Type && !Type->operator==(*r.Type)) return false;
  return SC == r.SC;
}

bool FunctionSymbolNode::operator==(Node const &rhs) const {
  if (!Node::operator==(rhs)) return false;
  auto &r = (FunctionSymbolNode const &) rhs;
  if ((Signature == nullptr) != (r.Signature == nullptr)) return false;
  if (Signature && !Signature->operator==(*r.Signature)) return false;
  return true;
}
