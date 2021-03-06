//===-- Function.cpp - Implement the Global object classes ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Function class for the VMCore library.
//
//===----------------------------------------------------------------------===//

#include "llvm/Module.h"
#include "llvm/DerivedTypes.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/Support/LeakDetector.h"
#include "llvm/Support/StringPool.h"
#include "SymbolTableListTraitsImpl.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringExtras.h"
using namespace llvm;

BasicBlock *ilist_traits<BasicBlock>::createSentinel() {
  BasicBlock *Ret = BasicBlock::Create();
  // This should not be garbage monitored.
  LeakDetector::removeGarbageObject(Ret);
  return Ret;
}

iplist<BasicBlock> &ilist_traits<BasicBlock>::getList(Function *F) {
  return F->getBasicBlockList();
}

Argument *ilist_traits<Argument>::createSentinel() {
  Argument *Ret = new Argument(Type::Int32Ty);
  // This should not be garbage monitored.
  LeakDetector::removeGarbageObject(Ret);
  return Ret;
}

iplist<Argument> &ilist_traits<Argument>::getList(Function *F) {
  return F->getArgumentList();
}

// Explicit instantiations of SymbolTableListTraits since some of the methods
// are not in the public header file...
template class SymbolTableListTraits<Argument, Function>;
template class SymbolTableListTraits<BasicBlock, Function>;

//===----------------------------------------------------------------------===//
// Argument Implementation
//===----------------------------------------------------------------------===//

Argument::Argument(const Type *Ty, const std::string &Name, Function *Par)
  : Value(Ty, Value::ArgumentVal) {
  Parent = 0;

  // Make sure that we get added to a function
  LeakDetector::addGarbageObject(this);

  if (Par)
    Par->getArgumentList().push_back(this);
  setName(Name);
}

void Argument::setParent(Function *parent) {
  if (getParent())
    LeakDetector::addGarbageObject(this);
  Parent = parent;
  if (getParent())
    LeakDetector::removeGarbageObject(this);
}

/// getArgNo - Return the index of this formal argument in its containing
/// function.  For example in "void foo(int a, float b)" a is 0 and b is 1. 
unsigned Argument::getArgNo() const {
  const Function *F = getParent();
  assert(F && "Argument is not in a function");
  
  Function::const_arg_iterator AI = F->arg_begin();
  unsigned ArgIdx = 0;
  for (; &*AI != this; ++AI)
    ++ArgIdx;

  return ArgIdx;
}

/// hasByValAttr - Return true if this argument has the byval attribute on it
/// in its containing function.
bool Argument::hasByValAttr() const {
  if (!isa<PointerType>(getType())) return false;
  return getParent()->paramHasAttr(getArgNo()+1, ParamAttr::ByVal);
}

/// hasNoAliasAttr - Return true if this argument has the noalias attribute on
/// it in its containing function.
bool Argument::hasNoAliasAttr() const {
  if (!isa<PointerType>(getType())) return false;
  return getParent()->paramHasAttr(getArgNo()+1, ParamAttr::NoAlias);
}

/// hasSRetAttr - Return true if this argument has the sret attribute on
/// it in its containing function.
bool Argument::hasStructRetAttr() const {
  if (!isa<PointerType>(getType())) return false;
  if (this != getParent()->arg_begin()) return false; // StructRet param must be first param
  return getParent()->paramHasAttr(1, ParamAttr::StructRet);
}

/// addAttr - Add a ParamAttr to an argument
void Argument::addAttr(ParameterAttributes attr) {
  getParent()->setParamAttrs(
    getParent()->getParamAttrs().addAttr(getArgNo() + 1, attr));
}
  
/// removeAttr - Remove a ParamAttr from an argument
void Argument::removeAttr(ParameterAttributes attr) {
  getParent()->setParamAttrs(
    getParent()->getParamAttrs().removeAttr(getArgNo() + 1, attr));
}



//===----------------------------------------------------------------------===//
// Helper Methods in Function
//===----------------------------------------------------------------------===//

const FunctionType *Function::getFunctionType() const {
  return cast<FunctionType>(getType()->getElementType());
}

bool Function::isVarArg() const {
  return getFunctionType()->isVarArg();
}

const Type *Function::getReturnType() const {
  return getFunctionType()->getReturnType();
}

void Function::removeFromParent() {
  getParent()->getFunctionList().remove(this);
}

void Function::eraseFromParent() {
  getParent()->getFunctionList().erase(this);
}

//===----------------------------------------------------------------------===//
// Function Implementation
//===----------------------------------------------------------------------===//

Function::Function(const FunctionType *Ty, LinkageTypes Linkage,
                   const std::string &name, Module *ParentModule)
  : GlobalValue(PointerType::getUnqual(Ty), 
                Value::FunctionVal, 0, 0, Linkage, name) {
  SymTab = new ValueSymbolTable();

  assert((getReturnType()->isFirstClassType() ||getReturnType() == Type::VoidTy
          || isa<StructType>(getReturnType()))
         && "LLVM functions cannot return aggregate values!");

  // If the function has arguments, mark them as lazily built.
  if (Ty->getNumParams())
    SubclassData = 1;   // Set the "has lazy arguments" bit.
  
  // Make sure that we get added to a function
  LeakDetector::addGarbageObject(this);

  if (ParentModule)
    ParentModule->getFunctionList().push_back(this);

  // Ensure intrinsics have the right parameter attributes.
  if (unsigned IID = getIntrinsicID(true))
    setParamAttrs(Intrinsic::getParamAttrs(Intrinsic::ID(IID)));
}

Function::~Function() {
  dropAllReferences();    // After this it is safe to delete instructions.

  // Delete all of the method arguments and unlink from symbol table...
  ArgumentList.clear();
  delete SymTab;

  // Remove the function from the on-the-side collector table.
  clearCollector();
}

void Function::BuildLazyArguments() const {
  // Create the arguments vector, all arguments start out unnamed.
  const FunctionType *FT = getFunctionType();
  for (unsigned i = 0, e = FT->getNumParams(); i != e; ++i) {
    assert(FT->getParamType(i) != Type::VoidTy &&
           "Cannot have void typed arguments!");
    ArgumentList.push_back(new Argument(FT->getParamType(i)));
  }
  
  // Clear the lazy arguments bit.
  const_cast<Function*>(this)->SubclassData &= ~1;
}

size_t Function::arg_size() const {
  return getFunctionType()->getNumParams();
}
bool Function::arg_empty() const {
  return getFunctionType()->getNumParams() == 0;
}

void Function::setParent(Module *parent) {
  if (getParent())
    LeakDetector::addGarbageObject(this);
  Parent = parent;
  if (getParent())
    LeakDetector::removeGarbageObject(this);
}

// dropAllReferences() - This function causes all the subinstructions to "let
// go" of all references that they are maintaining.  This allows one to
// 'delete' a whole class at a time, even though there may be circular
// references... first all references are dropped, and all use counts go to
// zero.  Then everything is deleted for real.  Note that no operations are
// valid on an object that has "dropped all references", except operator
// delete.
//
void Function::dropAllReferences() {
  for (iterator I = begin(), E = end(); I != E; ++I)
    I->dropAllReferences();
  BasicBlocks.clear();    // Delete all basic blocks...
}

void Function::setDoesNotThrow(bool doesNotThrow) {
  PAListPtr PAL = getParamAttrs();
  if (doesNotThrow)
    PAL = PAL.addAttr(0, ParamAttr::NoUnwind);
  else
    PAL = PAL.removeAttr(0, ParamAttr::NoUnwind);
  setParamAttrs(PAL);
}

// Maintain the collector name for each function in an on-the-side table. This
// saves allocating an additional word in Function for programs which do not use
// GC (i.e., most programs) at the cost of increased overhead for clients which
// do use GC.
static DenseMap<const Function*,PooledStringPtr> *CollectorNames;
static StringPool *CollectorNamePool;

bool Function::hasCollector() const {
  return CollectorNames && CollectorNames->count(this);
}

const char *Function::getCollector() const {
  assert(hasCollector() && "Function has no collector");
  return *(*CollectorNames)[this];
}

void Function::setCollector(const char *Str) {
  if (!CollectorNamePool)
    CollectorNamePool = new StringPool();
  if (!CollectorNames)
    CollectorNames = new DenseMap<const Function*,PooledStringPtr>();
  (*CollectorNames)[this] = CollectorNamePool->intern(Str);
}

void Function::clearCollector() {
  if (CollectorNames) {
    CollectorNames->erase(this);
    if (CollectorNames->empty()) {
      delete CollectorNames;
      CollectorNames = 0;
      if (CollectorNamePool->empty()) {
        delete CollectorNamePool;
        CollectorNamePool = 0;
      }
    }
  }
}

/// getIntrinsicID - This method returns the ID number of the specified
/// function, or Intrinsic::not_intrinsic if the function is not an
/// intrinsic, or if the pointer is null.  This value is always defined to be
/// zero to allow easy checking for whether a function is intrinsic or not.  The
/// particular intrinsic functions which correspond to this value are defined in
/// llvm/Intrinsics.h.
///
unsigned Function::getIntrinsicID(bool noAssert) const {
  const ValueName *ValName = this->getValueName();
  if (!ValName)
    return 0;
  unsigned Len = ValName->getKeyLength();
  const char *Name = ValName->getKeyData();
  
  if (Len < 5 || Name[4] != '.' || Name[0] != 'l' || Name[1] != 'l'
      || Name[2] != 'v' || Name[3] != 'm')
    return 0;  // All intrinsics start with 'llvm.'

  assert((Len != 5 || noAssert) && "'llvm.' is an invalid intrinsic name!");

#define GET_FUNCTION_RECOGNIZER
#include "llvm/Intrinsics.gen"
#undef GET_FUNCTION_RECOGNIZER
  assert(noAssert && "Invalid LLVM intrinsic name");
  return 0;
}

std::string Intrinsic::getName(ID id, const Type **Tys, unsigned numTys) { 
  assert(id < num_intrinsics && "Invalid intrinsic ID!");
  const char * const Table[] = {
    "not_intrinsic",
#define GET_INTRINSIC_NAME_TABLE
#include "llvm/Intrinsics.gen"
#undef GET_INTRINSIC_NAME_TABLE
  };
  if (numTys == 0)
    return Table[id];
  std::string Result(Table[id]);
  for (unsigned i = 0; i < numTys; ++i) 
    if (Tys[i])
      Result += "." + MVT::getValueTypeString(MVT::getValueType(Tys[i]));
  return Result;
}

const FunctionType *Intrinsic::getType(ID id, const Type **Tys, 
                                       unsigned numTys) {
  const Type *ResultTy = NULL;
  std::vector<const Type*> ArgTys;
  bool IsVarArg = false;
  
#define GET_INTRINSIC_GENERATOR
#include "llvm/Intrinsics.gen"
#undef GET_INTRINSIC_GENERATOR

  return FunctionType::get(ResultTy, ArgTys, IsVarArg); 
}

PAListPtr Intrinsic::getParamAttrs(ID id) {
  ParameterAttributes Attr = ParamAttr::None;

#define GET_INTRINSIC_ATTRIBUTES
#include "llvm/Intrinsics.gen"
#undef GET_INTRINSIC_ATTRIBUTES

  // Intrinsics cannot throw exceptions.
  Attr |= ParamAttr::NoUnwind;

  ParamAttrsWithIndex PAWI = ParamAttrsWithIndex::get(0, Attr);
  return PAListPtr::get(&PAWI, 1);
}

Function *Intrinsic::getDeclaration(Module *M, ID id, const Type **Tys, 
                                    unsigned numTys) {
  // There can never be multiple globals with the same name of different types,
  // because intrinsics must be a specific type.
  return
    cast<Function>(M->getOrInsertFunction(getName(id, Tys, numTys),
                                          getType(id, Tys, numTys)));
}

// vim: sw=2 ai
