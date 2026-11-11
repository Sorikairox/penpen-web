// S4: decompile the functions containing the given addresses (hex, 0x optional) and print the pseudo-C,
// every line prefixed with "  " (so tools/ghidra.sh's caller can filter). Read-only: nothing is saved.
// Output is disc-derived (it is the game's code): read it in the terminal, never commit it.
//@category PenPen
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;

public class S4Decomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        FunctionManager fm = currentProgram.getFunctionManager();
        for (String a : getScriptArgs()) {
            // "@ADDR": decompile the function whose address is stored in the word at ADDR
            long v = Long.parseLong(a.replace("@", "").replace("0x", ""), 16);
            if (a.startsWith("@")) v = getInt(toAddr(v)) & 0xFFFFFFFFL;
            Function f = fm.getFunctionContaining(toAddr(v));
            if (f == null) { println("S4Decomp: no function at " + a); continue; }
            DecompileResults r = d.decompileFunction(f, 60, monitor);
            System.out.println("S4D == " + f.getName() + " @ " + f.getEntryPoint());
            if (r.getDecompiledFunction() == null) { System.out.println("S4D failed"); continue; }
            String src = r.getDecompiledFunction().getC();
            for (String l : src.split("\n"))
                if (!l.trim().isEmpty()) System.out.println("S4D " + l);
            // the values of the literal-pool words the pseudo-C names PTR_..._ADDR / DAT_ADDR
            java.util.regex.Matcher m = java.util.regex.Pattern.compile("(PTR|DAT)_[A-Za-z0-9_]*?([0-9a-f]{8})\\b").matcher(src);
            java.util.TreeSet<String> seen = new java.util.TreeSet<>();
            while (m.find()) seen.add(m.group(2));
            StringBuilder b = new StringBuilder();
            for (String w : seen) {
                try { b.append(" ").append(w).append("=").append(String.format("%08x", getInt(toAddr(Long.parseLong(w, 16))))); }
                catch (Exception e) { }
            }
            System.out.println("S4D // words:" + b);
        }
    }
}
