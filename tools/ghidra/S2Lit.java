// S2: literal-pool constants (mov.l @(disp,PC)) matching a mask/value, with the
// functions that load them. Prints value and loading function addresses only.
// Args: mask value [mask value ...]  (hex)
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import java.util.*;

public class S2Lit extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        String[] a = getScriptArgs();
        long[] m = new long[a.length / 2], v = new long[a.length / 2];
        for (int k = 0; k < m.length; k++) { m[k] = Long.parseLong(a[2 * k], 16); v[k] = Long.parseLong(a[2 * k + 1], 16); }
        Map<Long, Set<String>> hit = new TreeMap<>();
        for (Instruction i : currentProgram.getListing().getInstructions(true)) {
            long pc = i.getAddress().getOffset();
            int op = mem.getShort(i.getAddress()) & 0xFFFF;
            if ((op & 0xF000) != 0xD000) continue;
            long val;
            try { val = mem.getInt(toAddr(((pc + 4) & ~3L) + (op & 0xFF) * 4L)) & 0xFFFFFFFFL; } catch (Exception e) { continue; }
            for (int k = 0; k < m.length; k++) if ((val & m[k]) == v[k]) {
                Function f = currentProgram.getFunctionManager().getFunctionContaining(i.getAddress());
                hit.computeIfAbsent(val, x -> new TreeSet<>()).add(f == null ? "?" : Long.toHexString(f.getEntryPoint().getOffset()));
            }
        }
        for (Map.Entry<Long, Set<String>> e : hit.entrySet()) println(String.format("%08x %s", e.getKey(), String.join(",", e.getValue())));
    }
}
