// S2: push the assigned names of port/symbols/functions.tsv (mounted read-only
// at /symbols) into the Ghidra project, so later scripts and sessions see them.
// Names beginning with FUN_ or j_ are left alone. Removes the bogus function S1
// created on the static-constructor table at 0x8C23D900 (data, not code).
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.*;

public class S2Names extends GhidraScript {
    @Override
    public void run() throws Exception {
        FunctionManager fm = currentProgram.getFunctionManager();
        int n = 0;
        for (String line : Files.readAllLines(Paths.get("/symbols/functions.tsv"))) {
            if (line.startsWith("#")) continue;
            String[] c = line.split("\t");
            if (c.length < 5) continue;
            Function f = fm.getFunctionAt(toAddr(Long.parseLong(c[0], 16)));
            if (f == null) continue;
            if (c[4].equals("data")) { fm.removeFunction(f.getEntryPoint()); continue; }
            String nm = c[3];
            if (nm.startsWith("FUN_") || nm.startsWith("j_") || nm.equals(f.getName())) continue;
            f.setName(nm, SourceType.USER_DEFINED);
            n++;
        }
        println("S2Names: renamed " + n);
    }
}
