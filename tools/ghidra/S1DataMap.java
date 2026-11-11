// Diagnostic: for SDK code, which data/bss addresses each 4 KiB code window
// references (literal-pool pointers into the image's data parts and RAM after it).
// Prints window, count, min / median / max referenced address (addresses only).
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import java.util.*;

public class S1DataMap extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory(); Listing L = currentProgram.getListing();
        String[] a = getScriptArgs();
        long cs = Long.decode(a[0]), ce = Long.decode(a[1]), ds = Long.decode(a[2]), de = Long.decode(a[3]);
        int win = a.length > 4 ? Integer.decode(a[4]) : 0x1000;
        TreeMap<Long, List<Long>> m = new TreeMap<>();
        for (Instruction i : L.getInstructions(toAddr(cs), true)) {
            long o = i.getAddress().getOffset();
            if (o >= ce) break;
            int op = mem.getShort(i.getAddress()) & 0xFFFF;
            if ((op & 0xF000) != 0xD000) continue;
            long pa = ((o + 4) & ~3L) + (op & 0xFF) * 4;
            long v = mem.getInt(toAddr(pa)) & 0xFFFFFFFFL;
            if (v < ds || v >= de) continue;
            m.computeIfAbsent(o / win * win, k -> new ArrayList<>()).add(v);
        }
        for (Map.Entry<Long, List<Long>> e : m.entrySet()) {
            List<Long> v = e.getValue(); Collections.sort(v);
            println(String.format("%08x n %3d min %08x q1 %08x med %08x q3 %08x max %08x", e.getKey(), v.size(), v.get(0), v.get(v.size() / 4), v.get(v.size() / 2), v.get(3 * v.size() / 4), v.get(v.size() - 1)));
        }
    }
}
