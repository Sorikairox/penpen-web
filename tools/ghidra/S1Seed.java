// Second-pass function discovery for 1ST_READ.BIN (session S1).
// Auto-analysis only finds code reachable by calls from the entry point. This
// adds, until nothing changes:
//   A. orphan code: disassembled instructions in no function, whose start is
//      not reached by fall-through (a run after data, padding or an rts/bra);
//   B. function pointers: aligned words in non-code parts of the image that
//      point at an undefined, valid-looking subroutine in the image;
//   C. gap starts: the first undefined halfword after a function end (past nop
//      padding and literal-pool data) when it begins a valid subroutine
//      (counted apart when it has an SHC prologue: sts.l pr,@-r15 /
//      mov.l rN,@-r15 / add #-n,r15 / fmov to stack).
// Arguments: the code ranges (start-end, hex); nothing is created outside them.
// Each new function is followed by Ghidra's own analysis (analyzeChanges).
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.app.util.PseudoDisassembler;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.util.*;

public class S1Seed extends GhidraScript {
    Memory mem; Listing L; FunctionManager fm; MemoryBlock img; PseudoDisassembler pd;
    long[] rs, re;

    boolean prologue(int op) {
        if (op == 0x4F22) return true;                         // sts.l pr,@-r15
        if ((op & 0xFF0F) == 0x2F06 && ((op >> 4) & 15) >= 8) return true; // mov.l r8..r14,@-r15
        if ((op & 0xFF00) == 0x7F00 && (op & 0x80) != 0) return true;       // add #-n,r15
        if ((op & 0xFF0F) == 0xFF0B) return true;               // fmov.s frN,@-r15
        if (op == 0x4F02 || op == 0x4F12 || op == 0x4F52 || op == 0x4F62) return true;      // sts.l mach/fpul/fpscr,@-r15
        return false;
    }

    int u16(Address a) throws Exception { return mem.getShort(a) & 0xFFFF; }

    boolean inCode(Address a) { long o = a.getOffset(); for (int k = 0; k < rs.length; k++) if (o >= rs[k] && o < re[k]) return true; return false; }

    boolean tryCreate(Address a, Map<String,Integer> stat, String why) throws Exception {
        if (fm.getFunctionAt(a) != null || fm.getFunctionContaining(a) != null) return false;
        if (L.getInstructionAt(a) == null) {
            if (L.getDefinedDataContaining(a) != null) return false;
            if (!pd.isValidSubroutine(a, true)) return false;
            disassemble(a);
            if (L.getInstructionAt(a) == null) return false;
        }
        Function f = createFunction(a, null);
        if (f == null) return false;
        stat.merge(why, 1, Integer::sum);
        return true;
    }

    @Override
    public void run() throws Exception {
        mem = currentProgram.getMemory(); L = currentProgram.getListing();
        fm = currentProgram.getFunctionManager(); img = mem.getBlock("image");
        pd = new PseudoDisassembler(currentProgram);
        String[] args = getScriptArgs();
        // args: code ranges as start-end pairs (hex), e.g. 0x8C010000-0x8C066900
        rs = new long[args.length]; re = new long[args.length];
        for (int k = 0; k < args.length; k++) {
            String[] p = args[k].split("-"); rs[k] = Long.decode(p[0]); re[k] = Long.decode(p[1]);
        }
        Map<String,Integer> stat = new TreeMap<>();
        for (int round = 0; round < 80; round++) {
            int before = fm.getFunctionCount();
            // A: orphan runs
            List<Address> orphan = new ArrayList<>();
            for (Instruction i : L.getInstructions(img.getStart(), true)) {
                if (!inCode(i.getAddress())) continue;
                if (fm.getFunctionContaining(i.getAddress()) != null) continue;
                Instruction p = L.getInstructionContaining(i.getAddress().subtract(2));
                boolean start = p == null || fm.getFunctionContaining(p.getAddress()) != null
                    && !p.getFlowType().isFallthrough();
                if (p != null && p.isInDelaySlot()) {
                    Instruction b = L.getInstructionBefore(p.getAddress());
                    start = b != null && !b.getFlowType().isConditional()
                        && (b.getFlowType().isTerminal() || b.getFlowType().isJump());
                } else if (p != null && !p.getFlowType().hasFallthrough()) start = true;
                if (start) orphan.add(i.getAddress());
            }
            for (Address a : orphan) tryCreate(a, stat, "A_orphan");
            // B: pointer words in non-instruction parts of the whole image
            for (long o = img.getStart().getOffset(); o + 4 <= img.getEnd().getOffset() + 1; o += 4) {
                Address w = toAddr(o);
                if (L.getInstructionContaining(w) != null) continue;
                long t = mem.getInt(w) & 0xFFFFFFFFL;
                if ((t & 1) != 0 || !img.contains(toAddr(t))) continue;
                Address ta = toAddr(t);
                if (!inCode(ta)) continue;
                if (L.getInstructionAt(ta) != null && fm.getFunctionAt(ta) == null
                        && fm.getFunctionContaining(ta) == null) { tryCreate(ta, stat, "B_ptr_insn"); continue; }
                if (L.getInstructionAt(ta) == null)
                    tryCreate(ta, stat, prologue(u16(ta)) ? "B_ptr_prologue" : "B_ptr_valid");
            }
            // C: gap prologues after each function
            List<Address> gaps = new ArrayList<>();
            for (Function f : fm.getFunctions(img.getStart(), true)) {
                if (!inCode(f.getEntryPoint())) continue;
                Address a = f.getBody().getMaxAddress().add(1);
                if ((a.getOffset() & 1) != 0) a = a.add(1);
                for (int k = 0; k < 4096 && inCode(a); k++) {
                    if (fm.getFunctionContaining(a) != null || L.getInstructionAt(a) != null) break;
                    Data d = L.getDefinedDataContaining(a);
                    if (d != null) { a = d.getMaxAddress().add(1); if ((a.getOffset() & 1) != 0) a = a.add(1); continue; }
                    int op = u16(a);
                    if (op == 0x0009 || op == 0x0000) { a = a.add(2); continue; }
                    gaps.add(a);
                    break;
                }
            }
            for (Address a : gaps) tryCreate(a, stat, prologue(u16(a)) ? "C_gap_prologue" : "C_gap_leaf");
            // D: far chunks: a body range far from the entry (reached by a tail
            // bra/jmp) is another function
            List<Address> far = new ArrayList<>();
            for (Function f : fm.getFunctions(img.getStart(), true)) {
                if (!inCode(f.getEntryPoint()) || f.getBody().getNumAddressRanges() < 2) continue;
                AddressRange er = f.getBody().getRangeContaining(f.getEntryPoint());
                for (AddressRange r : f.getBody()) {
                    if (r.equals(er)) continue;
                    long dist = r.getMinAddress().compareTo(er.getMinAddress()) > 0
                        ? r.getMinAddress().subtract(er.getMaxAddress()) : er.getMinAddress().subtract(r.getMaxAddress());
                    if (dist > 0x1000) far.add(r.getMinAddress());
                }
            }
            for (Address a : far) {
                Function owner = fm.getFunctionContaining(a);
                if (owner == null || owner.getEntryPoint().equals(a)) continue;
                Function nf = createFunction(a, null);
                if (nf != null) {
                    stat.merge("D_far_chunk", 1, Integer::sum);
                    ghidra.app.cmd.function.CreateFunctionCmd.fixupFunctionBody(currentProgram, owner, monitor);
                }
            }
            analyzeChanges(currentProgram);
            int after = fm.getFunctionCount();
            println("round " + round + " functions " + before + " -> " + after + " " + stat);
            if (after == before) break;
        }
    }
}
