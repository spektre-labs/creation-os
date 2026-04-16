//! Zig 0.13+ shim: `zig run infra/cos_gate.zig` (from `creation-os/`) runs `make merge-gate`.
//! Honors CREATION_OS_ROOT (default ".").
const std = @import("std");

pub fn main() u8 {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const alloc = gpa.allocator();

    const root = std.posix.getenv("CREATION_OS_ROOT") orelse ".";
    std.posix.chdir(root) catch {
        std.debug.print("cos_gate.zig: chdir {s}\n", .{root});
        return 1;
    };

    const argv = [_][]const u8{ "make", "merge-gate" };
    const r = std.process.Child.run(.{
        .allocator = alloc,
        .argv = &argv,
        .max_output_bytes = 64 * 1024 * 1024,
    }) catch {
        return 1;
    };
    defer {
        alloc.free(r.stdout);
        alloc.free(r.stderr);
    }
    return switch (r.term) {
        .Exited => |c| if (c == 0) 0 else 1,
        else => 1,
    };
}
