//===--- RDFLiveness.h ----------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Recalculate the liveness information given a data flow graph.
// This includes block live-ins and kill flags.

#ifndef RDF_LIVENESS_H
#define RDF_LIVENESS_H

#include "RDFGraph.h"
#include "llvm/ADT/DenseMap.h"
#include <map>

using namespace llvm;

namespace llvm {
  class MachineBasicBlock;
  class MachineFunction;
  class MachineRegisterInfo;
  class TargetRegisterInfo;
  class MachineDominatorTree;
  class MachineDominanceFrontier;

namespace rdf {
  struct Liveness {
  public:
    // This is really a std::map, except that it provides a non-trivial
    // default constructor to the element accessed via [].
    struct LiveMapType {
      LiveMapType(const PhysicalRegisterInfo &pri) : Empty(pri) {}

      RegisterAggr &operator[] (MachineBasicBlock *B) {
        return Map.emplace(B, Empty).first->second;
      }
    private:
      RegisterAggr Empty;
      std::map<MachineBasicBlock*,RegisterAggr> Map;
    };

    typedef std::pair<NodeId,LaneBitmask> NodeRef;
    typedef std::set<NodeRef> NodeRefSet;
    // RegisterId in RefMap must be normalized.
    typedef std::map<RegisterId,NodeRefSet> RefMap;

    Liveness(MachineRegisterInfo &mri, const DataFlowGraph &g)
      : DFG(g), TRI(g.getTRI()), PRI(g.getPRI()), MDT(g.getDT()),
        MDF(g.getDF()), MRI(mri), LiveMap(g.getPRI()), Empty(),
        NoRegs(g.getPRI()), Trace(false) {}

    NodeList getAllReachingDefs(RegisterRef RefRR, NodeAddr<RefNode*> RefA,
        bool FullChain, const RegisterAggr &DefRRs);
    NodeList getAllReachingDefs(NodeAddr<RefNode*> RefA) {
      return getAllReachingDefs(RefA.Addr->getRegRef(DFG), RefA, false, NoRegs);
    }
    NodeList getAllReachingDefs(RegisterRef RefRR, NodeAddr<RefNode*> RefA) {
      return getAllReachingDefs(RefRR, RefA, false, NoRegs);
    }
    NodeSet getAllReachingDefsRec(RegisterRef RefRR, NodeAddr<RefNode*> RefA,
        NodeSet &Visited, const NodeSet &Defs);
    NodeSet getAllReachedUses(RegisterRef RefRR, NodeAddr<DefNode*> DefA,
        const RegisterAggr &DefRRs);
    NodeSet getAllReachedUses(RegisterRef RefRR, NodeAddr<DefNode*> DefA) {
      return getAllReachedUses(RefRR, DefA, NoRegs);
    }

    LiveMapType &getLiveMap() { return LiveMap; }
    const LiveMapType &getLiveMap() const { return LiveMap; }
    const RefMap &getRealUses(NodeId P) const {
      auto F = RealUseMap.find(P);
      return F == RealUseMap.end() ? Empty : F->second;
    }

    void computePhiInfo();
    void computeLiveIns();
    void resetLiveIns();
    void resetKills();
    void resetKills(MachineBasicBlock *B);

    void trace(bool T) { Trace = T; }

  private:
    const DataFlowGraph &DFG;
    const TargetRegisterInfo &TRI;
    const PhysicalRegisterInfo &PRI;
    const MachineDominatorTree &MDT;
    const MachineDominanceFrontier &MDF;
    MachineRegisterInfo &MRI;
    LiveMapType LiveMap;
    const RefMap Empty;
    const RegisterAggr NoRegs;
    bool Trace;

    // Cache of mapping from node ids (for RefNodes) to the containing
    // basic blocks. Not computing it each time for each node reduces
    // the liveness calculation time by a large fraction.
    typedef DenseMap<NodeId,MachineBasicBlock*> NodeBlockMap;
    NodeBlockMap NBMap;

    // Phi information:
    //
    // RealUseMap
    // map: NodeId -> (map: RegisterId -> NodeRefSet)
    //      phi id -> (map: register -> set of reached non-phi uses)
    std::map<NodeId, RefMap> RealUseMap;

    // Inverse iterated dominance frontier.
    std::map<MachineBasicBlock*,std::set<MachineBasicBlock*>> IIDF;

    // Live on entry.
    std::map<MachineBasicBlock*,RefMap> PhiLON;

    // Phi uses are considered to be located at the end of the block that
    // they are associated with. The reaching def of a phi use dominates the
    // block that the use corresponds to, but not the block that contains
    // the phi itself. To include these uses in the liveness propagation (up
    // the dominator tree), create a map: block -> set of uses live on exit.
    std::map<MachineBasicBlock*,RefMap> PhiLOX;

    bool isRestrictedToRef(NodeAddr<InstrNode*> IA, NodeAddr<RefNode*> RA,
        RegisterRef RR) const;
    RegisterRef getRestrictedRegRef(NodeAddr<RefNode*> RA) const;
    MachineBasicBlock *getBlockWithRef(NodeId RN) const;
    void traverse(MachineBasicBlock *B, RefMap &LiveIn);
    void emptify(RefMap &M);
  };
} // namespace rdf
} // namespace llvm

#endif // RDF_LIVENESS_H
