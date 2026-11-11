// S2: census of the SDK functions the game code calls. For each distinct SDK
// target (thunks resolved) prints one feature line: call sites / callers from
// the game, decompiler prototype (parameter count, float params, return size),
// constants the game passes in r4..r7 / fr4.. at its call sites, hardware
// register blocks and BIOS vectors touched (the function and its SDK callees to
// depth 3), SDK callees, SDK-identifier tokens from strings it loads (only
// identifier-shaped tokens with an SDK prefix leave the sandbox), FPU/cache op
// counts and loop count. No code text is printed.
// Args: game_start game_end(rt start) outfile
//@category PenPen
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.block.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.pcode.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;
import java.util.regex.*;

public class S2Census extends GhidraScript {
    Memory mem; Listing L; FunctionManager fm; ReferenceManager rm; MemoryBlock img;
    long gs, ge;
    static final Pattern ID = Pattern.compile("(?:ADX[A-Z]*_|adx[a-z]*_|mw[A-Z]|MW[A-Z]+_|SFD_|MPS_|MPV_|SJ[A-Z_]|sd[A-Z]|nj[A-Z]|km[A-Z]|gdFs|GDFS|pd[A-Z]|bu[A-Z]|sy[A-Z]|kd[A-Z]|fm[A-Z])[A-Za-z0-9_]{1,40}");

    boolean game(long a) { return a >= gs && a < ge; }

    Function resolve(Function f) {
        int n = 0;
        while (f != null && f.isThunk() && n++ < 4) { Function t = f.getThunkedFunction(false); if (t == null) break; f = t; }
        return f;
    }

    long u32(long a) { try { return mem.getInt(toAddr(a)) & 0xFFFFFFFFL; } catch (Exception e) { return -1; } }
    int u16(long a) { try { return mem.getShort(toAddr(a)) & 0xFFFF; } catch (Exception e) { return -1; } }

    // literal value loaded by a PC-relative mov, or immediate; null otherwise
    Long litOf(Instruction i, int[] rnOut) {
        int op = u16(i.getAddress().getOffset());
        long pc = i.getAddress().getOffset();
        if ((op & 0xF000) == 0xD000) { rnOut[0] = (op >> 8) & 15; return u32(((pc + 4) & ~3L) + (op & 0xFF) * 4L); }
        if ((op & 0xF000) == 0x9000) { rnOut[0] = (op >> 8) & 15; long v = (short) u16(pc + 4 + (op & 0xFF) * 2L); return v & 0xFFFFFFFFL; }
        if ((op & 0xF000) == 0xE000) { rnOut[0] = (op >> 8) & 15; long v = (byte) (op & 0xFF); return v & 0xFFFFFFFFL; }
        return null;
    }

    String hw(long v) {
        if (v >= 0xFFD80000L && v < 0xFFD80030L) return "TMU";
        if (v >= 0xFFD00000L && v < 0xFFD00014L) return "INTC";
        if (v >= 0xFFC80000L && v < 0xFFC80040L) return "RTC";
        if (v >= 0xFF000000L && v < 0xFF000100L) return "CCN";
        if (v >= 0xFFA00000L && v < 0xFFA00100L) return "DMAC";
        if (v >= 0xFFE80000L && v < 0xFFE80030L) return "SCIF";
        if (v == 0xF1000000L || v == 0xF4000000L || v == 0xF5000000L) return "CACHEARR";
        if (v >= 0xE0000000L && v < 0xE4000000L && (v & 0xFFFF) == 0) return "SQ";
        if (v >= 0x10000000L && v < 0x11000000L && (v & 0xFFFF) == 0) return "TAFIFO";
        if (v >= 0x8C0000B0L && v <= 0x8C0000E0L && (v & 3) == 0) return String.format("BIOS%02X", v & 0xFF);
        long top = v & 0xFFF00000L;
        long p = v & 0x1FFFFFFFL;
        if (top == 0xA0500000L || top == 0x80500000L || top == 0x00500000L) {
            if (p >= 0x005F6800L && p < 0x005F6900L) return String.format("SB%04X", p & 0xFFFF);
            if (p >= 0x005F6900L && p < 0x005F6A00L) return String.format("ASIC%04X", p & 0xFFFF);
            if (p >= 0x005F6C00L && p < 0x005F6D00L) return "MAPLE";
            if (p >= 0x005F7000L && p < 0x005F7100L) return "GDATA";
            if (p >= 0x005F7400L && p < 0x005F7500L) return "G1";
            if (p >= 0x005F7800L && p < 0x005F7900L) return "G2";
            if (p >= 0x005F7C00L && p < 0x005F7D00L) return "PVRDMA";
            if (p >= 0x005F8000L && p < 0x005FA000L) return String.format("PVR%04X", p & 0x1FFF);
        }
        if (v >= 0xA0700000L && v < 0xA0710000L) return "AICAREG";
        if (v >= 0xA0710000L && v < 0xA0720000L) return "AICARTC";
        if (v >= 0xA0800000L && v < 0xA0A00000L) return "AICARAM";
        if ((v & 0xFF000000L) == 0xA4000000L || (v & 0xFF000000L) == 0xA5000000L) return "VRAM";
        return null;
    }

    static class Feat {
        Set<String> hw = new TreeSet<>(); List<Long> callees = new ArrayList<>(); Set<String> ids = new TreeSet<>();
        Set<Long> globals = new TreeSet<>();
        int flt, ftrv, fipr, fsca, fschg, frchg, pref, ocb, movca, lds_fpscr, loops, trapa, ldcsr, insns;
    }
    Map<Long, Feat> cache = new HashMap<>();

    String strAt(long a) {
        if (!img.contains(toAddr(a))) return null;
        StringBuilder sb = new StringBuilder();
        for (int k = 0; k < 200; k++) {
            int b; try { b = mem.getByte(toAddr(a + k)) & 0xFF; } catch (Exception e) { return null; }
            if (b == 0) break;
            if (b < 0x20 || b >= 0x7F) { if (b != 9 && b != 10 && b != 13) return null; }
            sb.append((char) b);
        }
        return sb.length() >= 3 ? sb.toString() : null;
    }

    Feat feat(Function f) throws Exception {
        long e = f.getEntryPoint().getOffset();
        Feat r = cache.get(e);
        if (r != null) return r;
        r = new Feat(); cache.put(e, r);
        int[] rn = new int[1];
        for (Instruction i : L.getInstructions(f.getBody(), true)) {
            r.insns++;
            String m = i.getMnemonicString().toLowerCase();
            if (m.startsWith("f")) r.flt++;
            if (m.equals("ftrv")) r.ftrv++;
            if (m.equals("fipr")) r.fipr++;
            if (m.equals("fsca") || m.equals("fsrra")) r.fsca++;
            if (m.equals("fschg")) r.fschg++;
            if (m.equals("frchg")) r.frchg++;
            if (m.equals("pref")) r.pref++;
            if (m.startsWith("ocb")) r.ocb++;
            if (m.startsWith("movca")) r.movca++;
            if (m.equals("trapa")) r.trapa++;
            String s = i.toString().toLowerCase();
            if (s.contains("fpscr") && (m.startsWith("lds"))) r.lds_fpscr++;
            if (m.startsWith("ldc") && s.contains(",sr")) r.ldcsr++;
            for (Address fl : i.getFlows()) if (fl.compareTo(i.getAddress()) < 0 && f.getBody().contains(fl) && !i.getFlowType().isCall()) r.loops++;
            Long v = litOf(i, rn);
            if (v != null) {
                String h = hw(v);
                if (h != null) r.hw.add(h);
                if (img.contains(toAddr(v)) || (v >= 0x8C23E8BCL && v < 0x8D000000L)) {
                    Function t = fm.getFunctionAt(toAddr(v));
                    if (t == null) {
                        String st = strAt(v);
                        if (st != null) { Matcher mm = ID.matcher(st); while (mm.find()) r.ids.add(mm.group()); }
                        else if (v >= 0x8C066904L) r.globals.add(v);
                    }
                }
            }
            if (i.getFlowType().isCall()) {
                for (Address fl : i.getFlows()) {
                    Function t = resolve(fm.getFunctionAt(fl));
                    if (t != null) r.callees.add(t.getEntryPoint().getOffset());
                }
                for (Reference ref : i.getReferencesFrom()) if (ref.getReferenceType().isCall()) {
                    Function t = resolve(fm.getFunctionAt(ref.getToAddress()));
                    if (t != null && !r.callees.contains(t.getEntryPoint().getOffset())) r.callees.add(t.getEntryPoint().getOffset());
                }
            }
        }
        return r;
    }

    void deepHw(Function f, int depth, Set<String> out, Set<Long> seen) throws Exception {
        if (f == null || !seen.add(f.getEntryPoint().getOffset())) return;
        Feat ft = feat(f);
        for (String h : ft.hw) out.add(depth == 0 ? h : h.toLowerCase());
        if (depth >= 3) return;
        for (long c : ft.callees) if (!game(c)) deepHw(fm.getFunctionAt(toAddr(c)), depth + 1, out, seen);
    }

    String nm(long a) {
        Function f = fm.getFunctionAt(toAddr(a));
        String n = f == null ? "?" : f.getName();
        return n.startsWith("FUN_") ? String.format("%x", a) : n;
    }

    @Override
    public void run() throws Exception {
        mem = currentProgram.getMemory(); L = currentProgram.getListing();
        fm = currentProgram.getFunctionManager(); rm = currentProgram.getReferenceManager();
        img = mem.getBlock("image");
        String[] a = getScriptArgs();
        gs = Long.decode(a[0]); ge = Long.decode(a[1]);
        PrintWriter w = new PrintWriter(new FileWriter(a[2]));
        // call sites from game code
        Map<Long, List<Address>> sites = new TreeMap<>();
        Map<Long, Set<Long>> callers = new HashMap<>();
        for (Function f : fm.getFunctions(true)) {
            long fe = f.getEntryPoint().getOffset();
            if (!game(fe) || fe < 0x8C010184L) continue;
            for (Instruction i : L.getInstructions(f.getBody(), true)) {
                if (!i.getFlowType().isCall() && !(i.getFlowType().isJump() && i.getMnemonicString().equalsIgnoreCase("jmp"))) continue;
                Set<Long> ts = new HashSet<>();
                for (Address fl : i.getFlows()) ts.add(fl.getOffset());
                for (Reference ref : i.getReferencesFrom()) if (ref.getReferenceType().isCall() || ref.getReferenceType().isJump()) ts.add(ref.getToAddress().getOffset());
                for (long t0 : ts) {
                    Function t = resolve(fm.getFunctionAt(toAddr(t0)));
                    if (t == null) { if (!game(t0)) { sites.computeIfAbsent(t0, k -> new ArrayList<>()).add(i.getAddress()); callers.computeIfAbsent(t0, k -> new HashSet<>()).add(fe);} continue; }
                    long te = t.getEntryPoint().getOffset();
                    if (game(te)) continue;
                    sites.computeIfAbsent(te, k -> new ArrayList<>()).add(i.getAddress());
                    callers.computeIfAbsent(te, k -> new HashSet<>()).add(fe);
                }
            }
        }
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        int[] rn = new int[1];
        for (long t : sites.keySet()) {
            Function f = fm.getFunctionAt(toAddr(t));
            StringBuilder sb = new StringBuilder();
            sb.append(String.format("%08x\t%s\t%d\t%d", t, f == null ? "nofunc" : nm(t), sites.get(t).size(), callers.get(t).size()));
            if (f == null) { w.println(sb); println(sb.toString()); continue; }
            sb.append("\tsz=").append(f.getBody().getNumAddresses());
            // prototype
            String proto = "?";
            try {
                DecompileResults dr = di.decompileFunction(f, 30, monitor);
                HighFunction hf = dr.getHighFunction();
                if (hf != null) {
                    FunctionPrototype fp = hf.getFunctionPrototype();
                    int np = fp.getNumParams(); int nf = 0;
                    StringBuilder ps = new StringBuilder();
                    for (int k = 0; k < np; k++) {
                        HighSymbol hs = fp.getParam(k);
                        String reg = hs.getStorage().isRegisterStorage() ? hs.getStorage().getRegister().getName() : "stk";
                        if (reg.startsWith("fr")) nf++;
                        ps.append(k > 0 ? "," : "").append(reg);
                    }
                    String ret = fp.getReturnType() == null ? "?" : (fp.getReturnType().getLength() <= 0 ? "void" : fp.getReturnType().getName());
                    proto = np + "(" + ps + ")->" + ret;
                }
            } catch (Exception ex) { proto = "err"; }
            sb.append("\tp=").append(proto);
            // constants at call sites (backwards in the same block, up to 12 insns)
            Map<String, Map<Long, Integer>> cs = new TreeMap<>();
            for (Address s : sites.get(t)) {
                Instruction i = L.getInstructionAt(s);
                Set<Integer> done = new HashSet<>();
                // delay slot instruction first
                Instruction ds = L.getInstructionAfter(s);
                List<Instruction> scan = new ArrayList<>();
                if (ds != null) scan.add(ds);
                Instruction p = i;
                for (int k = 0; k < 12; k++) { p = L.getInstructionBefore(p.getAddress()); if (p == null || p.getFlowType().isCall() || p.getFlowType().isJump() || p.getFlowType().isTerminal()) break; scan.add(p); }
                for (Instruction q : scan) {
                    Long v = litOf(q, rn);
                    int op = u16(q.getAddress().getOffset());
                    int dst = v != null ? rn[0] : -1;
                    if (v == null) {
                        // any other write to r4..r7 kills tracking
                        String ms = q.toString();
                        for (int r = 4; r <= 7; r++) if (ms.endsWith(",r" + r) ) done.add(r);
                        continue;
                    }
                    if (dst < 4 || dst > 7 || done.contains(dst)) continue;
                    done.add(dst);
                    cs.computeIfAbsent("r" + dst, k -> new HashMap<>()).merge(v, 1, Integer::sum);
                }
            }
            StringBuilder c = new StringBuilder();
            for (String r : cs.keySet()) {
                List<Map.Entry<Long, Integer>> es = new ArrayList<>(cs.get(r).entrySet());
                es.sort((x, y) -> y.getValue() - x.getValue());
                c.append(r).append(":");
                for (int k = 0; k < Math.min(4, es.size()); k++) {
                    long v = es.get(k).getKey();
                    String vs = (v < 0x10000 || v > 0xFFFF0000L) ? Long.toString((int) v) : String.format("%x", v);
                    if (fm.getFunctionAt(toAddr(v)) != null) vs = "fn" + String.format("%x", v);
                    else if (strAt(v) != null) vs = "str";
                    c.append(k > 0 ? "/" : "").append(vs).append("x").append(es.get(k).getValue());
                }
                c.append(" ");
            }
            sb.append("\tc=").append(c.toString().trim());
            Feat ft = feat(f);
            Set<String> dh = new TreeSet<>(); deepHw(f, 0, dh, new HashSet<>());
            sb.append("\thw=").append(String.join(",", dh));
            StringBuilder ce = new StringBuilder();
            for (long ca : ft.callees) ce.append(ce.length() > 0 ? "," : "").append(nm(ca));
            sb.append("\tcall=").append(ce);
            Set<String> ids = new TreeSet<>(ft.ids);
            for (long ca : ft.callees) if (!game(ca)) { Function cf = fm.getFunctionAt(toAddr(ca)); if (cf != null) ids.addAll(feat(cf).ids); }
            sb.append("\tid=").append(String.join(",", ids));
            sb.append(String.format("\tn=%d f=%d trv=%d ipr=%d sca=%d schg=%d rchg=%d pref=%d ocb=%d movca=%d fpscr=%d loop=%d trapa=%d sr=%d g=%d",
                ft.insns, ft.flt, ft.ftrv, ft.fipr, ft.fsca, ft.fschg, ft.frchg, ft.pref, ft.ocb, ft.movca, ft.lds_fpscr, ft.loops, ft.trapa, ft.ldcsr, ft.globals.size()));
            StringBuilder gl = new StringBuilder();
            int k = 0; for (long g : ft.globals) { if (k++ >= 6) break; gl.append(String.format("%x,", g)); }
            sb.append("\tgl=").append(gl);
            w.println(sb);
        }
        w.close();
        println("targets " + sites.size());
    }
}
