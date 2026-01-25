const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});

    const optimize = b.standardOptimizeOption(.{
        .preferred_optimize_mode = .Debug,
    });

    // zig test
    const exe = b.addExecutable(.{
        .name = "clig2",
        .root_module = b.createModule(.{
            // .root_source_file = b.path("main.zig"),
            .link_libc = true,
            .target = target,
            .optimize = optimize,
            .sanitize_c = .full,
        }),
    });

    exe.root_module.addCSourceFile(.{ .file = b.path("moveBench.c"), .flags = &.{
        "-w",
        "-D_GNU_SOURCE",
        // "-fsanitize=address",
        // "-lasan",
        "-std=c2y",
    } });

    b.installArtifact(exe);

    const run_step = b.step("run", "Run the app");

    const run_cmd = b.addRunArtifact(exe);
    run_step.dependOn(&run_cmd.step);

    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
}
