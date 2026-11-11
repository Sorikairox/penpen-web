// S2: direct hardware / BIOS-vector tags per function (literal values the
// function loads), written to <outdir>/hw.tsv in the sandbox volume.
// Args: outdir
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import java.io.*;
import java.util.*;

public class S2Hw extends GhidraScript {
    Memory mem;
    long u32(long a) { try { return mem.getInt(toAddr(a)) & 0xFFFFFFFFL; } catch (Exception e) { return -1; } }
    int u16(long a) { try { return mem.getShort(toAddr(a)) & 0xFFFF; } catch (Exception e) { return -1; } }
    @Override
    public void run() throws Exception {
        mem = currentProgram.getMemory();
        Listing L = currentProgram.getListing();
        PrintWriter w = new PrintWriter(new FileWriter(getScriptArgs()[0] + "/hw.tsv"));
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            Set<String> t = new TreeSet<>();
            Map<String, Integer> mn = new TreeMap<>();
            for (Instruction i : L.getInstructions(f.getBody(), true)) {
                long pc = i.getAddress().getOffset();
                int op = u16(pc);
                String m = i.getMnemonicString().toLowerCase();
                if (m.matches("pref|ocbi|ocbp|ocbwb|movca.l|fschg|frchg|trapa|sleep|rte|ldtlb|fsca|fsrra|ftrv|fipr") || (m.startsWith("ldc") && i.toString().toLowerCase().matches(".*,(sr|vbr|ssr|spc|gbr|dbr)")) || (m.startsWith("lds") && i.toString().toLowerCase().contains("fpscr")) || (m.startsWith("stc") && i.toString().toLowerCase().contains("vbr")))
                    mn.merge(m.startsWith("ldc") || m.startsWith("lds") || m.startsWith("stc") ? m + ":" + i.toString().toLowerCase().replaceAll(".*,", "").replaceAll("^.*(vbr|fpscr).*$", "$1") : m, 1, Integer::sum);
                long v;
                if ((op & 0xF000) == 0xD000) v = u32(((pc + 4) & ~3L) + (op & 0xFF) * 4L);
                else continue;
                if (v >= 0x8C000000L && v < 0x8C010000L) t.add(String.format("sys%x", v));
                long p = v & 0x1FFFFFFFL, top = v >>> 29;
                if ((top == 5 || top == 4 || top == 0) && p >= 0x005F6800L && p < 0x005FA000L) t.add(String.format("hw%x", p & 0xFFFFFF00L));
                if (v >= 0xA0700000L && v < 0xA0A00000L) t.add("aica");
                if (v >= 0xFF000000L) t.add(String.format("p4_%x", v & 0xFFFF0000L));
                if (v >= 0xE0000000L && v < 0xE4000000L) t.add("sq");
                if (v >= 0x10000000L && v < 0x12000000L && (v & 0xFFFF) == 0) t.add("ta");
                if ((v >>> 24) == 0xA4 || (v >>> 24) == 0xA5) t.add("vram");
            }
            if (t.isEmpty() && mn.isEmpty()) continue;
            StringBuilder sb = new StringBuilder();
            for (Map.Entry<String, Integer> e : mn.entrySet()) sb.append(e.getKey()).append("=").append(e.getValue()).append(",");
            w.println(String.format("%08x\t%s\t%s", f.getEntryPoint().getOffset(), String.join(",", t), sb));
        }
        w.close();
        println("hw done");
    }
}
