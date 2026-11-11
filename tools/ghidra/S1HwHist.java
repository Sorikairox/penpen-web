// Diagnostic: histogram of literal-pool constants that hit a hardware rule of
// S1Regions (value, count, functions), and functions with split bodies.
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import java.util.*;

public class S1HwHist extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory(); Listing L = currentProgram.getListing();
        MemoryBlock img = mem.getBlock("image");
        TreeMap<Long, Integer> h = new TreeMap<>();
        int split = 0; long far = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (!img.contains(f.getEntryPoint())) continue;
            if (f.getBody().getNumAddressRanges() > 1) {
                split++;
                long span = f.getBody().getMaxAddress().getOffset() - f.getBody().getMinAddress().getOffset();
                if (span > 0x10000) { far++; if (far <= 8) println("far " + f.getEntryPoint() + " ranges " + f.getBody().getNumAddressRanges() + " " + f.getBody().getMinAddress() + "-" + f.getBody().getMaxAddress()); }
            }
            if (f.getEntryPoint().getOffset() < 0x8C0998D4L) continue;
            for (Instruction i : L.getInstructions(f.getBody(), true)) {
                int op = mem.getShort(i.getAddress()) & 0xFFFF;
                if ((op & 0xF000) != 0xD000) continue;
                long pa = ((i.getAddress().getOffset() + 4) & ~3L) + (op & 0xFF) * 4;
                if (!img.contains(toAddr(pa))) continue;
                long v = mem.getInt(toAddr(pa)) & 0xFFFFFFFFL;
                long p = v >= 0x80000000L && v < 0xE0000000L ? v & 0x1FFFFFFFL : v;
                boolean hit = v >= 0xFFA00000L && v < 0xFFE00000L || (v & 0xF8FFFFFFL) == 0xF0000000L
                    || p >= 0x005F6800L && p < 0x005FA000L || p >= 0x00700000L && p < 0x00A00000L
                    || p >= 0x10000000L && p < 0x14000000L && (p & 0xFFFFFFL) == 0;
                if (hit) h.merge(v, 1, Integer::sum);
            }
        }
        println("split functions " + split + " spanning >64K " + far);
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<Long, Integer> e : h.entrySet()) sb.append(String.format("%08x:%d ", e.getKey(), e.getValue()));
        println(sb.toString());
    }
}
