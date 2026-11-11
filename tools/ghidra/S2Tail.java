// S2: for each given function: instruction count, mnemonic histogram, and the
// targets of branches that leave the function (tail calls). No operands printed.
// Args: fn ...
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import java.util.*;

public class S2Tail extends GhidraScript {
    @Override
    public void run() throws Exception {
        FunctionManager fm = currentProgram.getFunctionManager();
        for (String s : getScriptArgs()) {
            Function f = fm.getFunctionAt(toAddr(Long.parseLong(s, 16)));
            if (f == null) { println(s + " none"); continue; }
            Map<String, Integer> h = new TreeMap<>(); Set<String> out = new TreeSet<>();
            for (Instruction i : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                h.merge(i.getMnemonicString().toLowerCase(), 1, Integer::sum);
                for (Address a : i.getFlows()) if (!f.getBody().contains(a)) out.add((i.getFlowType().isCall() ? "c:" : "j:") + Long.toHexString(a.getOffset()));
            }
            println(s + " " + h + " out=" + out);
        }
    }
}
