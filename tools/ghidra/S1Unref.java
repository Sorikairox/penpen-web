// Counts functions with no references at all (no calls, jumps or data pointers),
// by 64 KiB window. Args: code ranges.
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.util.*;

public class S1Unref extends GhidraScript {
    @Override
    public void run() throws Exception {
        ReferenceManager rm = currentProgram.getReferenceManager();
        TreeMap<Long, int[]> w = new TreeMap<>();
        int n = 0, un = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            long e = f.getEntryPoint().getOffset();
            if (e < 0x8C010000L || e >= 0x8C240000L) continue;
            n++;
            boolean r = rm.getReferenceCountTo(f.getEntryPoint()) > 0;
            int[] c = w.computeIfAbsent(e >> 16, k -> new int[2]);
            c[0]++;
            if (!r) { un++; c[1]++; }
        }
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<Long, int[]> x : w.entrySet()) sb.append(String.format("%x:%d/%d ", x.getKey() & 0xFFF, x.getValue()[1], x.getValue()[0]));
        println("functions " + n + " unreferenced " + un + " by window (unref/all) " + sb);
    }
}
