//===- SparcInstrInfo.cpp - Sparc Instruction Information -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the Sparc implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "SparcInstrInfo.h"
#include "SparcSubtarget.h"
#include "Sparc.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "SparcGenInstrInfo.inc"
using namespace llvm;

SparcInstrInfo::SparcInstrInfo(SparcSubtarget &ST)
  : TargetInstrInfoImpl(SparcInsts, array_lengthof(SparcInsts)),
    RI(ST, *this), Subtarget(ST) {
}

static bool isZeroImm(const MachineOperand &op) {
  return op.isImmediate() && op.getImm() == 0;
}

/// Return true if the instruction is a register to register move and
/// leave the source and dest operands in the passed parameters.
///
bool SparcInstrInfo::isMoveInstr(const MachineInstr &MI,
                                 unsigned &SrcReg, unsigned &DstReg) const {
  // We look for 3 kinds of patterns here:
  // or with G0 or 0
  // add with G0 or 0
  // fmovs or FpMOVD (pseudo double move).
  if (MI.getOpcode() == SP::ORrr || MI.getOpcode() == SP::ADDrr) {
    if (MI.getOperand(1).getReg() == SP::G0) {
      DstReg = MI.getOperand(0).getReg();
      SrcReg = MI.getOperand(2).getReg();
      return true;
    } else if (MI.getOperand(2).getReg() == SP::G0) {
      DstReg = MI.getOperand(0).getReg();
      SrcReg = MI.getOperand(1).getReg();
      return true;
    }
  } else if ((MI.getOpcode() == SP::ORri || MI.getOpcode() == SP::ADDri) &&
             isZeroImm(MI.getOperand(2)) && MI.getOperand(1).isRegister()) {
    DstReg = MI.getOperand(0).getReg();
    SrcReg = MI.getOperand(1).getReg();
    return true;
  } else if (MI.getOpcode() == SP::FMOVS || MI.getOpcode() == SP::FpMOVD ||
             MI.getOpcode() == SP::FMOVD) {
    SrcReg = MI.getOperand(1).getReg();
    DstReg = MI.getOperand(0).getReg();
    return true;
  }
  return false;
}

/// isLoadFromStackSlot - If the specified machine instruction is a direct
/// load from a stack slot, return the virtual or physical register number of
/// the destination along with the FrameIndex of the loaded stack slot.  If
/// not, return 0.  This predicate must return 0 if the instruction has
/// any side effects other than loading from the stack slot.
unsigned SparcInstrInfo::isLoadFromStackSlot(MachineInstr *MI,
                                             int &FrameIndex) const {
  if (MI->getOpcode() == SP::LDri ||
      MI->getOpcode() == SP::LDFri ||
      MI->getOpcode() == SP::LDDFri) {
    if (MI->getOperand(1).isFrameIndex() && MI->getOperand(2).isImmediate() &&
        MI->getOperand(2).getImm() == 0) {
      FrameIndex = MI->getOperand(1).getIndex();
      return MI->getOperand(0).getReg();
    }
  }
  return 0;
}

/// isStoreToStackSlot - If the specified machine instruction is a direct
/// store to a stack slot, return the virtual or physical register number of
/// the source reg along with the FrameIndex of the loaded stack slot.  If
/// not, return 0.  This predicate must return 0 if the instruction has
/// any side effects other than storing to the stack slot.
unsigned SparcInstrInfo::isStoreToStackSlot(MachineInstr *MI,
                                            int &FrameIndex) const {
  if (MI->getOpcode() == SP::STri ||
      MI->getOpcode() == SP::STFri ||
      MI->getOpcode() == SP::STDFri) {
    if (MI->getOperand(0).isFrameIndex() && MI->getOperand(1).isImmediate() &&
        MI->getOperand(1).getImm() == 0) {
      FrameIndex = MI->getOperand(0).getIndex();
      return MI->getOperand(2).getReg();
    }
  }
  return 0;
}

unsigned
SparcInstrInfo::InsertBranch(MachineBasicBlock &MBB,MachineBasicBlock *TBB,
                             MachineBasicBlock *FBB,
                             const std::vector<MachineOperand> &Cond)const{
  // Can only insert uncond branches so far.
  assert(Cond.empty() && !FBB && TBB && "Can only handle uncond branches!");
  BuildMI(&MBB, get(SP::BA)).addMBB(TBB);
  return 1;
}

void SparcInstrInfo::copyRegToReg(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator I,
                                     unsigned DestReg, unsigned SrcReg,
                                     const TargetRegisterClass *DestRC,
                                     const TargetRegisterClass *SrcRC) const {
  if (DestRC != SrcRC) {
    cerr << "Not yet supported!";
    abort();
  }

  if (DestRC == SP::IntRegsRegisterClass)
    BuildMI(MBB, I, get(SP::ORrr), DestReg).addReg(SP::G0).addReg(SrcReg);
  else if (DestRC == SP::FPRegsRegisterClass)
    BuildMI(MBB, I, get(SP::FMOVS), DestReg).addReg(SrcReg);
  else if (DestRC == SP::DFPRegsRegisterClass)
    BuildMI(MBB, I, get(Subtarget.isV9() ? SP::FMOVD : SP::FpMOVD),DestReg)
      .addReg(SrcReg);
  else
    assert (0 && "Can't copy this register");
}

void SparcInstrInfo::
storeRegToStackSlot(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                    unsigned SrcReg, bool isKill, int FI,
                    const TargetRegisterClass *RC) const {
  // On the order of operands here: think "[FrameIdx + 0] = SrcReg".
  if (RC == SP::IntRegsRegisterClass)
    BuildMI(MBB, I, get(SP::STri)).addFrameIndex(FI).addImm(0)
      .addReg(SrcReg, false, false, isKill);
  else if (RC == SP::FPRegsRegisterClass)
    BuildMI(MBB, I, get(SP::STFri)).addFrameIndex(FI).addImm(0)
      .addReg(SrcReg, false, false, isKill);
  else if (RC == SP::DFPRegsRegisterClass)
    BuildMI(MBB, I, get(SP::STDFri)).addFrameIndex(FI).addImm(0)
      .addReg(SrcReg, false, false, isKill);
  else
    assert(0 && "Can't store this register to stack slot");
}

void SparcInstrInfo::storeRegToAddr(MachineFunction &MF, unsigned SrcReg,
                                       bool isKill,
                                       SmallVectorImpl<MachineOperand> &Addr,
                                       const TargetRegisterClass *RC,
                                 SmallVectorImpl<MachineInstr*> &NewMIs) const {
  unsigned Opc = 0;
  if (RC == SP::IntRegsRegisterClass)
    Opc = SP::STri;
  else if (RC == SP::FPRegsRegisterClass)
    Opc = SP::STFri;
  else if (RC == SP::DFPRegsRegisterClass)
    Opc = SP::STDFri;
  else
    assert(0 && "Can't load this register");
  MachineInstrBuilder MIB = BuildMI(get(Opc));
  for (unsigned i = 0, e = Addr.size(); i != e; ++i) {
    MachineOperand &MO = Addr[i];
    if (MO.isRegister())
      MIB.addReg(MO.getReg());
    else if (MO.isImmediate())
      MIB.addImm(MO.getImm());
    else {
      assert(MO.isFI());
      MIB.addFrameIndex(MO.getIndex());
    }
  }
  MIB.addReg(SrcReg, false, false, isKill);
  NewMIs.push_back(MIB);
  return;
}

void SparcInstrInfo::
loadRegFromStackSlot(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                     unsigned DestReg, int FI,
                     const TargetRegisterClass *RC) const {
  if (RC == SP::IntRegsRegisterClass)
    BuildMI(MBB, I, get(SP::LDri), DestReg).addFrameIndex(FI).addImm(0);
  else if (RC == SP::FPRegsRegisterClass)
    BuildMI(MBB, I, get(SP::LDFri), DestReg).addFrameIndex(FI).addImm(0);
  else if (RC == SP::DFPRegsRegisterClass)
    BuildMI(MBB, I, get(SP::LDDFri), DestReg).addFrameIndex(FI).addImm(0);
  else
    assert(0 && "Can't load this register from stack slot");
}

void SparcInstrInfo::loadRegFromAddr(MachineFunction &MF, unsigned DestReg,
                                        SmallVectorImpl<MachineOperand> &Addr,
                                        const TargetRegisterClass *RC,
                                 SmallVectorImpl<MachineInstr*> &NewMIs) const {
  unsigned Opc = 0;
  if (RC == SP::IntRegsRegisterClass)
    Opc = SP::LDri;
  else if (RC == SP::FPRegsRegisterClass)
    Opc = SP::LDFri;
  else if (RC == SP::DFPRegsRegisterClass)
    Opc = SP::LDDFri;
  else
    assert(0 && "Can't load this register");
  MachineInstrBuilder MIB = BuildMI(get(Opc), DestReg);
  for (unsigned i = 0, e = Addr.size(); i != e; ++i) {
    MachineOperand &MO = Addr[i];
    if (MO.isReg())
      MIB.addReg(MO.getReg());
    else if (MO.isImm())
      MIB.addImm(MO.getImm());
    else {
      assert(MO.isFI());
      MIB.addFrameIndex(MO.getIndex());
    }
  }
  NewMIs.push_back(MIB);
  return;
}

MachineInstr *SparcInstrInfo::foldMemoryOperand(MachineFunction &MF,
                                                MachineInstr* MI,
                                                SmallVectorImpl<unsigned> &Ops,
                                                int FI) const {
  if (Ops.size() != 1) return NULL;

  unsigned OpNum = Ops[0];
  bool isFloat = false;
  MachineInstr *NewMI = NULL;
  switch (MI->getOpcode()) {
  case SP::ORrr:
    if (MI->getOperand(1).isRegister() && MI->getOperand(1).getReg() == SP::G0&&
        MI->getOperand(0).isRegister() && MI->getOperand(2).isRegister()) {
      if (OpNum == 0)    // COPY -> STORE
        NewMI = BuildMI(get(SP::STri)).addFrameIndex(FI).addImm(0)
                                   .addReg(MI->getOperand(2).getReg());
      else               // COPY -> LOAD
        NewMI = BuildMI(get(SP::LDri), MI->getOperand(0).getReg())
                      .addFrameIndex(FI).addImm(0);
    }
    break;
  case SP::FMOVS:
    isFloat = true;
    // FALLTHROUGH
  case SP::FMOVD:
    if (OpNum == 0)  // COPY -> STORE
      NewMI = BuildMI(get(isFloat ? SP::STFri : SP::STDFri))
               .addFrameIndex(FI).addImm(0).addReg(MI->getOperand(1).getReg());
    else             // COPY -> LOAD
      NewMI = BuildMI(get(isFloat ? SP::LDFri : SP::LDDFri),
                     MI->getOperand(0).getReg()).addFrameIndex(FI).addImm(0);
    break;
  }

  if (NewMI)
    NewMI->copyKillDeadInfo(MI);
  return NewMI;
}
