//! Zig: stderr banner, then merge-gate.
const std = @import("std");

pub fn main() u8 {
    std.io.getStdErr().writeAll("OMNIGATE :: zig channel :: invoking make merge-gate\n") catch {};

    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const alloc = gpa.allocator();

    const root = std.posix.getenv("CREATION_OS_ROOT") orelse ".";
    std.posix.chdir(root) catch return 1;

    const argv = [_][]const u8{ "make", "merge-gate" };
    const r = std.process.Child.run(.{
        .allocator = alloc,
        .argv = &argv,
        .max_output_bytes = 128 * 1024 * 1024,
    }) catch return 1;
    defer {
        alloc.free(r.stdout);
        alloc.free(r.stderr);
    }
    return switch (r.term) {
        .Exited => |c| if (c == 0) 0 else 1,
        else => 1,
    };
}
