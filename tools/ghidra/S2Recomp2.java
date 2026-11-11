// S2: more recompiler checks (counts/addresses only): write references into the
// code ranges (self-modifying code / code copied in place), flows into the P2
// uncached mirror of the image, PC-relative loads in delay slots (raw opcodes),
// function-pointer words in the data ranges only, and which functions hold an
// ldc ...,vbr.
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.util.*;

public class S2Recomp2 extends GhidraScript {
    boolean code(long a) { return (a >= 0x8C010000L && a < 0x8C066900L) || (a >= 0x8C0998D4L && a < 0x8C17994CL) || (a >= 0x8C18F112L && a < 0x8C197090L); }
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        Listing L = currentProgram.getListing();
        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceManager rm = currentProgram.getReferenceManager();
        Set<String> wr = new TreeSet<>(); int nwr = 0;
        for (Function f : fm.getFunctions(true)) for (Instruction i : L.getInstructions(f.getBody(), true)) {
            for (Reference r : i.getReferencesFrom()) {
                long t = r.getToAddress().getOffset();
                if (r.getReferenceType().isWrite() && code(t) && fm.getFunctionContaining(r.getToAddress()) != null) { nwr++; if (wr.size() < 20) wr.add(String.format("%x>%x", f.getEntryPoint().getOffset(), t)); }
            }
        }
        println("write refs into function bodies " + nwr + " " + wr);
        Set<String> p2 = new TreeSet<>();
        for (Instruction i : L.getInstructions(true)) for (Address a : i.getFlows()) { long t = a.getOffset(); if (t >= 0xAC010000L && t < 0xAC240000L) { Function f = fm.getFunctionContaining(i.getAddress()); p2.add(String.format("%x>%x", f == null ? 0 : f.getEntryPoint().getOffset(), t)); } }
        println("flows into P2 mirror " + p2.size() + " " + p2);
        int ds = 0, dspc = 0, dsmova = 0;
        MemoryBlock img = mem.getBlock("image");
        for (Instruction i : L.getInstructions(true)) {
            if (!img.contains(i.getAddress())) continue;
            String m = i.getMnemonicString().toLowerCase();
            if (!m.matches("bra|bsr|jmp|jsr|rts|rte|braf|bsrf|bt/s|bf/s")) continue;
            int op = mem.getShort(i.getAddress().add(2)) & 0xFFFF;
            ds++;
            if ((op & 0xF000) == 0xD000 || (op & 0xF000) == 0x9000) dspc++;
            if ((op & 0xFF00) == 0xC700) dsmova++;
        }
        println("delayed branches " + ds + " slot mov.l/mov.w @(disp,PC) " + dspc + " slot mova " + dsmova);
        long[][] dr = {{0x8C066904L, 0x8C0998D4L}, {0x8C17994CL, 0x8C18F112L}, {0x8C23D900L, 0x8C23E8BCL}};
        for (long[] r : dr) {
            int w = 0, tab = 0, run = 0, lng = 0; Set<Long> tg = new HashSet<>();
            for (long a = r[0] & ~3L; a + 4 <= r[1]; a += 4) {
                long v = mem.getInt(toAddr(a)) & 0xFFFFFFFFL;
                boolean fp = code(v) && fm.getFunctionAt(toAddr(v)) != null;
                if (fp) { w++; tg.add(v); run++; } else { if (run >= 3) { tab++; lng = Math.max(lng, run); } run = 0; }
            }
            println(String.format("data %x-%x: function-pointer words %d, distinct targets %d, tables(>=3) %d, longest %d", r[0], r[1], w, tg.size(), tab, lng));
        }
        for (Function f : fm.getFunctions(true)) for (Instruction i : L.getInstructions(f.getBody(), true))
            if (i.getMnemonicString().toLowerCase().startsWith("ldc") && i.toString().toLowerCase().endsWith("vbr")) {
                boolean reached = rm.getReferencesTo(i.getAddress()).hasNext() || L.getInstructionBefore(i.getAddress()) != null;
                println(String.format("ldc vbr at %x in %x (%s)", i.getAddress().getOffset(), f.getEntryPoint().getOffset(), f.getName()));
            }
    }
}
