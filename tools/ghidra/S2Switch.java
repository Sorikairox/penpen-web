// S2: switch tables of the given functions as case value -> first call made in
// that case (callee address and the record fields it is passed). Addresses and
// integers only. Args: fn ...
//@category PenPen
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.*;
import java.util.*;

public class S2Switch extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        FunctionManager fm = currentProgram.getFunctionManager();
        Listing L = currentProgram.getListing();
        for (String s : getScriptArgs()) {
            Function f = fm.getFunctionAt(toAddr(Long.parseLong(s, 16)));
            HighFunction hf = di.decompileFunction(f, 60, monitor).getHighFunction();
            for (JumpTable jt : hf.getJumpTables()) {
                Integer[] lab = jt.getLabelValues();
                Address[] ad = jt.getCases();
                StringBuilder sb = new StringBuilder(String.format("%s switch@%x:", s, jt.getSwitchAddress().getOffset()));
                for (int k = 0; k < ad.length; k++) {
                    String callee = "-";
                    Instruction i = L.getInstructionAt(ad[k]);
                    for (int n = 0; i != null && n < 16; n++, i = i.getNext()) {
                        if (i.getFlowType().isCall()) {
                            Address[] fl = i.getFlows();
                            if (fl.length > 0) { callee = Long.toHexString(fl[0].getOffset()); }
                            else callee = "ind";
                            // second call (first is usually the offset->pointer relocation)
                            if (!callee.equals("8c012cf4")) break;
                        }
                        if (i.getFlowType().isJump() && !i.getFlowType().isConditional()) { if (callee.equals("-")) callee = "jmp" + (i.getFlows().length > 0 ? Long.toHexString(i.getFlows()[0].getOffset()) : ""); break; }
                    }
                    sb.append(String.format(" %d>%s", k < lab.length ? lab[k] : -1, callee));
                }
                println(sb.toString());
            }
        }
    }
}
