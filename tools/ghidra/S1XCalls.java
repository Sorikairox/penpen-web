// Call edges between the game range [arg0,arg1) and the rest of the image: counts,
// distinct targets, and the game functions the SDK side calls (addresses only).
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.util.*;

public class S1XCalls extends GhidraScript {
    @Override
    public void run() throws Exception {
        long gs = Long.decode(getScriptArgs()[0]), ge = Long.decode(getScriptArgs()[1]);
        int g2s = 0, s2g = 0; Set<Long> g2sT = new TreeSet<>(); Map<Long, Integer> s2gT = new TreeMap<>();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            long e = f.getEntryPoint().getOffset();
            boolean fg = e >= gs && e < ge;
            for (Function c : f.getCalledFunctions(monitor)) {
                long t = c.getEntryPoint().getOffset();
                boolean tg = t >= gs && t < ge;
                if (fg && !tg) { g2s++; g2sT.add(t); }
                if (!fg && tg) { s2g++; s2gT.merge(t, 1, Integer::sum); }
            }
        }
        println("game->sdk caller/callee pairs " + g2s + " distinct sdk targets " + g2sT.size());
        println("sdk->game pairs " + s2g + " distinct game targets " + s2gT.size() + " " + s2gT);
    }
}
