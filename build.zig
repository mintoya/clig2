const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "clig2",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });
    exe.linkLibC();
    if (target.result.os.tag != .windows) {
        exe.addCSourceFile(.{
            .file = b.path("textboxExample.c"),
            .flags = &.{
                "-g",
                "-w",
            },
        });
    } else {
        exe.addCSourceFile(.{
            .file = b.path("textboxExample.c"),
            .flags = &.{
                "-g",
                "-w",
                "-D_XOPEN_SOURCE=700",
                // "-D_POSIX_C_SOURCE=200809L",
            },
        });
    }

    b.installArtifact(exe);

    const run_step = b.step("run", "Run the app");

    const run_cmd = b.addRunArtifact(exe);
    run_step.dependOn(&run_cmd.step);

    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
}
