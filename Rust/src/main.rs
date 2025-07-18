use std::fs;
use std::process::exit;
use std::io::Write;

fn main() -> std::io::Result<()> {
    let args = std::env::args().collect::<Vec<_>>();
    if args.len() != 2 {
        eprintln!("Usage: cargo run -- <file>");
        exit(1);
    }

    let str = fs::read_to_string(&args[1])?;

    let mut file = fs::OpenOptions::new().create(true).write(true).truncate(true).open("out.s")?;

    let mut loop_stack: Vec<usize> = Vec::new();
    let mut loop_count = 0;

    writeln!(file,
            ".section __TEXT, __text\n
            .globl _start\n
            .align 2\n
            .globl _start\n
            .extern _exit\n
            _start:\n
            adrp x21, _tape@PAGE\n
            add x21, x21, _tape@PAGEOFF")?;

    // Generate code for brainfuck operators
    for (i, c) in str.chars().enumerate() {
        match c {
            '>' => { writeln!(file, "add x21, x21, #1")?; },
            '<' => { writeln!(file, "sub x21, x21, #1")?; },
            '[' => {
                let label = loop_count;
                loop_count += 1;
                loop_stack.push(label);
                writeln!(file,
                         "loop_start_{label}:\n
                         \tldrb w22, [x21]\n
                         \tcbz w22, loop_end_{label}")?;
            },
            ']' => {
                if let Some(label) = loop_stack.pop() {
                    writeln!(file, "\tb loop_start_{label}\nloop_end_{label}:")?;
                } else {
                    eprintln!("Unmatched closing bracket at position: {i}");
                }
            },
            '.' => {
                writeln!(file,
                        "mov x0, #1\n
                        mov x1, x21\n\
                        mov x2, #1\n\
                        bl _write\n")?;
            },
            ',' => {
                writeln!(file,
                        "mov x0, #0\n\
                        mov x1, x21\n\
                        mov x2, #1\n\
                        bl _read")?;
            },
            '+' => {
                writeln!(file,
                        "ldrb w0, [x21]\n\
                        add w0, w0, #1\n
                        strb w0, [x21]")?;
            },
            '-' => {
                writeln!(file,
                         "ldrb w0, [x21]\n\
                         sub w0, w0, #1\n\
                         strb w0, [x21]")?;
            },
            _ => {}
        }
    }

    writeln!(file,
            "mov x0, #0\nbl _exit\n
            .section __DATA, __bss\n
            .balign 16\n
            .globl _tape\n
            _tape:\n
            .skip 30000")?;

    if !loop_stack.is_empty() {
        eprintln!("Unmatched closing bracket(s): {loop_stack:?}");
        exit(1);
    }

    let status = std::process::Command::new("clang")
        .args(["-arch", "arm64", "-nostartfiles", "-Wl,-e,_start", "-o", "out", "out.s"])
        .status()?;

    if !status.success() {
        eprintln!("Clang failed with status: {}", status);
        exit(1);
    }

    Ok(())
}
