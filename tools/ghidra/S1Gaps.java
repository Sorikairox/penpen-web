// Lists address ranges of the image without instructions (>= arg bytes, default 2048), with counts.
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;

public class S1Gaps extends GhidraScript {
    @Override
    public void run() throws Exception {
        Listing L = currentProgram.getListing();
        MemoryBlock img = currentProgram.getMemory().getBlock("image");
        String[] a = getScriptArgs();
        long min = a.length > 0 ? Long.decode(a[0]) : 2048;
        long prevEnd = img.getStart().getOffset();
        Memory mem = currentProgram.getMemory();
        for (Instruction i : L.getInstructions(img.getStart(), true)) {
            long s = i.getAddress().getOffset();
            if (s > img.getEnd().getOffset()) break;
            if (s - prevEnd >= min) {
                long z = 0, rts = 0, pr = 0;
                for (long o = prevEnd; o < s; o += 2) {
                    int h = mem.getShort(toAddr(o)) & 0xFFFF;
                    if (h == 0) z += 2; if (h == 0x000B) rts++; if (h == 0x4F22) pr++;
                }
                println(String.format("gap %08x-%08x %7d zero %7d rts %4d stspr %4d", prevEnd, s, s - prevEnd, z, rts, pr));
            }
            prevEnd = s + i.getLength();
        }
        long e = img.getEnd().getOffset() + 1;
        println(String.format("tail %08x-%08x %d", prevEnd, e, e - prevEnd));
    }
}
