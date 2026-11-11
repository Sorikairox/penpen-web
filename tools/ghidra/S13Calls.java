// S13: the call graph of the game's functions: "S4D call FROM TO" per direct call (bsr, jsr through a literal-pool
// address, jmp tail calls), from the functions in the given range. Addresses only. Read-only: nothing is saved.
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

public class S13Calls extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        long lo = Long.parseLong(a[0].replace("0x", ""), 16), hi = Long.parseLong(a[1].replace("0x", ""), 16);
        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceManager rm = currentProgram.getReferenceManager();
        for (Function f : fm.getFunctions(toAddr(lo), true)) {
            if (f.getEntryPoint().getOffset() >= hi) break;
            java.util.TreeSet<Long> out = new java.util.TreeSet<>();
            for (Function c : f.getCalledFunctions(monitor)) out.add(c.getEntryPoint().getOffset());
            StringBuilder b = new StringBuilder();
            for (long t : out) b.append(String.format(" %08x", t));
            System.out.println(String.format("S4D call %08x%s", f.getEntryPoint().getOffset(), b));
        }
    }
}
