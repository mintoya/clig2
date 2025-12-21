const c = @cImport({
    @cInclude("term_input.h");
    @cInclude("term_screen.h");
});
const timespec = extern struct {
    seconds: c.time_t,
    nanoseconds: c.time_t,
};
extern fn nanosleep(*const timespec, ?*timespec) callconv(.c) c_int;
pub fn msleep(millis: u32) void {
    const seconds = millis / 1000;
    const nanoseconds = millis % 1000 * 1_000_000;
    _ = nanosleep(&timespec{
        .seconds = seconds,
        .nanoseconds = nanoseconds,
    }, null);
}
const std = @import("std");
pub fn main() !void {
    std.debug.print("hello world\n", .{});
    std.debug.print("{}\n", .{c.term_getInput(1)});
    for (1..10) |is| {
        const i: i32 = @intCast(is);
        c.term_setCell_Ref(
            c.term_position{ .row = i, .col = i },
            c.term_makeCell(
                @as(c.wchar, '└'),
                c.term_color_fromIdx(255),
                c.term_color_fromIdx(0),
                c.term_cell_VISIBLE,
            ),
        );
    }
    c.term_render();
    msleep(2000);
    c.term_dump();
}
