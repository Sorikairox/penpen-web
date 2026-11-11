// S13: S4Decomp with the code range made read-only and FPSCR.SZ / FPSCR.PR known to be 0 (the game's mode), so the
// decompiler folds the literal-pool constants into the pseudo-C and drops the 64-bit fmov branches. Nothing is saved
// (-readOnly). Output is disc-derived (it is the game's code): read it in the terminal or the gitignored port_work/,
// never commit it, never paste it into docs.
//@category PenPen
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import java.math.BigInteger;

public class S13Decomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        Address lo = toAddr(0x8C010000L), hi = toAddr(0x8C066900L);
        // the game's code (and its literal pools) never changes at run time: a read-only block is a constant
        MemoryBlock b = mem.getBlock(lo);
        if (b.getStart().compareTo(lo) < 0) { mem.split(b, lo); b = mem.getBlock(lo); }
        if (b.getEnd().compareTo(hi) > 0) mem.split(b, hi);
        mem.getBlock(lo).setWrite(false);
        System.out.println("S4D // code block " + mem.getBlock(lo).getStart() + "-" + mem.getBlock(lo).getEnd() + " write " + mem.getBlock(lo).isWrite());
        ProgramContext pc = currentProgram.getProgramContext();
        for (String n : new String[] {"FPSCR_SZ", "FPSCR_PR", "fpscr_sz", "fpscr_pr"}) {
            Register r = currentProgram.getRegister(n);
            if (r != null) {
                try { pc.setValue(r, lo, toAddr(0x8C197090L), BigInteger.ZERO); }
                catch (Exception e) { System.out.println("S4D // " + n + ": " + e.getMessage()); }
            }
        }
        DecompInterface d = new DecompInterface();
        DecompileOptions o = new DecompileOptions();
        o.grabFromProgram(currentProgram);
        o.setRespectReadOnly(true);
        d.setOptions(o);
        d.openProgram(currentProgram);
        FunctionManager fm = currentProgram.getFunctionManager();
        for (String a : getScriptArgs()) {
            long v = Long.parseLong(a.replace("@", "").replace("0x", ""), 16);
            if (a.startsWith("@")) v = getInt(toAddr(v)) & 0xFFFFFFFFL;
            Function f = fm.getFunctionContaining(toAddr(v));
            if (f == null) { System.out.println("S4D // no function at " + a); continue; }
            DecompileResults r = d.decompileFunction(f, 60, monitor);
            System.out.println("S4D == " + f.getName() + " @ " + f.getEntryPoint() + " size " + f.getBody().getNumAddresses());
            if (r.getDecompiledFunction() == null) { System.out.println("S4D failed"); continue; }
            String src = r.getDecompiledFunction().getC();
            for (String l : src.split("\n"))
                if (!l.trim().isEmpty()) System.out.println("S4D " + l);
            java.util.regex.Matcher m = java.util.regex.Pattern.compile("(PTR|DAT)_[A-Za-z0-9_]*?([0-9a-f]{8})\\b").matcher(src);
            java.util.TreeSet<String> seen = new java.util.TreeSet<>();
            while (m.find()) seen.add(m.group(2));
            StringBuilder sb = new StringBuilder();
            for (String w : seen) {
                try { sb.append(" ").append(w).append("=").append(String.format("%08x", getInt(toAddr(Long.parseLong(w, 16))))); }
                catch (Exception e) { }
            }
            System.out.println("S4D // words:" + sb);
        }
    }
}
