// Coverage of the code ranges by functions (counts only).
// Args: code ranges start-end (hex).
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;

public class S1Coverage extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory(); Listing L = currentProgram.getListing();
        FunctionManager fm = currentProgram.getFunctionManager();
        long tr = 0, tf = 0, tp = 0; int tn = 0;
        for (String a : getScriptArgs()) {
            String[] p = a.split("-"); long s = Long.decode(p[0]), e = Long.decode(p[1]);
            long fn = 0, orphan = 0, data = 0, und = 0, zero = 0, pool = 0;
            int n = 0;
            for (Function f : fm.getFunctions(toAddr(s), true)) { if (f.getEntryPoint().getOffset() >= e) break; n++; }
            for (long o = s; o < e; o += 2) {
                Address ad = toAddr(o);
                Instruction i = L.getInstructionContaining(ad);
                if (i != null) { if (fm.getFunctionContaining(ad) != null) fn += 2; else orphan += 2; continue; }
                Data d = L.getDefinedDataContaining(ad);
                if (d != null) {
                    // data between two halves of the same function or right after one = literal pool
                    Function before = fm.getFunctionContaining(L.getInstructionBefore(ad) != null ? L.getInstructionBefore(ad).getAddress() : ad);
                    if (before != null) pool += 2; else data += 2;
                    continue;
                }
                if ((mem.getShort(ad) & 0xFFFF) == 0) zero += 2; else und += 2;
            }
            println(String.format("range %08x-%08x bytes %d functions %d insn_in_func %d (%.1f%%) +pool %d (%.1f%%) orphan_insn %d other_data %d undefined_nonzero %d zero %d",
                s, e, e - s, n, fn, 100.0 * fn / (e - s), pool, 100.0 * (fn + pool) / (e - s), orphan, data, und, zero));
            tr += e - s; tf += fn; tp += pool; tn += n;
        }
        println(String.format("total bytes %d functions %d insn_in_func %d (%.1f%%) insn+pool %d (%.1f%%)", tr, tn, tf, 100.0 * tf / tr, tf + tp, 100.0 * (tf + tp) / tr));
    }
}
