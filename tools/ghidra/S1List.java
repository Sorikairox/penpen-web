// Lists functions in [arg0, arg1): address, size, number of callers/refs, and ref source kinds.
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

public class S1List extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Address s = toAddr(Long.decode(a[0])), e = toAddr(Long.decode(a[1]));
        ReferenceManager rm = currentProgram.getReferenceManager();
        for (Function f : currentProgram.getFunctionManager().getFunctions(s, true)) {
            if (f.getEntryPoint().compareTo(e) >= 0) break;
            int calls = 0, data = 0, other = 0; String from = "";
            for (Reference r : rm.getReferencesTo(f.getEntryPoint())) {
                if (r.getReferenceType().isCall()) calls++;
                else if (r.getReferenceType().isData()) data++;
                else other++;
                if (from.length() < 60) from += r.getFromAddress() + " ";
            }
            println(String.format("%s %6d calls %3d data %3d other %3d %s", f.getEntryPoint(), f.getBody().getNumAddresses(), calls, data, other, from));
        }
    }
}
