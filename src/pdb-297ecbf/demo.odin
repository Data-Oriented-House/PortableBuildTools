package demo
import "pdb"
import "core:fmt"

main ::proc() {
    // register exception handler
    pdb.SetUnhandledExceptionFilter(pdb.dump_stack_trace_on_exception)

    foo()
}

foo :: proc() {
    fmt.println("foo..")
    bar()
    fmt.println("...bar")
}

bar :: proc() {
    // trigger an out of range exception here
    aov := make([]uint, 8)
    for i in 0..=8 {
        v := aov[i]
        fmt.println(v)
    }
}
