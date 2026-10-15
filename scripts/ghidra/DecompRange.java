// Decompile all functions whose entry lies in [lo,hi] (args: out lo hi) or specific addresses (args: out @addr ...)
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;
import java.util.*;

public class DecompRange extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter w = new PrintWriter(new FileWriter(a[0]));
        DecompInterface di = new DecompInterface(); di.openProgram(currentProgram);
        List<Function> fs = new ArrayList<>();
        FunctionManager fm = currentProgram.getFunctionManager();
        if (a[1].startsWith("@")) {
            for (int i = 1; i < a.length; i++) {
                Function f = fm.getFunctionContaining(toAddr(Long.parseLong(a[i].substring(1), 16)));
                if (f != null) fs.add(f);
            }
        } else {
            long lo = Long.parseLong(a[1], 16), hi = Long.parseLong(a[2], 16);
            for (Function f : fm.getFunctions(toAddr(lo), true)) {
                if (f.getEntryPoint().getOffset() > hi) break;
                fs.add(f);
            }
        }
        for (Function f : fs) {
            DecompileResults r = di.decompileFunction(f, 60, monitor);
            w.println("// ===== " + f.getName() + " @ " + f.getEntryPoint());
            w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// failed");
        }
        w.close();
        println("decompiled " + fs.size());
    }
}
