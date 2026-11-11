// Diagnostic: mnemonic sequence (no operands) of functions at the given addresses.
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;

public class S1Mnem extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String a : getScriptArgs()) {
            Function f = getFunctionAt(toAddr(Long.decode(a)));
            if (f == null) { println(a + " none"); continue; }
            StringBuilder sb = new StringBuilder(f.getName() + " callers " + f.getCallingFunctions(monitor).size() + ":");
            int n = 0;
            for (Instruction i : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                sb.append(' ').append(i.getMnemonicString());
                if (i.getFlowType().isCall() || i.getFlowType().isJump()) {
                    ghidra.program.model.address.Address[] fl = i.getFlows();
                    if (fl.length == 1) sb.append("->").append(fl[0]);
                }
                if (++n > 24) { sb.append(" ..."); break; }
            }
            println(sb.toString());
        }
    }
}
