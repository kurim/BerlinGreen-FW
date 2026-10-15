// Create functions at Xtensa 'entry aN, imm' prologues that Ghidra missed (callbacks via pointers).
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;

public class FixFuncs extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        FunctionManager fm = currentProgram.getFunctionManager();
        Listing l = currentProgram.getListing();
        Address start = toAddr(0x400d0018L), end = toAddr(0x400d0018L + 0x125c6cL - 4);
        int created = 0;
        for (Address a = start; a.compareTo(end) < 0; a = a.add(1)) {
            if (monitor.isCancelled()) break;
            if ((mem.getByte(a) & 0xff) != 0x36) continue;
            int b1 = mem.getByte(a.add(1)) & 0xff;
            if ((b1 & 0x0f) != 0x01) continue;           // entry a1, imm
            if (fm.getFunctionContaining(a) != null) continue;
            // only if the area is not a defined data item (literal pool)
            Data d = l.getDefinedDataAt(a);
            if (d != null) continue;
            new DisassembleCommand(a, null, true).applyTo(currentProgram, monitor);
            if (fm.getFunctionAt(a) == null) {
                createFunction(a, null);
            }
            if (fm.getFunctionAt(a) != null) created++;
        }
        println("functions created: " + created + ", total now: " + fm.getFunctionCount());
    }
}
