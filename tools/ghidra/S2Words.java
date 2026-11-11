// S2: prints aligned 32-bit words in [start,end) that are addresses inside main
// RAM (0x8C000000-0x8D000000) or small integers (< 0x100000); any other value
// is printed only as '-' (no raw data leaves the sandbox).
// Args: start end
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.*;

public class S2Words extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        long s = Long.parseLong(getScriptArgs()[0], 16), e = Long.parseLong(getScriptArgs()[1], 16);
        StringBuilder sb = new StringBuilder();
        for (long a = s; a < e; a += 4) {
            long v;
            try { v = mem.getInt(toAddr(a)) & 0xFFFFFFFFL; } catch (Exception x) { sb.append(String.format("%x:?\n", a)); continue; }
            String t = (v >= 0x8C000000L && v < 0x8D000000L) ? String.format("%08x", v) : v < 0x100000 ? Long.toString(v) : "-";
            sb.append(String.format("%x:%s ", a, t));
            if (((a - s) / 4) % 8 == 7) sb.append("\n");
        }
        println(sb.toString());
    }
}
