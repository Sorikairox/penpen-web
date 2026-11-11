// S8: disassembly listing of [ADDR, ADDR+LEN) for each "ADDR:LEN" argument (hex), every line prefixed "S8A "
// (tools/ghidra.sh asm). Read-only. Output is disc-derived (the game's code): terminal only, never commit.
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;

public class S8Asm extends GhidraScript {
    @Override
    public void run() throws Exception {
        Listing l = currentProgram.getListing();
        for (String a : getScriptArgs()) {
            String[] p = a.split(":");
            long s = Long.parseLong(p[0].replace("0x", ""), 16), n = p.length > 1 ? Long.parseLong(p[1], 16) : 0x40;
            Function f = currentProgram.getFunctionManager().getFunctionContaining(toAddr(s));
            System.out.println("S8A == " + p[0] + (f != null ? " in " + f.getName() + " @ " + f.getEntryPoint() : ""));
            for (Address x = toAddr(s); x.getOffset() < s + n; ) {
                Instruction i = l.getInstructionAt(x);
                if (i == null) { Data d = l.getDataAt(x); System.out.println("S8A " + x + "  .data " + (d != null ? d.getDefaultValueRepresentation() : "?")); x = x.add(d != null ? Math.max(1, d.getLength()) : 2); continue; }
                StringBuilder sb = new StringBuilder();
                for (Object o : i.getReferencesFrom()) { ghidra.program.model.symbol.Reference r = (ghidra.program.model.symbol.Reference) o;
                    if (r.getReferenceType().isData()) { try { sb.append(String.format(" ; [%s]=%08x", r.getToAddress(), getInt(r.getToAddress()))); } catch (Exception e) {} } }
                System.out.println("S8A " + x + "  " + i + sb);
                x = x.add(i.getLength());
            }
        }
    }
}
