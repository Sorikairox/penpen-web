// S2: for every game function that calls SDK code, the ordered list of its
// calls (SDK and game targets, by address/assigned name) with constant
// arguments recovered by a backward scan of r4..r7 in the same block.
// Output stays in the sandbox volume. Args: game_start game_end outdir
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class S2Seq extends GhidraScript {
    Memory mem; Listing L; FunctionManager fm; MemoryBlock img;
    long gs, ge;

    long u32(long a) { try { return mem.getInt(toAddr(a)) & 0xFFFFFFFFL; } catch (Exception e) { return -1; } }
    int u16(long a) { try { return mem.getShort(toAddr(a)) & 0xFFFF; } catch (Exception e) { return -1; } }
    Long litOf(Instruction i, int[] rn) {
        int op = u16(i.getAddress().getOffset()); long pc = i.getAddress().getOffset();
        if ((op & 0xF000) == 0xD000) { rn[0] = (op >> 8) & 15; return u32(((pc + 4) & ~3L) + (op & 0xFF) * 4L); }
        if ((op & 0xF000) == 0x9000) { rn[0] = (op >> 8) & 15; return ((long) (short) u16(pc + 4 + (op & 0xFF) * 2L)) & 0xFFFFFFFFL; }
        if ((op & 0xF000) == 0xE000) { rn[0] = (op >> 8) & 15; return ((long) (byte) (op & 0xFF)) & 0xFFFFFFFFL; }
        return null;
    }
    // destination general register of an instruction (n field) for the common forms
    int dst(Instruction i) {
        Object[] res = i.getResultObjects();
        for (Object o : res) if (o instanceof ghidra.program.model.lang.Register) {
            String n = ((ghidra.program.model.lang.Register) o).getName();
            if (n.matches("r[0-9]+")) return Integer.parseInt(n.substring(1));
        }
        return -1;
    }
    Function resolve(Function f) {
        int n = 0;
        while (f != null && f.isThunk() && n++ < 4) { Function t = f.getThunkedFunction(false); if (t == null) break; f = t; }
        return f;
    }
    String nm(long a) {
        Function f = fm.getFunctionAt(toAddr(a));
        if (f == null) return String.format("?%x", a);
        String n = f.getName();
        return n.startsWith("FUN_") ? String.format("%x", a) : n;
    }
    String val(long v) {
        if (fm.getFunctionAt(toAddr(v)) != null) return "&" + nm(v);
        if (v >= 0x8C000000L && v < 0x8D000000L) return String.format("g%x", v);
        if (v < 0x10000 || v > 0xFFFF0000L) return Integer.toString((int) v);
        return String.format("0x%x", v);
    }

    @Override
    public void run() throws Exception {
        mem = currentProgram.getMemory(); L = currentProgram.getListing(); fm = currentProgram.getFunctionManager();
        img = mem.getBlock("image");
        String[] a = getScriptArgs();
        gs = Long.decode(a[0]); ge = Long.decode(a[1]);
        PrintWriter w = new PrintWriter(new FileWriter(a[2] + "/seq.txt"));
        int[] rn = new int[1];
        for (Function f : fm.getFunctions(true)) {
            long fe = f.getEntryPoint().getOffset();
            if (fe < 0x8C010000L || fe >= 0x8C197090L) continue;
            StringBuilder sb = new StringBuilder();
            boolean sdk = false;
            for (Instruction i : L.getInstructions(f.getBody(), true)) {
                if (!i.getFlowType().isCall()) continue;
                long t0 = -1;
                for (Reference r : i.getReferencesFrom()) if (r.getReferenceType().isCall()) t0 = r.getToAddress().getOffset();
                for (Address fl : i.getFlows()) t0 = fl.getOffset();
                String tn;
                if (t0 < 0) tn = "IND";
                else {
                    Function t = resolve(fm.getFunctionAt(toAddr(t0)));
                    long te = t == null ? t0 : t.getEntryPoint().getOffset();
                    if (!(te >= gs && te < ge)) sdk = true;
                    tn = (te >= gs && te < ge ? "" : "S:") + nm(te);
                }
                // args
                String[] av = new String[4];
                List<Instruction> scan = new ArrayList<>();
                Instruction ds = L.getInstructionAfter(i.getAddress());
                if (ds != null) scan.add(ds);
                Instruction p = i;
                for (int k = 0; k < 14; k++) { p = L.getInstructionBefore(p.getAddress()); if (p == null || p.getFlowType().isCall() || p.getFlowType().isJump() || p.getFlowType().isTerminal() || p.getFlowType().isConditional()) break; scan.add(p); }
                boolean[] done = new boolean[16];
                for (Instruction q : scan) {
                    Long v = litOf(q, rn);
                    int d = v != null ? rn[0] : dst(q);
                    if (d < 4 || d > 7 || done[d]) continue;
                    done[d] = true;
                    if (v != null) av[d - 4] = val(v);
                    else {
                        String m = q.getMnemonicString().toLowerCase();
                        av[d - 4] = m.startsWith("mov") && q.toString().contains("r15") ? "stk" : m.equals("mov") ? "r" : "~";
                    }
                }
                int last = -1; for (int k = 0; k < 4; k++) if (av[k] != null) last = k;
                sb.append(" ").append(tn).append("(");
                for (int k = 0; k <= last; k++) sb.append(k > 0 ? "," : "").append(av[k] == null ? "_" : av[k]);
                sb.append(")");
            }
            if (sdk || (fe >= gs && fe < ge)) if (sb.length() > 0) w.println(String.format("%08x%s", fe, sb));
        }
        w.close();
        println("seq done");
    }
}
