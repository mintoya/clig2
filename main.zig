const c = @cImport({
    @cInclude("term_input.h");
    @cInclude("term_screen.h");
});
const std = @import("std");
pub fn main() !void {
    std.debug.print("hello world\n", .{});
    const five: c.uint = 5;
    std.debug.print("{}\n", .{five});
    std.debug.print("{}\n", .{c.term_getInput(1)});
    std.debug.print("{}\n", .{c.term_cell});
}
