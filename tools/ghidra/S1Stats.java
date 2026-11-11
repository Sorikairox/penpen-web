// Quick statistics (counts only) after analysis.
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.MemoryBlock;

public class S1Stats extends GhidraScript {
    @Override
    public void run() throws Exception {
        MemoryBlock img = currentProgram.getMemory().getBlock("image");
        long base = img.getStart().getOffset();
        long[] win = new long[(int) ((img.getSize() + 0xFFFF) / 0x10000)];
        long[] inst = new long[win.length];
        int n = 0; long bytes = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (!img.contains(f.getEntryPoint())) continue;
            n++;
            long b = f.getBody().getNumAddresses();
            bytes += b;
            win[(int) ((f.getEntryPoint().getOffset() - base) >> 16)]++;
        }
        for (Instruction i : currentProgram.getListing().getInstructions(img.getStart(), true)) {
            if (!img.contains(i.getAddress())) break;
            inst[(int) ((i.getAddress().getOffset() - base) >> 16)] += i.getLength();
        }
        println("functions " + n + " bytes " + bytes);
        for (int k = 0; k < win.length; k++)
            println(String.format("win %06x funcs %5d insnbytes %6d", k << 16, win[k], inst[k]));
    }
}
