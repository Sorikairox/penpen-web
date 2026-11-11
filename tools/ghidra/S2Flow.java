// S2: data-flow summary of SDK functions (no code text). For each function:
// prototype, field accesses relative to parameters (P<k>+off:size R/W),
// global words read/written (G<addr>, with constant stores), calls with an
// argument descriptor each (P<k>, #const, G<addr>, *(...) loads, ret(fn)),
// return value descriptor, constants compared, float constants.
// Descriptors are computed from the decompiler's p-code; they describe data
// flow in our own notation, not the code.
// Args: outfile [addr ...]  (no addresses: every SDK function the game calls,
// read from /work/s2/census.tsv)
//@category PenPen
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;

public class S2Flow extends GhidraScript {
    FunctionManager fm;

    // literal-pool words live inside the code ranges (read-only in practice)
    boolean lit(long o) { return (o >= 0x8C010000L && o < 0x8C066900L) || (o >= 0x8C0998D4L && o < 0x8C17994CL) || (o >= 0x8C18F112L && o < 0x8C197090L); }

    String g(long v) {
        Function f = fm.getFunctionAt(toAddr(v));
        if (f != null) return "fn" + (f.getName().startsWith("FUN_") ? Long.toHexString(v) : f.getName());
        if (v >= 0x8C000000L && v < 0x8D000000L) return "G" + Long.toHexString(v);
        if (v >= 0xA0000000L || (v >= 0x10000000L && v < 0x14000000L) || v >= 0xE0000000L) return "H" + Long.toHexString(v);
        if (v > 0xFFFF && v < 0xFFFF0000L) return "#" + Long.toHexString(v) + "h";
        return "#" + (int) v;
    }

    String desc(Varnode v, int d) {
        if (v == null) return "?";
        if (v.isConstant()) return g(v.getOffset() & 0xFFFFFFFFL);
        HighVariable hv = v.getHigh();
        if (hv instanceof HighParam) return "P" + ((HighParam) hv).getSlot();
        if (v.isAddress()) {
            long o = v.getOffset();
            if (v.getSize() == 4 && lit(o)) { try { return "=" + g(currentProgram.getMemory().getInt(toAddr(o)) & 0xFFFFFFFFL); } catch (Exception e) { } }
            return "G" + Long.toHexString(o);
        }
        PcodeOp def = v.getDef();
        if (def == null) {
            if (v.isRegister()) return "reg";
            return "in";
        }
        if (d <= 0) return "~";
        int op = def.getOpcode();
        switch (op) {
            case PcodeOp.COPY: case PcodeOp.CAST: case PcodeOp.INT_ZEXT: case PcodeOp.INT_SEXT: case PcodeOp.SUBPIECE:
                return desc(def.getInput(0), d);
            case PcodeOp.INT_ADD: case PcodeOp.PTRSUB: case PcodeOp.PTRADD: {
                Varnode a = def.getInput(0), b = def.getInput(1);
                if (b.isConstant() && op != PcodeOp.PTRADD) return desc(a, d - 1) + "+" + Long.toHexString(b.getOffset() & 0xFFFFFFFFL);
                if (a.isConstant() && op != PcodeOp.PTRADD) {
                    long av = a.getOffset() & 0xFFFFFFFFL;
                    if (av >= 0x8C000000L) return g(av + (b.isConstant() ? b.getOffset() : 0)) + (b.isConstant() ? "" : "[" + desc(b, d - 1) + "]");
                    return desc(b, d - 1) + "+" + Long.toHexString(av);
                }
                if (op == PcodeOp.PTRADD) return desc(a, d - 1) + "[" + desc(b, d - 1) + "*" + def.getInput(2).getOffset() + "]";
                return "(" + desc(a, d - 1) + "+" + desc(b, d - 1) + ")";
            }
            case PcodeOp.LOAD: {
                Varnode ad = def.getInput(1);
                if (ad.isConstant() && def.getOutput().getSize() == 4 && lit(ad.getOffset())) { try { return "=" + g(currentProgram.getMemory().getInt(toAddr(ad.getOffset())) & 0xFFFFFFFFL); } catch (Exception e) { } }
                return "*(" + desc(ad, d - 1) + ")";
            }
            case PcodeOp.CALL: return "ret(" + g(def.getInput(0).getOffset()) + ")";
            case PcodeOp.CALLIND: return "ret(ind)";
            case PcodeOp.MULTIEQUAL: return "phi";
            case PcodeOp.FLOAT_INT2FLOAT: return "i2f(" + desc(def.getInput(0), d - 1) + ")";
            case PcodeOp.FLOAT_MULT: return "fmul(" + desc(def.getInput(0), d - 1) + "," + desc(def.getInput(1), d - 1) + ")";
            default: return def.getMnemonic().toLowerCase().replace("int_", "").replace("float_", "f");
        }
    }

    @Override
    public void run() throws Exception {
        fm = currentProgram.getFunctionManager();
        String[] a = getScriptArgs();
        List<Long> addrs = new ArrayList<>();
        if (a.length > 1) for (int k = 1; k < a.length; k++) addrs.add(Long.parseLong(a[k].replace("0x", ""), 16));
        else for (String line : Files.readAllLines(Paths.get("/work/s2/census.tsv"))) addrs.add(Long.parseLong(line.split("\t")[0], 16));
        PrintWriter w = new PrintWriter(new FileWriter(a[0]));
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (long ad : addrs) {
            Function f = fm.getFunctionAt(toAddr(ad));
            if (f == null) { w.println(Long.toHexString(ad) + " nofunc"); continue; }
            DecompileResults dr = di.decompileFunction(f, 60, monitor);
            HighFunction hf = dr.getHighFunction();
            if (hf == null) { w.println(Long.toHexString(ad) + " nodecomp"); continue; }
            Set<String> acc = new LinkedHashSet<>(), calls = new LinkedHashSet<>(), rets = new LinkedHashSet<>(), cmps = new TreeSet<>(), fl = new TreeSet<>();
            int nind = 0;
            Iterator<PcodeOpAST> it = hf.getPcodeOps();
            while (it.hasNext()) {
                PcodeOpAST op = it.next();
                int oc = op.getOpcode();
                if (oc == PcodeOp.LOAD && op.getInput(1).isConstant() && lit(op.getInput(1).getOffset())) { }
                else if (oc == PcodeOp.LOAD) acc.add("R" + op.getOutput().getSize() + ":" + desc(op.getInput(1), 3));
                else if (oc == PcodeOp.STORE) acc.add("W" + op.getInput(2).getSize() + ":" + desc(op.getInput(1), 3) + "=" + desc(op.getInput(2), 2));
                else if (oc == PcodeOp.CALL || oc == PcodeOp.CALLIND) {
                    StringBuilder sb = new StringBuilder(oc == PcodeOp.CALL ? g(op.getInput(0).getOffset()) : "IND(" + desc(op.getInput(0), 2) + ")");
                    sb.append("(");
                    for (int k = 1; k < op.getNumInputs(); k++) sb.append(k > 1 ? "," : "").append(desc(op.getInput(k), 2));
                    sb.append(")");
                    calls.add(sb.toString());
                    if (oc == PcodeOp.CALLIND) nind++;
                } else if (oc == PcodeOp.RETURN && op.getNumInputs() > 1) rets.add(desc(op.getInput(1), 3));
                else if (oc == PcodeOp.INT_EQUAL || oc == PcodeOp.INT_NOTEQUAL || oc == PcodeOp.INT_LESS || oc == PcodeOp.INT_SLESS || oc == PcodeOp.INT_SLESSEQUAL || oc == PcodeOp.INT_LESSEQUAL || oc == PcodeOp.INT_AND) {
                    for (int k = 0; k < 2; k++) if (op.getInput(k).isConstant()) cmps.add((oc == PcodeOp.INT_AND ? "&" : "") + g(op.getInput(k).getOffset() & 0xFFFFFFFFL) + ":" + desc(op.getInput(1 - k), 2));
                }
                for (int k = 0; k < op.getNumInputs(); k++) {
                    Varnode in = op.getInput(k);
                    if (in.isConstant() && in.getSize() == 4 && (oc == PcodeOp.FLOAT_MULT || oc == PcodeOp.FLOAT_ADD || oc == PcodeOp.FLOAT_DIV || oc == PcodeOp.FLOAT_SUB || oc == PcodeOp.FLOAT_LESS || oc == PcodeOp.FLOAT_LESSEQUAL || oc == PcodeOp.FLOAT_EQUAL)) fl.add(Float.toString(Float.intBitsToFloat((int) in.getOffset())));
                }
            }
            FunctionPrototype fp = hf.getFunctionPrototype();
            StringBuilder ps = new StringBuilder();
            for (int k = 0; k < fp.getNumParams(); k++) {
                HighSymbol hs = fp.getParam(k);
                ps.append(k > 0 ? "," : "").append(hs.getStorage().isRegisterStorage() ? hs.getStorage().getRegister().getName() : "stk").append(":").append(hs.getDataType().getLength());
            }
            w.println("== " + Long.toHexString(ad) + " " + f.getName() + " (" + ps + ") ind=" + nind);
            w.println("  acc " + String.join(" ", acc));
            w.println("  call " + String.join(" ", calls));
            w.println("  ret " + String.join(" ", rets) + "  cmp " + String.join(" ", cmps) + "  flt " + String.join(" ", fl));
        }
        w.close();
        println("flow " + addrs.size());
    }
}
