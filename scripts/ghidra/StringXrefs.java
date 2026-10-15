// Dump keyword strings, their xref'd functions, and decompiled code of those functions.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.data.*;
import ghidra.program.util.DefinedStringIterator;
import java.io.*;
import java.util.*;

public class StringXrefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        String outDir = getScriptArgs().length > 0 ? getScriptArgs()[0] : ".";
        String[] kw = {"Stripe","switchtime","duration","onOffOverride","timeofday","wrong packet","wrong checksum",
            "SimpleBLE","Characteristic","Breathe","Water","water","temperature","Light ","VoxdaleTest","Wifi","WiFi AP",
            "firmware.bin","EEPROM","eeprom","schedule","shedule","override","brightness","%"};
        Listing l = currentProgram.getListing();
        ReferenceManager rm = currentProgram.getReferenceManager();
        Map<Function, List<String>> fnStrings = new LinkedHashMap<>();
        PrintWriter idx = new PrintWriter(new FileWriter(outDir + "/string_xrefs.txt"));
        PrintWriter all = new PrintWriter(new FileWriter(outDir + "/all_strings.txt"));
        for (Data d : DefinedStringIterator.forProgram(currentProgram)) {
            String s = d.getDefaultValueRepresentation();
            all.println(d.getAddress() + "\t" + s);
            boolean hit = false;
            for (String k : kw) if (s.contains(k)) { hit = true; break; }
            if (!hit) continue;
            for (Reference r : rm.getReferencesTo(d.getAddress())) {
                Function f = getFunctionContaining(r.getFromAddress());
                String fn = f == null ? "(none)" : f.getName() + "@" + f.getEntryPoint();
                idx.println(d.getAddress() + "\t" + s + "\t<- " + r.getFromAddress() + " in " + fn);
                if (f != null) fnStrings.computeIfAbsent(f, x -> new ArrayList<>()).add(s);
            }
        }
        idx.close(); all.close();
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        PrintWriter dec = new PrintWriter(new FileWriter(outDir + "/decompiled_app.c"));
        for (Map.Entry<Function, List<String>> e : fnStrings.entrySet()) {
            Function f = e.getKey();
            DecompileResults r = di.decompileFunction(f, 60, monitor);
            dec.println("// ===== " + f.getName() + " @ " + f.getEntryPoint() + " strings: " + e.getValue().size());
            if (r.decompileCompleted()) dec.println(r.getDecompiledFunction().getC());
            else dec.println("// decompile failed: " + r.getErrorMessage());
        }
        dec.close();
        println("functions total: " + currentProgram.getFunctionManager().getFunctionCount()
                + ", app functions dumped: " + fnStrings.size());
    }
}
