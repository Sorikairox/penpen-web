// Pre-analysis setup for 1ST_READ.BIN (Pen Pen TriIcelon, US), session S1.
// Run by analyzeHeadless as -preScript after a raw-binary import at 0x8C010000.
// Adds the rest of main RAM (uninitialised) around the image, the SH-4 on-chip
// register areas and the Holly/G2 register windows, and the entry point.
//@category PenPen
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SourceType;

public class S1Setup extends GhidraScript {
    private void ublock(String name, long start, long size, boolean x, boolean vol) throws Exception {
        Memory mem = currentProgram.getMemory();
        Address a = toAddr(start);
        if (mem.getBlock(a) != null) return;
        MemoryBlock b = mem.createUninitializedBlock(name, a, size, false);
        b.setRead(true); b.setWrite(true); b.setExecute(x); b.setVolatile(vol);
    }

    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        MemoryBlock img = mem.getBlock(toAddr(0x8C010000L));
        img.setName("image");
        img.setRead(true); img.setWrite(true); img.setExecute(true);
        long end = img.getEnd().getOffset() + 1;
        long aend = (end + 0xFFF) & ~0xFFFL;
        ublock("sysram", 0x8C000000L, 0x10000, false, false);
        if (aend > end) {
            // tail of the image page: keep it in its own block so 'image' stays exact
            ublock("bss0", end, aend - end, false, false);
        }
        ublock("ram", aend, 0x8D000000L - aend, false, false);
        // Holly / PowerVR2 / G1 / G2 / Maple registers (P2 mirror the SDK uses)
        ublock("hw_a05f", 0xA05F0000L, 0x10000, false, true);
        ublock("aica_reg", 0xA0700000L, 0x10000, false, true);
        ublock("aica_ram", 0xA0800000L, 0x200000, false, true);
        ublock("vram64", 0xA4000000L, 0x800000, false, true);
        ublock("vram32", 0xA5000000L, 0x800000, false, true);
        ublock("ta_fifo", 0x10000000L, 0x1000000, false, true);
        // SH-4 store queues, on-chip registers (P4)
        ublock("sq", 0xE0000000L, 0x4000000, false, true);
        ublock("p4_ff", 0xFF000000L, 0x1000000, false, true);

        Address entry = toAddr(0x8C010000L);
        currentProgram.getSymbolTable().addExternalEntryPoint(entry);
        createLabel(entry, "start", true, SourceType.USER_DEFINED);
        disassemble(entry);
        createFunction(entry, "start");
        println("S1Setup: image " + img.getStart() + "-" + img.getEnd());
    }
}
