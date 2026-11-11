// Writes <outdir>/strrefs.tsv INSIDE the sandbox volume (never copied to the host):
// string address, functions whose code loads a pointer to it (direct or via
// literal pool), number of pointer words to it, and the text. Used to locate SDK
// library code by its version and error strings.
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class S1Strings extends GhidraScript {
    Memory mem; ReferenceManager rm; FunctionManager fm;

    void funcsUsing(Address a, Set<Function> out, int depth) {
        for (Reference r : rm.getReferencesTo(a)) {
            Address f = r.getFromAddress();
            Function fn = fm.getFunctionContaining(f);
            if (fn != null) out.add(fn);
            else if (depth > 0) funcsUsing(f, out, depth - 1);   // pool word -> loader
        }
    }

    @Override
    public void run() throws Exception {
        mem = currentProgram.getMemory(); rm = currentProgram.getReferenceManager();
        fm = currentProgram.getFunctionManager();
        MemoryBlock img = mem.getBlock("image");
        byte[] d = new byte[(int) img.getSize()];
        img.getBytes(img.getStart(), d);
        long base = img.getStart().getOffset();
        // index: 4-aligned words that point into the image (literal pools and tables)
        Map<Long, List<Long>> ptrs = new HashMap<>();
        for (int o = 0; o + 4 <= d.length; o += 4) {
            long v = (d[o] & 0xFFL) | (d[o + 1] & 0xFFL) << 8 | (d[o + 2] & 0xFFL) << 16 | (d[o + 3] & 0xFFL) << 24;
            if (v >= base && v < base + d.length) ptrs.computeIfAbsent(v, k -> new ArrayList<>()).add(base + o);
        }
        PrintWriter w = new PrintWriter(new FileWriter(getScriptArgs()[0] + "/strrefs.tsv"));
        int n = 0;
        for (int o = 0; o < d.length; ) {
            int e = o;
            while (e < d.length && ((d[e] >= 0x20 && d[e] < 0x7F) || d[e] == 9 || d[e] == 10 || d[e] == 13)) e++;
            if (e - o >= 4 && e < d.length && d[e] == 0) {
                Set<Function> fs = new TreeSet<>(Comparator.comparing(Function::getEntryPoint));
                Address sa = toAddr(base + o);
                funcsUsing(sa, fs, 1);
                for (long p : ptrs.getOrDefault(base + o, List.of())) funcsUsing(toAddr(p), fs, 1);
                StringBuilder sb = new StringBuilder();
                for (Function f : fs) sb.append(sb.length() > 0 ? "," : "").append(f.getEntryPoint());
                String t = new String(d, o, e - o, "ISO-8859-1").replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r");
                w.println(String.format("%08x\t%s\t%d\t%s", base + o, sb, ptrs.getOrDefault(base + o, List.of()).size(), t));
                n++;
                o = e + 1;
            } else o = Math.max(e, o + 1);
        }
        w.close();
        println("strings " + n);
    }
}
