// S1: SDK library regions and high-confidence SDK entry-point names for 1ST_READ.BIN.
//
// Evidence kinds (column 'evidence' of regions.tsv / functions.tsv):
//   str     the function loads a pointer to a library version / error / config
//           string (the string text never leaves the sandbox; only the library
//           label derived from it does)
//   name    the string names the function itself ("E1234:mwFoo ...", "(ADXT_Foo)",
//           "... in mwFoo") or the callee whose error it reports ("sdFoo return error")
//   hw      the function loads the address of a hardware register block owned by
//           one library (PVR/TA -> Kamui, Maple -> pd, GD-ROM -> gdFs, AICA -> sd,
//           TMU/INTC/cache/RTC and the BIOS sysinfo/flash vectors -> Shinobi sy,
//           the BIOS GD-ROM vector -> gdFs)
//   fpu     ftrv/fipr in the function (Ninja matrix/vector code; weak)
//   data    the function shares data/bss words with anchored functions of the
//           label (a library's objects share its globals)
//   graph   no direct evidence: label from callers/callees and address
//           neighbours (the linker pulls library objects in on demand, so
//           libraries interleave; contiguity is only a weak prior)
// Args: outdir, then code ranges start-end (hex).
// Writes outdir/regions.tsv and outdir/functions.tsv (addresses, sizes, names,
// labels only), and renames the identified SDK functions in the project.
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.block.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;
import java.util.regex.*;

public class S1Regions extends GhidraScript {
    Memory mem; Listing L; FunctionManager fm; ReferenceManager rm; MemoryBlock img;
    long[] rs, re;
    long rtStart;
    Map<Long, String> rowLabel = new HashMap<>();

    String fixedLabel(long e) { return e < 0x8C010184L ? "sy_start" : e < rtStart ? "game" : "shc_rt"; }
    String fixedEvid(long e) { return e < 0x8C010184L ? "str" : e < rtStart ? "layout" : "xref"; }
    // per function entry offset
    TreeMap<Long, Function> funcs = new TreeMap<>();
    Map<Long, Map<String, Integer>> anchor = new HashMap<>();   // label -> weight
    Map<Long, Set<String>> anchorKind = new HashMap<>();
    Map<Long, Map<String, Integer>> nameVotes = new HashMap<>();
    Map<Long, String> label = new HashMap<>();
    Map<Long, String> evid = new HashMap<>();

    boolean inCode(long o) { for (int k = 0; k < rs.length; k++) if (o >= rs[k] && o < re[k]) return true; return false; }

    void addAnchor(Function f, String lib, int w, String kind) {
        long e = f.getEntryPoint().getOffset();
        anchor.computeIfAbsent(e, k -> new HashMap<>()).merge(lib, w, Integer::sum);
        anchorKind.computeIfAbsent(e, k -> new TreeSet<>()).add(kind);
    }

    void addName(long e, String name) {
        nameVotes.computeIfAbsent(e, k -> new HashMap<>()).merge(name, 1, Integer::sum);
    }

    // library from an SDK identifier
    static String libOf(String id) {
        if (id.matches("(?i)adx.*")) return "CRI_ADX";
        if (id.matches("mw(Ply|Sfd|Gsc|Mng|Rna|Rnv|Snd).*|MW(SFD|GSC|RNA|SND).*")) return "CRI_mw";
        if (id.matches("(SFD|MPS|MPV|SFA|sfd|mps|mpv|sfa)_.*")) return "CRI_SFD";
        if (id.matches("(SJ|sj)[A-Z_].*")) return "CRI_SJ";
        if (id.matches("sd[A-Z].*")) return "sd";
        if (id.matches("nj[A-Z].*")) return "Ninja";
        if (id.matches("km[A-Z].*")) return "Kamui";
        if (id.matches("gdFs.*")) return "gdFs";
        if (id.matches("pd[A-Z].*")) return "pd";
        if (id.matches("bu[A-Z].*")) return "bu";
        if (id.matches("sy[A-Z].*")) return "sy";
        return null;
    }

    // string text -> library label (version banners, error prefixes, config keys)
    static final String[][] STRLIB = {
        {"mwPly|mwSfd|mwGsc|MWGSC|mwMng|mwRnv|mwRna|mwSnd|MWRNA|MWSND", "CRI_mw"},
        {"ADXSTM|ADXT_|adxt_|adxstm|ADXF|adxf_", "CRI_ADX"},
        {"^(SFD|MPS|MPV) Version|CRI SFD|^SFA Ver", "CRI_SFD"},
        {"^\\s*SJ Ver", "CRI_SJ"},
        {"KAMUI|^(VideoMode|DisplayMode|AntiAlias|Dither|RENDER|VSYNCCount|RegionSize|ObjListSize|ISPParamSize|TSPParamSize|VertexBuffer|BufferBase|BufferSize|PoolingTAStatus|RenderTimeOut|VertexTimeOut|VSYNCTimeOut)$", "Kamui"},
        {"^GBIXPVRT$|Ninja", "Ninja"},
        {"KATANA_FLASH", "sy"},
        {"^\\s*sy[A-Z][A-Za-z]+ Ver|Shinobi Library", "sy"},
        {"^\\s*gdFs Ver|GDFS\\)", "gdFs"},
        {"^\\s*pd(Lcd|Vib)? Ver", "pd"},
        {"^\\s*bu Ver|VMS File System", "bu"},
        {"^\\s*sd Ver", "sd"},
        {"^\\s*fmMng Ver", "sy"},
        {"^\\s*kd Ver", "sy"},
    };

    static final Pattern ID = Pattern.compile("(?:ADX[A-Z]*_|adx[a-z]*_|mw[A-Z]|MW[A-Z]+_|sd[A-Z]|nj[A-Z]|km[A-Z]|gdFs|pd[A-Z]|bu[A-Z]|sy[A-Z])[A-Za-z0-9_]*");

    // Exact register addresses / block bases only: wide ranges catch masks and
    // float constants (0xF0000000, FLT_MIN 0x00800000, 0x10000000 ...).
    String hwLib(long v) {
        if (v >= 0xFFD80000L && v < 0xFFD80030L) return "sy";      // TMU
        if (v >= 0xFFD00000L && v < 0xFFD00014L) return "sy";      // INTC
        if (v >= 0xFFC80000L && v < 0xFFC80040L) return "sy";      // SH-4 RTC
        if (v == 0xF1000000L || v == 0xF4000000L || v == 0xF5000000L) return "sy"; // cache arrays (not 0xF0000000: a common mask)
        // BIOS system call vectors (Katana: GD-ROM, flash, system info)
        if (v == 0x8C0000BCL) return "gdFs";
        if (v == 0x8C0000B0L || v == 0x8C0000B8L || v == 0x8C0000E0L) return "sy";
        long top = v & 0xFFF00000L;
        if (top != 0xA0500000L && top != 0x80500000L && top != 0x00500000L
            && top != 0xA0700000L && top != 0xA0800000L && top != 0xA0900000L) return null;
        long p = v & 0x1FFFFFFFL;
        if (p >= 0x005F6C00L && p < 0x005F6D00L) return "pd";          // Maple
        if (p >= 0x005F7000L && p < 0x005F7100L) return "gdFs";        // GD-ROM ATA
        if (p >= 0x005F7400L && p < 0x005F7500L) return "gdFs";        // G1 GD DMA
        if (p >= 0x005F8000L && p < 0x005FA000L) return "Kamui";       // PVR core, palette
        if (v >= 0xA0700000L && v < 0xA0710000L) return "sd";          // AICA regs
        if (v >= 0xA0800000L && v < 0xA0A00000L) return "sd";          // AICA RAM (P2)
        return null;
    }

    Address callTarget(Instruction i) {
        if (!i.getFlowType().isCall()) return null;
        Address[] fl = i.getFlows();
        return fl.length == 1 ? fl[0] : null;
    }

    // last call executed before instruction 'at' in its function (backwards in the
    // block, then through a single predecessor block, at most 4 blocks)
    Address callBefore(Instruction at, BasicBlockModel bbm) throws Exception {
        Instruction i = at;
        for (int depth = 0; depth < 4 && i != null; depth++) {
            CodeBlock b = bbm.getFirstCodeBlockContaining(i.getAddress(), monitor);
            if (b == null) return null;
            Instruction p = L.getInstructionBefore(i.getAddress());
            while (p != null && b.contains(p.getAddress())) {
                Address t = callTarget(p);
                if (t != null) return t;
                p = L.getInstructionBefore(p.getAddress());
            }
            // single predecessor
            CodeBlockReferenceIterator it = b.getSources(monitor);
            Address src = null; int n = 0;
            while (it.hasNext()) {
                CodeBlockReference r = it.next();
                if (r.getFlowType().isCall()) continue;
                n++; src = r.getReferent();
            }
            if (n != 1 || src == null) return null;
            i = L.getInstructionAt(src);
            if (i == null) return null;
            // scan from the source instruction itself (include it)
            Address t0 = callTarget(i);
            if (t0 != null) return t0;
        }
        return null;
    }

    @Override
    public void run() throws Exception {
        mem = currentProgram.getMemory(); L = currentProgram.getListing();
        fm = currentProgram.getFunctionManager(); rm = currentProgram.getReferenceManager();
        img = mem.getBlock("image");
        String[] args = getScriptArgs();
        String out = args[0];
        rtStart = Long.decode(args[1]);   // start of the SHC C runtime at the end of the game range
        rs = new long[args.length - 2]; re = new long[args.length - 2];
        for (int k = 2; k < args.length; k++) {
            String[] p = args[k].split("-"); rs[k - 2] = Long.decode(p[0]); re[k - 2] = Long.decode(p[1]);
        }
        for (Function f : fm.getFunctions(true))
            if (img.contains(f.getEntryPoint())) funcs.put(f.getEntryPoint().getOffset(), f);

        BasicBlockModel bbm = new BasicBlockModel(currentProgram);
        byte[] d = new byte[(int) img.getSize()];
        img.getBytes(img.getStart(), d);
        long base = img.getStart().getOffset();

        // --- string anchors and names ---
        Map<Long, List<Long>> ptrs = new HashMap<>();
        for (int o = 0; o + 4 <= d.length; o += 4) {
            long v = (d[o] & 0xFFL) | (d[o + 1] & 0xFFL) << 8 | (d[o + 2] & 0xFFL) << 16 | (d[o + 3] & 0xFFL) << 24;
            if (v >= base && v < base + d.length) ptrs.computeIfAbsent(v, k -> new ArrayList<>()).add(base + o);
        }
        int nstr = 0, nname = 0, ncallee = 0;
        for (int o = 0; o < d.length; ) {
            int e = o;
            while (e < d.length && ((d[e] >= 0x20 && d[e] < 0x7F) || d[e] == 9 || d[e] == 10 || d[e] == 13)) e++;
            if (e - o < 4 || e >= d.length || d[e] != 0) { o = Math.max(e, o + 1); continue; }
            String t = new String(d, o, e - o, "ISO-8859-1").trim();
            long sa = base + o;
            o = e + 1;
            String lib = null;
            for (String[] sl : STRLIB) if (Pattern.compile(sl[0]).matcher(t).find()) { lib = sl[1]; break; }
            // loads of this string: instructions referencing the string or a pool word holding it
            List<Instruction> loads = new ArrayList<>();
            List<Address> holders = new ArrayList<>();
            holders.add(toAddr(sa));
            for (long p : ptrs.getOrDefault(sa, List.of())) holders.add(toAddr(p));
            for (Address h : holders)
                for (Reference r : rm.getReferencesTo(h)) {
                    Instruction ins = L.getInstructionAt(r.getFromAddress());
                    if (ins != null && fm.getFunctionContaining(ins.getAddress()) != null) loads.add(ins);
                }
            if (loads.isEmpty()) continue;
            Set<Function> users = new HashSet<>();
            for (Instruction ins : loads) users.add(fm.getFunctionContaining(ins.getAddress()));
            if (lib == null) {
                // an SDK identifier inside the string decides the library
                Matcher m = ID.matcher(t);
                if (m.find()) lib = libOf(m.group());
            }
            if (lib != null) for (Function f : users) { addAnchor(f, lib, 3, "str"); nstr++; }
            // names: only for strings used by exactly one function
            if (users.size() != 1) continue;
            Function f = users.iterator().next();
            String self = null, callee = null;
            Matcher m;
            if ((m = Pattern.compile("^[EW]\\d+\\s*:?\\s*([A-Za-z_][A-Za-z0-9_]+)\\s*[:;]").matcher(t)).find()) self = m.group(1);
            else if ((m = Pattern.compile("^[EW]\\d+\\s*:\\s*([A-Za-z_][A-Za-z0-9_]+) (Illigal|Illegal|Not |Cannot|Time out|Play )").matcher(t)).find()) self = m.group(1);
            if ((m = Pattern.compile(" in ([A-Za-z_][A-Za-z0-9_]+)$").matcher(t)).find()) self = m.group(1);
            if ((m = Pattern.compile("\\(([A-Za-z_][A-Za-z0-9_]+)\\)\\s*(\\\\n)?$").matcher(t)).find()) self = m.group(1);
            if ((m = Pattern.compile("^[EW]\\d+\\s*:\\s*([A-Za-z_][A-Za-z0-9_]+) return error").matcher(t)).find()) callee = m.group(1);
            if (self != null && (libOf(self) == null || !ID.matcher(self).matches())) self = null;
            if (callee != null && (libOf(callee) == null || !ID.matcher(callee).matches())) callee = null;
            if (self != null) { addName(f.getEntryPoint().getOffset(), self); addAnchor(f, libOf(self), 5, "name"); nname++; }
            if (callee != null) {
                Set<Address> tg = new HashSet<>();
                for (Instruction ins : loads) { Address c = callBefore(ins, bbm); if (c != null) tg.add(c); }
                if (tg.size() == 1) {
                    Function cf = fm.getFunctionAt(tg.iterator().next());
                    if (cf != null) { addName(cf.getEntryPoint().getOffset(), callee); addAnchor(cf, libOf(callee), 5, "name"); ncallee++; }
                }
            }
        }
        println("string anchors " + nstr + " self names " + nname + " callee names " + ncallee);

        // --- hardware register anchors ---
        int nhw = 0;
        for (Function f : funcs.values()) {
            Set<String> libs = new HashSet<>();
            for (Instruction i : L.getInstructions(f.getBody(), true)) {
                int op = mem.getShort(i.getAddress()) & 0xFFFF;
                if ((op & 0xF000) != 0xD000) continue;
                long pa = ((i.getAddress().getOffset() + 4) & ~3L) + (op & 0xFF) * 4;
                if (!img.contains(toAddr(pa))) continue;
                long v = mem.getInt(toAddr(pa)) & 0xFFFFFFFFL;
                String hl = hwLib(v);
                if (hl != null) libs.add(hl);
            }
            for (String hl : libs) { addAnchor(f, hl, 2, "hw"); nhw++; }
        }
        println("hw anchors " + nhw);

        long gameEnd = re[0];
        // --- FPU signature: ftrv / fipr (Ninja matrix and vector code), weak ---
        int nfpu = 0;
        for (Function f : funcs.values()) {
            if (f.getEntryPoint().getOffset() < gameEnd) continue;
            for (Instruction i : L.getInstructions(f.getBody(), true)) {
                String m = i.getMnemonicString();
                if (m.equals("ftrv") || m.equals("fipr")) { addAnchor(f, "Ninja", 1, "fpu"); nfpu++; break; }
            }
        }
        println("fpu anchors " + nfpu);

        // --- segmentation of each SDK code range: libraries are linked as runs of
        // objects, so pick the labelling with the fewest disagreements with the
        // anchors plus a penalty per label change (dynamic programming) ---
        String[] LB = {"CRI_mw", "CRI_SFD", "CRI_ADX", "CRI_SJ", "sd", "Ninja", "Kamui", "sy", "gdFs", "pd", "bu"};
        Map<String, Integer> li = new HashMap<>();
        for (int k = 0; k < LB.length; k++) li.put(LB[k], k);
        Map<Long, String> dom = new HashMap<>();                  // dominant anchor label
        for (Map.Entry<Long, Map<String, Integer>> en : anchor.entrySet()) {
            String best = null; int bw = 0, tot = 0;
            for (Map.Entry<String, Integer> x : en.getValue().entrySet()) { tot += x.getValue(); if (x.getValue() > bw) { bw = x.getValue(); best = x.getKey(); } }
            if (best != null && bw * 2 > tot) dom.put(en.getKey(), best);
        }
        // data affinity: each library owns its D/B sections, so the data and bss
        // words a function addresses (literal-pool pointers past the SDK code)
        // vote for the labels of the other functions that address them
        final int MAXSHARE = 40;
        Map<Long, Set<Long>> drefs = new HashMap<>();
        Map<Long, Set<Long>> dusers = new HashMap<>();
        long dataLo = re[1];
        for (long e : funcs.keySet()) {
            if (e < gameEnd || !inCode(e)) continue;
            Set<Long> ds = new HashSet<>();
            for (Instruction i : L.getInstructions(funcs.get(e).getBody(), true)) {
                int op = mem.getShort(i.getAddress()) & 0xFFFF;
                if ((op & 0xF000) != 0xD000) continue;
                long pa = ((i.getAddress().getOffset() + 4) & ~3L) + (op & 0xFF) * 4;
                if (!img.contains(toAddr(pa))) continue;
                long v = mem.getInt(toAddr(pa)) & 0xFFFFFFFFL;
                if (v >= dataLo && v < 0x8D000000L && !inCode(v)) ds.add(v);
            }
            drefs.put(e, ds);
            for (long v : ds) dusers.computeIfAbsent(v, k -> new HashSet<>()).add(e);
        }
        // components: functions joined by the data words they share (words used by
        // 2..MAXSHARE functions); a library's objects share its globals
        Map<Long, Long> uf = new HashMap<>();
        for (long e : drefs.keySet()) uf.put(e, e);
        java.util.function.LongUnaryOperator find = new java.util.function.LongUnaryOperator() {
            public long applyAsLong(long x) { while (uf.get(x) != x) { uf.put(x, uf.get(uf.get(x))); x = uf.get(x); } return x; } };
        for (Map.Entry<Long, Set<Long>> en : dusers.entrySet()) {
            Set<Long> us = en.getValue();
            if (us.size() < 2 || us.size() > MAXSHARE) continue;
            long first = -1;
            for (long u : us) { if (first < 0) first = u; else { long a2 = find.applyAsLong(first), b2 = find.applyAsLong(u); if (a2 != b2) uf.put(a2, b2); } }
        }
        Map<Long, List<Long>> comps = new HashMap<>();
        for (long e : drefs.keySet()) comps.computeIfAbsent(find.applyAsLong(e), k -> new ArrayList<>()).add(e);
        Map<Long, double[]> compScore = new HashMap<>();
        int ncomp = 0;
        for (List<Long> c : comps.values()) {
            if (c.size() < 2) continue;
            double[] h = new double[LB.length]; double t = 0;
            for (long e : c) { String l = dom.get(e); if (l != null && li.containsKey(l)) { h[li.get(l)]++; t++; } }
            if (t < 2) continue;
            ncomp++;
            for (long e : c) {
                double[] v = new double[LB.length];
                String own = dom.get(e);
                double tt = t - (own != null && li.containsKey(own) ? 1 : 0);
                if (tt < 1) continue;
                for (int l = 0; l < LB.length; l++) v[l] = 6.0 * (h[l] - (own != null && li.containsKey(own) && li.get(own) == l ? 1 : 0)) / tt;
                compScore.put(e, v);
            }
        }
        println("data components with anchors " + ncomp + ", functions scored " + compScore.size());
        final double LAMBDA = 2.5;
        for (int r = 1; r < rs.length; r++) {
            List<Long> seq = new ArrayList<>();
            for (long e : funcs.subMap(rs[r], re[r]).keySet()) seq.add(e);
            int n = seq.size(), m = LB.length;
            if (n == 0) continue;
            double[][] sc = new double[n][m];
            for (int i = 0; i < n; i++) {
                long e = seq.get(i);
                for (Map.Entry<String, Integer> x : anchor.getOrDefault(e, Map.of()).entrySet())
                    if (li.containsKey(x.getKey())) sc[i][li.get(x.getKey())] += 2 * x.getValue();
                Function f = funcs.get(e);
                Set<Long> nbs = new HashSet<>();
                for (Function c : f.getCalledFunctions(monitor)) nbs.add(c.getEntryPoint().getOffset());
                for (Function c : f.getCallingFunctions(monitor)) nbs.add(c.getEntryPoint().getOffset());
                for (long x : nbs) { String l = dom.get(x); if (x != e && x >= gameEnd && l != null && li.containsKey(l)) sc[i][li.get(l)] += 0.5; }
                double[] cs = compScore.get(e);
                if (cs != null) for (int l = 0; l < m; l++) sc[i][l] += cs[l];
            }
            double[][] best = new double[n][m]; int[][] from = new int[n][m];
            for (int i = 0; i < n; i++) {
                double tot = 0; for (int l = 0; l < m; l++) tot += sc[i][l];
                int pmin = 0;
                if (i > 0) for (int l = 1; l < m; l++) if (best[i - 1][l] < best[i - 1][pmin]) pmin = l;
                for (int l = 0; l < m; l++) {
                    double c = tot - sc[i][l];
                    if (i == 0) { best[i][l] = c; from[i][l] = l; continue; }
                    double stay = best[i - 1][l], sw = best[i - 1][pmin] + LAMBDA;
                    if (stay <= sw) { best[i][l] = c + stay; from[i][l] = l; } else { best[i][l] = c + sw; from[i][l] = pmin; }
                }
            }
            int l = 0; for (int k = 1; k < m; k++) if (best[n - 1][k] < best[n - 1][l]) l = k;
            for (int i = n - 1; i >= 0; i--) {
                long e = seq.get(i);
                label.put(e, LB[l]);
                Set<String> ks = new TreeSet<>(anchorKind.getOrDefault(e, Set.of()));
                double[] cs2 = compScore.get(e);
                if (cs2 != null) { int am = 0; for (int k = 1; k < m; k++) if (cs2[k] > cs2[am]) am = k; if (am == l && cs2[am] > 0) ks.add("data"); }
                if (ks.isEmpty()) ks.add("graph");
                evid.put(e, String.join("+", ks));
                l = from[i][l];
            }
        }
        println("labelled SDK functions " + label.size());

        // --- names: apply when votes agree ---
        Map<String, Long> nameAt = new HashMap<>();
        Map<Long, String> names = new HashMap<>();
        Set<String> dup = new HashSet<>();
        for (Map.Entry<Long, Map<String, Integer>> en : nameVotes.entrySet()) {
            if (en.getValue().size() != 1) continue;            // conflicting names: skip
            String n = en.getValue().keySet().iterator().next();
            if (nameAt.containsKey(n)) dup.add(n);
            nameAt.put(n, en.getKey()); names.put(en.getKey(), n);
        }
        for (String n : dup) names.values().removeIf(x -> x.equals(n));
        int applied = 0;
        Map<Long, String> nconf = new HashMap<>();
        for (Map.Entry<Long, String> en : names.entrySet()) {
            Function f = funcs.get(en.getKey());
            if (f == null) continue;
            int votes = nameVotes.get(en.getKey()).get(en.getValue());
            boolean selfNamed = anchorKind.getOrDefault(en.getKey(), Set.of()).contains("name");
            nconf.put(en.getKey(), votes >= 2 ? "high" : "medium");
            f.setName(en.getValue(), SourceType.USER_DEFINED);
            applied++;
        }
        println("names applied " + applied + " (dropped duplicates " + dup.size() + ")");

        // --- regions: runs of equal labels in address order ---
        PrintWriter rw = new PrintWriter(new FileWriter(out + "/regions.tsv"));
        rw.println("# start\tend\tlibrary\tfunctions\tbytes\tanchored\tconflicting\tfirst_anchor\tlast_anchor\tevidence\tconfidence");
        List<String[]> rows = new ArrayList<>();
        String cur = null; long rsA = 0, rEnd = 0, fa = 0, la = 0, nb2 = 0; int nf = 0, na = 0, nc = 0; Set<String> kinds = new TreeSet<>();
        for (Map.Entry<Long, Function> en : funcs.entrySet()) {
            long e = en.getKey(); Function f = en.getValue();
            if (!inCode(e)) continue;
            String reg = e < gameEnd ? fixedLabel(e) : label.getOrDefault(e, "unknown");
            if (!reg.equals(cur) || (cur != null && rEnd < e && !inCodeSpan(rEnd, e))) {
                if (cur != null) rows.add(row(rsA, rEnd, cur, nf, nb2, na, nc, fa, la, kinds));
                cur = reg; rsA = e; nf = 0; na = 0; nc = 0; nb2 = 0; fa = 0; la = 0; kinds = new TreeSet<>();
            }
            nf++; nb2 += extentEnd(f) - e;
            rEnd = Math.max(rEnd, extentEnd(f));
            if (e >= gameEnd && dom.containsKey(e)) {
                if (dom.get(e).equals(reg)) { na++; kinds.addAll(anchorKind.get(e)); if (fa == 0) fa = e; la = e; }
                else nc++;
            }
            if (e >= gameEnd && evid.getOrDefault(e, "").contains("data")) kinds.add("data");
        }
        if (cur != null) rows.add(row(rsA, rEnd, cur, nf, nb2, na, nc, fa, la, kinds));
        for (String[] r : rows) rw.println(String.join("\t", r));
        for (String[] r : rows) {
            long a0 = Long.parseLong(r[0], 16), a1 = Long.parseLong(r[1], 16);
            for (long e : funcs.subMap(a0, a1).keySet()) rowLabel.put(e, r[2]);
        }
        rw.close();
        println("regions " + rows.size());
        // --- function table ---
        PrintWriter w = new PrintWriter(new FileWriter(out + "/functions.tsv"));
        w.println("# address\tsize\tinsn_bytes\tname\tregion\tname_conf\tregion_evidence");
        for (Map.Entry<Long, Function> en : funcs.entrySet()) {
            long e = en.getKey(); Function f = en.getValue();
            String reg = e < gameEnd ? fixedLabel(e) : label.getOrDefault(e, inCode(e) ? "unknown" : "unknown_noncode");
            String ev = e < gameEnd ? fixedEvid(e) : evid.getOrDefault(e, "-");
            if (e >= gameEnd && rowLabel.containsKey(e)) reg = rowLabel.get(e);
            String nm = f.getName();
            // a Ghidra thunk shows its target's name: make the row's name unique
            if (f.isThunk()) {
                Function t = f.getThunkedFunction(true);
                String tn = t == null ? "unknown" : t.getName();
                if (tn.startsWith("FUN_")) tn = tn.substring(4);
                nm = "j_" + tn + "_" + String.format("%08x", e);
            }
            if (!nm.matches("[A-Za-z0-9_]+")) nm = String.format("FUN_%08x", e);
            w.println(String.format("%08x\t%d\t%d\t%s\t%s\t%s\t%s", e, extentEnd(f) - e, f.getBody().getNumAddresses(), nm, reg,
                nconf.getOrDefault(e, f.getSymbol().getSource() == SourceType.USER_DEFINED && e == 0x8C010000L ? "high" : "-"), ev));
        }
        w.close();

    }

    // end (exclusive) of the function's local extent: the entry's body range plus
    // ranges that follow within 1 KiB (literal pools split the body)
    long extentEnd(Function f) {
        AddressSetView b = f.getBody();
        long end = b.getRangeContaining(f.getEntryPoint()).getMaxAddress().getOffset();
        for (AddressRange r : b) {
            long mn = r.getMinAddress().getOffset();
            if (mn > end && mn - end <= 0x400) end = Math.max(end, r.getMaxAddress().getOffset());
        }
        return end + 1;
    }

    boolean inCodeSpan(long a, long b) { for (int k = 0; k < rs.length; k++) if (a >= rs[k] && b <= re[k]) return true; return false; }

    String[] row(long s, long e, String lib, int nf, long nb, int na, int nc, long fa, long la, Set<String> kinds) {
        String conf, ev;
        if (lib.equals("game")) { conf = "medium"; ev = "layout"; }
        else if (lib.equals("sy_start")) { conf = "high"; ev = "str"; }
        else if (lib.equals("shc_rt")) { conf = "medium"; ev = "xref"; }
        else {
            boolean strong = kinds.contains("str") || kinds.contains("name");
            if (na >= 3 && strong && nc * 5 <= na) conf = "high";
            else if (na >= 2 && nc <= na) conf = "medium";
            else conf = "low";
            ev = kinds.isEmpty() ? "graph" : String.join("+", kinds) + "+graph";
            if (na == 0 && !kinds.contains("data")) { ev = "graph:" + lib; lib = "unknown"; }
        }
        return new String[] { String.format("%08x", s), String.format("%08x", e), lib, "" + nf, "" + nb, "" + na, "" + nc,
            fa == 0 ? "-" : String.format("%08x", fa), la == 0 ? "-" : String.format("%08x", la), ev, conf };
    }
}
