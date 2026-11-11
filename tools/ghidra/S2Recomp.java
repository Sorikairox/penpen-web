// S2: recompiler-feasibility counts for S3 (numbers and addresses only).
// Per area (game = 0x8C010000-0x8C062440, rt = 0x8C062440-0x8C066900, sdk = the rest):
// call/jump forms, register-indirect calls, computed jumps (jump tables), FPU mode
// switches and FPSCR loads (with the literal value when it comes from a pool),
// delay-slot cases a translator must special-case, cache/store-queue ops,
// privileged/system instructions, jumps into the P2 (uncached) mirror, literal
// call targets outside the image, and function-pointer words in data.
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import java.util.*;

public class S2Recomp extends GhidraScript {
    Memory mem;
    long u32(long a) { try { return mem.getInt(toAddr(a)) & 0xFFFFFFFFL; } catch (Exception e) { return -1; } }
    int u16(long a) { try { return mem.getShort(toAddr(a)) & 0xFFFF; } catch (Exception e) { return -1; } }
    String area(long a) { return a < 0x8C062440L ? "game" : a < 0x8C066900L ? "rt" : "sdk"; }
    Map<String, Map<String, Integer>> C = new TreeMap<>();
    Map<String, Set<String>> W = new TreeMap<>();
    void inc(String ar, String k) { C.computeIfAbsent(k, x -> new TreeMap<>()).merge(ar, 1, Integer::sum); }
    void where(String k, long fn) { Set<String> s = W.computeIfAbsent(k, x -> new TreeSet<>()); if (s.size() < 12) s.add(Long.toHexString(fn)); }

    // value loaded into register rn by the nearest preceding PC-relative mov in the block
    Long poolValue(Instruction at, int rn, Listing L) {
        Instruction p = at;
        for (int k = 0; k < 12; k++) {
            p = L.getInstructionBefore(p.getAddress());
            if (p == null || p.getFlowType().isJump() || p.getFlowType().isCall() || p.getFlowType().isTerminal()) return null;
            int op = u16(p.getAddress().getOffset());
            long pc = p.getAddress().getOffset();
            if ((op & 0xF000) == 0xD000 && ((op >> 8) & 15) == rn) return u32(((pc + 4) & ~3L) + (op & 0xFF) * 4L);
            if ((op & 0xF000) == 0xE000 && ((op >> 8) & 15) == rn) return ((long) (byte) (op & 0xFF)) & 0xFFFFFFFFL;
            for (Object o : p.getResultObjects()) if (o instanceof ghidra.program.model.lang.Register && ((ghidra.program.model.lang.Register) o).getName().equals("r" + rn)) return null;
        }
        return null;
    }

    @Override
    public void run() throws Exception {
        mem = currentProgram.getMemory();
        Listing L = currentProgram.getListing();
        FunctionManager fm = currentProgram.getFunctionManager();
        MemoryBlock img = mem.getBlock("image");
        Map<Long, Integer> fpscrVals = new TreeMap<>();
        for (Function f : fm.getFunctions(true)) {
            long fe = f.getEntryPoint().getOffset();
            if (!img.contains(f.getEntryPoint())) continue;
            String ar = area(fe);
            Set<Address> targetsInBody = new HashSet<>();
            for (Instruction i : L.getInstructions(f.getBody(), true)) for (Address fl : i.getFlows()) if (f.getBody().contains(fl)) targetsInBody.add(fl);
            for (Instruction i : L.getInstructions(f.getBody(), true)) {
                long pc = i.getAddress().getOffset();
                int op = u16(pc);
                String m = i.getMnemonicString().toLowerCase();
                String t = i.toString().toLowerCase();
                inc(ar, "insn");
                if (m.equals("jsr") || m.equals("jmp")) {
                    int rn = (op >> 8) & 15;
                    Long v = poolValue(i, rn, L);
                    if (v != null) {
                        inc(ar, m + "_pool");
                        if (fm.getFunctionAt(toAddr(v)) == null && !(v >= 0x8C0000B0L && v <= 0x8C0000E0L)) { inc(ar, m + "_pool_nonfn"); where(m + "_pool_nonfn", fe); }
                        if ((v & 0xE0000000L) == 0xA0000000L && v >= 0xAC010000L && v < 0xAC240000L) { inc(ar, "jump_to_P2_mirror"); where("jump_to_P2_mirror", fe); }
                        if (!img.contains(toAddr(v)) && !(v >= 0xAC010000L && v < 0xAC240000L)) { inc(ar, m + "_outside_image"); where(m + "_outside_image", fe); }
                    } else {
                        // load through memory (table, struct field, BIOS vector) or computed
                        inc(ar, m + "_reg_indirect"); where(m + "_reg_indirect", fe);
                        if (m.equals("jmp") && i.getFlows().length > 1) { inc(ar, "jmp_table_recovered"); where("jmp_table_recovered", fe); }
                    }
                }
                if (m.equals("braf") || m.equals("bsrf")) {
                    inc(ar, m); where(m, fe);
                    if (i.getFlows().length > 1) inc(ar, m + "_table_recovered");
                }
                if (m.equals("rte")) { inc(ar, "rte"); where("rte", fe); }
                if (m.equals("trapa") || m.equals("sleep") || m.equals("ldtlb")) { inc(ar, m); where(m, fe); }
                if (m.equals("fschg")) inc(ar, "fschg");
                if (m.equals("frchg")) { inc(ar, "frchg"); }
                if (m.startsWith("lds") && t.contains("fpscr")) {
                    inc(ar, "lds_fpscr");
                    if (m.equals("lds")) {
                        int rm = (op >> 8) & 15;
                        Long v = poolValue(i, rm, L);
                        if (v != null) fpscrVals.merge(v, 1, Integer::sum); else inc(ar, "lds_fpscr_computed");
                    } else inc(ar, "lds_fpscr_from_stack");
                }
                if (m.startsWith("sts") && t.contains("fpscr")) inc(ar, "sts_fpscr");
                if (m.startsWith("ldc") && t.matches(".*,(sr|ssr|spc|vbr|dbr|gbr|r[0-7]_bank)")) { String k = "ldc_" + t.replaceAll(".*,", ""); inc(ar, k); if (!k.endsWith("gbr")) where(k, fe); }
                if (m.startsWith("stc") && t.matches("stc.*(sr|ssr|spc|vbr|dbr|sgr|r[0-7]_bank).*")) inc(ar, "stc_system");
                if (m.equals("pref")) inc(ar, "pref");
                if (m.equals("ocbi") || m.equals("ocbp") || m.equals("ocbwb") || m.startsWith("movca")) { inc(ar, m); where(m, fe); }
                if (m.equals("ftrv") || m.equals("fipr") || m.equals("fsca") || m.equals("fsrra")) inc(ar, m);
                if (m.startsWith("mac.") || m.equals("clrmac") || m.equals("div1") || m.equals("div0s") || m.equals("div0u") || m.equals("dmuls.l") || m.equals("dmulu.l")) inc(ar, "mac_div_" + m);
                if (m.equals("tas.b")) { inc(ar, "tas"); where("tas", fe); }
                // delay slots
                if (i.getFlowType().isJump() || i.getFlowType().isCall() || i.getFlowType().isTerminal()) {
                    boolean delayed = m.matches("bra|bsr|jmp|jsr|rts|rte|braf|bsrf|bt/s|bf/s");
                    if (delayed) {
                        Instruction ds = L.getInstructionAfter(i.getAddress());
                        if (ds == null) continue;
                        int dop = u16(ds.getAddress().getOffset());
                        String dm = ds.getMnemonicString().toLowerCase().replace("_", "");
                        inc(ar, "delay_slot");
                        if ((dop & 0xF000) == 0xD000 || (dop & 0xF000) == 0x9000 || (dop & 0xFF00) == 0xC700) inc(ar, "delay_slot_pc_relative");
                        if (targetsInBody.contains(ds.getAddress())) { inc(ar, "delay_slot_is_branch_target"); where("delay_slot_is_branch_target", fe); }
                        if (m.equals("rts") && ds.toString().toLowerCase().contains(",pr")) { inc(ar, "rts_slot_writes_pr"); where("rts_slot_writes_pr", fe); }
                        if (m.matches("jsr|jmp|braf|bsrf")) {
                            int rn = (op >> 8) & 15;
                            for (Object o : ds.getResultObjects()) if (o instanceof ghidra.program.model.lang.Register && ((ghidra.program.model.lang.Register) o).getName().equals("r" + rn)) { inc(ar, "slot_writes_jump_reg"); where("slot_writes_jump_reg", fe); }
                        }
                        if (dm.startsWith("fschg") || dm.startsWith("frchg")) inc(ar, "slot_fpu_mode_switch");
                        if (dm.startsWith("lds") && ds.toString().toLowerCase().contains("fpscr")) inc(ar, "slot_fpscr_load");
                    }
                }
            }
        }
        // function pointers in data (outside function bodies), and runs of them (tables)
        byte[] d = new byte[(int) img.getSize()];
        img.getBytes(img.getStart(), d);
        long base = img.getStart().getOffset();
        int words = 0, tables = 0, inTable = 0, run = 0; long maxRun = 0;
        Set<Long> targets = new HashSet<>();
        for (int o = 0; o + 4 <= d.length; o += 4) {
            long v = (d[o] & 0xFFL) | (d[o + 1] & 0xFFL) << 8 | (d[o + 2] & 0xFFL) << 16 | (d[o + 3] & 0xFFL) << 24;
            Address wa = toAddr(base + o);
            boolean isFp = fm.getFunctionContaining(wa) == null && v >= base && v < base + d.length && fm.getFunctionAt(toAddr(v)) != null;
            if (isFp) { words++; targets.add(v); run++; }
            else { if (run >= 3) { tables++; inTable += run; maxRun = Math.max(maxRun, run); } run = 0; }
        }
        println("data function-pointer words " + words + " distinct targets " + targets.size() + " tables(>=3 consecutive) " + tables + " words in tables " + inTable + " longest " + maxRun);
        for (Map.Entry<String, Map<String, Integer>> e : C.entrySet())
            println(String.format("%-28s %s %s", e.getKey(), e.getValue(), W.containsKey(e.getKey()) ? W.get(e.getKey()) : ""));
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<Long, Integer> e : fpscrVals.entrySet()) sb.append(String.format("%x:%d ", e.getKey(), e.getValue()));
        println("fpscr literal values " + sb);
    }
}
