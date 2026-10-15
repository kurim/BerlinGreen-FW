// List references to given function addresses (args: addr ...), with containing function.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.*;
import ghidra.program.model.listing.*;
public class Callers extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String s : getScriptArgs()) {
            println("== callers of " + s);
            for (Reference r : currentProgram.getReferenceManager().getReferencesTo(toAddr(Long.parseLong(s, 16)))) {
                Function f = getFunctionContaining(r.getFromAddress());
                println("  " + r.getFromAddress() + " " + r.getReferenceType() + " in " + (f == null ? "none" : f.getName()));
            }
        }
    }
}
