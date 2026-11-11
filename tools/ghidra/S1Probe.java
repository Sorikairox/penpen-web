// Diagnostic counts (no disassembly output): entry alignment, orphan code,
// unresolved call targets.
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.address.*;
import java.util.*;

public class S1Probe extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        MemoryBlock img = mem.getBlock("image");
        Listing L = currentProgram.getListing();
        FunctionManager fm = currentProgram.getFunctionManager();
        int[] al = new int[4];
        for (Function f : fm.getFunctions(true)) al[(int) (f.getEntryPoint().getOffset() & 3)]++;
        println("entry mod4: " + Arrays.toString(al));
        long orphanBytes = 0; int orphanRuns = 0; Address prev = null;
        long[] ow = new long[40];
        for (Instruction i : L.getInstructions(img.getStart(), true)) {
            if (!img.contains(i.getAddress())) break;
            if (fm.getFunctionContaining(i.getAddress()) == null) {
                orphanBytes += i.getLength();
                ow[(int) ((i.getAddress().getOffset() - 0x8C010000L) >> 16)] += i.getLength();
                if (prev == null || !prev.add(2).equals(i.getAddress())) orphanRuns++;
                prev = i.getAddress();
            }
        }
        println("orphan insn bytes " + orphanBytes + " runs " + orphanRuns);
        StringBuilder sb = new StringBuilder();
        for (int k = 0; k < ow.length; k++) if (ow[k] > 0) sb.append(String.format("%02x:%d ", k, ow[k]));
        println("orphan by 64K window " + sb);
        // jsr/jmp literal targets
        int jsrT = 0, jsrNotFn = 0, jsrNotInsn = 0;
        for (Instruction i : L.getInstructions(img.getStart(), true)) {
            if (!img.contains(i.getAddress())) break;
            String m = i.getMnemonicString();
            if (!m.equals("jsr")) continue;
            int op = mem.getShort(i.getAddress()) & 0xFFFF;
            int rn = (op >> 8) & 15;
            Instruction p = i;
            for (int k = 0; k < 10; k++) {
                p = p.getPrevious();
                if (p == null) break;
                int po = mem.getShort(p.getAddress()) & 0xFFFF;
                if ((po & 0xFF00) == (0xD000 | (rn << 8))) {
                    long la = ((p.getAddress().getOffset() + 4) & ~3L) + (po & 0xFF) * 4;
                    long t = mem.getInt(toAddr(la)) & 0xFFFFFFFFL;
                    if (img.contains(toAddr(t))) {
                        jsrT++;
                        if (fm.getFunctionAt(toAddr(t)) == null) jsrNotFn++;
                        if (L.getInstructionAt(toAddr(t)) == null) jsrNotInsn++;
                    }
                    break;
                }
                if (((po >> 8) & 15) == rn && (po & 0xF000) != 0x2000) break;
            }
        }
        println("jsr literal targets " + jsrT + " not function " + jsrNotFn + " not insn " + jsrNotInsn);
        int refs = 0;
        for (Instruction i : L.getInstructions(img.getStart(), true)) {
            if (!img.contains(i.getAddress())) break;
            if (i.getMnemonicString().equals("jsr") && i.getReferencesFrom().length > 0) refs++;
        }
        println("jsr with references " + refs);
        // per window: function bytes, data bytes, undefined non-zero bytes, rts halfwords in undefined bytes
        int W = (int) ((img.getSize() + 0xFFFF) >> 16);
        long[] fb = new long[W], db = new long[W], ub = new long[W], ur = new long[W], up = new long[W];
        long base = img.getStart().getOffset();
        for (long o = base; o <= img.getEnd().getOffset(); o += 2) {
            Address a = toAddr(o); int w = (int) ((o - base) >> 16);
            if (fm.getFunctionContaining(a) != null) { fb[w] += 2; continue; }
            if (L.getInstructionContaining(a) != null) continue;
            if (L.getDefinedDataContaining(a) != null) { db[w] += 2; continue; }
            int h = mem.getShort(a) & 0xFFFF;
            if (h != 0) ub[w] += 2;
            if (h == 0x000B) ur[w]++;
            if (h == 0x4F22) up[w]++;
        }
        for (int k = 0; k < W; k++)
            println(String.format("w%02x fn %6d data %6d undef_nz %6d undef_rts %4d undef_stspr %4d", k, fb[k], db[k], ub[k], ur[k], up[k]));
    }
}
