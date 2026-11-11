// S2: for each given function, which of the given 'probe' globals its code (and
// its callees, depth <= 5) loads the address of. Addresses only.
// Args: probe1,probe2,... fn1 fn2 ...
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.util.*;

public class S2Reach extends GhidraScript {
    Memory mem; Listing L; FunctionManager fm; Set<Long> probes = new TreeSet<>();
    long u32(long a) { try { return mem.getInt(toAddr(a)) & 0xFFFFFFFFL; } catch (Exception e) { return -1; } }
    int u16(long a) { try { return mem.getShort(toAddr(a)) & 0xFFFF; } catch (Exception e) { return -1; } }
    void walk(Function f, int d, Set<Long> seen, Set<Long> hit, Set<Long> fns) {
        if (f == null || !seen.add(f.getEntryPoint().getOffset())) return;
        for (Instruction i : L.getInstructions(f.getBody(), true)) {
            long pc = i.getAddress().getOffset(); int op = u16(pc);
            if ((op & 0xF000) == 0xD000) {
                long v = u32(((pc + 4) & ~3L) + (op & 0xFF) * 4L);
                for (long p : probes) if (v >= p && v < p + 16) hit.add(p);
                Function t = fm.getFunctionAt(toAddr(v));
                if (t != null && d < 5) { fns.add(v); walk(t, d + 1, seen, hit, fns); }
            }
            if (i.getFlowType().isCall() && d < 5) for (Address a : i.getFlows()) { fns.add(a.getOffset()); walk(fm.getFunctionAt(a), d + 1, seen, hit, fns); }
        }
    }
    @Override
    public void run() throws Exception {
        mem = currentProgram.getMemory(); L = currentProgram.getListing(); fm = currentProgram.getFunctionManager();
        String[] a = getScriptArgs();
        for (String p : a[0].split(",")) probes.add(Long.parseLong(p, 16));
        for (int k = 1; k < a.length; k++) {
            Set<Long> hit = new TreeSet<>(), fns = new TreeSet<>();
            walk(fm.getFunctionAt(toAddr(Long.parseLong(a[k], 16))), 0, new HashSet<>(), hit, fns);
            StringBuilder sb = new StringBuilder(a[k] + ": ");
            for (long h : hit) sb.append(Long.toHexString(h)).append(" ");
            println(sb.toString() + " | fns " + fns.size());
        }
    }
}
