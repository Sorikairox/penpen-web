// S2: references to the given addresses (addresses only): calling functions,
// and pointer words holding the address (with the function that loads that
// word, or 'data'). Also scans the image for aligned pointer words.
// Args: addr ...
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.util.*;

public class S2Xref extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        MemoryBlock img = mem.getBlock("image");
        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceManager rm = currentProgram.getReferenceManager();
        byte[] d = new byte[(int) img.getSize()];
        img.getBytes(img.getStart(), d);
        long base = img.getStart().getOffset();
        for (String s : getScriptArgs()) {
            long a = Long.parseLong(s.replace("0x", ""), 16);
            StringBuilder sb = new StringBuilder(String.format("%x:", a));
            Set<String> c = new TreeSet<>();
            for (Reference r : rm.getReferencesTo(toAddr(a))) {
                Function f = fm.getFunctionContaining(r.getFromAddress());
                c.add((r.getReferenceType().isCall() ? "call:" : r.getReferenceType().isData() ? "dref:" : "ref:") + (f == null ? String.format("@%x", r.getFromAddress().getOffset()) : String.format("%x", f.getEntryPoint().getOffset())));
            }
            for (int o = 0; o + 4 <= d.length; o += 4) {
                long v = (d[o] & 0xFFL) | (d[o + 1] & 0xFFL) << 8 | (d[o + 2] & 0xFFL) << 16 | (d[o + 3] & 0xFFL) << 24;
                if (v != a) continue;
                Function f = fm.getFunctionContaining(toAddr(base + o));
                String who = "";
                for (Reference r : rm.getReferencesTo(toAddr(base + o))) {
                    Function g = fm.getFunctionContaining(r.getFromAddress());
                    if (g != null) who += String.format("<%x", g.getEntryPoint().getOffset());
                }
                c.add(String.format("word@%x%s%s", base + o, f != null ? "(pool)" : "", who));
            }
            sb.append(" ").append(String.join(" ", c));
            println(sb.toString());
        }
    }
}
